#include "ffmpeg_hwdevice.h"

#include <exception>
#include "ffmpeg_avutil.h"
#include <algorithm>
#include <stdexcept>
#include <thread>
extern "C" {
#include "libavutil/buffer.h"
}
namespace FFmpeg { 

/*******************************AVHWDeviceContext********************************************/
HWDeviceContext::HWDeviceContext(AVHWDeviceType type, const std::string& device_name) {
    ctx_ = Create(type, device_name);
    // if(!ctx_) {
    //     throw std::runtime_error("av_hwdevice_ctx_create failed");
    // }
}

HWDeviceContext::~HWDeviceContext() {
    Free(ctx_);
}

HWDeviceContext::HWDeviceContext(HWDeviceContext&& other) noexcept {
    move_from(other);
}

HWDeviceContext& HWDeviceContext::operator=(HWDeviceContext&& other) noexcept {
    if(this != &other) {
        Free(ctx_);
        move_from(other);
    }
    return *this;
}

AVHWDeviceType HWDeviceContext::getType() const noexcept {
    return GetType(ctx_);
}
AVBufferRef* HWDeviceContext::Create(AVHWDeviceType type, const std::string& device_name) {
    AVBufferRef* ctx = nullptr;

    if(type == AV_HWDEVICE_TYPE_NONE) {
        throw std::runtime_error("av_hwdevice_ctx_create failed, type: " + tools::hw_device_type_name(type));
    }
    int ret = av_hwdevice_ctx_create(&ctx, 
                                    type, 
                                    device_name.empty() ? nullptr : device_name.c_str(), 
                                    nullptr, 
                                    0);
    if(ret < 0) {
        throw std::runtime_error("av_hwdevice_ctx_create failed, ret: " + tools::av_err(ret));
    }
    return ctx;
}

void HWDeviceContext::Free(AVBufferRef* ctx) noexcept {
    av_buffer_unref(&ctx);
    ctx = nullptr;
}

AVHWDeviceType HWDeviceContext::GetType(AVBufferRef* ctx) noexcept {
    return ((AVHWDeviceContext*)ctx->data)->type;
}

bool HWDeviceContext::isValid() const noexcept {
    return ctx_ != nullptr;
}

void HWDeviceContext::move_from(HWDeviceContext& other) noexcept{
    ctx_ = other.ctx_;
    other.ctx_ = nullptr;
}

AVBufferRef* HWDeviceContext::raw() noexcept {
    return ctx_;
}

const AVBufferRef* HWDeviceContext::raw() const noexcept {
    return ctx_;
}

AVBufferRef* HWDeviceContext::get() noexcept {
    return ctx_;
}

const AVBufferRef* HWDeviceContext::get() const noexcept {
    return ctx_;
}    

/*******************************DeviceHandle********************************/

DeviceHandle::DeviceHandle(std::shared_ptr<HWDeviceEntry> entry) : entry_(std::move(entry))
{
}

DeviceHandle::DeviceHandle(DeviceHandle&& other) noexcept {
    move_from(other);
}

DeviceHandle& DeviceHandle::operator=(DeviceHandle&& other) noexcept {
    if(this != &other) {
        free();
        move_from(other);
    }
    return *this;
}

DeviceHandle::~DeviceHandle() {
    free();
}

void DeviceHandle::move_from(DeviceHandle& other) {
    entry_ = std::move(other.entry_);

    // 制空other
    other.entry_.reset();
}

void DeviceHandle::free() {
    if(entry_) {
        entry_->active_count.fetch_sub(1, std::memory_order_relaxed);
        entry_.reset();
    }
}

bool DeviceHandle::valid() const noexcept {
    return entry_ && entry_->hw_ctx && entry_->hw_ctx->isValid();
}

AVBufferRef* DeviceHandle::hw_device_ctx() const noexcept {
    if (!entry_ || !entry_->hw_ctx) {
        return nullptr;
    }
    return entry_->hw_ctx->get();
}

size_t DeviceHandle::id() const noexcept {
    return entry_ ? entry_->id : static_cast<size_t>(-1);
}

AVHWDeviceType DeviceHandle::type() const noexcept {
    return entry_ ? entry_->type : AV_HWDEVICE_TYPE_NONE;
}

const std::string& DeviceHandle::device_name() const noexcept {
    static const std::string empty;
    return entry_ ? entry_->device_name : empty;
}

int DeviceHandle::load() const noexcept {
    return entry_ ? static_cast<int>(entry_->active_count.load()) : -1;
}

/********************************HWDevicePool************************************************/
HWDevicePool::HWDevicePool(int health_check_interval, int check_stop_interval, int max_try_count, int retry_backoff) 
    : health_check_interval_(health_check_interval), check_stop_interval_(check_stop_interval), max_retry_count_(max_try_count), retry_backoff_(retry_backoff) {
    // 启动守护线程
    health_check_thread_ = std::thread(&HWDevicePool::health_check_loop, this);
}

HWDevicePool::~HWDevicePool() noexcept {
    // 停止守护线程
    health_check_stopped_.store(true);
    cv_.notify_all();
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
}

size_t HWDevicePool::AddDevice(AVHWDeviceType type, const std::string& name) { 
    std::unique_lock<std::shared_mutex> lock(shared_mutex_);
    std::size_t new_id = next_id_++;
    auto ctx = std::make_shared<HWDeviceContext>(type, name);

    auto entry = std::make_shared<HWDeviceEntry>(new_id, type, name, ctx);
    entries_.emplace_back(entry);

    cv_.notify_all();
    return new_id;
}

void HWDevicePool::RemoveDevice(size_t id) {
    std::unique_lock<std::shared_mutex> lock(shared_mutex_);
    auto it = std::find_if(entries_.begin(), entries_.end(), 
        [id](const std::shared_ptr<HWDeviceEntry>& entry) {
        return entry->id == id;
    });
    if (it == entries_.end()) {
        return;
    }

    // 等待active_count为0
    auto entry = *it;
    lock.unlock();  // 解锁等待
    // 阻塞等待释放
    while (entry->active_count.load(std::memory_order_acquire) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    lock.lock();    // 重新加锁
    // 不要直接erase(it)!!!
    // 其它线程可能已经将迭代器改变了！
    // entries_.erase(it);
     
    // 重新查找并删除
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), 
        [id](const std::shared_ptr<HWDeviceEntry>& e) {
            return e->id == id;
    }), entries_.end());
    
}

