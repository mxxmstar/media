#!/usr/bin/env bash
set -e                          # 遇错即停
PROJ_DIR=$(dirname "$(realpath "$0")")   # 脚本所在目录=工程根目录
BUILD_DIR="${PROJ_DIR}/build"

# 忽略大小写匹配
shopt -s nocasematch

# 用法：
#   ./build.sh                  # 默认：动态编译（USE_STATIC_LIBS=OFF）
#   ./build.sh static           # 手动切换：静态编译（USE_STATIC_LIBS=ON）
#   ./build.sh clean            # 清理构建树
#   ./build.sh incremental      # 增量编译

INCREMENTAL_BUILD=false

if [ "$1" = "static" ]; then
    #静态编译
    STATIC_FLAG="-DUSE_STATIC_LIBS=ON"
    BUILD_MODE="STATIC"
elif [ "$1" = "clean" ]; then
    #清理构建树
    echo "=== Clean build tree ==="
    rm -rf "${BUILD_DIR}"
    echo "=== Clean build tree done ==="
    exit 0
elif [[ "${1:0:3}" = "inc"* || "${1,,}" == "incremental"* ]]; then
    # 增量编译
    INCREMENTAL_BUILD=true
    STATIC_FLAG="-DUSE_STATIC_LIBS=OFF"
    BUILD_MODE="DYNAMIC(INCREMENTAL)"
else
    #动态编译
    STATIC_FLAG="-DUSE_STATIC_LIBS=OFF"
    BUILD_MODE="DYNAMIC"
fi
# 打印当前编译模式（方便用户确认）
echo "=== Build Mode: ${BUILD_MODE} ==="
echo "=== Project Path: ${PROJ_DIR} ==="
echo "=== Build Path: ${BUILD_DIR} ==="


# 1. 清理旧构建树（可选，如需增量编译可注释）
if [ "$INCREMENTAL_BUILD" = "false" ]; then
    echo "=== Clean old build tree ==="
    rm -rf "${BUILD_DIR}"
else 
    echo "=== Incremental build mode: Skipping clean step ==="
fi

# 2. 新建并进入构建树
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 3. 配置 + 编译 + 测试（可选）
echo "=== CMake configuration: (${BUILD_MODE}) ==="

cmake .. -DCMAKE_BUILD_TYPE=Release \
    ${STATIC_FLAG}


CPU_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build . -j "${CPU_CORES}"

# 如需单元测试再打开
# echo "=== Run tests ==="
# ctest --output-on-failure # 测试失败时输出详细日志

MAIN_BIN="${PROJ_DIR}/bin/demo"
TEST_BIN_DIR="${PROJ_DIR}/bin/test"

echo "=== Build finished. Binary at: ${BUILD_DIR}/bin/demo ==="
echo "${BUILD_MODE}: ${MAIN_BIN}"
if [ -d "${TEST_BIN_DIR}" ]; then
    echo "Test bin dir: ${TEST_BIN_DIR}."
fi
echo "Change Build Mode:"
echo "  Dynamic: ./build.sh"
echo "  Static: ./build.sh static"
echo "  Clean: ./build.sh clean"
echo "  Incremental: ./build.sh incremental"
