cmake_minimum_required(VERSION 3.20)
project(Youtonomous CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)
add_compile_definitions(UNICODE _UNICODE)

find_package(nlohmann_json REQUIRED)

add_library(core STATIC
    src/timecode.cpp
    src/library.cpp
)
target_include_directories(core PUBLIC src)
target_link_libraries(core PUBLIC nlohmann_json::nlohmann_json)

add_executable(youtonomous WIN32
    src/main.cpp
    src/window.cpp
)
target_link_libraries(youtonomous PRIVATE core)
target_link_options(youtonomous PRIVATE -municode)

enable_testing()
add_executable(tests tests/tests.cpp)
target_link_libraries(tests PRIVATE core)
add_test(NAME tests COMMAND tests)
