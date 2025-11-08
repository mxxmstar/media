#pragma once
#include <atomic>

class SpinLock {
public:
    void lock() {
        bool expected = false;
        // 原子对象当前的值等于expected, 则将原子对象的值设置为true
        while(!flag_.compare_exchange_weak(expected, true, std::memory_order_qcquire, 
            std::memory_order_relaxed)) {
                expected = false;   
        }
    }

    void unlock() {
        flag_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> flag_ = {false};
}

