target_sources(rng PRIVATE rng.c)
theseus_mark_first_party(rng rng.c)
target_link_libraries(rng PRIVATE nrfx)
