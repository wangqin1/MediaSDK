include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/../sample_common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../sample_plugins/rotate_cpu/include
)

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES libmfx dl pthread)

FIND_LIBRARY(MESA_LIBRARY libEGL.so libGL.so libgbm.so PATHS /usr/lib64 /usr/local/lib64)

if (NOT MESA_LIBRARY MATCHES NOTFOUND)
    link_libraries(EGL GL gbm)
endif()

make_executable( shortname universal )

install( TARGETS ${target} RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
