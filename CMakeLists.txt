cmake_minimum_required(VERSION 3.20)

project(BinaryDB VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(BinaryDB
    src/main.cpp

    src/app/cli.cpp

    src/database/database.cpp
)

target_include_directories(BinaryDB PRIVATE
    src
)
