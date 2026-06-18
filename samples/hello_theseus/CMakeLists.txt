target_sources(${EXECUTABLE_NAME} PRIVATE main.c)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NRFX_CLOCK_ENABLED=1)
target_link_libraries(${EXECUTABLE_NAME} PUBLIC nrfx log)
