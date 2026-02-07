# 这里的源文件是 example 目录下的 test.cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test.cpp")
    
    # 1. 定义可执行文件目标
    add_executable(test_app test.cpp)

    # 2. 链接我们在 net 目录下定义的 rmuduo 库
    # 由于 net/CMakeLists.txt 中使用了 PUBLIC 属性，
    # test_app 会自动继承 spdlog 的头文件路径和链接库
    target_link_libraries(test_app PRIVATE rmuduo)

    # 3. (可选) 如果你的代码里习惯用 #include "net/TcpServer.h" 这种带路径的包含
    # 则需要把项目根目录加入包含路径
    target_include_directories(test_app PRIVATE ${CMAKE_SOURCE_DIR})

    # 打印一条信息，方便调试确认
    message(STATUS "example: test_app will be built and linked to rmuduo")
endif()