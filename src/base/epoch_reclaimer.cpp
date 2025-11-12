#include "epoch_reclaimer.h"


thread_local EpochReclaimer::Participant* EpochReclaimer::tls_participant_ = nullptr;

EpochReclaimer::EpochReclaimer() : global_epoch_(0), participants_(nullptr) {

}

EpochReclaimer::~EpochReclaimer() {
    drain_all();
    auto* p = participants_.load();
    while(p) {
        auto* next = p->next;
        delete p;
        p = next;
    }
}

EpochReclaimer::Participant* EpochReclaimer::register_thread() {
    // 已注册
    if(tls_participant_) {
        return tls_participant_;
    }
    // 每个线程只有一个Paritcipant，记录它的local_epoch、active、退休节点
    auto* p = new Participant();
    Participant* old_head = participants_.load(std::memory_order_acquire);
    do {
        // 将新节点插入到全局参与者链表的头部
        p->next = old_head;
        // 循环重试，直至old_head更新成新注册的participants
    } while(!participants_.compare_exchange_weak(old_head, p, std::memory_order_release, std::memory_order_acquire));
    // 注册成功，将线程本地变量指向新注册的participant
    tls_participant_ = p;
    return p;
}

EpochReclaimer::Guard::Guard(EpochReclaimer& reclaimer) : reclaimer_(reclaimer) {
    // 获取当前线程对应的Participant节点
    self_ = reclaimer_.register_thread();
    // 将线程标记为active
    self_->active.store(true, std::memory_order_release);

    // 读取并将线程本地的epoch更新为全局epoch，表明线程正在使用该epoch内的数据
    Epoch g = reclaimer_.global_epoch_.load(std::memory_order_acquire);
    self_->local_epoch.store(g, std::memory_order_release);
}

EpochReclaimer::Guard::~Guard() {
    self_->active.store(false, std::memory_order_release);
    reclaimer_.maybe_advance_and_reclaim(self_);
}

// 线程执行到安全点，未持有任何共享指针时，线程可以手动调用这个函数尝试推进epoch和回收
void EpochReclaimer::quiescent_point() {
    maybe_advance_and_reclaim(register_thread());
}


bool EpochReclaimer::can_advance(Epoch cur) {
    Participant* p = participants_.load(std::memory_order_acquire);
    while(p) {
        // 线程活跃则读取它的local_epoch，线程还停留在旧的epoch就不能推进
        if(p->active.load(std::memory_order_acquire)) {
            Epoch e = p->local_epoch.load(std::memory_order_acquire);
            if(e < cur) {
                return false;
            }
        }
        p = p->next;
    }
    return true;
}

