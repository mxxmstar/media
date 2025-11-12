#pragma once
#include <cstdint>
#include <memory>

// 内部服务传输
struct AVMediaFrame {
    void* ptrData;
    std::shared_ptr<uint8_t> header;
    std::shared_ptr<uint8_t> buffer;
    uint32_t size;
    uint8_t type;
    struct timeval timestamp;
};

