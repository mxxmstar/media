cmake_minimum_required(VERSION 3.15)
project(MyFFmpegApp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)


# 告诉 pkg-config 在 ffmpeg-build-script 的 lib/pkgconfig 中找
set(ENV{PKG_CONFIG_PATH} "/home/ub22/mx/mx_project/media/lib/ffmpeg_8_0/lib/pkgconfig")

# 查找 FFmpeg 组件
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswresample
    libswscale
    libavfilter
)

# FFmpeg 安装路径（修改为你实际路径）
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/lib/ffmpeg_8_0")

# 包含头文件目录
include_directories(${FFMPEG_ROOT}/include)

# 指定 FFmpeg 库目录
link_directories(${FFMPEG_ROOT}/lib)

# 源文件
add_executable(MyFFmpegApp src/main.cpp)


message(STATUS "FFMPEG_LIBRARIES = ${FFMPEG_LIBRARIES}")
message(STATUS "FFMPEG_LIBS = ${FFMPEG_LIBS}")
message(STATUS "FFMPEG_LDFLAGS = ${FFMPEG_LDFLAGS}")
message(STATUS "FFMPEG_LDFLAGS_OTHER = ${FFMPEG_LDFLAGS_OTHER}")

# 链接 FFmpeg 静态库
target_link_libraries(MyFFmpegApp PRIVATE ${FFMPEG_LIBRARIES} ${FFMPEG_LDFLAGS_OTHER})

# # 如果你使用 pthread / math / dl 等系统库（推荐加上）
# target_link_libraries(MyFFmpegApp PRIVATE pthread m dl)
