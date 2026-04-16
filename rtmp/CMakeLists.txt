# rmuduo/rtmp/CMakeLists.txt

# 1. 自动搜集当前目录下所有的 .cc 源码文件
file(GLOB RTMP_SOURCES "*.cc")

# 2. 自动搜集当前目录下所有的 .h 头文件（用于安装）
file(GLOB RTMP_HEADERS "*.h")

# 3. 创建 RTMP 库
add_library(rmuduo_rtmp SHARED ${RTMP_SOURCES})

# 4. 配置链接属性
# PUBLIC 确保依赖向下传递：链接了 rmuduo_rtmp 的目标也会自动链接 rmuduo 和 spdlog
target_link_libraries(rmuduo_rtmp 
    PUBLIC 
        rmuduo          # 基础网络库（TcpServer, EventLoop 等）
        spdlog::spdlog  # 日志库
)

# 5. 设置头文件搜索路径
target_include_directories(rmuduo_rtmp 
    PUBLIC 
        # 构建阶段：指向项目根目录，支持 <rmuduo/rtmp/xxx.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        # 安装阶段：指向系统 include 目录
        $<INSTALL_INTERFACE:include>
)

# 6. 设置编译选项（可选，与 net/http 保持一致）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(rmuduo_rtmp PRIVATE "-static-libstdc++" "-static-libgcc")
endif()

# 7. 安装配置
# 安装生成的动态库 (.so 文件) 到 lib 目录
install(TARGETS rmuduo_rtmp DESTINATION lib)

# 安装所有搜集到的头文件到 include/rmuduo/rtmp
install(FILES ${RTMP_HEADERS} DESTINATION include/rmuduo/rtmp)