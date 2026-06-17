target_sources(${EXECUTABLE_NAME} PRIVATE nrfx_glue.c port.c module.c)
add_subdirectory(modules)