void EpochReclaimer::maybe_advance_and_reclaim(Participant* self) {
    // Epoch cur = global_epoch_.load(std::memory_order_acquire);
    // if(can_advance(cur)) {
    //     // 将global_epoch推进到cur+1，这是一个原子操作
    //     global_epoch_.store(cur + 1, std::memory_order_release);
    //     // 回收这个epoch对应的安全桶(落后该epoch两代)
    //     reclaim_safe_bukets(cur + 1);
    // }

    // // 回收一次当前epoch对应的安全epoch
    // reclaim_safe_bukets(global_epoch_.load(std::memory_order_acquire));
    
    (void)self;

    /// 只有在当前线程的retired数量超过阈值时才会触发epoch推进和回收
    // // 1. 统计本线程退休对象数量
    // std::size_t threads = active_thread_count();
    // retire_batch_.store(base_batch_ * threads, std::memory_order_relaxed);
    
    // std::size_t local_reitred = 0;
    // for(int i = 0; i < 3; ++i) {
    //     local_reitred += self->retired[i].size();
    // }
    // // 2. 只有当超过批量阈值时，才尝试推进epoch
    // if(local_reitred >= retire_batch_.load(std::memory_order_relaxed)) {
    //     Epoch cur = global_epoch_.load(std::memory_order_acquire);
    //     if(can_advance(cur)) {
    //         // 将global_epoch推进到cur+1，这是一个原子操作
    //         global_epoch_.store(cur + 1, std::memory_order_release);
    //         // 回收这个epoch对应的安全桶(落后该epoch两代)
    //         reclaim_safe_bukets(cur + 1);
    //     }

    //     //无论是否推进，仍然尝试清理当前安全桶
    //     reclaim_safe_bukets(global_epoch_.load(std::memory_order_acquire));
    // }


    std::size_t threads = active_thread_count();
    std::size_t threshold = base_batch_.load(std::memory_order_relaxed) * threads;
    retire_batch_.store(threshold, std::memory_order_relaxed);

    // 本线程累计retired数
    std::size_t local_retired = 0;
    for(int i = 0; i < 3; ++i) {
        local_retired += self->retired[i].size();
    }

    // 周期性探测：避免长期”达不到阈值“时完全不推进
    // 计数器自增
    std::size_t cnt = ++self->probe_counter;
    // 当前探测步长
    std::size_t stride = probe_stride_.load(std::memory_order_relaxed);
    bool probe = (cnt % stride == 0);

    // 取出全局累计retired数，并清零
    std::size_t global_retired = take_and_reset_global_retired_count();
    
    // 触发条件：
    // 1. 本线程达到阈值
    // 2. 全局累计达到阈值
    // 3. 周期性探测命中
    if(!(local_retired >= threshold || global_retired >= threshold || probe)) {
        return;
    }

    Epoch cur = global_epoch_.load(std::memory_order_acquire);
    if(can_advance(cur)) {
        // 将global_epoch推进到cur+1，这是一个原子操作
        global_epoch_.store(cur + 1, std::memory_order_release);
        // 回收这个epoch对应的安全桶(落后该epoch两代)
        reclaim_safe_bukets(cur + 1);
    }
    
    //无论是否推进，仍然尝试清理当前安全桶
    reclaim_safe_bukets(global_epoch_.load(std::memory_order_acquire));

}

void EpochReclaimer::reclaim_safe_bukets(Epoch g_now) {
    std::size_t safe_buket = static_cast<std::size_t>((g_now + 1) % 3);
    Participant* p = participants_.load(std::memory_order_acquire);
    while(p) {
        auto& vec = p->retired[safe_buket];
        for(auto& ele : vec) {
            ele.deleter(ele.ptr);
        }
        vec.clear();
        p = p->next;
    }
}

void EpochReclaimer::drain_all() {
    for(int i = 0; i < 4; ++i) {
        Epoch cur = global_epoch_.load(std::memory_order_acquire);
        global_epoch_.store(cur + 1, std::memory_order_release);
        reclaim_safe_bukets(cur + 1);
    }

    //再扫一遍当前的安全桶
    reclaim_safe_bukets(global_epoch_.load(std::memory_order_acquire));
    //清零全局计数
    global_retired_count_.store(0, std::memory_order_relaxed);
}


void EpochReclaimer::force_reclaim_all_unsafe() {
    Participant* p = participants_.load(std::memory_order_acquire);
    while(p) {
        for(int i = 0; i < 3; ++i) {
            auto& vec = p->retired[i];
            for(auto& ele : vec) {
                ele.deleter(ele.ptr);
            }
            vec.clear();
        }
        p = p->next;
    }
    global_retired_count_.store(0, std::memory_order_relaxed);
}

std::size_t EpochReclaimer::active_thread_count() {
    std::size_t count = 0;
    Participant* p = participants_.load(std::memory_order_acquire);
    while(p) {
        if(p->active.load(std::memory_order_acquire)) {
            ++count;
        }
        p = p->next;
    }
    return std::max<std::size_t>(1, count);
}

std::size_t EpochReclaimer::take_and_reset_global_retired_count() {
    return global_retired_count_.exchange(0, std::memory_order_acq_rel);
}



