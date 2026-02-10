# 根目录 CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# 1. vcpkg 自动化配置
set(CMAKE_TOOLCHAIN_FILE "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "vcpkg triplet")


project(rmuduo_project)

# 2. 编译选项设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 3. 设置全局安装路径 (可选)
# 如果执行 make install 时不指定，默认会安装到 /usr/local/
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "..." FORCE)
endif()

# 4. 设置统一输出路径 (本地构建生成的库和可执行文件)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 5. 查找公共依赖
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

# 6. 进入子目录执行 (注意顺序！)
# 首先构建基础网络库 (生成 rmuduo)
add_subdirectory(net)

# 其次构建 HTTP 库 (生成 rmuduo_http，依赖 rmuduo)
add_subdirectory(http)

# 最后构建示例程序 (依赖上述库)
add_subdirectory(example)