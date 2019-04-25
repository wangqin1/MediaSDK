set(sources
    enctools.cpp
    src/mfx_enctools_brc.cpp
    src/mfx_enctools_common.cpp
    src/mfx_enctools_aenc.cpp
    src/mfx_enctools_utils.cpp
    src/mfx_enctools_lpla.cpp
    src/mfx_enctools_allocator.cpp
    include/mfx_enctools.h
    include/mfx_enctools_aenc.h
    include/mfx_enctools_brc.h
    include/mfx_enctools_defs.h
    include/mfx_enctools_lpla.h
    include/mfx_enctools_utils.h
    include/mfx_enctools_allocator.h
    ../shared/include/mfxenctools-int.h
    ../shared/umc/core/vm/include/vm_strings.h
    ../shared/umc/core/umc/include/umc_defs.h

    aenc/src/aenc.cpp
    aenc/src/av1_asc_agop_tree.cpp
    aenc/src/av1_asc_tree.cpp
    aenc/src/av1_asc.cpp
    aenc/include/aenc.h
    aenc/include/aenc++.h
    aenc/include/asc_cpu_detect.h
    aenc/include/av1_scd.h
  )

#if (OPEN_SOURCE)
  if (MFX_ENABLE_AENC)
    find_library( AENC_LIBRARY libaenc.a PATHS ${CMAKE_HOME_DIRECTORY}/contrib/aenc/ )
    if(NOT ${AENC_LIBRARY} MATCHES NOTFOUND)
      message(STATUS "aenc library was found here ${AENC_LIBRARY}")

      add_library(aenc STATIC IMPORTED GLOBAL)
      set_target_properties(aenc PROPERTIES IMPORTED_LOCATION "${CMAKE_HOME_DIRECTORY}/contrib/aenc/libaenc.a")
      target_include_directories(aenc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    else()
      message(STATUS "aenc library was not found (optional)! mfxlib will be built without aenc")
      set(MFX_ENABLE_AENC OFF)
    endif()
  endif()
#endif()

add_library(enctools_hw STATIC ${sources})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")

target_compile_definitions(enctools_hw
  PRIVATE
    $<$<BOOL:${MFX_ENABLE_AENC}>:MFX_ENABLE_AENC>
  )

target_include_directories(enctools_hw
  PUBLIC
    include
    ../shared/include
    ../shared/umc/core/vm/include
    ../shared/umc/core/umc/include
    aenc/include
  )
target_link_libraries(enctools_hw
  PUBLIC 
    vm
  PRIVATE
    $<$<BOOL:${MFX_ENABLE_AENC}>:aenc> 
  )

#include(sources_ext.cmake OPTIONAL)
