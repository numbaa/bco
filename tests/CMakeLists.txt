project(bco_test)

include_directories(doctest)

add_executable(${PROJECT_NAME}
    "main.cpp"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE bco
)