std::size_t HWDevicePool::size() const noexcept{
    std::shared_lock<std::shared_mutex> lock(shared_mutex_);
    return entries_.size();
}

std::vector<HWDevicePool::DeviceSnapshot> HWDevicePool::snapshots() const{
    std::shared_lock<std::shared_mutex> lock(shared_mutex_);
    std::vector<DeviceSnapshot> res;
    res.reserve(entries_.size());
    for (auto& e : entries_) {
        res.emplace_back(DeviceSnapshot{
            e->id,
            e->type,
            e->device_name,
            e->active_count.load(),
            e->healthy.load()
        });
    }
    return res;
}

std::optional<DeviceHandle> HWDevicePool::Acquire(SelectionStrategy strategy, 
                         std::optional<std::chrono::milliseconds> timeout,
                         std::optional<std::size_t> manual_idx) {
    auto deadline = timeout ? std::optional(std::chrono::steady_clock::now() + *timeout) : std::nullopt;
    while (true){
        {
            std::shared_lock<std::shared_mutex> lock(shared_mutex_);
            auto chosen = pick_entry(strategy, manual_idx);
            if (chosen) {
                // 增加负载计数
                chosen->active_count.fetch_add(1, std::memory_order_relaxed);
                // 返回设备句柄
                return DeviceHandle(chosen);
            }
        }

        // 没有立即可用的设备句柄，等待或重试
        if (deadline) {
            auto now = std::chrono::steady_clock::now();
            if (now >= *deadline) {
                // 抛异常/空句柄
                throw std::runtime_error("HWDevicePool::acquire timeout");
                // return nullopt;
            }
            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(WAIT_MILSEC));
        } else {
            // 简短睡眠，避免忙等待
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MILSEC));
        }
    }    
}

