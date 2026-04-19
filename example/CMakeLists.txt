# example/CMakeLists.txt

# 打印一下路径，编译时看一眼屏幕，确认 CMake 跑到了这里
message(STATUS "Entering example directory: ${CMAKE_CURRENT_SOURCE_DIR}")

# 建议直接指定，如果文件不存在，CMake 运行阶段就会报错提示你
add_executable(test_server Httpserver_test.cc)
add_executable(rtmp_server_test rtmp_server_test.cc)
add_executable(login_server login_server.cc)

# 链接库
# 此时 rmuduo 目标必须已经由 add_subdirectory(net) 定义过了
target_link_libraries(test_server PRIVATE 
    rmuduo_http   # 这是存放 HttpServer 实现的库
    rmuduo        # 这是存放 TcpServer 等基础类的库
)

target_link_libraries(rtmp_server_test PRIVATE
    rmuduo_rtmp
    rmuduo
)

target_link_libraries(login_server PRIVATE
    rmuduo_auth
    rmuduo_rtmp
    rmuduo_api
    rmuduo_http
    rmuduo
)

# 包含路径
target_include_directories(test_server PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(rtmp_server_test PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(login_server PRIVATE ${PROJECT_SOURCE_DIR})

# 设置输出位置到 bin 目录
set_target_properties(test_server PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

set_target_properties(rtmp_server_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

set_target_properties(login_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
