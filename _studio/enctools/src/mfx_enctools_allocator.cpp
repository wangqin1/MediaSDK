// Copyright (c) 2008-2021 Intel Corporation
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

#include <assert.h>
#include <algorithm>
#include <functional>
#include "mfx_enctools_allocator.h"

MFXFrameAllocator::MFXFrameAllocator()
{
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
    GetHDL = GetHDL_;
}

MFXFrameAllocator::~MFXFrameAllocator()
{
}

mfxStatus MFXFrameAllocator::Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.AllocFrames(request, response);
}

mfxStatus MFXFrameAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.LockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.UnlockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Free_(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.FreeFrames(response);
}

mfxStatus MFXFrameAllocator::GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.GetFrameHDL(mid, handle);
}

BaseFrameAllocator::BaseFrameAllocator()
{
}

BaseFrameAllocator::~BaseFrameAllocator()
{
}

mfxStatus BaseFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    if (0 == request)
        return MFX_ERR_NULL_PTR;

    // check that Media SDK component is specified in request
    if ((request->Type & MEMTYPE_FROM_MASK) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus BaseFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{

    if (0 == request || 0 == response || 0 == request->NumFrameSuggested)
        return MFX_ERR_MEMORY_ALLOC;

    if (MFX_ERR_NONE != CheckRequestType(request))
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;

    if ( (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) && (request->Type & MFX_MEMTYPE_FROM_DECODE) )
    {
        // external decoder allocations
        std::list<UniqueResponse>::iterator it =
            std::find_if( m_ExtResponses.begin()
                        , m_ExtResponses.end()
                        , UniqueResponse (*response, request->Info.CropW, request->Info.CropH, 0));

        if (it != m_ExtResponses.end())
        {
            // check if enough frames were allocated
            if (request->NumFrameMin > it->NumFrameActual)
                return MFX_ERR_MEMORY_ALLOC;

            it->m_refCount++;
            // return existing response
            *response = (mfxFrameAllocResponse&)*it;
        }
        else
        {
            sts = AllocImpl(request, response);
            if (sts == MFX_ERR_NONE)
            {
                m_ExtResponses.push_back(UniqueResponse(*response, request->Info.CropW, request->Info.CropH, request->Type & MEMTYPE_FROM_MASK));
            }
        }
    }
    else
    {
        // internal allocations

        // reserve space before allocation to avoid memory leak
        std::list<mfxFrameAllocResponse> tmp(1, mfxFrameAllocResponse(), m_responses.get_allocator());

        sts = AllocImpl(request, response);
        if (sts == MFX_ERR_NONE)
        {
            m_responses.splice(m_responses.end(), tmp);
            m_responses.back() = *response;
        }
    }

    return sts;
}

mfxStatus BaseFrameAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    if (response == 0)
        return MFX_ERR_INVALID_HANDLE;

    if (response->mids == nullptr || response->NumFrameActual == 0)
        return MFX_ERR_NONE;

    mfxStatus sts = MFX_ERR_NONE;

    // check whether response is an external decoder response
    std::list<UniqueResponse>::iterator i =
        std::find_if( m_ExtResponses.begin(), m_ExtResponses.end(), std::bind1st(IsSame(), *response));

    if (i != m_ExtResponses.end())
    {
        if ((--i->m_refCount) == 0)
        {
            sts = ReleaseResponse(response);
            m_ExtResponses.erase(i);
        }
        return sts;
    }

    // if not found so far, then search in internal responses
    std::list<mfxFrameAllocResponse>::iterator i2 =
        std::find_if(m_responses.begin(), m_responses.end(), std::bind1st(IsSame(), *response));

    if (i2 != m_responses.end())
    {
        sts = ReleaseResponse(response);
        m_responses.erase(i2);
        return sts;
    }

    // not found anywhere, report an error
    return MFX_ERR_INVALID_HANDLE;
}

