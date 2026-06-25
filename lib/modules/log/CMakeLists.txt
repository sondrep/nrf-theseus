target_sources(log PRIVATE log.c)
theseus_mark_first_party(log log.c)

target_link_libraries(log PUBLIC nrfx freertos_config freertos_kernel c board)
target_link_libraries(nrfx PUBLIC log)
