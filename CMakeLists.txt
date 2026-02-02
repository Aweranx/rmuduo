cmake_minimum_required(VERSION 3.20)

project(rmuduo)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

aux_source_directory(. SRC_LIST)

add_library(rmuduo SHARED ${SRC_LIST})