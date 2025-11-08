#ifndef __DEFER_H__
#define __DEFER_H__

#include <utility>
#include <type_traits>

template <typename F>
class Defer {
public:
    /// @brief 构造函数
    /// @param f 延迟执行的函数
    explicit Defer(F&& f) noexcept : f_(std::forward<F>(f)), active_(true) {
    }

    /// @brief 移动构造
    /// @param other 另外一个Defer对象
    Defer(Defer&& other) noexcept : f_(std::move(other.f_)), active_(other.active_){
        other.active_ = false;
    }

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;
    Defer& operator=(Defer&&) = delete;

    /// @brief 析构函数
    ~Defer() noexcept{
        if(active_) {
            f_();
        }
    }

    void cancle() noexcept {
        active_ = false;
    }

private:
    // std::function<void()> f_;
    
    /// @brief 存储原始类型，不用进行转换
    typename std::decay<F>::type f_;
    /// @brief 是否激活
    bool active_;
};

template <typename F>
Defer<F> make_defer(F&& f) noexcept {
    return Defer<F>(std::forward<F>(f));
}

// 拼接a和b
#define DEFER_SPLICE2(a, b) a##b
// 只写一次宏的话__LINE__不会被替换成数字
#define DEFER_SPLICE(a, b) DEFER_SPLICE2(a, b)
// 生成唯一的变量名
#define DEFER_UNIQ(a) DEFER_SPLICE(a, __LINE__)
// 生成Defer对象
#define DEFER(f)    \
    do {    \
            auto DEFER_UNIQ(defer) = make_defer([&]() { f; });  \
            (void)DEFER_UNIQ(defer);    \
    } while (0)

#endif // __DEFER_H__