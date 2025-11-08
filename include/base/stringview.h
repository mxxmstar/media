#pragma once

#if __cplusplus >= 202002L
#include <string_view>
/// @brief 同std::string_view
using StringView = std::string_view;

#else

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <string>
/// @brief 仿std::string_view实现
class StringView {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    /// @brief 默认构造
    StringView() noexcept = default;

    /// @brief 构造函数
    /// @param str 字符串指针
    StringView(const char* str) : data_(str), size_(str ? std::strlen(str) : 0) {}

    /// @brief 构造函数
    /// @param str 字符串指针
    /// @param len 字符串长度
    StringView(const char* str, size_t len) : data_(str), size_(len) {}

    /// @brief 构造函数
    /// @param str 字符串引用
    StringView(const std::string& str) noexcept : data_(str.data()), size_(str.size()) {}

    /// @brief 获取字符串指针
    /// @return 字符串指针
    constexpr const char* data() const noexcept { return data_; }

    /// @brief 获取字符串长度
    /// @return 字符串长度
    constexpr std::size_t size() const noexcept { return size_; }

    /// @brief 判断字符串是否为空
    /// @return 是否为空
    constexpr bool empty() const noexcept { return size_ == 0; }

    /// @brief 重载[]运算符
    /// @param pos 位置
    /// @return 字符
    char operator[](std::size_t pos) const { return data_[pos]; }

    /// @brief 获取字符串开始迭代器
    /// @return 迭代器
    const char* begin() const { return data_; }

    /// @brief 获取字符串结束迭代器
    /// @return 迭代器
    const char* end() const { return data_ + size_; }

    /// @brief 重载==运算符
    /// @param other 另一个字符串视图
    /// @return 是否相等
    bool operator==(const StringView& other) const {
        return size_ == other.size_ && std::memcmp(data_, other.data_, size_) == 0;
    }

    /// @brief 重载!=运算符
    /// @param other 另一个字符串视图
    /// @return 是否不相等
    bool operator!=(const StringView& other) const {
        return !(*this == other);
    }

    /// ============子串===============

    /// @brief 获取子串
    /// @param pos 起始位置，从零开始
    /// @param count 字符串长度
    /// @return 子串
    StringView substr(std::size_t pos, size_t count) const {
        if(pos > size_) {
            pos = size_;
        }
        if(count > size_ - pos) {
            count = size_ - pos;
        }
        return StringView(data_ + pos, count);
    }

    /// @brief 移除前面n个字符
    /// @param n 字符数
    /// @return 移除后的字符引用
    StringView remove_prefix(std::size_t n) {
        if(n > size_) {
            n = size_;
        }
        data_ += n;
        size_ -= n;
        return *this;
    }

    /// @brief 移除后面n个字符
    /// @param n 字符数
    /// @return 移除后的字符引用
    StringView remove_suffix(std::size_t n) {
        if(n > size_) {
            n = size_;
        }
        size_ -= n;
        return *this;
    }

    /// @brief 查找字符
    /// @param c 字符
    /// @param pos 起始位置
    /// @return 字符在字符串中的位置，未找到返回npos
    size_t find(char c, size_t pos = 0) const {
        if(pos >= size_) {
            return std::string::npos;
        }

        const char* ptr = static_cast<const char*>(std::memchr(data_+ pos, c, size_ - pos));
        return ptr ? static_cast<size_t>(ptr - data_) : std::string::npos;
    }

    /// @brief 查找StringView字符串
    /// @param sv StringView引用
    /// @param pos 起始位置
    /// @return 字符子串在字符串中的位置，未找到返回npos
    std::size_t find(const StringView& sv, size_t pos = 0) const {
        if(sv.size_ == 0) {
            // 空字符串直接返回pos
            return pos <= size_ ? pos : std::string::npos;
        }

        if(sv.size_ > size_) {
            return std::string::npos;
        }

        for(std::size_t i = pos; i <= size_ - sv.size_; ++i) {
            if (std::memcmp(data_ + i, sv.data_, sv.size_) == 0) return i;
        }
        return std::string::npos;
    }

