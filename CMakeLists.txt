cmake_minimum_required(VERSION 3.20)
project(Youtonomous CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)
add_compile_definitions(UNICODE _UNICODE)

add_executable(youtonomous WIN32
    src/main.cpp
    src/window.cpp
)
target_link_options(youtonomous PRIVATE -municode)
