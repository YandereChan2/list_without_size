cmake_minimum_required(VERSION 3.14)
project(list_without_size LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖不固定版本：每次配置时从 main 分支取最新版本。
# 需要强制更新时删除构建目录后重新配置（或使用 cmake --fresh）。
include(FetchContent)
FetchContent_Declare(
    intrusive_list
    URL https://github.com/YandereChan2/intrusive_list/archive/refs/heads/main.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(intrusive_list)

add_library(list_without_size INTERFACE)
target_include_directories(list_without_size INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(list_without_size INTERFACE intrusive_list)

add_executable(test_list_without_size test_list_without_size.cpp)
target_link_libraries(test_list_without_size PRIVATE list_without_size)
add_test(NAME test_list_without_size COMMAND test_list_without_size)

if(MSVC)
    target_compile_options(test_list_without_size PRIVATE /utf-8 /W4)
else()
    target_compile_options(test_list_without_size PRIVATE -Wall -Wextra)
endif()
