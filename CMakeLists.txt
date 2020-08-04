cmake_minimum_required(VERSION 3.12)

project(bco)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-fcoroutines)

add_library(${PROJECT_NAME} STATIC
    include/bco/bco.h
    include/bco/executor.h
    include/bco/io.h
    include/bco/promise.h
    include/bco/task.h

    src/executor.cpp
    src/io.cpp
    src/task.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
)

add_subdirectory(examples)