cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
# 强制指定寻找 vcpkg 的安装目录
set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "vcpkg triplet")

project(rmuduo)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib) # 静态库
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib) # 动态库
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin) # 可执行文件

find_package(spdlog CONFIG REQUIRED)

file(GLOB_RECURSE LIB_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.cc"
)
list(REMOVE_ITEM LIB_SOURCES "test.cpp")

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    spdlog::spdlog
)


target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 强制静态链接标准库，解决 GLIBCXX 版本不匹配
    target_link_options(${PROJECT_NAME} PRIVATE "-static-libstdc++" "-static-libgcc")
endif()
# 编译测试程序 (test.cpp)
# 只有当 test.cpp 存在时才编译
if(EXISTS "${PROJECT_SOURCE_DIR}/test.cpp")
    add_executable(test_app test.cpp)
    
    # 核心步骤：将测试程序链接到你的库
    # target_link_libraries 会自动处理库路径和头文件路径
    target_link_libraries(test_app PRIVATE ${PROJECT_NAME})
    
endif()