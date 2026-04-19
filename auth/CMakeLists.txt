# rmuduo/auth/CMakeLists.txt

file(GLOB AUTH_SOURCES "*.cc")
file(GLOB AUTH_HEADERS "*.h")

add_library(rmuduo_auth SHARED ${AUTH_SOURCES})

target_include_directories(rmuduo_auth
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rmuduo_auth DESTINATION lib)
install(FILES ${AUTH_HEADERS} DESTINATION include/rmuduo/auth)
