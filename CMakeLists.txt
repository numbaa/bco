cmake_minimum_required(VERSION 3.12)

project(bco)

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_definitions(-DNOMINMAX)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-fcoroutines)
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

if(WIN32)
    set(PLAT_SPEC
        "include/bco/net/proactor/iocp.h"
        "src/net/proactor/iocp.cpp"
        )
else()
    set(PLAT_SPEC
        "include/bco/net/proactor/epoll.h"
        "include/bco/net/proactor/iouring.h"
        "src/net/proactor/epoll.cpp"
        "src/net/proactor/iouring.cpp"
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
    "include/bco/coroutine/cofunc.h"
    
    "include/bco/executor/simple_executor.h"
    "src/executor/simple_executor.cpp"
    
    "include/bco/net/socket.h"
    "include/bco/net/udp.h"
    "include/bco/net/proactor/select.h"
    "src/net/proactor/select.cpp"
    "include/bco/net/event.h"
    "src/net/event.cpp"
     "src/executor.cpp"  "include/bco/net/tcp.h" "src/net/tcp.cpp" "src/net/udp.cpp" "src/net/address.cpp" "src/coroutine/cofunc.cpp" "include/bco/coroutine/channel.h")

 if(WIN32)
 target_link_libraries(${PROJECT_NAME}
    "ws2_32.lib"
    "mswsock.lib"
 )
 else()
 target_link_libraries(${PROJECT_NAME}
    "pthread"
 )
 endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "include"
)

add_subdirectory(examples)