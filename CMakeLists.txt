cmake_minimum_required(VERSION 3.14)
project(list_without_size LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YC_INTRUSIVE_LIST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/intrusive_list)
if(NOT EXISTS ${YC_INTRUSIVE_LIST_DIR}/intrusive_list.h)
    message(FATAL_ERROR
        "缺少依赖 intrusive_list.h：请先运行 git submodule update --init")
endif()

add_library(list_without_size INTERFACE)
target_include_directories(list_without_size INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${YC_INTRUSIVE_LIST_DIR})

add_executable(test_list_without_size test_list_without_size.cpp)
target_link_libraries(test_list_without_size PRIVATE list_without_size)
add_test(NAME test_list_without_size COMMAND test_list_without_size)

if(MSVC)
    target_compile_options(test_list_without_size PRIVATE /utf-8 /W4)
else()
    target_compile_options(test_list_without_size PRIVATE -Wall -Wextra)
endif()
