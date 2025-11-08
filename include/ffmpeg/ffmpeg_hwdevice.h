#pragma once
extern "C" {
#include "libavutil/hwcontext.h"
}
#include <string>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <vector>
#include <condition_variable>
#include <optional>
#include <thread>

namespace FFmpeg {
    
/// @brief 硬件设备上下文
class HWDeviceContext {
public:
    HWDeviceContext() = default;

    /// @brief 构造函数 创建硬件设备上下文
    /// @param type 硬件设备类型
    /// @param device_name 硬件设备名称
    HWDeviceContext(AVHWDeviceType type, const std::string& device_name = "");

    /// @brief 析构函数
    ~HWDeviceContext();

    /// @brief 删除拷贝构造函数和赋值运算符
    HWDeviceContext(const HWDeviceContext&) = delete;
    HWDeviceContext& operator=(const HWDeviceContext&) = delete;

    HWDeviceContext(HWDeviceContext&& other) noexcept;
    HWDeviceContext& operator=(HWDeviceContext&& other) noexcept;

    /// @brief 获取硬件设备类型
    /// @return 硬件设备类型
    AVHWDeviceType getType() const noexcept;

    /// @brief 工厂函数，创建硬件设备上下文
    /// @param type 硬件设备类型
    /// @param device_name 硬件设备名称
    static AVBufferRef* Create(AVHWDeviceType type, const std::string& device_name = "");
    
    /// @brief 工厂函数，释放硬件设备上下文
    /// @param ctx 硬件设备上下文
    static void Free(AVBufferRef* ctx) noexcept;

    /// @brief 工厂函数，获取硬件设备类型
    /// @param ctx 硬件设备上下文
    /// @return 硬件设备类型
    static AVHWDeviceType GetType(AVBufferRef* ctx) noexcept;

    /// @brief 检查上下文是否有效
    /// @return true 有效 false 无效
    bool isValid() const noexcept;

    /// @brief 获取内部的AVHWDeviceContext指针
    ::AVBufferRef* get() noexcept;

    /// @brief 获取内部的AVBufferRef指针（常量版本）
    const ::AVBufferRef* get() const noexcept;

    /// @brief 获取内部的AVBufferRef指针
    ::AVBufferRef* raw() noexcept;

    /// @brief 获取内部的AVBufferRef指针（常量版本）
    const ::AVBufferRef* raw() const noexcept;

private:
    void move_from(HWDeviceContext& other) noexcept;

    // ::AVHWDeviceContext* ctx_ = nullptr;
    /// @brief 硬件设备上下文
    AVBufferRef* ctx_ = nullptr;
};    

enum class SelectionStrategy {
    RoundRobin,     // 轮询选择
    LeastLoaded,    // 选择当前负载最小的设备
    Manual  // 手动选择
};

/// @brief 硬件设备条目
struct HWDeviceEntry {
    /// @brief pool内设备id
    std::size_t                         id;
    /// @brief 设备类型 CUDA DX VAAPI QSV
    AVHWDeviceType                      type;
    /// @brief 设备名称
    std::string                         device_name;
    /// @brief 设备上下文
    std::shared_ptr<HWDeviceContext>    hw_ctx;
    /// @brief 设备被占用的计数（负载）
    std::atomic<int>                    active_count{ 0 };
    /// @brief 设备是否健康(可用)
    std::atomic<bool>                   healthy{ true };

    HWDeviceEntry(std::size_t id_, 
                   AVHWDeviceType type_, 
                   std::string device_name_, 
                   std::shared_ptr<HWDeviceContext> hw_ctx_)
        : id(id_), type(type_), device_name(std::move(device_name_)), hw_ctx(std::move(hw_ctx_))
    {
    }
};


/// @brief 硬件设备句柄
/// @details 持有时代表占用一个设备；析构时会自动释放设备
class DeviceHandle {
public:
    
    /// @brief 构造函数
    DeviceHandle() = default;
    
    /// @brief 构造函数
    /// @param entry 硬件设备条目
    DeviceHandle(std::shared_ptr<HWDeviceEntry> entry);

    DeviceHandle(DeviceHandle&&) noexcept;
    DeviceHandle& operator=(DeviceHandle&&) noexcept;
    ~DeviceHandle();
    /// @brief 禁用拷贝构造函数和赋值运算符
    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;
    
    /// @brief 设备句柄是否有效
    bool valid() const noexcept;

    /// @brief 获取内部的AVBufferRef指针
    AVBufferRef* hw_device_ctx() const noexcept;

    /// @brief 获取硬件设备ID
    size_t id() const noexcept;

    /// @brief 获取硬件设备类型
    AVHWDeviceType type() const noexcept;

    /// @brief 获取硬件设备名称
    const std::string& device_name() const noexcept;

    /// @brief 加载硬件设备
    /// @return 0 成功 -1 失败
    int load() const noexcept;
    
private:
    void move_from(DeviceHandle& other);
    void free();
    std::shared_ptr<HWDeviceEntry> entry_;
};

class HWDevicePool {
public:
    /// @brief 防止忙等待休眠时间 2ms
    const int SLEEP_MILSEC = 2;
    /// @brief 没有立即可用的设备循环等待时间 5ms
    const int WAIT_MILSEC = 5;
    HWDevicePool(int health_check_interval = 2000, int check_stop_interval = 100, int max_try_count = 3, int retry_backoff = 1000);
    ~HWDevicePool() noexcept;

