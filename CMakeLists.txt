cmake_minimum_required(VERSION 3.15)
project(demo CXX)

# 设置C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 启用位置无关 pic
set(CMAKE_POSITION_INDEPENDENT_CODE ON) 

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
# 设置静态库输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/lib)


# 定义编译选项 USE_STATIC_LIBS，默认为 OFF (动态编译)
option(USE_STATIC_LIBS "Use static libraries" OFF)


# 添加编译定义解决 ffmpeg 宏问题
add_definitions(-D__STDC_CONSTANT_MACROS)
#ffmpeg库配置
find_package(PkgConfig REQUIRED)

# 告诉 pkg-config 在 ffmpeg-build-script 的 lib/pkgconfig 中找
# set(ENV{PKG_CONFIG_PATH} "/home/ub22/mx/mx_project/media/lib/ffmpeg_8_0/lib/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SOURCE_DIR}/lib/ffmpeg_8_0/lib/pkgconfig")

# 库文件路径
set(THIRD_PARTY_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
# FFmpeg 安装路径（修改为你实际路径）
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/lib/ffmpeg_8_0")
#Boost 库
set(BOOST_ROOT "${THIRD_PARTY_LIB_DIR}/boost_1_86_0")
#json 库
set(JSONCPP_ROOT "${THIRD_PARTY_LIB_DIR}/json_1_9_6/")

# 编译选择
if(USE_STATIC_LIBS)
    message(STATUS "Using static libraries")
    # 查找 FFmpeg 组件
    pkg_check_modules(FFMPEG REQUIRED
        libavformat
        libavcodec
        libavutil
        libswresample
        libswscale
        libavfilter
    )
    set(BOOST_LIBRARIES 
        ${BOOST_ROOT}/lib/libboost_system.a
        ${BOOST_ROOT}/lib/libboost_filesystem.a
    )
    set(JSONCPP_LIBRARIES 
        ${JSONCPP_ROOT}/lib/libjsoncpp.a
    )

    # 链接系统库（静态编译时需要）
    set(SYSTEM_LIBS pthread m dl z)

    # 静态编译不需要运行时路径
    set(RUNTIME_LIBRARY_PATHS "")
else()
    message(STATUS "Using shared libraries")
    # 获取 FFmpeg 库
    set(FFMPEG_DYNAMIC_ROOT "${THIRD_PARTY_LIB_DIR}/FFmpeg-7.0.2")
    
    # 设置包含和链接路径为动态库路径
    set(FFMPEG_ROOT "${FFMPEG_DYNAMIC_ROOT}")
    # 直接指定动态库名称
    set(FFMPEG_LIBRARIES
        avformat
        avcodec
        avutil
        swresample
        swscale
        avfilter
    )

    set(BOOST_LIBRARIES 
        boost_system
        boost_filesystem
    )
    set(JSONCPP_LIBRARIES 
        jsoncpp
    )
    # 链接系统库
    set(SYSTEM_LIBS pthread m dl z)

    # 设置运行时库路径
    set(RUNTIME_LIBRARY_PATHS 
        ${FFMPEG_ROOT}/lib
        ${BOOST_ROOT}/lib
        ${JSONCPP_ROOT}/lib
    )
endif()

# 包含头文件目录
include_directories(${FFMPEG_ROOT}/include
                    ${THIRD_PARTY_LIB_DIR}/boost_1_86_0/include
                    ${THIRD_PARTY_LIB_DIR}/json_1_9_6/include
                    )

# 指定库目录
link_directories(${FFMPEG_ROOT}/lib
                ${THIRD_PARTY_LIB_DIR}/boost_1_86_0/lib
                ${THIRD_PARTY_LIB_DIR}/json_1_9_6/lib
                )

# 添加头文件路径
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/base)
include_directories(${CMAKE_SOURCE_DIR}/include/log)
include_directories(${CMAKE_SOURCE_DIR}/include/net)
include_directories(${CMAKE_SOURCE_DIR}/include/media)
include_directories(${CMAKE_SOURCE_DIR}/include/third_part)
include_directories(${CMAKE_SOURCE_DIR}/include/utils)
include_directories(${CMAKE_SOURCE_DIR}/include/ffmpeg)
include_directories(${CMAKE_SOURCE_DIR}/include/queue)



# 将变量设置为CACHE INTERNAL，这样子目录可以访问
set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES} CACHE INTERNAL "FFmpeg libraries")
set(FFMPEG_LDFLAGS_OTHER ${FFMPEG_LDFLAGS_OTHER} CACHE INTERNAL "FFmpeg ld flags")
set(MANUAL_BOOST_LIBRARIES ${BOOST_LIBRARIES} CACHE INTERNAL "Manual Boost libraries")
set(MANUAL_JSONCPP_LIBRARIES ${JSONCPP_LIBRARIES} CACHE INTERNAL "Manual JsonCpp libraries")
set(SYSTEM_LIBS ${SYSTEM_LIBS} CACHE INTERNAL "System libraries")
set(RUNTIME_LIBRARY_PATHS "${RUNTIME_LIBRARY_PATHS}" CACHE INTERNAL "Runtime library paths")


# 添加.cpp文件目录
file(GLOB BASE_SRC ${CMAKE_SOURCE_DIR}/src/base/*.cpp)
file(GLOB FFMPEG_SRC ${CMAKE_SOURCE_DIR}/src/ffmpeg/*.cpp)
# 创建静态库用于共享代码
add_library(src_code STATIC ${FFMPEG_SRC}
                            ${BASE_SRC}
                            )

# 源文件
add_executable(demo src/main.cpp)

# 设置主程序输出路径
set_target_properties(demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
# 设置运行时路径（仅动态编译需要）
if(NOT USE_STATIC_LIBS)
    set_target_properties(demo PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${RUNTIME_LIBRARY_PATHS}"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()


message(STATUS "FFMPEG_LIBRARIES = ${FFMPEG_LIBRARIES}")
message(STATUS "FFMPEG_ROOT = ${FFMPEG_ROOT}")
# message(STATUS "FFMPEG_LIBS = ${FFMPEG_LIBS}")
# message(STATUS "FFMPEG_LDFLAGS = ${FFMPEG_LDFLAGS}")
# message(STATUS "FFMPEG_LDFLAGS_OTHER = ${FFMPEG_LDFLAGS_OTHER}")
message(STATUS "FFmpeg dynamic root: ${FFMPEG_DYNAMIC_ROOT}")
message(STATUS "Checking if FFmpeg include dir exists: ${FFMPEG_ROOT}/include")
if(EXISTS "${FFMPEG_ROOT}/include")
    message(STATUS "FFmpeg include dir exists")
else()
    message(WARNING "FFmpeg include dir does not exist")
endif()

message(STATUS "Checking if FFmpeg lib dir exists: ${FFMPEG_ROOT}/lib")
if(EXISTS "${FFMPEG_ROOT}/lib")
    message(STATUS "FFmpeg lib dir exists")
else()
    message(WARNING "FFmpeg lib dir does not exist")
endif()

# 链接 FFmpeg 静态库
target_link_libraries(demo 
                        PRIVATE 
                        src_code
                        ${FFMPEG_LIBRARIES} ${FFMPEG_LDFLAGS_OTHER}
                        ${MANUAL_BOOST_LIBRARIES}
                        ${MANUAL_JSONCPP_LIBRARIES}
                        ${SYSTEM_LIBS}
                    )


# 添加测试目录
enable_testing()
if(EXISTS ${CMAKE_SOURCE_DIR}/test/CMakeLists.txt)
    add_subdirectory(test)
endif()
