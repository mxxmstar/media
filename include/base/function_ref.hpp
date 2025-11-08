#pragma once
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <functional>
#include <cstddef>


struct bad_function_ref : std::bad_function_call {
    const char* what() const noexcept override {
        return "bad_function_ref: bad function call";
    }
};

template <typename>
class FunctionRef;

template <typename R, typename... Args>
class FunctionRef<R(Args...)> { 
    using stub_t = R(*)(void*, Args&&...);
    void* obj_ptr = nullptr;
    stub_t stub = nullptr;
    R(*fubc_ptr)(Args...) = nullptr;
};