std::optional<DeviceHandle> HWDevicePool::TryAcquire(SelectionStrategy strategy, 
                        std::optional<std::size_t> manual_idx) {
    std::shared_lock<std::shared_mutex> lock(shared_mutex_);
    auto chosen = pick_entry(strategy, manual_idx);
    if (!chosen) {
        return std::nullopt;
    }                       
    
    chosen->active_count.fetch_add(1, std::memory_order_relaxed);
    return DeviceHandle{chosen};
}

void HWDevicePool::AdjustLoad(std::size_t id, int delta) { 
    std::shared_lock<std::shared_mutex> lock(shared_mutex_);
    auto it = std::find_if(entries_.begin(), entries_.end(), [id](const auto& entry) {
        return entry->id == id;
    });

    if (it == entries_.end()) {
        return;
    }

    if (delta > 0) {
        (*it)->active_count.fetch_add(delta, std::memory_order_relaxed);
    } else {
        (*it)->active_count.fetch_sub(-delta, std::memory_order_relaxed);
    }
}

void HWDevicePool::SetHealth(std::size_t id, bool health) {
    std::shared_lock<std::shared_mutex> lock(shared_mutex_);
    auto it = std::find_if(entries_.begin(), entries_.end(), [id](const std::shared_ptr<HWDeviceEntry>& entry) {
        return entry->id == id;
    });

    if (it != entries_.end()) {
        (*it)->healthy.store(health, std::memory_order_relaxed);
    }
}

void HWDevicePool::setHealthCheckInterval(int milsec) {
    health_check_interval_ = milsec;
}

void HWDevicePool::setCheckStopInterval(int milsec) {
    check_stop_interval_ = milsec;
}

void HWDevicePool::SetMaxRetryCount(int count) {
    max_retry_count_ = count;
}

void HWDevicePool::SetRetryBackoff(int milsec) {
    retry_backoff_ = milsec;
}

std::shared_ptr<HWDeviceEntry> HWDevicePool::pick_entry(SelectionStrategy strategy, std::optional<std::size_t> manual_idx) const {
    if (entries_.empty()) {
        return nullptr;
    }

    if (strategy == SelectionStrategy::Manual) {
        if (!manual_idx) {
            return nullptr;
        }
        
        std::size_t want = *manual_idx;
        auto it = std::find_if(entries_.begin(), entries_.end(), 
            [&](const std::shared_ptr<HWDeviceEntry>& entry) {
            return entry->id == want;
        });

        if (it == entries_.end()) {
            return nullptr;
        }

        auto e = *it;
        if (!e->healthy.load()) {
            return nullptr;
        }
        return e;
    }

    if (strategy == SelectionStrategy::RoundRobin) {
        // 跳过非健康的设备
        std::size_t n = entries_.size();
        size_t start = rr_cursor_.fetch_add(1) % n;
        for (std::size_t i = 0; i < n; i++) {
            std::size_t idx = (start + i) % n;
            auto& e = entries_[idx];
            if (!e->healthy.load()) {
                continue;
            }
            // 选择首个健康的设备
            return e;
        }    
        return nullptr;    
    }

    if (strategy == SelectionStrategy::LeastLoaded) {
        // 找到负载最小的设备
        std::shared_ptr<HWDeviceEntry> least_loaded = nullptr;
        int least = INT_MAX;
        for (auto& e : entries_) {
            if (!e->healthy.load()) {
                continue;
            }
            int count = e->active_count.load(std::memory_order_relaxed);
            if (least_loaded != nullptr || count < least) {
                least = count;
                least_loaded = e;
            }
        }
        return least_loaded;
    }

    return nullptr;
}


