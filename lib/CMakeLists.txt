target_sources(${EXECUTABLE_NAME} PRIVATE nrfx_glue.c port.c log.c)
add_subdirectory(bluetooth)
