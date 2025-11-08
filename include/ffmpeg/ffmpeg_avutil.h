#pragma once

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/dict.h"
#include "libavutil/rational.h"
#include "libavcodec/avcodec.h"
#include "libavutil/hwcontext.h"
}

#include <stdexcept>
#include <string>
#include <utility>
#include <initializer_list>


namespace FFmpeg {

enum class FFmpegResult {
    // FFmpeg_
    SUCCESS = 1,    // 成功
    OK = 1,    // 成功
    TRUE = 1,    // 成功
    FALSE = 0,    // 失败
    FAILURE = 0,    // 失败
    ERROR = -1,   // 错误
    ENDFILE = -2,     // 到达文件末尾
    SEND_AGAIN = -3,    // send 侧EAGAIN，输入缓冲区满了, 需要send_packet(解复用器)
    RECV_AGAIN = -4, // receive 侧EAGAIN，输出缓冲区满了, 需要receive_frame(解码器) / receive_packet
    
    
    NOT_SUPPORT = -11,     // 不支持, 例如不支持的文件格式、不支持的编码等
    NOT_HW_DEVICE = -12,   // 硬件设备不支持
    INVALID_PARAM = -13,   // 无效参数

    TIMEOUT = -21, // 超时
    NET_ERROR = -22,   // 网络错误

    
};
namespace FFmpegResultHelper {

inline constexpr int toInt(FFmpegResult r) noexcept {
    return static_cast<int>(r);
}

inline constexpr FFmpegResult toFFmpegResult(int r) noexcept {
    return static_cast<FFmpegResult>(r);
}

}




/// @brief 封装FFmpeg中的 AVDictionary 类
class Dictionary {
public:
    Dictionary() = default;

    /// @brief 接管从外部传入的AVDictionary指针
    /// @param dict AVDictionary指针
    explicit Dictionary(AVDictionary* dict) : dict_(dict) {}

    void swap(Dictionary& other) noexcept {
        std::swap(dict_, other.dict_);
    }
    ~Dictionary() {
        free();
    }

    /// @brief 设置字典中的值 允许拷贝字符串
    /// @param key 键
    /// @param value 值
    /// @param flags 标志
    void set(const std::string& key, const std::string& value, int flags = 0) {
        int ret = av_dict_set(&dict_, key.c_str(), value.c_str(), flags);
        if(ret < 0) {
            throw std::runtime_error("av_dict_set failed, and key is: " + key);
        }
    }

    /// @brief 设置字典中的值 不允许拷贝字符串，需要保证key 和 value 的生命周期大于dict_!!!
    /// @param key 键
    /// @param value 值
    /// @param flags 标志
    void setNoCopy(char* key, char* value, int flags = 0) {
        int ret = av_dict_set(&dict_, key, value, flags | AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);
        if(ret < 0) {
            throw std::runtime_error("av_dict_set(no copy) failed, and key is: " + std::string(key));
        }
    }

    /// @brief 获取字典中的值
    /// @param key 键
    /// @param default_value 默认值
    /// @return 值
    std::string get(const std::string& key, const std::string& default_value = "") const {
        AVDictionaryEntry* entry = av_dict_get(dict_, key.c_str(), nullptr, 0);
        return entry ? std::string(entry->value) : default_value;
    }

    /// @brief 删除字典中的key
    /// @param key 键
    void remove(const std::string& key) {
        av_dict_set(&dict_, key.c_str(), nullptr, 0);
    }

    /// @brief  获取字典中key的条目数
    /// @return 条目数
    int count() const {
        return av_dict_count(dict_);
    }

    /// @brief 获取内部的AVDictionary指针
    /// @return AVDictionary指针
    AVDictionary* dict() const {
        return dict_;
    }
    
    /// @brief 获取内部的AVDictionary指针
    /// @return AVDictionary指针
    AVDictionary* raw() const {
        return dict_;
    }

    /// @brief 转移内部AVDictionary指针的所有权
    /// @return AVDictionary指针
    AVDictionary* release() {
        AVDictionary* dict = dict_;
        dict_ = nullptr;
        return dict;
    }
private:
    class Proxy {
    public:
        Proxy(Dictionary& d, std::string& key) : dict_(d), key_(key) {}

        /// @brief 赋值运算符
        /// @param value 值
        /// @return 引用
        Proxy& operator=(const std::string& value) {
            dict_.set(key_, value);
            return *this;
        }

        /// @brief 转换为字符串,读取值
        /// @return 字符串
        operator std::string() const {
            return dict_.get(key_);
        }

    private:
        Dictionary& dict_;
        std::string& key_;
    };

public:
    /// @brief 支持初始化列表创建字典
    /// @param list 初始化列表
    Dictionary(std::initializer_list<std::pair<std::string, std::string>> list) {
        for(auto& [key, value] : list) {
            set(key, value);
        }
    }
    Proxy operator[](const std::string& key) {
        return Proxy(*this, const_cast<std::string&>(key));
    }
private:


    void free() {
        if(dict_) {
            av_dict_free(&dict_);
            dict_ = nullptr;
        }
    }
    AVDictionary* dict_ = nullptr;
};

class Rational {
public:
    /// @brief 创建一个 rational 每秒帧率：num/den
    /// @param num 分子
    /// @param den 分母
    Rational(int num = 0, int den = 1) : r_{num, den} {}

    AVRational raw() const {
        return r_;
    }

    /// @brief 将rational转换为double
    double toDouble() const {
        return av_q2d(r_);
    }

    /// @brief 缩放rational, 将当前rational转换为dst倍
    /// @param dst 目标rational
    /// @return 缩放后的rational
    Rational rescale(const Rational &dst) const {
        AVRational res = av_mul_q(r_, dst.r_);
        return Rational(res.num, res.den);
    }

    /// @brief 缩放int64_t值    val * src / dst
    /// @param val 待缩放值
    /// @param src 源rational
    /// @param dst 目标rational
    /// @return 缩放后的int64_t值
    static int64_t rescale_q(int64_t val, const Rational& src, const Rational& dst) {
        return av_rescale_q(val, src.raw(), dst.raw());
    }

private:
    AVRational r_;
};


namespace tools {
    /// @brief  codec_name 转 codec_id
    /// @param name codec_name
    /// @return codec_id
    AVCodecID codec_name2id(const std::string& name);

    /// @brief codec_id 转 codec_name
    /// @param id codec_id
    /// @return codec_name
    const std::string codec_id2name(AVCodecID id);

    /// @brief av_err 转 string
    /// @param err av_err
    /// @return string
    std::string av_err(int err);

    /// @brief hw_device_type 转 string
    /// @param type hw_device_type
    /// @return string
    std::string hw_device_type_name(AVHWDeviceType type);

    /// @brief 根据错误码抛出异常
    /// @param err 错误码
    inline void throw_error(int err, const std::string& msg, const std::string& file = __FILE__, int line = __LINE__) {
        if (err < 0) {
            std::string err_msg = av_err(err);
            std::string msg_ = msg + ": " + err_msg;
            throw std::runtime_error(msg_);
        }
    }
}

}
