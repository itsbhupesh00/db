cmake_minimum_required(VERSION 3.20)

project(BinaryDB VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCE_FILES
    src/main.cpp
    src/app/cli.cpp
    src/database/database.cpp
)

add_executable(BinaryDB ${SOURCE_FILES})

target_include_directories(BinaryDB PRIVATE src)

target_compile_features(BinaryDB PRIVATE cxx_std_17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(BinaryDB PRIVATE DEBUG_BUILD)
endif()

set_target_properties(BinaryDB PROPERTIES
    OUTPUT_NAME "binarydb"
)
