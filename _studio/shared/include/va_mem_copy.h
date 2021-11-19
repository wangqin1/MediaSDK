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

#ifndef __VA_MEM_COPY_H__
#define __VA_MEM_COPY_H__

#include "mfxdefs.h"
#include "mfxstructures.h"

#include "umc_mutex.h"

#include <algorithm>
#include <map>

typedef struct
{
    mfxU32    width;
    mfxU32    height;
    mfxU32    fourCC;
    mfxU32    format;
    mfxU32    memtype;
    mfxU8    *pBufBase;
    uintptr_t ptrb;
} mfxSurfaceInfo;

enum
{
    MFX_COPY_METHOD_VEBOX  = 0,
    MFX_COPY_METHOD_RENDER = 1,
    MFX_COPY_METHOD_BLT    = 2
};

class VaCopyWrapper
{
public:

    // constructor
    VaCopyWrapper();

    // destructor
    virtual ~VaCopyWrapper(void);

    // initialize available functionality
    mfxStatus Initialize(VADisplay dpy);

    // release object
    mfxStatus Release(void);

    static bool CanUseVaCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, mfxSize roi);

    mfxStatus CopySysToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    mfxStatus CopyVideoToSys(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);

    void SetVaCopyMode(mfxU16 mode);

protected:

    VADisplay m_dpy;

    VASurfaceID m_sysSurfaceID;
    mfxSurfaceInfo m_sysSurfaceInfo;

    std::map<VASurfaceID, mfxSurfaceInfo> m_sysSurfaces;

    mfxU32 m_vaCopyMode;
 
    UMC::Mutex m_guard;

    static bool IsVaCopyFormatSupported(mfxU32 fourCC);
    static mfxU32 ConvertVAFormatToMfxFourcc(mfxU32 fourCC);
    static mfxU32 ConvertMfxFourccToVAFormat(mfxU32 fourCC);

    mfxStatus AcquireUserVaSurface(mfxFrameSurface1 *pSurface);
    VAStatus CreateUserVaSurface(VASurfaceID *pSurface, mfxSurfaceInfo &surfaceInfo);

    VAStatus ReleaseUserVaSurface(VASurfaceID *pSurface);

    mfxStatus VACopy(VASurfaceID srcSurface, VASurfaceID dstSurface);
};

#endif // __VA_MEM_COPY_H__
