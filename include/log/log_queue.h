// #include "bound_mpmc_queue.h"
#include "queue/concurrentqueue.h"
#include "queue/blockingconcurrentqueue.h"
template <typename T>
class LogQueue {
public:
    LogQueue(std::size_t capacity = 64)
        : queue_(capacity)
    {
    }
    ~LogQueue() {
        stop();
    }

    /// @brief 非阻塞入队, 允许分配内存,失败时返回false
    bool push_alloc(T&& item) {
        return queue_.enqueue(std::move(item));
    }

    /// @brief 非阻塞入队, 允许分配内存,失败时返回false
    bool push_alloc(const T& item) {
        return queue_.enqueue(item);
    }
    
    /// @brief 非阻塞入队, 不允许分配内存,失败时返回false
    bool push(T&& item) {
        return queue_.try_enqueue(std::move(item));
    }

    /// @brief 非阻塞入队, 不允许分配内存,失败时返回false
    bool push(const T& item) {
        return queue_.try_enqueue(item);
    }

    /// @brief 一直阻塞线程，直至出队,队列为空会返回false
    bool pop(T& item) {
        return queue_.try_dequeue(item);
    }

    /// @brief 一直阻塞线程，直至出队,队列为空会等待生产者生产
    void pop_block(T& item) {
        queue_.wait_dequeue(item);
    }

    template <typename Rep, typename Period>
    bool pop_wait(T& item, const std::chrono::duration<Rep, Period>& timeout) {
        return queue_.wait_dequeue_timed(item, timeout);
    }

    /// @brief 返回队列近似为空, 可能会有误差
    bool empty_approx() const {
        return queue_.size_approx() == 0;
    }

    /// @brief 返回队列近似大小, 可能会有误差
    std::size_t size_approx() const {
        return queue_.size_approx();
    }

    void stop() {
        // 放一个空包, 通知消费者退出
        queue_.enqueue({});
    }
private:
    // BoundMPMCQueue<T> queue_;
    moodycamel::BlockingConcurrentQueue<T> queue_;
};
