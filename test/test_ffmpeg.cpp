#include <iostream>
#include <string>
#include <sys/stat.h>
// #include "ffmpeg_log.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
#include "ffmpeg/ffmpeg_codec.h"
#include "ffmpeg/ffmpeg_transcode.h"
using namespace FFmpeg;

int test_audio_transcode() {
    std::string input_url = "/home/ub22/mx/mx_project/media/v1080.mp4";
    std::string output_url = "/home/ub22/mx/mx_project/media/test.mp3";
    struct stat buffer;
    if (stat(input_url.c_str(), &buffer) != 0) {
        std::cerr << "Input file does not exist: " << input_url << std::endl;
        return 1;
    }
    std::cout << "Audio transcoding started" << std::endl;

    // 音频编码参数
    int sample_rate = 44100;
    int channels = 2;
    AVSampleFormat sample_fmt = AV_SAMPLE_FMT_FLTP;
    int bit_rate = 128000;

}
int main() {
#if 0
    // 1. 打印 FFmpeg 版本
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;

    // 2. 打印各库版本号（十六进制）
    unsigned vc = avcodec_version();
    unsigned vf = avformat_version();
    unsigned vu = avutil_version();
    std::cout << "avcodec  0x" << std::hex << vc << std::dec << std::endl;
    std::cout << "avformat 0x" << std::hex << vf << std::dec << std::endl;
    std::cout << "avutil   0x" << std::hex << vu << std::dec << std::endl;
    std::cout << "✅ FFmpeg libraries linked successfully!" << std::endl;
#endif

    std::string input_url = "/home/ub22/mx/mx_project/media/v1080.mp4";
    std::string output_url = "/home/ub22/mx/mx_project/media/test.mp4";
    struct stat buffer;
    if (stat(input_url.c_str(), &buffer) != 0) {
        std::cerr << "Input file does not exist: " << input_url << std::endl;
        return 1;
    } else {
            std::cout << "Program started" << std::endl;
    }
    int w = 1280;
    int h = 720;
    double fps = 25;
    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
    int bit_rate = 4000000;
        // 测试编码器可用性
    const char* test_encoders[] = {"libx264", "h264", "mpeg4", "libx265", nullptr};
    std::string encoder_name;
    
    for (int i = 0; test_encoders[i] != nullptr; i++) {
        const AVCodec* codec = avcodec_find_encoder_by_name(test_encoders[i]);
        if (codec) {
            std::cout << "Found encoder: " << test_encoders[i] << std::endl;
            encoder_name = test_encoders[i];
            break;
        } else {
            std::cout << "Encoder not found: " << test_encoders[i] << std::endl;
        }
    }
    
    if (encoder_name.empty()) {
        std::cerr << "No suitable encoder found!" << std::endl;
        return 1;
    }
    std::cout << "Using encoder: " << encoder_name << std::endl;
    VideoCodecParams codec_params(encoder_name, w, h, fps, pix_fmt, bit_rate);
    VideoTranscoder trans(input_url, output_url, codec_params, false, nullptr);//=============
    std::cout << "Transcoding started" << std::endl;
    FFmpegResult res = trans.Transcode();
    if (res != FFmpegResult::TRUE) {
        std::cerr << "Transcode failed, error: " << FFmpegResultHelper::toInt(res) << std::endl;
    }
}