    /// @brief 从后往前查找字符
    /// @param c 字符
    /// @return 字符在字符串中的位置，未找到返回npos
    size_t rfind(char c) const {
        for (size_t i = size_; i-- > 0;) {
            if (data_[i] == c) return i;
        }
        return std::string::npos;
    }

    /// @brief 查找第一个匹配sv中任意字符的位置
    /// @param sv StringView引用
    /// @param pos 起始位置
    /// @return 字符在字符串中的位置，未找到返回npos
    /// @note 从pos位置开始查找，返回第一个匹配字符的位置
    size_t find_first_of(const StringView& sv, size_t pos = 0) const {
        for (size_t i = pos; i < size_; ++i) {
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[i] == sv.data_[j]) return i;
            }
        }
        return std::string::npos;
    }

    /// @brief 查找最后一个匹配sv中任意字符的位置
    /// @param sv StringView引用
    /// @return 字符在字符串中的位置，未找到返回npos
    /// @note 从字符串末尾开始查找，返回最后一个匹配字符的位置
    size_t find_last_of(const StringView& sv) const {
        for (size_t i = size_; i-- > 0;) {
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[i] == sv.data_[j]) return i;
            }
        }
        return std::string::npos;
    }

    /// @brief 查找第一个不匹配sv中任意字符的位置
    /// @param sv StringView引用
    /// @param pos 起始位置
    /// @return 字符在字符串中的位置，未找到返回npos
    /// @note 从pos位置开始查找，返回第一个不匹配字符的位置
    size_t find_first_not_of(const StringView& sv, size_t pos = 0) const {
        for (size_t i = pos; i < size_; ++i) {
            bool match = false;
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[i] == sv.data_[j]) {
                    match = true;
                    break;
                }
            }
            if (!match) return i;
        }
        return std::string::npos;
    }

    /// @brief 查找最后一个不匹配sv中任意字符的位置
    /// @param sv StringView引用
    /// @return 字符在字符串中的位置，未找到返回npos
    /// @note 从字符串末尾开始查找，返回最后一个不匹配字符的位置
    /// @warning 该方法的时间复杂度为O(n^2)，不建议在长字符串中使用
    size_t find_last_not_of(const StringView& sv) const {
        for (size_t i = size_; i-- > 0;) {
            bool match = false;
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[i] == sv.data_[j]) {
                    match = true;
                    break;
                }
            }
            if (!match) return i;
        }
        return std::string::npos;
    }

    // ===== 前缀/后缀判断 =====
    
    /// @brief 是否以sv开头
    /// @param sv StringView引用
    /// @return 以sv开头返回true，否则返回false
    bool starts_with(const StringView& sv) const {
        return size_ >= sv.size_ && std::memcmp(data_, sv.data_, sv.size_) == 0;
    }

    /// @brief 是否以c开头
    /// @param c 字符
    /// @return 以c开头返回true，否则返回false
    bool starts_with(char c) const {
        return !empty() && data_[0] == c;
    }

    /// @brief 是否以c结尾
    /// @param c 字符
    /// @return 以c结尾返回true，否则返回false
    bool ends_with(char c) const {
        return !empty() && data_[size_ - 1] == c;
    }

    /// @brief 是否以sv结尾
    /// @param sv StringView引用
    /// @return 以sv结尾返回true，否则返回false
    bool ends_with(const StringView& sv) const {
        return size_ >= sv.size_ && std::memcmp(data_ + size_ - sv.size_, sv.data_, sv.size_) == 0;
    }

    // ===== 转换 =====
    
    /// @brief 转换为std::string
    /// @return std::string对象
    std::string to_string() const {
        return std::string(data_, size_);
    }

    /// @brief 隐式转换成std::string
    operator std::string() const {
        return to_string();
    }

    friend std::ostream& operator<<(std::ostream& os, const StringView& sv) {
        return os.write(sv.data_, sv.size_);
    }
private:
    const char* data_ = nullptr;
    std::size_t size_ = 0;
};
#endif