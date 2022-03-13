// Copyright (c) 2017-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#include "va_mem_copy.h"

#include "mfx_common_int.h"

#define VACOPY_MIN_WIDTH  64
#define VACOPY_MIN_HEIGHT 64
#define ALIGN(value, alignment) (alignment) * ( (value) / (alignment) + (((value) % (alignment)) ? 1 : 0))

mfxStatus va_to_mfx_status(VAStatus va_res)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res)
    {
    case VA_STATUS_SUCCESS:
        mfxRes = MFX_ERR_NONE;
        break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
        mfxRes = MFX_ERR_MEMORY_ALLOC;
        break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
        mfxRes = MFX_ERR_UNSUPPORTED;
        break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
        mfxRes = MFX_ERR_NOT_INITIALIZED;
        break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }
    return mfxRes;
}

VaCopyWrapper::VaCopyWrapper()
    : m_dpy(nullptr), m_sysSurfaceID(VA_INVALID_SURFACE), m_guard()
{
    m_tableSysRelations.clear();
} // VaCopyWrapper::VaCopyWrapper(void)

VaCopyWrapper::~VaCopyWrapper(void)
{
    Release();
} // VaCopyWrapper::~VaCopyWrapper(void)

mfxStatus VaCopyWrapper::Initialize(VADisplay dpy)
{
    m_dpy = dpy; 
    return MFX_ERR_NONE;
}

VAStatus VaCopyWrapper::ReleaseUserVaSurface(
    VASurfaceID *pSurface)
{
    VAStatus va_sts = VA_STATUS_SUCCESS;

    if (VA_INVALID_SURFACE != *pSurface){
        va_sts = vaDestroySurfaces(m_dpy, pSurface, 1);
        if (VA_STATUS_SUCCESS != va_sts) {
            return va_sts;
        }
    }

    return va_sts;
}

mfxStatus VaCopyWrapper::Release(void)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    VAStatus va_sts = VA_STATUS_SUCCESS;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    for (std::map<mfxU8 *, VASurfaceID>::iterator it = m_tableSysRelations.begin(); it != m_tableSysRelations.end(); ++it) {
        if (VA_INVALID_SURFACE != (VASurfaceID)(it->second)) {
            va_sts = ReleaseUserVaSurface((VASurfaceID *)(&(it->second)));
            mfx_sts = va_to_mfx_status(va_sts);
            MFX_CHECK_STS(mfx_sts);
        }
    }

    m_tableSysRelations.clear();

    return MFX_ERR_NONE;
} // mfxStatus VaCopyWrapper::Release(void)

bool VaCopyWrapper::CanUseVaCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, mfxSize roi)
{
    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);
    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    if (NULL == pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if ((IsVaCopyFormatSupported(ConvertMfxFourccToVAFormat(pSrc->Info.FourCC))) &&
            (pSrc->Info.CropX == 0) &&
            (pSrc->Info.CropY == 0) &&
            (roi.width >= VACOPY_MIN_WIDTH) &&
            (roi.height >= VACOPY_MIN_HEIGHT) &&
            ((reinterpret_cast<size_t>(srcPtr) & 0xfff) == 0))
        {
            return true;
        }
    }
    else if (NULL != pSrc->Data.MemId && NULL == pDst->Data.MemId)
    {
        if ((IsVaCopyFormatSupported(ConvertMfxFourccToVAFormat(pDst->Info.FourCC))) &&
            (pDst->Info.CropX == 0) &&
            (pDst->Info.CropY == 0) &&
            (roi.width >= VACOPY_MIN_WIDTH) &&
            (roi.height >= VACOPY_MIN_HEIGHT) &&
            ((reinterpret_cast<mfxU64>(dstPtr) & 0xfff) == 0))
        {
            return true;
        }        
    }

    return false;
}

