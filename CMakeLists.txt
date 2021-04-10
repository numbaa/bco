cmake_minimum_required(VERSION 3.12)

project(bco)

option(BCO_BUILD_WITH_TEST "Build bco with test" ON)
option(BCO_BUILD_WITH_EXAMPLE "Build bco with example" ON)

set(CMAKE_CXX_STANDARD 20)


# macro WIN32_LEAN_AND_MEAN
# gcc with windows, how ??

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
    "include/bco.h"
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
    "src/executor.cpp"
    "include/bco/net/tcp.h"
    "src/net/tcp.cpp"
    "src/net/udp.cpp"
    "src/net/address.cpp"
    "src/coroutine/cofunc.cpp"
    "include/bco/coroutine/channel.h"
    "src/trick.cpp"
    "include/bco/buffer.h"
    "src/buffer.cpp"
    "src/common.h"
    "src/common.cpp"
    "src/utils.cpp" "src/magic.h" "src/coroutine/task.cpp" "include/bco/executor/multithread_executor.h" "src/executor/multithread_executor.cpp")

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

if(BCO_BUILD_WITH_EXAMPLE)
    add_subdirectory(examples)
endif()

if(BCO_BUILD_WITH_TEST)
    enable_testing()
    add_subdirectory(tests)
    add_test(NAME bco_test COMMAND test_all)
endif()