    HWDevicePool(const HWDevicePool&) = delete;
    HWDevicePool& operator=(const HWDevicePool&) = delete;

    /// @brief 向硬件设备池子box中添加硬件设备
    /// @param type 设备类型
    /// @param name 设备名称
    /// @return 该硬件设备在硬件池中的ID
    size_t AddDevice(AVHWDeviceType type, const std::string& name = "");

    /// @brief 移除硬件设备（若设备被占用会阻塞到不被占用后再移除）
    /// @param id 设备ID
    void RemoveDevice(size_t id);

    /// @brief 获取硬件设备池子中硬件设备数量
    std::size_t size() const noexcept;

    /// @brief 查询设备信息（快照）
    struct DeviceSnapshot {
        std::size_t id;
        AVHWDeviceType type;
        std::string name;
        int load;
        bool healthy;
    };

    std::vector<DeviceSnapshot> snapshots() const;

    /// @brief 阻塞式获取设备句柄， 可选超时等待时间
    /// @param strategy 选择策略
    /// @param timeout 超时时间
    /// @param manual_idx 指定设备ID
    /// @return 成功返回DeviceHandle，失败返回nullopt
    std::optional<DeviceHandle> Acquire(SelectionStrategy strategy, 
                         std::optional<std::chrono::milliseconds> timeout = std::nullopt,
                         std::optional<std::size_t> manual_idx = std::nullopt);

    /// @brief 尝试获取设备句柄， 可选指定设备ID,失败立即返回nullopt
    /// @param strategy 选择策略
    /// @param manual_idx 指定设备ID
    /// @return 获取成功返回DeviceHandle，失败返回nullopt
    std::optional<DeviceHandle> TryAcquire(SelectionStrategy strategy, std::optional<std::size_t> manual_idx = std::nullopt);
    
    /// @brief 手动增加/减少设备的load值
    /// @param id 设备ID
    /// @param delta 加减load值
    void AdjustLoad(std::size_t id, int delta);
    
    /// @brief 手动设置设备的健康状态
    void SetHealth(std::size_t id, bool health);

    /// @brief 设置健康检查间隔(ms)
    void setHealthCheckInterval(int milsec);

    /// @brief 设置设备检查停止标志间隔(ms)
    void setCheckStopInterval(int milsec);

    /// @brief 设置设备重试次数
    void SetMaxRetryCount(int count);

    /// @brief 设置设备重试延迟基数(ms)
    void SetRetryBackoff(int milsec);
private:
    /// @brief 根据策略挑选一个可用entry，不改变active_count
    /// @param strategy 策略
    /// @param manual_idx 手动指定entry的索引
    /// @return 硬件设备entry
    std::shared_ptr<HWDeviceEntry> pick_entry(SelectionStrategy strategy, std::optional<std::size_t> manual_idx) const;
    
    /// @brief 健康检查线程
    void health_check_loop();
    
    /// @brief 健康检查
    void perform_health_check();
    
    /// @brief 轻量级探测，尝试创建和销毁一个临时设备上下文
    bool probe_device(AVHWDeviceType type, const std::string& device_name);

    /// @brief 读写锁
    mutable std::shared_mutex shared_mutex_;

    /// @brief 硬件设备条目
    std::vector<std::shared_ptr<HWDeviceEntry>> entries_;

    /// @brief 下一个设备ID
    std::size_t next_id_ = 0;

    /// @brief 互斥锁，用于等待可用设备
    std::mutex cv_mutex_;
    std::condition_variable cv_;

    /// @brief 健康检查线程
    std::thread health_check_thread_;
    std::atomic<bool> health_check_stopped_{false};
    
    /// @brief 健康检查间隔(ms)
    std::atomic<int> health_check_interval_{2000};
    /// @brief 检查硬件池是否停止的时间间隔
    std::atomic<int> check_stop_interval_{100};
    // @brief 设备重试次数
    std::atomic<int> max_retry_count_{3};

    /// @brief 设备重试延迟基数(ms)
    std::atomic<int> retry_backoff_{1000};

    /// @brief 轮询索引
    mutable std::atomic<std::size_t> rr_cursor_{0};

};


// // 假设 pool 已经在进程启动时初始化并填充设备：
// ffmpegx::HWDevicePool pool;
// pool.add_device(AV_HWDEVICE_TYPE_CUDA, "0");
// pool.add_device(AV_HWDEVICE_TYPE_CUDA, "1");

// // 在解码任务线程里：
// try {
//     // 阻塞获取一个设备，使用最小负载策略
//     auto handle = pool.acquire(ffmpegx::SelectionStrategy::LeastLoaded, std::chrono::milliseconds(500));
//     if (!handle.valid()) throw std::runtime_error("got invalid device handle");

//     // 将 hw_device_ctx 绑定到 codec_ctx（CodecContext 内部实现）
//     // 这里假设你实现了 CodecContext::BindHWDevice(AVBufferRef*)
//     codec_ctx_->bind_hw_device(handle.hw_device_ctx());

//     // 打开、解码、使用 GPU 等
//     codec_ctx_->Open();

//     // 注意：在 handle 存在期间，pool 会保持 active_count 加 1
//     // 当 handle 析构（离开作用域）时，active_count 自动减 1
// } catch (const std::exception& e) {
//     // 处理设备不可用 / 超时等
// }







} // namespace FFmpeg