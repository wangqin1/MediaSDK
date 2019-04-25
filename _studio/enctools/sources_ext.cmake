if(OPEN_SOURCE)
  return()
endif()

set(sources
    aenc/src/aenc.cpp
    aenc/src/av1_asc_agop_tree.cpp
    aenc/src/av1_asc_tree.cpp
    aenc/src/av1_asc.cpp
    aenc/include/aenc.h
    aenc/include/aenc++.h
    aenc/include/asc_cpu_detect.h
    aenc/include/av1_scd.h
)

add_library(aenc STATIC ${sources})

target_include_directories(aenc
  PUBLIC
    aenc/include
)

target_link_libraries(aenc
  PRIVATE
    mfx_require_sse4_properties
    mfx_static_lib
    mfx_sdl_properties
  )

