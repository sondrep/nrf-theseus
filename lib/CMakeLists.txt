target_sources(${EXECUTABLE_NAME} PRIVATE port.c module.c)
target_sources(nrfx PRIVATE nrfx_glue.c)
add_subdirectory(modules)
