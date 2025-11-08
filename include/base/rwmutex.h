#ifndef MX_RWMUTEX_H
#define MX_RWMUTEX_H

#include <mutex>
#include <condition_variable>
#include <shared_mutex>

/*
*TODO:通过 write_waiters_赋予了​​写操作较高的优先级,在读多写少​​的场景下，可能会​​轻微降低读并发性能​​
*TODO:没有严格的先来先服务队列。等待的写线程被唤醒后，如果此时有新读请求，可能仍需等待。
*TODO:没有处理递归加锁的情况
*/


/**
 * @brief 读写锁实现类
 * 
 * 该类提供了一个高效且线程安全的读写锁实现，支持多个读线程同时访问共享资源，
 * 但只允许一个写线程独占访问。
 */
class RWMutex {
public:
    /**
     * @brief 构造函数
     */
    RWMutex() : readers_(0), writers_(0), write_waiters_(0) {}

    /**
     * @brief 获取读锁
     * 
     * 多个线程可以同时持有读锁，但如果有线程持有或等待写锁，则读锁请求会被阻塞。
     * 这种策略可以防止写线程饥饿。
     */
    void readLock() {
        std::unique_lock<std::mutex> lock(mutex_);
        // 如果有写线程或者有写线程在等待，则等待
        while (writers_ > 0 || write_waiters_ > 0) {
            read_cv_.wait(lock);
        }
        ++readers_;
    }

    /**
     * @brief 尝试获取读锁
     * 
     * @return 如果成功获取读锁返回true，否则返回false
     */
    bool tryReadLock() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (writers_ > 0 || write_waiters_ > 0) {
            return false;
        }
        ++readers_;
        return true;
    }

    /**
     * @brief 释放读锁
     */
    void readUnlock() {
        std::unique_lock<std::mutex> lock(mutex_);
        --readers_;
        // 如果没有读线程了，通知写线程
        if (readers_ == 0) {
            write_cv_.notify_one();
        }
    }

    /**
     * @brief 获取写锁
     * 
     * 只有一个线程可以持有写锁，且在写锁被持有时，其他读写请求都会被阻塞。
     */
    void writeLock() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++write_waiters_;
        // 等待没有读线程和写线程
        while (readers_ > 0 || writers_ > 0) {
            write_cv_.wait(lock);
        }
        --write_waiters_;
        ++writers_;
    }

    /**
     * @brief 尝试获取写锁
     * 
     * @return 如果成功获取写锁返回true，否则返回false
     */
    bool tryWriteLock() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (readers_ > 0 || writers_ > 0) {
            return false;
        }
        ++writers_;
        return true;
    }

    /**
     * @brief 释放写锁
     */
    void writeUnlock() {
        std::unique_lock<std::mutex> lock(mutex_);
        --writers_;
        // 优先通知等待的写线程，如果没有写线程等待，则通知所有读线程
        if (write_waiters_ > 0) {
            write_cv_.notify_one();
        } else {
            read_cv_.notify_all();
        }
    }

    /**
     * @brief 读锁守卫类，用于RAII方式管理读锁
     */
    class ReadGuard {
    public:
        explicit ReadGuard(RWMutex& mutex) : mutex_(mutex) {
            mutex_.readLock();
        }

        ~ReadGuard() {
            mutex_.readUnlock();
        }

        // 禁止拷贝构造和赋值
        ReadGuard(const ReadGuard&) = delete;
        ReadGuard& operator=(const ReadGuard&) = delete;

    private:
        RWMutex& mutex_;
    };

    /**
     * @brief 写锁守卫类，用于RAII方式管理写锁
     */
    class WriteGuard {
    public:
        explicit WriteGuard(RWMutex& mutex) : mutex_(mutex) {
            mutex_.writeLock();
        }

        ~WriteGuard() {
            mutex_.writeUnlock();
        }

        // 禁止拷贝构造和赋值
        WriteGuard(const WriteGuard&) = delete;
        WriteGuard& operator=(const WriteGuard&) = delete;

    private:
        RWMutex& mutex_;
    };

private:
    std::mutex mutex_;              // 互斥锁，用于保护内部状态
    std::condition_variable read_cv_;  // 读线程条件变量
    std::condition_variable write_cv_; // 写线程条件变量
    int readers_;                   // 当前持有读锁的线程数
    int writers_;                   // 当前持有写锁的线程数（0或1）
    int write_waiters_;             // 等待获取写锁的线程数
};


/**
 * @brief 读写锁使用案例
 *
 */
// void reader_function(int thread_id) {
//     // 创建 ReadGuard，构造函数内部会自动调用 rw_mutex.readLock()
//     RWMutex::ReadGuard read_guard(rw_mutex);
    
//     // 安全地读取共享数据
//     std::cout << "Reader " << thread_id << " sees: ";
//     for (const auto& item : shared_data) {
//         std::cout << item << " ";
//     }
//     std::cout << std::endl;
    
//     // read_guard 析构函数会自动调用 rw_mutex.readUnlock()
// }

// void writer_function(int new_value, int thread_id) {
//     // 创建 WriteGuard，构造函数内部会自动调用 rw_mutex.writeLock()
//     RWMutex::WriteGuard write_guard(rw_mutex);
    
//     // 安全地修改共享数据
//     shared_data.push_back(new_value);
//     std::cout << "Writer " << thread_id << " added: " << new_value << std::endl;
    
//     // write_guard 析构函数会自动调用 rw_mutex.writeUnlock()
// }



#endif // MX_RWMUTEX_H