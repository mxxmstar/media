#pragma once
#include <atomic>
#include <optional>
#include "epoch_reclaimer.h"
#include <condition_variable>
#include <mutex>
#include <chrono>

/// @brief 无界无锁队列
/// @tparam T 参数类型
template <typename T>
class MPMCQueue {
public:
    MPMCQueue() {
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        approximate_size_.store(0, std::memory_order_relaxed);
    }
    ~MPMCQueue() {
        stop();
        gc_.quiescent_point();
        
        // 直接绕过gc强制释放
        Node* h = head_.load(std::memory_order_relaxed);
        while (h) {
            Node* n = h->next.load(std::memory_order_relaxed);
            delete h;
            h = n;
        }
    }
    
    /// @brief 入队
    /// @tparam U 入队值的类型
    /// @param value 入队的值
    template <class U>
    void enqueue(U&& value) {
        EpochReclaimer::Guard guard(gc_);
        // Node* new_node = new Node(std::forward<U>(value));
        Node* new_node = std::make_unique<Node>(std::forward<U>(value));
        while(true) {
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            if(last == tail_.load(std::memory_order_acquire)){
                if(next == nullptr) {
                    if(last->next.compare_exchange_weak(next, new_node, std::memory_order_release, std::memory_order_acquire)){
                        tail_.compare_exchange_strong(last, new_node, std::memory_order_release, std::memory_order_acquire);
                        
                        // 加锁？
                        cv_.notify_one();   // 通知一个等待的消费者
                        
                        approximate_size_.fetch_add(1, std::memory_order_relaxed);

                        new_node.release(); // 手动释放内存
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(last, next, std::memory_order_release, std::memory_order_acquire);
                }
            }
        }
    }

    /// @brief 非阻塞出队
    /// @param value 出队的值
    /// @return 出队操作是否成功
    bool try_dequeue(T& value) {
        EpochReclaimer::Guard guard(gc_);
        while(true) {
            Node* first = head_.load(std::memory_order_acquire);    // dummy
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);
            
            // 再次确认head未被其他线程修改
            if(first == head_.load(std::memory_order_acquire)){
                if(next == nullptr){
                    return false;   // 队列为空
                }
                
                // head == tail && next != nullptr，说明tail未更新
                // 帮忙把tail前移到next处，然后重试
                // 这里只需要用if，只用帮忙推进一次即可
                if(first == last){
                    tail_.compare_exchange_strong(last, next, std::memory_order_release, std::memory_order_acquire);
                    continue;
                }

                // // 取值
                // if(next->value.has_value()) {
                //     value = std::move(next->value.value());
                // }
                
                // 先推进head，让next成为新的dummy
                if(head_.compare_exchange_weak(first, next, std::memory_order_acq_rel, std::memory_order_acquire)){
                    // 成功后，当前线程独占这个出队操作
                    // 安全地move数据(不会导致后一个线程访问时是已经move了的节点)
                    
                    // 取值
                    if(next->value.has_value()) {
                        value = std::move(next->value.value());
                    }
                    gc_.retire(first);  // 交给EBR延迟释放
                    approximate_size_.fetch_sub(1, std::memory_order_relaxed);// 计数器-1
                    return true;
                }
            }
            // CAS失败，则重试
        }
    }

    /// @brief 阻塞出队
    /// @return 出队的值
    T dequeue_blocking() {
        T value;
        while (true) {
            if (try_dequeue(value)) {
                return value;
            }
            // 等待生产者通知
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&]{ 
                return try_dequeue(value) || stopped_.load(std::memory_order_acquire); 
            });
            return value;
        }
    }

    /// @brief 带超时的阻塞出队
    /// @tparam Rep 超时时间的表示类型
    /// @tparam Period 超时时间的单位类型
    /// @param out 出队的值
    /// @param timeout 超时时间
    /// @return 如果在超时时间内出队成功返回true，否则返回false
    template <class Rep, class Period>
    bool dequeue_for(T& value, const std::chrono::duration<Rep, Period>& timeout) {
        if(try_dequeue(value)) {
            return true;
        }
        std::unique_lock<std::mutex> lk(mtx_);
        return cv_.wait_for(lk, timeout, [&]{ 
            return try_dequeue(value) || stopped_.load(std::memory_order_acquire); 
        });
    }

    void stop() {
        stopped_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lk(mtx_);
        cv_.notify_all();
    }

    std::size_t size() const {
        return approximate_size_.load(std::memory_order_relaxed);
    }
private:
    struct Node {
        /// @brief std::optional允许节点”有值“或”无值“，方便处理dummy节点
        /// 默认析构函数会正确销毁optional里面的值
        std::optional<T> value;
        std::atomic<Node*> next{nullptr};
        Node() = default;   //dummy哨兵节点
        explicit Node(T&& v) : value(std::move(v)){}
        explicit Node(const T& v) : value(v) {}
    };

    /// @brief 
    alignas(64) std::atomic<Node*> head_;
    /// @brief 
    alignas(64) std::atomic<Node*> tail_;
    /// @brief 
    EpochReclaimer gc_;

    /// @brief 
    std::mutex mtx_;
    
    /// @brief 生产者消费者通知
    std::condition_variable cv_;

    /// @brief 节点大概数量
    std::atomic<std::size_t> approximate_size_{0};

    std::atomic<bool> stopped_{false};
};