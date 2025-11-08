#pragma once
#include <optional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <chrono>
#include <vector>
#include <stdexcept>
#include <thread>
#include "defer.h"

/// @brief 严格有界MPMC队列
/// 基于Dmitry Vyukov的MPMC队列
/// 构造时固定容量、无堆分配、出入队使用CAS，无锁
/// 出队时如果队列为空，会阻塞等待，直到有元素入队
/// 入队时如果队列已满，会阻塞等待，直到有元素出队
/// @tparam T 队列元素类型
template <typename T>
class BoundMPMCQueue {
public:
    /// @brief 构造函数
    /// @param capacity 严格有界队列容量
    explicit BoundMPMCQueue(std::size_t capacity) : capacity_(round_up_to_power_two(capacity))
    , mask_(capacity_ - 1), buffer_(capacity_), stopped_(false), active_threads_(0){
        for(std::size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~BoundMPMCQueue() {
        stop();
        // 先等待所有线程退出
        // wait_for_all_threads();
        wait_for_all_threads(std::chrono::seconds(1));

        // 析构时，先将队列中的元素出队，再销毁队列
        T value;
        while(try_dequeue(value)) {
            // 出队成功，继续出队
            // std::optional会自动析构
            // try {
            //     value.~T();
            // } catch (...) {

            // }
        }
    }
    /// @brief 禁止复制构造和赋值运算符
    BoundMPMCQueue(const BoundMPMCQueue&) = delete;
    BoundMPMCQueue& operator=(const BoundMPMCQueue&) = delete;

    /// @brief 尝试入队
    /// @tparam U 入队参数类型
    /// @param value 入队参数的值 
    /// @return true 入队成功
    /// @return false 入队失败
    template <class U>
    bool try_enqueue(U&& value) {
        return enqueue_impl(std::forward<U>(value));
    }

    /// @brief 阻塞入队
    /// @tparam U 入队参数类型
    /// @param value 入队参数的值 
    template <class U>
    void enqueue_blocking(U&& value) {
        enter_thread(); // 进入阻塞接口
        DEFER([&]() {
                exit_thread(); // 确保退出阻塞接口
        });

        // std::unique_lock<std::mutex> lock(mtx_not_full_);
        auto pred = [&] { return stopped_.load(std::memory_order_acquire) || !is_full(); };
        // 防止一直阻塞
        while(!stopped_) {
            if(enqueue_impl(std::forward<U>(value))) {
                cv_not_empty_.notify_one();
                return;
            }
            // 快速入队失败 要么队列刚好满了，要么CAS竞争输了
            std::unique_lock<std::mutex> lk(mtx_not_full_); // CHANGED: 回退到等待
            cv_not_full_.wait(lk, pred);                    // 等非满或停止
            if (stopped_.load(std::memory_order_acquire)) { // CHANGED
                throw std::runtime_error("enqueue_blocking: queue is stopped");
            }
            // 循环重试：释放锁后再尝试无锁入队
            // （不在锁内做CAS，避免把锁当成“全局串行化”）
        }
        // // 等到非满或 stopped
        // cv_not_full_.wait(lock, pred);
        // if(stopped_.load(std::memory_order_acquire)) {
        //     throw std::runtime_error("enqueue_blocking: queue is stopped");
        // }

        // while(!enqueue_impl(std::forward<U>(value))) {
        //     // 理论上可以入队
        // }
        // cv_not_empty_.notify_one();
    }

    /// @brief 阻塞入队，超时返回
    /// @tparam U 入队参数类型
    /// @param value 入队参数的值 
    /// @param timeout 超时时间
    /// @return true 入队成功
    /// @return false 入队失败
    template <class U, class Rep, class Period>
    bool enqueue_for(U&& value, const std::chrono::duration<Rep, Period>& timeout) {
        enter_thread();
        DEFER([&](){
            exit_thread();
        });
        // std::unique_lock<std::mutex> lock(mtx_not_full_);
        auto deadline = timeout + std::chrono::steady_clock::now();
        auto pred = [&] { return stopped_.load(std::memory_order_acquire) || !is_full(); };
        while(true) { 
            if (enqueue_impl(std::forward<U>(value))) {     
                cv_not_empty_.notify_one(); //可以直接换成notify_all(),增加开销防止真正等待的线程死等              
                return true;
            }
            std::unique_lock<std::mutex> lk(mtx_not_full_);
            if (!cv_not_full_.wait_until(lk, deadline, pred)) {
                return false; // 超时
            }
            if (stopped_.load(std::memory_order_acquire)) {
                return false;
            }
        }

        
        // if(!cv_not_full_.wait_until(lock, deadline, pred)) {
        //     return false; // 超时
        // }
        // if(stopped_.load(std::memory_order_acquire)) {
        //     return false;
        // }
        // 这里可能会忙等
        // while(!enqueue_impl(std::forward<U>(value))) {
        //     // 理论上可以入队
        // }
        // cv_not_empty_.notify_one();
        // return true;
    }

    /// @brief 尝试出队
    /// @param value 出队参数的值 
    /// @return true 出队成功
    /// @return false 出队失败
    bool try_dequeue(T& value) {
        return dequeue_impl(value);
    }

    /// @brief 阻塞出队
    /// @param value 出队参数的值 
    void dequeue_blocking(T& value) {
        // std::unique_lock<std::mutex> lock(mtx_not_empty_);
        auto pred = [&] { return stopped_.load(std::memory_order_acquire) || !is_empty(); };
        // 防止一直阻塞
        while(true) {
            if(dequeue_impl(value)) {
                cv_not_full_.notify_one();
                return;
            }
            std::unique_lock<std::mutex> lk(mtx_not_empty_);
            cv_not_empty_.wait(lk, pred);
            if (stopped_.load(std::memory_order_acquire)) {
                throw std::runtime_error("dequeue_blocking: queue is stopped");
            }
        }

        // cv_not_empty_.wait(lock, pred);
        // // 加上判断队列为空，确保数据不丢失
        // if(stopped_.load(std::memory_order_acquire) && is_empty()) {
        //     throw std::runtime_error("dequeue_blocking: queue is stopped");
        // }

        // while(!dequeue_impl(value)) {
        // }
        // cv_not_full_.notify_one();
    }

    /// @brief 阻塞出队，超时返回
    /// @param value 出队参数的值 
    /// @param timeout 超时时间
    /// @return true 出队成功
    /// @return false 出队失败
    template <class Rep, class Period>
    bool dequeue_for(T& value, const std::chrono::duration<Rep, Period>& timeout) {
        enter_thread();
        DEFER([&](){
            exit_thread();
        });

        // std::unique_lock<std::mutex> lock(mtx_not_empty_);
        auto deadline = timeout + std::chrono::steady_clock::now();
        auto pred = [&] { return stopped_.load(std::memory_order_acquire) || !is_empty(); };
        // 防止一直阻塞
        while(true) {
            if(dequeue_impl(value)) {
                cv_not_full_.notify_one();
                return true;
            }
            std::unique_lock<std::mutex> lk(mtx_not_empty_);
            if (!cv_not_empty_.wait_until(lk, deadline, pred)) {
                return false; // 超时
            }
            if (stopped_.load(std::memory_order_acquire)) {
                return false;
            }
        }

        // if(!cv_not_empty_.wait_until(lock, deadline, pred)) {
        //     return false; // 超时
        // }
        // if(stopped_.load(std::memory_order_acquire)) {
        //     return false;
        // }

        // while(!dequeue_impl(value)) {
        //     // 理论上可以出队
        // }
        // cv_not_full_.notify_one();
        // return true;
    }

    /// @brief 停止队列，防止入队出队
    void stop() {
        stopped_.store(true, std::memory_order_release);
        // 确保之前的 side effect 可见
        // std::atomic_thread_fence(std::memory_order_release);

        cv_not_full_.notify_all();
        cv_not_empty_.notify_all();
    }

    /// @brief 队列近似大小
    /// @return std::size_t 队列近似大小
    std::size_t size_approx() const noexcept {
        std::size_t enq = enqueue_pos_.load(std::memory_order_acquire);
        std::size_t deq = dequeue_pos_.load(std::memory_order_acquire);
        // return enq - deq;
        return enq > deq ? enq - deq : 0;   // 防御式
    }
    
    /// @brief 队列容量
    /// @return 队列容量
    std::size_t capacity() const noexcept { return capacity_; }
private:
    

    /// @brief 计算大于等于n的最小2的幂
    /// @param n 输入参数
    /// @return 大于等于n的最小的2的幂 
    static std::size_t round_up_to_power_two(std::size_t n) {
        if(n < 2) {
            return 2;
        }
        // 防止溢出
        if (n > (std::numeric_limits<std::size_t>::max() >> 1) + 1) {
            throw std::invalid_argument("capacity overflow");
        }
        
        --n;
        for(std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1) {
            n |= n >> i;
        }
        return n + 1;
    }

    /// @brief 计算pos在buffer_中的索引
    /// @param pos 位置
    /// @return 索引
    std::size_t index_of(std::size_t pos) const noexcept {
        return pos & mask_;
    }

    /// @brief 队列是否已满的近似判断
    /// @return true 队列已满
    /// @return false 队列未满
    bool is_full() const noexcept {
        return size_approx() >= capacity_;
    }

    /// @brief 队列是否为空的近似判断
    /// @return true 队列为空
    /// @return false 队列非空
    bool is_empty() const noexcept {
        return size_approx() == 0;
    }

    /// @brief 队列是否已停止
    /// @return true 队列已停止
    /// @return false 队列未停止
    bool stopped() const noexcept {
        return stopped_.load(std::memory_order_acquire);
    }

    /// @brief 入队
    /// @param value 入队的值
    /// @return true 入队成功
    /// @return false 入队失败
    template <class U>
    bool enqueue_impl(U&& value) {
        if (stopped_.load(std::memory_order_acquire)) return false; // 早返回，避免在 stopped 时入队

        // 入队序号
        std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        while(!stopped_) {
            Cell& cell = buffer_[index_of(pos)];
            // 当前记录的序号
            std::size_t seq = cell.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                // 该槽位可读，尝试出队，出队成功，更新sequence为pos+capacity_
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    // 写入数据
                    try {
                        cell.value.emplace(std::forward<U>(value));
                    } catch (...) {
                        // 异常回滚，防止槽位丢失
                        enqueue_pos_.fetch_sub(1, std::memory_order_relaxed);
                        cell.sequence.store(seq, std::memory_order_release);
                        return false;
                    }
                    
                    // 将序号设置为pos + 1，表示该槽可读
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS失败，pos已更新为当前值，继续尝试
            } else if (diff < 0) {
                // 该槽位还未被消费者释放，队列满了
                return false;
            } else {
                // 该槽位还没写好（生产者未完成），重读pos
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
        return false;
    }

    /// @brief 出队
    /// @param value 出队的值
    /// @return true 出队成功
    /// @return false 出队失败
    bool dequeue_impl(T& value) {
        if (stopped_.load(std::memory_order_acquire)) return false; // 早返回，避免在 stopped 时入队

        // 出队序号
        std::size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        while(!stopped_) {
            Cell& cell = buffer_[index_of(pos)];
            // 当前记录的序号
            std::size_t seq = cell.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                // 该槽位可读，尝试出队，出队成功，更新sequence为pos+capacity_
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                     // 写入数据
                    try {
                        value = std::move(cell.value.value());
                        cell.value.reset();
                    } catch (...) {
                        // 异常回滚，防止槽位丢失
                        dequeue_pos_.fetch_sub(1, std::memory_order_relaxed);
                        // 重新标记槽位“可读”，防止丢失
                        cell.sequence.store(pos + 1, std::memory_order_release);
                        return false;   //也可以抛出异常
                    }
                    
                    // if (!cell.value) return false; 
                    // value = std::move(cell.value.value());
                    // cell.value.reset();// 销毁内容

                    // 将序号设置为pos + capacity_，表示该槽再次可写
                    cell.sequence.store(pos + capacity_, std::memory_order_release);
                    return true;
                }
                // CAS失败，pos已更新为当前值，继续尝试
            } else if (diff < 0) {
                // 该槽位还未被生产者释放，队列空了
                return false;
            } else {
                // 该槽位还没写好（生产者未完成），重读pos
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        return false;
    }

    void enter_thread() {
        active_threads_.fetch_add(1, std::memory_order_acq_rel);
    }

    void exit_thread() {
        active_threads_.fetch_sub(1, std::memory_order_acq_rel);
    }

    void wait_for_all_threads() {
        while(active_threads_.load(std::memory_order_acquire) != 0) {
            std::this_thread::yield();
        }
    }

    void wait_for_all_threads(std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();
        while (active_threads_.load(std::memory_order_acquire) != 0) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                break; // 超时退出
            }
            std::this_thread::yield();
        }
    }

    /// @brief 队列容量
    const std::size_t capacity_;
    
    /// @brief 掩码，用于快速计算数组下标
    /// @details 如果队列容量是2的幂，则mask_=capacity_-1，可以快速实现‘%’运算
    const std::size_t mask_;
    
        /// @brief 队列析构了
    std::atomic<bool> stopped_{false};

    /// @brief 活动线程数
    std::atomic<std::size_t> active_threads_{0};

    /// @brief 队列单元格 序号+值
    struct /*alignas(64)*/ Cell {
        /// @brief 序列号，标记当前Cell的状态，是否可读写
        /// sequence为n时，表示该单元格“可被生产者写入”，随后更新为n+1“可被消费者读取”
        /// 消费者读取数据后，会将sequence从n+1改为n+2,标记数据已消费，单元格可复用
        std::atomic<std::size_t> sequence{0};
        /// @brief 存储的数据
        std::optional<T> value;

        /// @brief 自然对齐需求  alignof(std::optional<T>)只会返回2的幂      
        static constexpr std::size_t natural_alignment = std::max(std::size_t{64}, alignof(std::optional<T>));

        /// @brief 强制结构体本身被 natural_alignment字节对齐
        struct alignas(natural_alignment) Impl
        {
            std::atomic<std::size_t> sequence{0};
            std::optional<T> value;
        };

        /// @brief 真正存储的对象
        alignas(natural_alignment) Impl impl;

        /// @brief 填充字节到满足 natural_alignment的整数倍
        char padding_[(natural_alignment - sizeof(Impl) % natural_alignment) % natural_alignment];
        
        // 6. 编译期检查
        static_assert(sizeof(Cell) % natural_alignment == 0, "size must be multiple of alignment");
        static_assert(alignof(Cell) == natural_alignment,   "alignment mismatch");
    };

    /// @brief 队列缓冲区，一次性分配capacity个槽位，保证不会扩容
    std::vector<Cell> buffer_;


    /// @brief 入队位置（分离的原子计数以减少伪共享）
    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};
    /// @brief 出队位置（分离的原子计数以减少伪共享）
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};

    /// @brief 用于阻塞等待的条件变量，当队列满时，入队线程阻塞等待
    mutable std::mutex mtx_not_full_;
    /// @brief 用于阻塞等待的条件变量，当队列空时，出队线程阻塞等待
    mutable std::mutex mtx_not_empty_;

    /// @brief 入队位置的条件变量，当队列满时，入队线程阻塞等待
    std::condition_variable cv_not_full_;
    /// @brief 出队位置的条件变量，当队列空时，出队线程阻塞等待
    std::condition_variable cv_not_empty_;

};