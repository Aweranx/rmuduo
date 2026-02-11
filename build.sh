#!/bin/bash

set -e # 遇到错误立即停止执行

# 1. 定义构建目录
BUILD_DIR=$(pwd)/build
INSTALL_DIR=/usr/local

# 2. 清理并创建构建目录
# if [ -d "$BUILD_DIR" ]; then
#     echo "清理旧的构建目录..."
#     rm -rf "$BUILD_DIR"
# fi
# mkdir "$BUILD_DIR"

# 3. 运行 CMake 配置
# 使用 vcpkg 工具链，并指定安装前缀
echo "正在配置 CMake..."
# cmake -S . -B "$BUILD_DIR" \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
# 在 build.sh 中明确指定 vcpkg 路径
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="x64-linux" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/usr/local"

# 4. 执行编译
echo "开始编译项目..."
cmake --build "$BUILD_DIR" -j$(nproc)

# 5. 执行安装
# 注意：安装到 /usr/local 需要 sudo 权限
echo "正在安装到系统目录 ($INSTALL_DIR)..."
sudo cmake --install "$BUILD_DIR"

echo "========================================"
echo "编译并安装完成！"
echo "头文件已安装至: $INSTALL_DIR/include/rmuduo"
echo "库文件已安装至: $INSTALL_DIR/lib"
echo "========================================"