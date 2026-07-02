target_sources(log PRIVATE log.c)
theseus_mark_first_party(log log.c)

target_link_libraries(log PUBLIC nrfx_nonsecure freertos_config freertos_kernel c board)
target_link_libraries(nrfx_nonsecure PRIVATE log freertos_kernel freertos_config)
target_link_libraries(log PUBLIC freertos_kernel)