mfxStatus VaCopyWrapper::CopyVideoToSys(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, mfxU16 vaCopyMode)
{
    mfxStatus mfx_sts = MFX_ERR_NONE;

    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        VASurfaceID *va_surface_id = (VASurfaceID*)(pSrc->Data.MemId);

        mfx_sts = AcquireUserVaSurface(pDst);
        MFX_CHECK_STS(mfx_sts);

        mfx_sts = VACopy(*va_surface_id, m_sysSurfaceID, vaCopyMode);
        MFX_CHECK_STS(mfx_sts);

        return mfx_sts;
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

mfxStatus VaCopyWrapper::CopySysToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, mfxU16 vaCopyMode)
{

    mfxStatus mfx_sts = MFX_ERR_NONE;

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);

    if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        VASurfaceID *va_surface_id = (VASurfaceID*)(pDst->Data.MemId);

        mfx_sts = AcquireUserVaSurface(pSrc);
        MFX_CHECK_STS(mfx_sts);

        mfx_sts = VACopy(m_sysSurfaceID, *va_surface_id, vaCopyMode);
        MFX_CHECK_STS(mfx_sts);

        return mfx_sts;    
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

VAStatus VaCopyWrapper::CreateUserVaSurface(
    VASurfaceID *pSurface,
    mfxSurfaceInfo &surfaceInfo)
{
    VAStatus va_sts = VA_STATUS_SUCCESS;

    VASurfaceAttrib surface_attrib[3];
    VASurfaceAttribExternalBuffers ext_buffer;
    uint32_t size = 0;

    surface_attrib[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib[0].type = VASurfaceAttribPixelFormat;
    surface_attrib[0].value.type = VAGenericValueTypeInteger;
    surface_attrib[0].value.value.i = surfaceInfo.fourCC;

    surface_attrib[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib[1].type = VASurfaceAttribMemoryType;
    surface_attrib[1].value.type = VAGenericValueTypeInteger;
    surface_attrib[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

    surface_attrib[2].flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib[2].type = VASurfaceAttribExternalBufferDescriptor;
    surface_attrib[2].value.type = VAGenericValueTypePointer;
    surface_attrib[2].value.value.p = (void *)&ext_buffer;

    memset(&ext_buffer, 0, sizeof(ext_buffer));

    switch (surfaceInfo.fourCC)
    {
    case VA_FOURCC_NV12:
        ext_buffer.pitches[0] = ALIGN(surfaceInfo.pitch, 32);
        ext_buffer.pitches[1] = ext_buffer.pitches[0];
        size = (ext_buffer.pitches[0] * ALIGN(surfaceInfo.height, 32)) * 3 / 2;// frame size align with pitch.
        ext_buffer.offsets[0] = 0;// Y channel
        ext_buffer.offsets[1] = ext_buffer.pitches[0] * ALIGN(surfaceInfo.height, 32); // UV channel.
        ext_buffer.offsets[2] = ext_buffer.offsets[1] + 1;
        ext_buffer.num_planes = 2;
        break;

    case VA_FOURCC_AYUV:
        ext_buffer.pitches[0] = ALIGN(surfaceInfo.pitch, 32);
        size = ext_buffer.pitches[0] * ALIGN(surfaceInfo.height, 16);// frame size align with pitch.
        ext_buffer.num_planes = 1;
        break;

    case VA_FOURCC_P010:
        ext_buffer.pitches[0] = ALIGN(surfaceInfo.pitch, 64);
        ext_buffer.pitches[1] = ext_buffer.pitches[0];
        size = ext_buffer.pitches[0] * ALIGN(surfaceInfo.height, 32) * 3 / 2;// frame size align with pitch.
        ext_buffer.offsets[0] = 0;// Y channel
        ext_buffer.offsets[1] = ext_buffer.pitches[0] * ALIGN(surfaceInfo.height, 32); // UV channel.
        ext_buffer.offsets[2] = ext_buffer.offsets[1] + 2;
        ext_buffer.num_planes = 2;
        break;

    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        ext_buffer.pitches[0] = ALIGN(surfaceInfo.pitch, 32);
        size = ext_buffer.pitches[0] * ALIGN(surfaceInfo.height, 32);// frame size align with pitch.
        ext_buffer.num_planes = 1;
        break;

    default:
        return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
    }

    ext_buffer.pixel_format = surfaceInfo.fourCC;
    ext_buffer.width = surfaceInfo.width;
    ext_buffer.height = surfaceInfo.height;
    ext_buffer.data_size = ALIGN(size, 4096);
    ext_buffer.num_buffers = 1;
    ext_buffer.buffers = &(surfaceInfo.ptrb);
    ext_buffer.buffers[0] = (uintptr_t)(surfaceInfo.pBufBase);
    ext_buffer.flags = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

    va_sts = vaCreateSurfaces(m_dpy,
                              surfaceInfo.format,
                              surfaceInfo.width,
                              surfaceInfo.height,
                              pSurface,
                              1,
                              surface_attrib,
                              3);

    return va_sts;
}

bool VaCopyWrapper::IsVaCopyFormatSupported(mfxU32 fourCC)
{
    switch (fourCC)
    {
    case VA_FOURCC_NV12:
    case VA_FOURCC_AYUV:
    case VA_FOURCC_P010:
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        return true;
    default:
        return false;
    }    
}

mfxU32 VaCopyWrapper::ConvertVAFormatToMfxFourcc(mfxU32 fourCC)
{
    switch (fourCC)
    {
    case VA_FOURCC_NV12:
        return MFX_FOURCC_NV12;
    case VA_FOURCC_YUY2:
        return MFX_FOURCC_YUY2;
    case VA_FOURCC_UYVY:
        return MFX_FOURCC_UYVY;
    case VA_FOURCC_YV12:
        return MFX_FOURCC_YV12;
#if (MFX_VERSION >= 1028)
    case VA_FOURCC_RGB565:
        return MFX_FOURCC_RGB565;
#endif
    case VA_FOURCC_ARGB:
        return MFX_FOURCC_RGB4;
    case VA_FOURCC_ABGR:
        return MFX_FOURCC_BGR4;
    case VA_FOURCC_RGBP:
        return MFX_FOURCC_RGBP;
    case VA_FOURCC_P208:
        return MFX_FOURCC_P8;
    case VA_FOURCC_P010:
        return MFX_FOURCC_P010;
    case VA_FOURCC_AYUV:
        return MFX_FOURCC_AYUV;
#if (MFX_VERSION >= 1027)
    case VA_FOURCC_Y210:
        return MFX_FOURCC_Y210;
    case VA_FOURCC_Y410:
        return MFX_FOURCC_Y410;
#endif
#if (MFX_VERSION >= 1031)
    case VA_FOURCC_P016:
        return MFX_FOURCC_P016;
    case VA_FOURCC_Y216:
        return MFX_FOURCC_Y216;
    case VA_FOURCC_Y416:
        return MFX_FOURCC_Y416;
#endif
    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

mfxU32 VaCopyWrapper::ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_UYVY:
        return VA_FOURCC_UYVY;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
#if (MFX_VERSION >= 1028)
    case MFX_FOURCC_RGB565:
        return VA_FOURCC_RGB565;
#endif
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_BGR4:
        return VA_FOURCC_ABGR;
    case MFX_FOURCC_RGBP:
        return VA_FOURCC_RGBP;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;
    case MFX_FOURCC_P010:
        return VA_FOURCC_P010;
    case MFX_FOURCC_A2RGB10:
        return VA_FOURCC_ARGB;  // rt format will be VA_RT_FORMAT_RGB32_10BPP
    case MFX_FOURCC_AYUV:
        return VA_FOURCC_AYUV;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
        return VA_FOURCC_Y210;
    case MFX_FOURCC_Y410:
        return VA_FOURCC_Y410;
#endif
#if (MFX_VERSION >= 1031)
    case MFX_FOURCC_P016:
        return VA_FOURCC_P016;
    case MFX_FOURCC_Y216:
        return VA_FOURCC_Y216;
    case MFX_FOURCC_Y416:
        return VA_FOURCC_Y416;
#endif
    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

mfxStatus VaCopyWrapper::AcquireUserVaSurface(mfxFrameSurface1 *pSurface)
{
    mfxStatus mfx_sts = MFX_ERR_NONE;
    VAStatus va_sts = VA_STATUS_SUCCESS;

    UMC::AutomaticUMCMutex guard(m_guard);

    std::map<mfxU8 *, VASurfaceID>::iterator it;
    it = m_tableSysRelations.find(GetFramePointer(pSurface->Info.FourCC, pSurface->Data));

    if (it == m_tableSysRelations.end())
    {
        mfxSurfaceInfo surfaceInfo;
        memset(&surfaceInfo, 0, sizeof(surfaceInfo));

        surfaceInfo.width = pSurface->Info.Width;
        surfaceInfo.height = pSurface->Info.Height;
        surfaceInfo.pitch = pSurface->Data.Pitch;
        surfaceInfo.memtype = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;
        surfaceInfo.pBufBase = GetFramePointer(pSurface->Info.FourCC, pSurface->Data);

        switch (pSurface->Info.FourCC)
        {
        case MFX_FOURCC_NV12:
            surfaceInfo.fourCC = VA_FOURCC_NV12;
            surfaceInfo.format = VA_RT_FORMAT_YUV420;
            break;

        case MFX_FOURCC_AYUV:
            surfaceInfo.fourCC = VA_FOURCC_AYUV;
            surfaceInfo.format = VA_FOURCC_AYUV;
            break;

        case MFX_FOURCC_P010:
            surfaceInfo.fourCC = VA_FOURCC_P010;
            surfaceInfo.format = VA_RT_FORMAT_YUV420_10;
            break;

        case MFX_FOURCC_YUY2:
            surfaceInfo.fourCC = VA_FOURCC_YUY2;
            surfaceInfo.format = VA_RT_FORMAT_YUV422;
            break;

        case MFX_FOURCC_UYVY:
            surfaceInfo.fourCC = VA_FOURCC_UYVY;
            surfaceInfo.format = VA_RT_FORMAT_YUV422;
            break;

        default:
            return MFX_ERR_UNSUPPORTED;
        }
        
        va_sts = CreateUserVaSurface(&m_sysSurfaceID, surfaceInfo);
        mfx_sts = va_to_mfx_status(va_sts);
        MFX_CHECK_STS(mfx_sts);

        m_tableSysRelations.insert(std::pair<mfxU8 *, VASurfaceID>(surfaceInfo.pBufBase, m_sysSurfaceID));
    }
    else
    {
        m_sysSurfaceID = it->second; 
    }

    return mfx_sts;
}

mfxStatus VaCopyWrapper::VACopy(VASurfaceID srcSurface, VASurfaceID dstSurface, mfxU16 vaCopyMode)
{
    VAStatus va_sts = VA_STATUS_SUCCESS;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    VACopyObject src_obj, dst_obj;
    VACopyOption option;

    // setup the option parameter for vaCopy
    memset(&src_obj, 0, sizeof(src_obj));
    memset(&dst_obj, 0, sizeof(dst_obj));
    memset(&option, 0, sizeof(option));

    src_obj.obj_type = VACopyObjectSurface;
    src_obj.object.surface_id = srcSurface;

    dst_obj.obj_type = VACopyObjectSurface;
    dst_obj.object.surface_id = dstSurface;

    option.bits.va_copy_sync = VA_EXEC_SYNC;
    option.bits.va_copy_mode = vaCopyMode;

    va_sts = vaCopy(m_dpy, &dst_obj, &src_obj, option);
    mfx_sts = va_to_mfx_status(va_sts);
    MFX_CHECK_STS(mfx_sts);

    va_sts = vaSyncSurface(m_dpy, dstSurface);
    mfx_sts = va_to_mfx_status(va_sts);
    MFX_CHECK_STS(mfx_sts);

    return mfx_sts;
}
