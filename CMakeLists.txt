cmake_minimum_required(VERSION 3.12)

project(bco)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
add_definitions(-DNOMINMAX)
#add_compile_options(/await)
set(PLAT_SPEC
    "include/bco/net/proactor/iocp.h"
    "src/net/proactor/iocp.cpp"
    )
else()
add_compile_options(-fcoroutines)
set(PLAT_SPEC
    "include/bco/detail/proactor_linux.h"
    )
endif()

add_library(${PROJECT_NAME} STATIC
    ${PLAT_SPEC}
    "include/bco/bco.h"
    "include/bco/context.h"
    "include/bco/executor.h"
    
    "include/bco/utils.h"

    "include/bco/coroutine/task.inl"
    "include/bco/coroutine/task.h"
    
    "include/bco/executor/simple_executor.h"
    "src/executor/simple_executor.cpp"
    
    "include/bco/net/socket.h"
    "include/bco/net/proactor/select.h"
    "src/net/proactor/select.cpp"
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