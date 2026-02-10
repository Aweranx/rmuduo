# rmuduo/http/CMakeLists.txt

# 1. 自动搜集当前目录下所有的 .cc 源码文件
file(GLOB HTTP_SOURCES "*.cc")

# 2. 自动搜集当前目录下所有的 .h 头文件（用于安装）
file(GLOB HTTP_HEADERS "*.h")

# 3. 创建 HTTP 库
add_library(rmuduo_http SHARED ${HTTP_SOURCES})

# 4. 配置链接属性
# PUBLIC 确保依赖向下传递
target_link_libraries(rmuduo_http 
    PUBLIC 
        rmuduo
        spdlog::spdlog
)

# 5. 设置头文件搜索路径
target_include_directories(rmuduo_http 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# 6. 安装配置
# 安装生成的动态库 (.so 文件)
install(TARGETS rmuduo_http DESTINATION lib)

# 安装所有搜集到的头文件
# 保持 rmuduo/http 的层级结构
install(FILES ${HTTP_HEADERS} DESTINATION include/rmuduo/http)