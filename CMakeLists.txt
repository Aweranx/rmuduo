# 根目录 CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "vcpkg triplet")

project(rmuduo_project) # 这里的项目名可以换一个，避免与库名 rmuduo 冲突

# 如果用户没有显式指定构建类型，则默认为 Release
# if(NOT CMAKE_BUILD_TYPE)
#     set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
# endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置统一输出路径
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 查找公共依赖
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

# 进入子目录执行
add_subdirectory(net)
add_subdirectory(example)