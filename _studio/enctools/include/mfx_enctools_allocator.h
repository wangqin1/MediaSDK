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

#ifndef __MFX_ENCTOOLS_ALLOCATOR_H__
#define __MFX_ENCTOOLS_ALLOCATOR_H__

#include <list>
#include <string.h> /* for memset on Linux/Android */
#include <functional> /* for std::binary_function on Linux/Android */
#include "mfxvideo.h"
#include "mfx_enctools_defs.h"

struct mfxAllocatorParams
{
    virtual ~mfxAllocatorParams(){};
};

// this class implements methods declared in mfxFrameAllocator structure
// simply redirecting them to virtual methods which should be overridden in derived classes
class MFXFrameAllocator : public mfxFrameAllocator
{
public:
    MFXFrameAllocator();
    virtual ~MFXFrameAllocator();

     // optional method, override if need to pass some parameters to allocator from application
    virtual mfxStatus Init(mfxAllocatorParams *pParams) = 0;
    virtual mfxStatus Close() = 0;
    
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) = 0;
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle) = 0;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response) = 0;

private:
    static mfxStatus MFX_CDECL  Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    static mfxStatus MFX_CDECL  Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    static mfxStatus MFX_CDECL  Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    static mfxStatus MFX_CDECL  GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    static mfxStatus MFX_CDECL  Free_(mfxHDL pthis, mfxFrameAllocResponse *response);
};

// This class implements basic logic of memory allocator
// Manages responses for different components according to allocation request type
// External frames of a particular component-related type are allocated in one call
// Further calls return previously allocated response.
// Ex. Preallocated frame chain with type=FROM_ENCODE | FROM_VPPIN will be returned when
// request type contains either FROM_ENCODE or FROM_VPPIN

// This class does not allocate any actual memory
class BaseFrameAllocator: public MFXFrameAllocator
{
public:
    BaseFrameAllocator();
    virtual ~BaseFrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams) = 0;
    virtual mfxStatus Close();
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response);

protected:
    typedef std::list<mfxFrameAllocResponse>::iterator Iter;
    static const mfxU32 MEMTYPE_FROM_MASK = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_FROM_VPPOUT;

    struct UniqueResponse
        : mfxFrameAllocResponse
    {
        mfxU16 m_cropw;
        mfxU16 m_croph;
        mfxU32 m_refCount;
        mfxU16 m_type;

        UniqueResponse()
        {
            memset(this, 0, sizeof(*this));
        }

        // compare responses by actual frame size, alignment (w and h) is up to application
        UniqueResponse(const mfxFrameAllocResponse & response, mfxU16 cropw, mfxU16 croph, mfxU16 type)
            : mfxFrameAllocResponse(response)
            , m_cropw(cropw)
            , m_croph(croph)
            , m_refCount(1)
            , m_type(type)
        { 
        } 
        //compare by resolution
        bool operator () (const UniqueResponse &response)const
        {
            return m_cropw == response.m_cropw && m_croph == response.m_croph;
        }
    };

    std::list<mfxFrameAllocResponse> m_responses;
    std::list<UniqueResponse> m_ExtResponses;

    struct IsSame
        : public std::binary_function<mfxFrameAllocResponse, mfxFrameAllocResponse, bool> 
    {
        bool operator () (const mfxFrameAllocResponse & l, const mfxFrameAllocResponse &r)const
        {
            return r.mids != 0 && l.mids != 0 &&
                r.mids[0] == l.mids[0] &&
                r.NumFrameActual == l.NumFrameActual;
        }
    };

    // checks if request is supported
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);

    // frees memory attached to response
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response) = 0;
    // allocates memory
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) = 0;

    template <class T>
    class safe_array
    {
    public:
        safe_array(T *ptr = 0):m_ptr(ptr)
        { // construct from object pointer
        };
        ~safe_array()
        {
            reset(0);
        }
        T* get()
        { // return wrapped pointer
            return m_ptr;
        }
        T* release()
        { // return wrapped pointer and give up ownership
            T* ptr = m_ptr;
            m_ptr = 0;
            return ptr;
        }
        void reset(T* ptr) 
        { // destroy designated object and store new pointer
            if (m_ptr)
            {
                delete[] m_ptr;
            }
            m_ptr = ptr;
        }        
    protected:
        T* m_ptr; // the wrapped object pointer
    };
};

#if defined(_WIN32) || defined(_WIN64)
#if defined(MFX_D3D11_ENABLED)