void HWDevicePool::health_check_loop() { 
    while(!health_check_stopped_.load()) {
        try {
            perform_health_check();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "HWDevicePool::health_check_thread error: %s\n", e.what());
        }

        // 将长时间（health_check_interval_）的休眠拆分成短的休眠（health_check_stopped_），每次检查stop标志
        for (int i = 0; i < health_check_interval_ / check_stop_interval_ && !health_check_stopped_.load(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(check_stop_interval_));
        }
        // 精确休眠时间
        if (!health_check_stopped_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(health_check_interval_ % check_stop_interval_));
        }
    }
}

void HWDevicePool::perform_health_check() {
    std::vector<std::shared_ptr<HWDeviceEntry>> entry_copy;
    {
        // 读锁
        std::shared_lock<std::shared_mutex> lock(shared_mutex_);
        entry_copy = entries_;
    }
    for (auto& entry : entry_copy) { 
        if (health_check_stopped_.load()) {
            break;
        }

        // 检测设备健康
        bool healthy = probe_device(entry->type, entry->device_name);
        if (healthy) {
            if (!entry->healthy.load()) {
                // 恢复设备
                entry->healthy.store(true);
                // TODO:添加日志
                std::fprintf(stderr, "HWDevicePool::perform_health_check: device %s(%ld) recovered!\n", entry->device_name.c_str(), entry->id);
            }
            continue;
        }

        // 设备不健康
        if (entry->healthy.load()) {
            // 停止设备
            entry->healthy.store(false);
            // TODO:添加日志
            std::fprintf(stderr, "HWDevicePool::perform_health_check: device %s(%ld) unhealthy, scheduling retries...\n", entry->device_name.c_str(), entry->id);
        }
        
        bool rebuild = false;
        for (int i = 1; i < max_retry_count_.load(); i++) { 
            if (health_check_stopped_.load()) {
                break;
            }
            // 在设备重试初始化时，延迟一段时间再重试，并且延迟时间会随重试次数增加
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_backoff_.load() * i));
            try {
                auto new_ctx = std::make_shared<HWDeviceContext>(entry->type, entry->device_name);
                // 替换上下文
                {
                    // 独占锁
                    std::unique_lock<std::shared_mutex> lock(shared_mutex_);
                    auto it = std::find_if(entry_copy.begin(), entry_copy.end(), 
                        [&](const std::shared_ptr<HWDeviceEntry>& e) {
                        return e->id == entry->id;
                    });
                    if (it != entry_copy.end()) {
                        (*it)->hw_ctx = new_ctx;
                        (*it)->healthy.store(true);
                        rebuild = true;
                        std::fprintf(stderr, "HWDevicePool::perform_health_check: device %s(%ld) rebuilt on attempt %d\n", entry->device_name.c_str(), entry->id, i);
                    }
                }
                break;
            } catch (...) {
                std::fprintf(stderr, "HWDevicePool::perform_health_check: device %s(%ld) rebuilt attempt %d failed\n", entry->device_name.c_str(), entry->id, i);
            }
        }
        if (!rebuild) {
            // 删除设备
            // {
            //     // 独占锁
            //     std::unique_lock<std::shared_mutex> lock(shared_mutex_);
            //     auto it = std::find_if(entry_copy.begin(), entry_copy.end(), 
            //         [&](const std::shared_ptr<HWDeviceEntry>& e) {
            //         return e->id == entry->id;
            //     });

            //     if (it != entry_copy.end()) {
            //         entry_copy.erase(it);
            //         std::swap(entry_copy, entries_);
            //         std::fprintf(stderr, "HWDevicePool::perform_health_check: device %s(%d) removed\n", entry->device_name.c_str(), entry->id);
            //     }
            // }
            std::fprintf(stderr, "HWDevicePool::perform_health_check: device %s(%ld) remains unhealthy after %d retries\n", entry->device_name.c_str(), entry->id, max_retry_count_.load());
        }
    }
}

bool HWDevicePool::probe_device(AVHWDeviceType type, const std::string& device_name) {
    AVBufferRef* tmp = nullptr;
    auto device = device_name.empty() ? nullptr:device_name.c_str();
    int ret = av_hwdevice_ctx_create(&tmp, type, device, nullptr, 0);
    if (ret < 0) {
        return false;
    }
    av_buffer_unref(&tmp);
    return true;
}





}