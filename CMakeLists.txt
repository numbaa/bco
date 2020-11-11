cmake_minimum_required(VERSION 3.12)

project(bco)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
add_definitions(-DNOMINMAX)
#add_compile_options(/await)
set(PLAT_SPEC
    "include/bco/detail/proactor_windows.h"
    "src/proactor_windows.cpp"
    )
else()
add_compile_options(-fcoroutines)
set(PLAT_SPEC
    "include/bco/detail/proactor_linux.h"
    )
endif()

add_library(${PROJECT_NAME} STATIC
    ${PLAT_SPEC}
    "include/bco/detail/utils.inl"
    "include/bco/detail/task.inl"
    "include/bco/bco.h"
    "include/bco/executor.h"
    "include/bco/io.h"
    "include/bco/context.h"
    "include/bco/buffer.h"
    "include/bco/task.h"
    "src/executor.cpp"
    "src/io.cpp"
    "src/task.cpp"
    "src/context.cpp"
 )

 if(WIN32)
 target_link_libraries(${PROJECT_NAME}
    "ws2_32.lib"
    "mswsock.lib"
 )
 else()
 endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "include"
)

add_subdirectory(examples)