//application can provide either generic mid from surface or this wrapper
//wrapper distinguishes from generic mid by highest 1 bit
//if it set then remained pointer points to extended structure of memid
//64 bits system layout
/*----+-----------------------------------------------------------+
|b63=1|63 bits remained for pointer to extended structure of memid|
|b63=0|63 bits from original mfxMemId                             |
+-----+----------------------------------------------------------*/
//32 bits system layout
/*--+---+--------------------------------------------+
|b31=1|31 bits remained for pointer to extended memid|
|b31=0|31 bits remained for surface pointer          |
+---+---+-------------------------------------------*/
//#pragma warning (disable:4293)
class MFXReadWriteMid
{
    static const uintptr_t bits_offset = std::numeric_limits<uintptr_t>::digits - 1;
    static const uintptr_t clear_mask = ~((uintptr_t)1 << bits_offset);
public:
    enum
    {
        //if flag not set it means that read and write
        not_set = 0,
        reuse = 1,
        read = 2,
        write = 4,
    };
    //here mfxmemid might be as MFXReadWriteMid or mfxMemId memid
    MFXReadWriteMid(mfxMemId mid, mfxU8 flag = not_set)
    {
        //setup mid
        m_mid_to_report = (mfxMemId)((uintptr_t)&m_mid | ((uintptr_t)1 << bits_offset));
        if (0 != ((uintptr_t)mid >> bits_offset))
        {
            //it points to extended structure
            mfxMedIdEx * pMemIdExt = reinterpret_cast<mfxMedIdEx *>((uintptr_t)mid & clear_mask);
            m_mid.pId = pMemIdExt->pId;
            if (reuse == flag)
            {
                m_mid.read_write = pMemIdExt->read_write;
            }
            else
            {
                m_mid.read_write = flag;
            }
        }
        else
        {
            m_mid.pId = mid;
            if (reuse == flag)
                m_mid.read_write = not_set;
            else
                m_mid.read_write = flag;
        }

    }
    bool isRead() const
    {
        return 0 != (m_mid.read_write & read) || !m_mid.read_write;
    }
    bool isWrite() const
    {
        return 0 != (m_mid.read_write & write) || !m_mid.read_write;
    }
    /// returns original memid without read write flags
    mfxMemId raw() const
    {
        return m_mid.pId;
    }
    operator mfxMemId() const
    {
        return m_mid_to_report;
    }

private:
    struct mfxMedIdEx
    {
        mfxMemId pId;
        mfxU8 read_write;
    };

    mfxMedIdEx m_mid;
    mfxMemId   m_mid_to_report;
};

#include <d3d11.h>
#include <vector>
#include <map>
#include <atlbase.h>

struct ID3D11VideoDevice;
struct ID3D11VideoContext;

const char RESOURCE_EXTENSION_KEY[16] = {
    'I','N','T','C',
    'E','X','T','N',
    'R','E','S','O',
    'U','R','C','E' };

#define EXTENSION_INTERFACE_VERSION 0x00040000

struct EXTENSION_BASE
{
    // Input
    char   Key[16];
    UINT   ApplicationVersion;
};

struct RESOURCE_EXTENSION_1_0 : EXTENSION_BASE
{
    // Enumeration of the extension
    UINT  Type;   //RESOURCE_EXTENSION_TYPE

    // Extension data
    union
    {
        UINT    Data[16];
        UINT64  Data64[8];
    };
};

typedef RESOURCE_EXTENSION_1_0 RESOURCE_EXTENSION;

struct STATE_EXTENSION_TYPE_1_0
{
    static const UINT STATE_EXTENSION_RESERVED = 0;
};

struct STATE_EXTENSION_TYPE_4_0 : STATE_EXTENSION_TYPE_1_0
{
    static const UINT STATE_EXTENSION_CONSERVATIVE_PASTERIZATION = 1 + EXTENSION_INTERFACE_VERSION;
};

struct RESOURCE_EXTENSION_TYPE_1_0
{
    static const UINT RESOURCE_EXTENSION_RESERVED = 0;
    static const UINT RESOURCE_EXTENSION_DIRECT_ACCESS = 1;
};

struct RESOURCE_EXTENSION_TYPE_4_0 : RESOURCE_EXTENSION_TYPE_1_0
{
    static const UINT RESOURCE_EXTENSION_CAMERA_PIPE = 1 + EXTENSION_INTERFACE_VERSION;
};

struct RESOURCE_EXTENSION_CAMERA_PIPE
{
    enum {
        INPUT_FORMAT_IRW0 = 0x0,
        INPUT_FORMAT_IRW1 = 0x1,
        INPUT_FORMAT_IRW2 = 0x2,
        INPUT_FORMAT_IRW3 = 0x3
    };
};

struct D3D11AllocatorParams : mfxAllocatorParams
{
    ID3D11Device *pDevice;
    bool bUseSingleTexture;
    bool bIsRawSurfLinear;
    DWORD uncompressedResourceMiscFlags;

    D3D11AllocatorParams()
        : pDevice()
        , bUseSingleTexture()
        , bIsRawSurfLinear()
        , uncompressedResourceMiscFlags()
    {
    }
};

class D3D11FrameAllocator : public BaseFrameAllocator
{
public:

