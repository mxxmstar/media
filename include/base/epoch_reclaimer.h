#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

class EpochReclaimer {
public:
    using Epoch = uint64_t; // 计数器

    // @brief 待回收对象的最小描述
    struct RetiredBase {
        void* ptr;
        void (*deleter)(void*); //类型擦除
    };

    /// @brief 描述一个参与内存回收的线程的状态
    struct Participant {
        std::atomic<Epoch> local_epoch{0};  // 线程进入临界区时记录的epoch
        std::atomic<bool> active{true};     // 线程是否活跃（是否在访问共享结构）
        // 将要回收的对象分桶，方便在”落后两代“时整体回收
        std::vector<RetiredBase> retired[3]; // 线程当前活跃的epoch的待回收对象,分桶存放
        Participant* next{nullptr}; // 链接到全局参与者链表, 全局参与者链表是一个环形链表

        uint32_t probe_counter{0};// 周期性探测，避免长时间不触发推进
    };

    /// @brief 守卫对象，用于线程注册和注销
    struct Guard {
        EpochReclaimer& reclaimer_;
        Participant* self_;

        /// @brief 构造函数，注册线程 active = true
        /// 将local_epoch设为当前global_epoch，表示线程进入临界区
        Guard(EpochReclaimer&);

        /// @brief 析构函数，注销线程 active = false
        /// 尝试推进epoch和回收
        ~Guard();

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    };

    /// @brief 使用CAS把本线程的Participant节点挂到participants头上，
    /// 并保存在thread_local tls_participant_中，只注册一次
    /// @return 本线程的Participant节点
    Participant* register_thread();

    /// @brief 延迟释放对象    
    /// "逻辑删除"的对象先丢到当前epoch的桶里，不立刻delete
    /// 当g_epoch前进两代后，这个桶里的对象就必然安全，可以统一释放
    template <class T>
    void retire(T* ptr) {
        auto* self = register_thread();
        Epoch g = global_epoch_.load(std::memory_order_acquire);
        
        // 选择当前epoch对应的桶
        std::size_t b = static_cast<std::size_t>(g % 3);

        self->retired[b].push_back(RetiredBase{ptr, &deleter_impl<T>});

        // // 达到批量阈值时尝试推进+回收
        // if (self->retired[b].size() >= retire_batch_) {
        //     maybe_advance_and_reclaim(self);
        // }

        global_retired_count_.fetch_add(1, std::memory_order_relaxed);
        maybe_advance_and_reclaim(self);
    }

    // template <typename T>
    // void retire(T* obj) {
    //     retire(static_cast<void*>(obj), [](void* p) {delete static_cast<T*>(p);});
    // }

    /// @brief 线程主动声明自己不再访问共享结构，尝试推进epoch和回收
    void quiescent_point();

    // 调参API
    void set_base_batch(std::size_t n) { base_batch_ = n; }
    std::size_t get_base_batch() const { return base_batch_.load(); }
    void set_retire_batch(std::size_t n) { retire_batch_ = n; }
    std::size_t get_retire_batch() const { return retire_batch_.load(); }
    void set_probe_stride(uint32_t n) { probe_stride_.store(std::max<uint32_t>(1, n), std::memory_order_relaxed); }
    uint32_t get_probe_stride() const { return probe_stride_.load(std::memory_order_relaxed); }
    
    EpochReclaimer();
    ~EpochReclaimer();

    /// @brief 强制回收，不考虑epoch安全，直接将所有桶清空
    /// 确保无并发时使用
    void force_reclaim_all_unsafe();

private:
    std::atomic<Epoch> global_epoch_{0};

    /// @brief 全局参与者链表头节点，存放所有已注册线程的参与者
    std::atomic<Participant*> participants_{nullptr};

    /// @brief 线程本地局部变量
    /*inline */static thread_local Participant* tls_participant_;
    // std::size_t retire_batch_ = 64;
    std::atomic<std::size_t> base_batch_ {32};
    std::atomic<std::size_t> retire_batch_{32};
    std::atomic<uint32_t> probe_stride_{256};   // 周期性探测频率

    std::atomic<std::size_t> global_retired_count_{0};  // 全局累计reitred计数

    template <class T>
    static void deleter_impl(void* p) { delete static_cast<T*>(p); }

    /// @brief 扫描所有参与者，如果所有活跃线程的local_epoch >= cur，则说明”没有线程还在用cur-1代的数据“，
    /// 此时可以安全地把global_epoch从cur推进到cur+1
    /// @param cur 
    /// @return 
    bool can_advance(Epoch cur);

    /// @brief 尝试推进epoch和回收
    /// 1. 读cur = global_epoch
    /// 2. 如果can_advance(cur)，则global_epoch = cur + 1
    /// 3. 调用reclaim_safe_bukets(cur+1)回收安全桶
    /// 4. 即使未推进，也再按最新global_epoch扫一次安全桶（避免漏回收）
    /// @param self 本线程的Participant节点
    void maybe_advance_and_reclaim(Participant* self);
    
    /// @brief 回收所有安全桶(落后两代)
    /// 1. 遍历所有Participant节点
    /// 2. 对每个节点，调用reclaim_safe_bukets(node->local_epoch)
    /// @param g_now 当前global_epoch
    void reclaim_safe_bukets(Epoch g_now);

    /// @brief 循环推进几次并回收，保证所有桶都被扫到
    void drain_all();

    /// @brief 动态计算活跃线程数
    std::size_t active_thread_count();

    /// @brief 取出全局累计reitred计数并清零
    /// @return 全局累计reitred计数
    std::size_t take_and_reset_global_retired_count();
};