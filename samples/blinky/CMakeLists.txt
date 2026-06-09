target_sources(${PROJECT_NAME}.elf PRIVATE main.c)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NRFX_CLOCK_ENABLED=1)
