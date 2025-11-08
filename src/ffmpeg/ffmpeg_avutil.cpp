#include "ffmpeg_avutil.h"
#include <string>
namespace FFmpeg { 









namespace tools {
    AVCodecID codec_name2id(const std::string& name) {
        if (name.empty()) {
            return AV_CODEC_ID_NONE;
        }
        
        auto codec = avcodec_find_decoder_by_name(name.c_str());
        if (codec == nullptr) {
            codec = avcodec_find_encoder_by_name(name.c_str());
        }

        if (codec == nullptr) {
            return AV_CODEC_ID_NONE;
        }
        return codec->id;
    }

    const std::string codec_id2name(AVCodecID id) {
        static const std::string none;
        if(const char* n = avcodec_get_name(id); n != nullptr) {
            return std::string(n);
        }
        return none;
    }

    std::string av_err(int err) {
        // return std::string(av_err2str(err));
        char buf[128] = {0};
        av_strerror(err, buf, sizeof(buf));
        return std::string(buf);
    }

    std::string hw_device_type_name(AVHWDeviceType type) {
        return std::string(av_hwdevice_get_type_name(type));
    }

}


}