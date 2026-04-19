# rmuduo/api/CMakeLists.txt

file(GLOB API_SOURCES "*.cc")
file(GLOB API_HEADERS "*.h")

add_library(rmuduo_api SHARED ${API_SOURCES})

target_link_libraries(rmuduo_api
    PUBLIC
        rmuduo_http
)

target_include_directories(rmuduo_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rmuduo_api DESTINATION lib)
install(FILES ${API_HEADERS} DESTINATION include/rmuduo/api)
