target_sources(${EXECUTABLE_NAME} PRIVATE port.c module.c)
target_sources(nrfx PRIVATE nrfx_glue.c)

# These are our files living in vendor targets we've silenced.
# Opt them back into strict warnings + UBSan so our own bugs still get caught.
theseus_mark_first_party(${EXECUTABLE_NAME} port.c module.c)
theseus_mark_first_party(nrfx nrfx_glue.c)

add_subdirectory(modules)
