# # net/CMakeLists.txt

# # 1. 搜集当前目录下所有的源码
# file(GLOB NET_SOURCES "*.cc")

# # 2. 将源码添加进名为 rmuduo 的库中
# # 注意：这里是目标定义的地方
# add_library(rmuduo SHARED ${NET_SOURCES})

# # 3. 配置库的链接属性
# target_link_libraries(rmuduo 
#     PUBLIC 
#         spdlog::spdlog    # 公开依赖，方便 example 使用
#     PRIVATE
#         Threads::Threads  # 私有依赖，库内部使用 pthread
# )

# # 4. 设置头文件搜索路径
# # PUBLIC 使得链接了 rmuduo 的目标能直接找到 net 文件夹下的头文件
# target_include_directories(rmuduo 
#     PUBLIC 
#         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> 
#         $<INSTALL_INTERFACE:include>
        
# )

# # 5. 设置编译选项
# if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#     target_link_options(rmuduo PRIVATE "-static-libstdc++" "-static-libgcc")
# endif()

# net/CMakeLists.txt

# 1. 搜集当前目录下所有的源码和头文件
file(GLOB NET_SOURCES "*.cc")
file(GLOB NET_HEADERS "*.h") # 自动搜集头文件用于安装

# 2. 将源码添加进名为 rmuduo 的库中
add_library(rmuduo SHARED ${NET_SOURCES})

# 3. 配置库的链接属性
target_link_libraries(rmuduo 
    PUBLIC 
        spdlog::spdlog    
    PRIVATE
        Threads::Threads  
)

# 4. 设置头文件搜索路径
target_include_directories(rmuduo 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> 
        $<INSTALL_INTERFACE:include>
)

# 5. 设置编译选项
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(rmuduo PRIVATE "-static-libstdc++" "-static-libgcc")
endif()

# ==================== 新增：安装配置 ====================

# 6. 安装动态库到 lib 目录
install(TARGETS rmuduo DESTINATION lib)

# 7. 安装头文件到 include/rmuduo/net
# 注意：这里加上 net 子目录，方便用户使用 #include <rmuduo/net/TcpServer.h>
install(FILES ${NET_HEADERS} DESTINATION include/rmuduo/net)