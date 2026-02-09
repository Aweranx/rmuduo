# example/CMakeLists.txt

# 打印一下路径，编译时看一眼屏幕，确认 CMake 跑到了这里
message(STATUS "Entering example directory: ${CMAKE_CURRENT_SOURCE_DIR}")

# 建议直接指定，如果文件不存在，CMake 运行阶段就会报错提示你
add_executable(echo_server echo_server.cc)

# 链接库
# 此时 rmuduo 目标必须已经由 add_subdirectory(net) 定义过了
target_link_libraries(echo_server PRIVATE rmuduo)

# 包含路径
target_include_directories(echo_server PRIVATE ${PROJECT_SOURCE_DIR})

# 设置输出位置到 bin 目录
set_target_properties(echo_server PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)