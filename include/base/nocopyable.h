#pragma once



class NoCopyable
{
public:
    NoCopyable() = default;
    ~NoCopyable() = default;
    NoCopyable(const NoCopyable&) = delete;
    NoCopyable& operator=(const NoCopyable&) = delete;
};

#define NoCopyable(Class)            \
    Class(const Class&) = delete;    \
    Class& operator=(const Class&) = delete;


/// @brief 仅移动构造和移动赋值的CRTP类
/// @tparam Derived 子类类型
/// @note 子类需要提供 swap 方法
template <class Derived>
class MoveOnly {
public:
    MoveOnly() = delete;
    ~MoveOnly() = delete;
    MoveOnly(MoveOnly&& other) noexcept {
        auto& self = static_cast<Derived&>(*this);
        auto& other_self = static_cast<Derived&>(other);
        self.swap(other_self);  // 子类需要提供 swap 方法
    }
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        // 防止 x = std::move(x)
        if(&self != &other_self) {
            auto& self = static_cast<Derived&>(*this);
            auto& other_self = static_cast<Derived&>(other);
            // 直接用子类引用来判断
            self.swap(other_self);
        }
        return self;
    }

    /// @brief 删除拷贝构造函数
    MoveOnly(const MoveOnly&) = delete;
    /// @brief 删除赋值运算符
    MoveOnly& operator=(const MoveOnly&) = delete;
};

#define MOVE_ONLY(Class)                      \
    Class(Class&&) noexcept        = default; \
    Class& operator=(Class&&) noexcept = default; \
    Class(const Class&)            = delete;  \
    Class& operator=(const Class&) = delete; \
    Class() = delete; \
    ~Class() = delete;