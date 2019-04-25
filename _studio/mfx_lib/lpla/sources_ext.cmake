add_library(lpla STATIC)

target_sources(lpla
  PRIVATE
    mfx_lp_lookahead.h
    mfx_lp_lookahead.cpp
)

target_include_directories(lpla
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MSDK_LIB_ROOT}/shared/include/feature_blocks
    ${MSDK_LIB_ROOT}/encode_hw/h265/include
    ${MSDK_LIB_ROOT}/encode_hw/shared
    ${MSDK_LIB_ROOT}/encode_hw/hevc
    ${MSDK_LIB_ROOT}/encode_hw/hevc/agnostic
    ${MSDK_LIB_ROOT}/encode_hw/hevc/agnostic/base
    ${MSDK_LIB_ROOT}/encode_hw/hevc/agnostic/g12
    ${MSDK_LIB_ROOT}/encode_hw/shared/embargo
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/agnostic/base
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/windows
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/windows/base
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/windows/g11lkf
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/windows/g12
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/windows/g12xehp
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/windows/g12dg2
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/agnostic/g11lkf
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/agnostic/g12
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/agnostic/g12xehp
    ${MSDK_LIB_ROOT}/encode_hw/hevc/embargo/agnostic/g12dg2
)

target_compile_definitions(lpla
  PRIVATE
    MFX_VA
  )

target_link_libraries(lpla
  PRIVATE
    mfx_static_lib
    mfx_sdl_properties
    umc_va_hw
    vm_plus
    vpp_hw
)
