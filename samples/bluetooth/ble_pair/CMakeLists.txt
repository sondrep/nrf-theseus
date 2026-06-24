target_sources(${EXECUTABLE_NAME} PRIVATE main.c)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE log rng bluetooth)
