#pragma once

// 基类模板，使用CRTP
template<typename T>
class Singleton {
public:
    // 获取单例实例的静态方法
    static T& getInstance() {
        // 静态局部变量，确保只初始化一次
        static T instance;
        return instance;
    }

    // 删除拷贝构造函数和赋值运算符，防止拷贝
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    // 保护构造函数，防止外部直接构造
    Singleton() = default;
    // 允许派生类调用析构函数
    ~Singleton() = default;
};

