set(THESEUS_MODULES bluetooth log rng tinycrypt)

foreach(module IN LISTS THESEUS_MODULES)
add_library(${module} EXCLUDE_FROM_ALL)
endforeach()

foreach(module IN LISTS THESEUS_MODULES)
add_subdirectory(${module})
endforeach()