mfxStatus BaseFrameAllocator::Close()
{
    std::list<UniqueResponse> ::iterator i;
    for (i = m_ExtResponses.begin(); i!= m_ExtResponses.end(); i++)
    {
        ReleaseResponse(&*i);
    }
    m_ExtResponses.clear();

    std::list<mfxFrameAllocResponse> ::iterator i2;
    for (i2 = m_responses.begin(); i2!= m_responses.end(); i2++)
    {
        ReleaseResponse(&*i2);
    }

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
#if defined(MFX_D3D11_ENABLED)

#define et_alloc_printf(...)
//#define et_alloc_printf printf

#include <objbase.h>
#include <initguid.h>
#include <iterator>

//for generating sequence of mfx handles
template <typename T>
struct sequence {
    T x;
    sequence(T seed) : x(seed) { }
};

template <>
struct sequence<mfxHDL> {
    mfxHDL x;
    sequence(mfxHDL seed) : x(seed) { }

    mfxHDL operator ()()
    {
        mfxHDL y = x;
        x = (mfxHDL)(1 + (size_t)(x));
        return y;
    }
};


D3D11FrameAllocator::D3D11FrameAllocator()
{
    m_pDeviceContext = NULL;
}

D3D11FrameAllocator::~D3D11FrameAllocator()
{
    Close();
}

D3D11FrameAllocator::TextureSubResource  D3D11FrameAllocator::GetResourceFromMid(mfxMemId mid)
{
    size_t index = (size_t)MFXReadWriteMid(mid).raw() - 1;

    if (m_memIdMap.size() <= index)
        return TextureSubResource();

    //reverse iterator dereferencing
    TextureResource * p = &(*m_memIdMap[index]);
    if (!p->bAlloc)
        return TextureSubResource();

    return TextureSubResource(p, mid);
}

mfxStatus D3D11FrameAllocator::Init(mfxAllocatorParams *pParams)
{
    D3D11AllocatorParams *pd3d11Params = 0;
    pd3d11Params = dynamic_cast<D3D11AllocatorParams *>(pParams);

    MFX_CHECK(pd3d11Params && pd3d11Params->pDevice, MFX_ERR_NOT_INITIALIZED);

    m_initParams = *pd3d11Params;
    m_pDeviceContext.Release();
    pd3d11Params->pDevice->GetImmediateContext(&m_pDeviceContext);

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::Close()
{
    mfxStatus sts = BaseFrameAllocator::Close();
    for (referenceType i = m_resourcesByRequest.begin(); i != m_resourcesByRequest.end(); i++)
    {
        i->Release();
    }
    m_resourcesByRequest.clear();
    m_memIdMap.clear();
    m_pDeviceContext.Release();
    return sts;
}

mfxStatus D3D11FrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    HRESULT hRes = S_OK;

    D3D11_TEXTURE2D_DESC desc = { 0 };
    D3D11_MAPPED_SUBRESOURCE lockedRect = { 0 };


    //check that texture exists
    TextureSubResource sr = GetResourceFromMid(mid);
    MFX_CHECK(sr.GetTexture(), MFX_ERR_LOCK_MEMORY);

    D3D11_MAP mapType = D3D11_MAP_READ;
    UINT mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;
    {
        if (NULL == sr.GetStaging())
        {
            hRes = m_pDeviceContext->Map(sr.GetTexture(), sr.GetSubResource(), D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &lockedRect);
            desc.Format = DXGI_FORMAT_P8;
        }
        else
        {
            sr.GetTexture()->GetDesc(&desc);

            if (DXGI_FORMAT_NV12 != desc.Format &&
                DXGI_FORMAT_420_OPAQUE != desc.Format &&
                DXGI_FORMAT_P010 != desc.Format &&
                DXGI_FORMAT_YUY2 != desc.Format &&
                DXGI_FORMAT_P8 != desc.Format &&
                DXGI_FORMAT_B8G8R8A8_UNORM != desc.Format &&
                DXGI_FORMAT_R8G8B8A8_UNORM != desc.Format &&
                DXGI_FORMAT_R10G10B10A2_UNORM != desc.Format &&
                DXGI_FORMAT_AYUV != desc.Format  &&
                DXGI_FORMAT_R16_UINT != desc.Format &&
                DXGI_FORMAT_R16_UNORM != desc.Format &&
                DXGI_FORMAT_R16_TYPELESS != desc.Format &&
                DXGI_FORMAT_Y210 != desc.Format &&
                DXGI_FORMAT_Y410 != desc.Format &&
                DXGI_FORMAT_P016 != desc.Format &&
                DXGI_FORMAT_Y216 != desc.Format &&
                DXGI_FORMAT_Y416 != desc.Format)
            {
                MFX_RETURN(MFX_ERR_LOCK_MEMORY);
            }

            //coping data only in case user wants to read from stored surface
            {

                if (MFXReadWriteMid(mid, MFXReadWriteMid::reuse).isRead())
                {
                    m_pDeviceContext->CopySubresourceRegion(sr.GetStaging(), 0, 0, 0, 0, sr.GetTexture(), sr.GetSubResource(), NULL);
                }

                do
                {
                    hRes = m_pDeviceContext->Map(sr.GetStaging(), 0, mapType, mapFlags, &lockedRect);
                    if (S_OK != hRes && DXGI_ERROR_WAS_STILL_DRAWING != hRes)
                    {
                        et_alloc_printf("ERROR: m_pDeviceContext->Map = 0x%X\n", hRes);
                    }
                } while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);
            }

        }
    }

    MFX_CHECK(!FAILED(hRes), MFX_ERR_LOCK_MEMORY);

    ptr->PitchHigh = (mfxU16)(lockedRect.RowPitch / (1 << 16));
    ptr->PitchLow = (mfxU16)(lockedRect.RowPitch % (1 << 16));

    switch (desc.Format)
    {
    case DXGI_FORMAT_NV12:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = (mfxU8 *)lockedRect.pData + desc.Height * lockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = (mfxU8 *)lockedRect.pData + desc.Height * lockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;

    case DXGI_FORMAT_420_OPAQUE: // can be unsupported by standard ms guid
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->V = ptr->Y + desc.Height * lockedRect.RowPitch;
        ptr->U = ptr->V + (desc.Height * lockedRect.RowPitch) / 4;

        break;

    case DXGI_FORMAT_YUY2:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;

        break;

    case DXGI_FORMAT_P8:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = 0;
        ptr->V = 0;

        break;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
        ptr->B = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;

    case DXGI_FORMAT_R8G8B8A8_UNORM:
        ptr->R = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->R + 1;
        ptr->B = ptr->R + 2;
        ptr->A = ptr->R + 3;

        break;

    case DXGI_FORMAT_R10G10B10A2_UNORM:
        ptr->B = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;

    case DXGI_FORMAT_AYUV:
        ptr->B = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_TYPELESS:
        ptr->Y16 = (mfxU16 *)lockedRect.pData;
        ptr->U16 = 0;
        ptr->V16 = 0;

        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        ptr->Y16 = (mfxU16*)lockedRect.pData;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->U16 + 3;
        break;

    case DXGI_FORMAT_Y410:
        ptr->Y410 = (mfxY410 *)lockedRect.pData;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        break;

    case DXGI_FORMAT_Y416:
        ptr->U16 = (mfxU16*)lockedRect.pData;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A = (mfxU8 *)(ptr->V16 + 1);
        break;

    default:

        MFX_RETURN(MFX_ERR_LOCK_MEMORY);
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    //check that texture exists
    TextureSubResource sr = GetResourceFromMid(mid);
    MFX_CHECK(sr.GetTexture(), MFX_ERR_LOCK_MEMORY);

    if (NULL == sr.GetStaging())
    {
        m_pDeviceContext->Unmap(sr.GetTexture(), sr.GetSubResource());
    }
    else
    {
        m_pDeviceContext->Unmap(sr.GetStaging(), 0);
        //only if user wrote something to texture
        if (MFXReadWriteMid(mid, MFXReadWriteMid::reuse).isWrite())
        {
            m_pDeviceContext->CopySubresourceRegion(sr.GetTexture(), sr.GetSubResource(), 0, 0, 0, sr.GetStaging(), 0, NULL);
        }
    }

    if (ptr)
    {
        ptr->PitchHigh = 0;
        ptr->PitchLow = 0;
        ptr->U = ptr->V = ptr->Y = 0;
        ptr->A = ptr->R = ptr->G = ptr->B = 0;
    }

    return MFX_ERR_NONE;
}


mfxStatus D3D11FrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    MFX_CHECK(handle, MFX_ERR_INVALID_HANDLE);

    TextureSubResource sr = GetResourceFromMid(mid);

    MFX_CHECK(sr.GetTexture(), MFX_ERR_INVALID_HANDLE);

    mfxHDLPair *pPair = (mfxHDLPair*)handle;

    pPair->first = sr.GetTexture();
    pPair->second = (mfxHDL)(UINT_PTR)sr.GetSubResource();

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    MFX_CHECK(((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0), MFX_ERR_UNSUPPORTED);
    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    MFX_CHECK(response, MFX_ERR_NULL_PTR);

    if (response->mids && 0 != response->NumFrameActual)
    {
        //check whether texture exsist
        TextureSubResource sr = GetResourceFromMid(response->mids[0]);

        MFX_CHECK(sr.GetTexture(), MFX_ERR_NULL_PTR);

        sr.Release();

        //if texture is last it is possible to remove also all handles from map to reduce fragmentation
        //search for allocated chunk
        if (m_resourcesByRequest.end() == std::find_if(m_resourcesByRequest.begin(), m_resourcesByRequest.end(), TextureResource::isAllocated))
        {
            m_resourcesByRequest.clear();
            m_memIdMap.clear();
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    HRESULT hRes;

    DXGI_FORMAT colorFormat = ConverColortFormat(request->Info.FourCC);

    if (DXGI_FORMAT_UNKNOWN == colorFormat)
    {
        et_alloc_printf("Unknown format: %c%c%c%c\n", ((char *)&request->Info.FourCC)[0], ((char *)&request->Info.FourCC)[1], ((char *)&request->Info.FourCC)[2], ((char *)&request->Info.FourCC)[3]);
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    TextureResource newTexture;

    if (request->Info.FourCC == MFX_FOURCC_P8)
    {
        D3D11_BUFFER_DESC desc = { 0 };

        desc.ByteWidth = request->Info.Width * request->Info.Height;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer * buffer = 0;
        hRes = m_initParams.pDevice->CreateBuffer(&desc, 0, &buffer);
        if (FAILED(hRes))
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);

        newTexture.textures.push_back(reinterpret_cast<ID3D11Texture2D *>(buffer));
    }
    else
    {
        D3D11_TEXTURE2D_DESC desc = { 0 };

        desc.Width = request->Info.Width;
        desc.Height = request->Info.Height;

        desc.MipLevels = 1;
        //number of subresources is 1 in case of not single texture
        desc.ArraySize = m_initParams.bUseSingleTexture ? request->NumFrameSuggested : 1;
        desc.Format = ConverColortFormat(request->Info.FourCC);
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.MiscFlags = m_initParams.uncompressedResourceMiscFlags;

        if ((request->Type&MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET) && (request->Type & MFX_MEMTYPE_INTERNAL_FRAME))
        {
            desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER;
        }
        else
            desc.BindFlags = D3D11_BIND_DECODER;

        if ((DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format) ||
            (DXGI_FORMAT_R8G8B8A8_UNORM == desc.Format) ||
            (DXGI_FORMAT_R10G10B10A2_UNORM == desc.Format))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
            (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        if (request->Type&MFX_MEMTYPE_SHARED_RESOURCE)
        {
            desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        }

        if (DXGI_FORMAT_P8 == desc.Format)
        {
            desc.BindFlags = 0;
        }

        if (DXGI_FORMAT_R16_TYPELESS == colorFormat)
        {
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            RESOURCE_EXTENSION extnDesc;
            ZeroMemory(&extnDesc, sizeof(RESOURCE_EXTENSION));
            memcpy(&extnDesc.Key[0], RESOURCE_EXTENSION_KEY, sizeof(extnDesc.Key));
            extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
            extnDesc.Type = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
            // TODO: Pass real Bayer type
            extnDesc.Data[0] = RESOURCE_EXTENSION_CAMERA_PIPE::INPUT_FORMAT_IRW0;
            hRes = SetResourceExtension(&extnDesc);
            if (FAILED(hRes))
            {
                et_alloc_printf("SetResourceExtension failed, hr = 0x%X\n", hRes);
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
            }
        }

        ID3D11Texture2D* pTexture2D;

        if (m_initParams.bIsRawSurfLinear)
        {
            if (desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_P010)
            {
                desc.BindFlags = D3D11_BIND_DECODER;
            }
            else if (desc.Format == DXGI_FORMAT_Y410 ||
                desc.Format == DXGI_FORMAT_Y210 ||
                desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM ||
                desc.Format == DXGI_FORMAT_YUY2 ||
                desc.Format == DXGI_FORMAT_AYUV)
            {
                desc.BindFlags = 0;
            }
        }

        for (size_t i = 0; i < request->NumFrameSuggested / desc.ArraySize; i++)
        {
            hRes = m_initParams.pDevice->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
            {
                et_alloc_printf("CreateTexture2D(%zd) failed, hr = 0x%X\n", i, hRes);
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
            }
            newTexture.textures.push_back(pTexture2D);
        }

        desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;

        for (size_t i = 0; i < request->NumFrameSuggested; i++)
        {
            hRes = m_initParams.pDevice->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
            {
                et_alloc_printf("Create staging texture(%zd) failed hr = 0x%X\n", i, hRes);
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
            }
            newTexture.stagingTexture.push_back(pTexture2D);
        }
    }

    // mapping to self created handles array, starting from zero or from last assigned handle + 1
    sequence<mfxHDL> seq_initializer(m_resourcesByRequest.empty() ? 0 : m_resourcesByRequest.back().outerMids.back());

    //incrementing starting index
    //1. 0(NULL) is invalid memid
    //2. back is last index not new one
    seq_initializer();

    std::generate_n(std::back_inserter(newTexture.outerMids), request->NumFrameSuggested, seq_initializer);

    //saving texture resources
    m_resourcesByRequest.push_back(newTexture);

    //providing pointer to mids externally
    response->mids = &m_resourcesByRequest.back().outerMids.front();
    response->NumFrameActual = request->NumFrameSuggested;

    //iterator prior end()
    std::list <TextureResource>::iterator it_last = m_resourcesByRequest.end();
    //fill map
    std::fill_n(std::back_inserter(m_memIdMap), request->NumFrameSuggested, --it_last);

    return MFX_ERR_NONE;
}

DXGI_FORMAT D3D11FrameAllocator::ConverColortFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return DXGI_FORMAT_NV12;

    case MFX_FOURCC_P010:
        return DXGI_FORMAT_P010;

    case MFX_FOURCC_A2RGB10:
        return DXGI_FORMAT_R10G10B10A2_UNORM;

    case MFX_FOURCC_YUY2:
        return DXGI_FORMAT_YUY2;

    case MFX_FOURCC_RGB4:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case MFX_FOURCC_BGR4:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case MFX_FOURCC_AYUV:
    case DXGI_FORMAT_AYUV:
        return DXGI_FORMAT_AYUV;

    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_TEXTURE:
        return DXGI_FORMAT_P8;

    case MFX_FOURCC_R16:
        return DXGI_FORMAT_R16_TYPELESS;

    case MFX_FOURCC_Y210:
        return DXGI_FORMAT_Y210;

    case MFX_FOURCC_Y410:
        return DXGI_FORMAT_Y410;

    case MFX_FOURCC_P016:
        return DXGI_FORMAT_P016;

    case MFX_FOURCC_Y216:
        return DXGI_FORMAT_Y216;

    case MFX_FOURCC_Y416:
        return DXGI_FORMAT_Y416;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

#endif // MFX_D3D11_ENABLED


#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define D3DFMT_P010 (D3DFORMAT)MAKEFOURCC('P','0','1','0')
#define D3DFMT_Y210 (D3DFORMAT)MFX_FOURCC_Y210
#define D3DFMT_P210 (D3DFORMAT)MFX_FOURCC_P210
#define D3DFMT_Y410 (D3DFORMAT)MFX_FOURCC_Y410
#define D3DFMT_AYUV (D3DFORMAT)MFX_FOURCC_AYUV
#define D3DFMT_P016 (D3DFORMAT)MFX_FOURCC_P016
#define D3DFMT_Y216 (D3DFORMAT)MFX_FOURCC_Y216
#define D3DFMT_Y416 (D3DFORMAT)MFX_FOURCC_Y416

D3DFORMAT ConvertMfxFourccToD3dFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return D3DFMT_NV12;
    case MFX_FOURCC_YV12:
        return D3DFMT_YV12;
    case MFX_FOURCC_YUY2:
        return D3DFMT_YUY2;
    case MFX_FOURCC_RGB3:
        return D3DFMT_R8G8B8;
    case MFX_FOURCC_RGB4:
        return D3DFMT_A8R8G8B8;
    case MFX_FOURCC_BGR4:
        return D3DFMT_A8B8G8R8;
    case MFX_FOURCC_P8:
        return D3DFMT_P8;
    case MFX_FOURCC_P010:
        return D3DFMT_P010;
    case MFX_FOURCC_A2RGB10:
        return D3DFMT_A2R10G10B10;
    case MFX_FOURCC_R16:
        return D3DFMT_R16F;
    case MFX_FOURCC_ARGB16:
        return D3DFMT_A16B16G16R16;
    case MFX_FOURCC_P210:
        return D3DFMT_P210;
    case MFX_FOURCC_AYUV:
        return D3DFMT_AYUV;

    case MFX_FOURCC_Y210:
        return D3DFMT_Y210;
    case MFX_FOURCC_Y410:
        return D3DFMT_Y410;

    case MFX_FOURCC_P016:
        return D3DFMT_P016;
    case MFX_FOURCC_Y216:
        return D3DFMT_Y216;
    case MFX_FOURCC_Y416:
        return D3DFMT_Y416;

    default:
        return D3DFMT_UNKNOWN;
    }
}

class DeviceHandle
{
public:
    DeviceHandle(IDirect3DDeviceManager9* manager)
        : m_manager(manager)
        , m_handle(0)
    {
        if (manager)
        {
            HRESULT hr = manager->OpenDeviceHandle(&m_handle);
            if (FAILED(hr))
                m_manager = 0;
        }
    }

    ~DeviceHandle()
    {
        if (m_manager && m_handle)
            m_manager->CloseDeviceHandle(m_handle);
    }

    HANDLE Detach()
    {
        HANDLE tmp = m_handle;
        m_manager = 0;
        m_handle = 0;
        return tmp;
    }

    operator HANDLE()
    {
        return m_handle;
    }

    bool operator !() const
    {
        return m_handle == 0;
    }

protected:
    CComPtr<IDirect3DDeviceManager9> m_manager;
    HANDLE m_handle;
};

D3DFrameAllocator::D3DFrameAllocator()
    : m_decoderService(0), m_processorService(0), m_hDecoder(0), m_hProcessor(0), m_manager(0), m_surfaceUsage(0)
{
}

D3DFrameAllocator::~D3DFrameAllocator()
{
    Close();
}

mfxStatus D3DFrameAllocator::Init(mfxAllocatorParams *pParams)
{
    D3DAllocatorParams *pd3dParams = 0;
    pd3dParams = dynamic_cast<D3DAllocatorParams *>(pParams);
    if (!pd3dParams)
        return MFX_ERR_NOT_INITIALIZED;

    m_manager = pd3dParams->pManager;
    m_surfaceUsage = pd3dParams->surfaceUsage;

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::Close()
{
    if (m_manager && m_hDecoder)
    {
        m_manager->CloseDeviceHandle(m_hDecoder);
        m_manager = 0;
        m_hDecoder = 0;
    }

    if (m_manager && m_hProcessor)
    {
        m_manager->CloseDeviceHandle(m_hProcessor);
        m_manager = 0;
        m_hProcessor = 0;
    }

    return BaseFrameAllocator::Close();
}

mfxStatus D3DFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)mid;
    if (pSurface == 0)
        return MFX_ERR_INVALID_HANDLE;

    if (ptr == 0)
        return MFX_ERR_LOCK_MEMORY;

    D3DSURFACE_DESC desc;
    HRESULT hr = pSurface->GetDesc(&desc);
    if (FAILED(hr))
        return MFX_ERR_LOCK_MEMORY;

    if (desc.Format != D3DFMT_NV12 &&
        desc.Format != D3DFMT_YV12 &&
        desc.Format != D3DFMT_YUY2 &&
        desc.Format != D3DFMT_R8G8B8 &&
        desc.Format != D3DFMT_A8R8G8B8 &&
        desc.Format != D3DFMT_A8B8G8R8 &&
        desc.Format != D3DFMT_P8 &&
        desc.Format != D3DFMT_P010 &&
        desc.Format != D3DFMT_A2R10G10B10 &&
        desc.Format != D3DFMT_R16F &&
        desc.Format != D3DFMT_A16B16G16R16 &&
        desc.Format != D3DFMT_AYUV &&
        desc.Format != D3DFMT_Y210 &&
        desc.Format != D3DFMT_Y410 &&
        desc.Format != D3DFMT_P016 &&
        desc.Format != D3DFMT_Y216 &&
        desc.Format != D3DFMT_Y416)
        return MFX_ERR_LOCK_MEMORY;

    D3DLOCKED_RECT locked;

    hr = pSurface->LockRect(&locked, 0, D3DLOCK_NOSYSLOCK);
    if (FAILED(hr))
        return MFX_ERR_LOCK_MEMORY;

    ptr->PitchHigh = (mfxU16)(locked.Pitch / (1 << 16));
    ptr->PitchLow = (mfxU16)(locked.Pitch % (1 << 16));
    switch ((DWORD)desc.Format)
    {
    case D3DFMT_NV12:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
        ptr->V = ptr->U + 1;
        break;

    case D3DFMT_P010:
    case D3DFMT_P016:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
        ptr->V = ptr->U + 1;
        break;

    case D3DFMT_A2R10G10B10:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;

    case D3DFMT_A16B16G16R16:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->Y16 = (mfxU16 *)locked.pBits;
        ptr->G = ptr->B + 2;
        ptr->R = ptr->B + 4;
        ptr->A = ptr->B + 6;
        break;

    case D3DFMT_YV12:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->V = ptr->Y + desc.Height * locked.Pitch;
        ptr->U = ptr->V + (desc.Height * locked.Pitch) / 4;
        break;

    case D3DFMT_YUY2:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;

    case D3DFMT_R8G8B8:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        break;

    case D3DFMT_A8R8G8B8:
    case D3DFMT_AYUV:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;

    case D3DFMT_A8B8G8R8:
        ptr->R = (mfxU8 *)locked.pBits;
        ptr->G = ptr->R + 1;
        ptr->B = ptr->R + 2;
        ptr->A = ptr->R + 3;
        break;

    case D3DFMT_P8:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = 0;
        ptr->V = 0;
        break;

    case D3DFMT_R16F:
        ptr->Y16 = (mfxU16 *)locked.pBits;
        ptr->U16 = 0;
        ptr->V16 = 0;
        break;

    case D3DFMT_Y210:
    case D3DFMT_Y216:
        ptr->Y16 = (mfxU16*)locked.pBits;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->U16 + 3;
        break;

    case D3DFMT_Y410:
        ptr->Y410 = (mfxY410 *)locked.pBits;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        break;

    case D3DFMT_Y416:
        ptr->U16 = (mfxU16*)locked.pBits;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A = (mfxU8 *)(ptr->V16 + 1);
        break;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)mid;
    if (pSurface == 0)
        return MFX_ERR_INVALID_HANDLE;

    pSurface->UnlockRect();

    if (NULL != ptr)
    {
        ptr->PitchHigh = 0;
        ptr->PitchLow = 0;
        ptr->Y = 0;
        ptr->U = 0;
        ptr->V = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    if (handle == 0)
        return MFX_ERR_INVALID_HANDLE;

    *handle = mid;
    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3DFrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    if (!response)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = MFX_ERR_NONE;

    if (response->mids)
    {
        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            if (response->mids[i])
            {
                IDirect3DSurface9* handle = 0;
                sts = GetFrameHDL(response->mids[i], (mfxHDL *)&handle);
                if (MFX_ERR_NONE != sts)
                    return sts;
                handle->Release();
            }
        }
    }

    delete[] response->mids;
    response->mids = 0;

    return sts;
}

mfxStatus D3DFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    HRESULT hr;

    D3DFORMAT format = ConvertMfxFourccToD3dFormat(request->Info.FourCC);

    if (format == D3DFMT_UNKNOWN)
        return MFX_ERR_UNSUPPORTED;

    safe_array<mfxMemId> mids(new mfxMemId[request->NumFrameSuggested]);
    if (!mids.get())
        return MFX_ERR_MEMORY_ALLOC;

    DWORD   target;

    if (MFX_MEMTYPE_DXVA2_DECODER_TARGET & request->Type)
    {
        target = DXVA2_VideoDecoderRenderTarget;
    }
    else if (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET & request->Type)
    {
        target = DXVA2_VideoProcessorRenderTarget;
    }
    else
        return MFX_ERR_UNSUPPORTED;

    // VPP may require at input/output surfaces with color format other than NV12 (in case of color conversion)
    // VideoProcessorService must used to create such surfaces
    if (target == DXVA2_VideoProcessorRenderTarget)
    {
        if (!m_hProcessor)
        {
            DeviceHandle device = DeviceHandle(m_manager);

            if (!device)
                return MFX_ERR_MEMORY_ALLOC;

            CComPtr<IDirectXVideoProcessorService> service = 0;

            hr = m_manager->GetVideoService(device, IID_IDirectXVideoProcessorService, (void**)&service);

            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;

            m_processorService = service;
            m_hProcessor = device.Detach();
        }

        hr = m_processorService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested - 1,
            format,
            D3DPOOL_DEFAULT,
            m_surfaceUsage,
            target,
            (IDirect3DSurface9 **)mids.get(),
            NULL);
    }
    else
    {
        if (!m_hDecoder)
        {
            DeviceHandle device = DeviceHandle(m_manager);

            if (!device)
                return MFX_ERR_MEMORY_ALLOC;

            CComPtr<IDirectXVideoDecoderService> service = 0;

            hr = m_manager->GetVideoService(device, IID_IDirectXVideoDecoderService, (void**)&service);

            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;

            m_decoderService = service;
            m_hDecoder = device.Detach();
        }

        hr = m_decoderService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested - 1,
            format,
            D3DPOOL_DEFAULT,
            m_surfaceUsage,
            target,
            (IDirect3DSurface9 **)mids.get(),
            NULL);
    }

    if (FAILED(hr))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    response->mids = mids.release();
    response->NumFrameActual = request->NumFrameSuggested;
    return MFX_ERR_NONE;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