    D3D11FrameAllocator();
    virtual ~D3D11FrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();
    virtual ID3D11Device * GetD3D11Device()
    {
        return m_initParams.pDevice;
    };
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    static  DXGI_FORMAT ConverColortFormat(mfxU32 fourcc);
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    template<typename Type>
    inline HRESULT SetResourceExtension(
        const Type* pExtnDesc)
    {
        D3D11_BUFFER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.ByteWidth = sizeof(Type);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory(&initData, sizeof(initData));
        initData.pSysMem = pExtnDesc;
        initData.SysMemPitch = sizeof(Type);
        initData.SysMemSlicePitch = 0;

        ID3D11Buffer* pBuffer = NULL;
        HRESULT result = m_initParams.pDevice->CreateBuffer(
            &desc,
            &initData,
            &pBuffer);

        if (pBuffer)
            pBuffer->Release();

        return result;
    }

    D3D11AllocatorParams m_initParams;
    CComPtr<ID3D11DeviceContext> m_pDeviceContext;

    struct TextureResource
    {
        std::vector<mfxMemId> outerMids;
        std::vector<ID3D11Texture2D*> textures;
        std::vector<ID3D11Texture2D*> stagingTexture;
        bool             bAlloc;

        TextureResource()
            : bAlloc(true)
        {
        }

        static bool isAllocated(TextureResource & that)
        {
            return that.bAlloc;
        }
        ID3D11Texture2D* GetTexture(mfxMemId id)
        {
            if (outerMids.empty())
                return NULL;

            return textures[((uintptr_t)id - (uintptr_t)outerMids.front()) % textures.size()];
        }
        UINT GetSubResource(mfxMemId id)
        {
            if (outerMids.empty())
                return NULL;

            return (UINT)(((uintptr_t)id - (uintptr_t)outerMids.front()) / textures.size());
        }
        void Release()
        {
            size_t i = 0;
            for (i = 0; i < textures.size(); i++)
            {
                textures[i]->Release();
            }
            textures.clear();

            for (i = 0; i < stagingTexture.size(); i++)
            {
                stagingTexture[i]->Release();
            }
            stagingTexture.clear();

            //marking texture as deallocated
            bAlloc = false;
        }
    };
    class TextureSubResource
    {
        TextureResource * m_pTarget;
        ID3D11Texture2D * m_pTexture;
        ID3D11Texture2D * m_pStaging;
        UINT m_subResource;
    public:
        TextureSubResource(TextureResource * pTarget = NULL, mfxMemId id = 0)
            : m_pTarget(pTarget)
            , m_pTexture()
            , m_subResource()
            , m_pStaging(NULL)
        {
            if (NULL != m_pTarget && !m_pTarget->outerMids.empty())
            {
                ptrdiff_t idx = (uintptr_t)MFXReadWriteMid(id).raw() - (uintptr_t)m_pTarget->outerMids.front();
                m_pTexture = m_pTarget->textures[idx % m_pTarget->textures.size()];
                m_subResource = (UINT)(idx / m_pTarget->textures.size());
                m_pStaging = m_pTarget->stagingTexture.empty() ? NULL : m_pTarget->stagingTexture[idx];
            }
        }
        ID3D11Texture2D* GetStaging()const
        {
            return m_pStaging;
        }
        ID3D11Texture2D* GetTexture()const
        {
            return m_pTexture;
        }
        UINT GetSubResource()const
        {
            return m_subResource;
        }
        void Release()
        {
            if (NULL != m_pTarget)
                m_pTarget->Release();
        }
    };

    TextureSubResource GetResourceFromMid(mfxMemId);

    std::list <TextureResource> m_resourcesByRequest;//each alloc request generates new item in list

    typedef std::list <TextureResource>::iterator referenceType;
    std::vector<referenceType> m_memIdMap;
};

#endif // MFX_D3D11_ENABLED 


#include <atlbase.h>
#include <d3d9.h>
#include <dxva2api.h>

enum eTypeHandle
{
    DXVA2_PROCESSOR = 0x00,
    DXVA2_DECODER = 0x01
};

struct D3DAllocatorParams : mfxAllocatorParams
{
    IDirect3DDeviceManager9 *pManager;
    DWORD surfaceUsage;

    D3DAllocatorParams()
        : pManager()
        , surfaceUsage()
    {
    }

};

class D3DFrameAllocator : public BaseFrameAllocator
{
public:
    D3DFrameAllocator();
    virtual ~D3DFrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();

    virtual IDirect3DDeviceManager9* GetDeviceManager()
    {
        return m_manager;
    };

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    CComPtr<IDirect3DDeviceManager9> m_manager;
    CComPtr<IDirectXVideoDecoderService> m_decoderService;
    CComPtr<IDirectXVideoProcessorService> m_processorService;
    HANDLE m_hDecoder;
    HANDLE m_hProcessor;
    DWORD m_surfaceUsage;
};

#endif // #if defined(_WIN32) || defined(_WIN64)
#endif // __MFX_ENCTOOLS_ALLOCATOR_H__
