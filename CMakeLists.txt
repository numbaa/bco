cmake_minimum_required(VERSION 3.12)

project(bco)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
add_compile_options(/await)
set(PLAT_SPEC
    include/bco/detail/proactor_windows.h
    include/bco/detail/promise_windows.h
    include/bco/detail/task_windows.h)
else()
add_compile_options(-fcoroutines)
set(PLAT_SPEC
    include/bco/detail/proactor_linux.h
    include/bco/detail/promise_linux.h
    include/bco/detail/task_linux.h)
endif()

add_library(${PROJECT_NAME} STATIC
    ${PLAT_SPEC}
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