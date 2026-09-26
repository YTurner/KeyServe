cmake_minimum_required(VERSION 3.22)

project(KeyServe
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(keyserve
    src/main.cpp
    src/database.cpp
    src/storage.cpp
    src/UniqueFd.cpp
)

target_include_directories(keyserve
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)