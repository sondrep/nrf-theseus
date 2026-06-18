target_sources(log PRIVATE log.c)
#target_link_libraries(c PRIVATE log)

target_link_libraries(log PUBLIC nrfx freertos_config freertos_kernel c)
target_link_libraries(nrfx PUBLIC log)
