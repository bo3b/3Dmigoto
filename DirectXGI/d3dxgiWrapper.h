#pragma once

#include <D3D11.h>

//
// Forward declerations
//
class IDirect3DUnknown;

extern FILE *LogFile;

class IDirect3DUnknown
{
protected:
    IUnknown*   m_pUnk;

public:
    ULONG       m_ulRef;

    IDirect3DUnknown(IUnknown* pUnk)
    {
        m_pUnk = pUnk;
        m_ulRef = 1;
    }

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);

    STDMETHOD_(ULONG,AddRef)(THIS)
    {
		++m_ulRef;
        return m_pUnk->AddRef();
    }

    STDMETHOD_(ULONG,Release)(THIS)
    {
        ULONG ulRef = m_pUnk->Release();
		--m_ulRef;
        if (0 == ulRef)
        {
            delete this;
            return 0;
        }
        return ulRef;
    }
};

class IDXGIAdapter;
class IDXGIAdapter1;
class IDXGISwapChain;
class IDXGIFactory : public IDirect3DUnknown
{
public:
    D3D11Base::IDXGIFactory		*m_pFactory;
	static ThreadSafePointerSet	m_List;

	IDXGIFactory(D3D11Base::IDXGIFactory *pFactory);
    static IDXGIFactory* GetDirectFactory(D3D11Base::IDXGIFactory *pOrig);
    inline D3D11Base::IDXGIFactory *GetFactory() { return m_pFactory; }

    //*** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	//*** IDXGIObject methods
    STDMETHOD(SetPrivateData)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData);
        
    STDMETHOD(SetPrivateDataInterface)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown);
        
    STDMETHOD(GetPrivateData)(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData);
        
    STDMETHOD(GetParent)(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent);

    //*** IDXGIFactory methods 
    STDMETHOD(EnumAdapters)(THIS_ 
            /* [in] */ UINT Adapter,
            /* [annotation][out] */ 
            __out IDXGIAdapter1 **ppAdapter);
        
    STDMETHOD(MakeWindowAssociation)(THIS_ 
            HWND WindowHandle,
            UINT Flags);
        
    STDMETHOD(GetWindowAssociation)(THIS_ 
            /* [annotation][out] */ 
            __out  HWND *pWindowHandle);
        
    STDMETHOD(CreateSwapChain)(THIS_ 
            /* [annotation][in] */ 
            __in  IUnknown *pDevice,
            /* [annotation][in] */ 
            __in  D3D11Base::DXGI_SWAP_CHAIN_DESC *pDesc,
            /* [annotation][out] */ 
            __out  IDXGISwapChain **ppSwapChain);
        
    STDMETHOD(CreateSoftwareAdapter)(THIS_ 
            /* [in] */ HMODULE Module,
            /* [annotation][out] */ 
            __out  D3D11Base::IDXGIAdapter **ppAdapter);

};

class IDXGIAdapter1;
class IDXGIFactory1 : public IDXGIFactory
{
public:
	IDXGIFactory1(D3D11Base::IDXGIFactory1 *pFactory);
    static IDXGIFactory1* GetDirectFactory(D3D11Base::IDXGIFactory1 *pOrig);
	inline D3D11Base::IDXGIFactory1 *GetFactory1() { return (D3D11Base::IDXGIFactory1 *) m_pFactory; }
	 
    STDMETHOD(EnumAdapters1)(THIS_ 
        /* [in] */ UINT Adapter,
        /* [annotation][out] */ 
        __out IDXGIAdapter1 **ppAdapter);
        
    STDMETHOD_(BOOL, IsCurrent)(THIS);
};

class IDXGIOutput;
class IDXGISwapChain1;
class IDXGIFactory2 : public IDXGIFactory1
{
public:
	IDXGIFactory2(D3D11Base::IDXGIFactory2 *pFactory);
    static IDXGIFactory2* GetDirectFactory(D3D11Base::IDXGIFactory2 *pOrig);
	inline D3D11Base::IDXGIFactory2 *GetFactory2() { return (D3D11Base::IDXGIFactory2 *) m_pFactory; }
	 
	STDMETHOD_(BOOL, IsWindowedStereoEnabled)(THIS);
        
    STDMETHOD(CreateSwapChainForHwnd)(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  HWND hWnd,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  D3D11Base::DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain);
        
	STDMETHOD(CreateSwapChainForCoreWindow)(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  IUnknown *pWindow,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain);
        
	STDMETHOD(GetSharedResourceAdapterLuid)(THIS_
            /* [annotation] */ 
            _In_  HANDLE hResource,
            /* [annotation] */ 
            _Out_  LUID *pLuid);
        
	STDMETHOD(RegisterStereoStatusWindow)(THIS_
            /* [annotation][in] */ 
            _In_  HWND WindowHandle,
            /* [annotation][in] */ 
            _In_  UINT wMsg,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie);
        
	STDMETHOD(RegisterStereoStatusEvent)(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie);
        
	STDMETHOD_(void, UnregisterStereoStatus)(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie);
        
	STDMETHOD(RegisterOcclusionStatusWindow)(THIS_
            /* [annotation][in] */ 
            _In_  HWND WindowHandle,
            /* [annotation][in] */ 
            _In_  UINT wMsg,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie);
        
	STDMETHOD(RegisterOcclusionStatusEvent)(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie);
        
	STDMETHOD_(void, UnregisterOcclusionStatus)(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie);
        
	STDMETHOD(CreateSwapChainForComposition)(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Outptr_  IDXGISwapChain1 **ppSwapChain);
};

class IDXGIAdapter : public IDirect3DUnknown
{
public:
    D3D11Base::IDXGIAdapter		*m_pAdapter;
	static ThreadSafePointerSet	m_List;

	IDXGIAdapter(D3D11Base::IDXGIAdapter *pAdapter);
    static IDXGIAdapter* GetDirectAdapter(D3D11Base::IDXGIAdapter *pOrig);
    inline D3D11Base::IDXGIAdapter *GetAdapter() { return m_pAdapter; }

    //*** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	//*** IDXGIObject methods
    STDMETHOD(SetPrivateData)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData);
        
    STDMETHOD(SetPrivateDataInterface)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown);
        
    STDMETHOD(GetPrivateData)(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData);
        
    STDMETHOD(GetParent)(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent);

    //*** IDXGIFactory methods 
	STDMETHOD(EnumOutputs)(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput);
        
    STDMETHOD(GetDesc)(THIS_
            /* [annotation][out] */ 
            __out D3D11Base::DXGI_ADAPTER_DESC *pDesc);
        
    STDMETHOD(CheckInterfaceSupport)(THIS_
            /* [annotation][in] */ 
            __in  REFGUID InterfaceName,
            /* [annotation][out] */ 
            __out  LARGE_INTEGER *pUMDVersion);

};

class IDXGIAdapter1 : public IDXGIAdapter
{
public:
	IDXGIAdapter1(D3D11Base::IDXGIAdapter1 *pAdapter)
		: IDXGIAdapter(pAdapter) {}
    static IDXGIAdapter1* GetDirectAdapter(D3D11Base::IDXGIAdapter1 *pOrig);
    inline D3D11Base::IDXGIAdapter1 *GetAdapter() { return (D3D11Base::IDXGIAdapter1*) m_pAdapter; }

    STDMETHOD(GetDesc1)(THIS_ 
	        /* [annotation][out] */ 
            __out  D3D11Base::DXGI_ADAPTER_DESC1 *pDesc);
};

class IDXGIOutput : public IDirect3DUnknown
{
public:
    D3D11Base::IDXGIOutput		*m_pOutput;
	static ThreadSafePointerSet	m_List;

	IDXGIOutput(D3D11Base::IDXGIOutput *pOutput);
    static IDXGIOutput* GetDirectOutput(D3D11Base::IDXGIOutput *pOrig);
    inline D3D11Base::IDXGIOutput *GetOutput() { return m_pOutput; }

    //*** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	//*** IDXGIObject methods
    STDMETHOD(SetPrivateData)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData);
        
    STDMETHOD(SetPrivateDataInterface)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown);
        
    STDMETHOD(GetPrivateData)(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData);
        
    STDMETHOD(GetParent)(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent);

    //*** IDXGIOutput methods 
	STDMETHOD(GetDesc)(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_OUTPUT_DESC *pDesc);
        
    STDMETHOD(GetDisplayModeList)(THIS_ 
        /* [in] */ D3D11Base::DXGI_FORMAT EnumFormat,
        /* [in] */ UINT Flags,
        /* [annotation][out][in] */ 
        __inout  UINT *pNumModes,
        /* [annotation][out] */ 
        __out_ecount_part_opt(*pNumModes,*pNumModes)  D3D11Base::DXGI_MODE_DESC *pDesc);
        
    STDMETHOD(FindClosestMatchingMode)(THIS_ 
        /* [annotation][in] */ 
        __in  const D3D11Base::DXGI_MODE_DESC *pModeToMatch,
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_MODE_DESC *pClosestMatch,
        /* [annotation][in] */ 
        __in_opt  IUnknown *pConcernedDevice);
        
    STDMETHOD(WaitForVBlank)(THIS_ );
        
    STDMETHOD(TakeOwnership)(THIS_  
        /* [annotation][in] */ 
        __in  IUnknown *pDevice,
        BOOL Exclusive);
        
    virtual void STDMETHODCALLTYPE ReleaseOwnership(void);
        
    STDMETHOD(GetGammaControlCapabilities)(THIS_  
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps);
        
    STDMETHOD(SetGammaControl)(THIS_ 
        /* [annotation][in] */ 
        __in  const D3D11Base::DXGI_GAMMA_CONTROL *pArray);
        
    STDMETHOD(GetGammaControl)(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_GAMMA_CONTROL *pArray);
        
    STDMETHOD(SetDisplaySurface)(THIS_ 
        /* [annotation][in] */ 
        __in  D3D11Base::IDXGISurface *pScanoutSurface);
        
    STDMETHOD(GetDisplaySurfaceData)(THIS_ 
        /* [annotation][in] */ 
        __in  D3D11Base::IDXGISurface *pDestination);
        
    STDMETHOD(GetFrameStatistics)(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_FRAME_STATISTICS *pStats);
};

// MIDL_INTERFACE("310d36a0-d2e7-4c0a-aa04-6a9d23b8886a")
class IDXGISwapChain : public IDirect3DUnknown
{
public:
    D3D11Base::IDXGISwapChain *m_pSwapChain;
	static ThreadSafePointerSet	m_List;
	IUnknown *m_WrappedDevice, *m_RealDevice;

	IDXGISwapChain(D3D11Base::IDXGISwapChain *pOutput);
    static IDXGISwapChain* GetDirectSwapChain(D3D11Base::IDXGISwapChain *pOrig);
    inline D3D11Base::IDXGISwapChain *GetSwapChain() { return m_pSwapChain; }

    //*** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	//*** IDXGIDeviceSubObject methods
	STDMETHOD(SetPrivateData)(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            _In_reads_bytes_(DataSize)  const void *pData);
        
	STDMETHOD(SetPrivateDataInterface)(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][in] */ 
            _In_  const IUnknown *pUnknown);
        
	STDMETHOD(GetPrivateData)(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][out][in] */ 
            _Inout_  UINT *pDataSize,
            /* [annotation][out] */ 
            _Out_writes_bytes_(*pDataSize)  void *pData);
        
	STDMETHOD(GetParent)(THIS_
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _Out_  void **ppParent);
        
	STDMETHOD(GetDevice)(THIS_
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _Out_  void **ppDevice);

	//**** IDXGISwapChain implementation
    STDMETHOD(Present)(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags);
        
    STDMETHOD(GetBuffer)(THIS_
            /* [in] */ UINT Buffer,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][out][in] */ 
            _Out_  void **ppSurface);
        
    STDMETHOD(SetFullscreenState)(THIS_
            /* [in] */ BOOL Fullscreen,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pTarget);
        
    STDMETHOD(GetFullscreenState)(THIS_
            /* [annotation][out] */ 
            _Out_opt_  BOOL *pFullscreen,
            /* [annotation][out] */ 
            _Out_opt_  IDXGIOutput **ppTarget);
        
    STDMETHOD(GetDesc)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_DESC *pDesc);
        
    STDMETHOD(ResizeBuffers)(THIS_
            /* [in] */ UINT BufferCount,
            /* [in] */ UINT Width,
            /* [in] */ UINT Height,
            /* [in] */ D3D11Base::DXGI_FORMAT NewFormat,
            /* [in] */ UINT SwapChainFlags);
        
    STDMETHOD(ResizeTarget)(THIS_
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_MODE_DESC *pNewTargetParameters);
        
    STDMETHOD(GetContainingOutput)(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput);
        
    STDMETHOD(GetFrameStatistics)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_FRAME_STATISTICS *pStats);
        
    STDMETHOD(GetLastPresentCount)(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount);
        
};

class IDXGISwapChain1 : public IDXGISwapChain
{
public:
	IDXGISwapChain1(D3D11Base::IDXGISwapChain1 *pSwapChain)
		: IDXGISwapChain(pSwapChain) {}
    static IDXGISwapChain1* GetDirectSwapChain(D3D11Base::IDXGISwapChain1 *pOrig);
    inline D3D11Base::IDXGISwapChain1 *GetSwapChain1() { return (D3D11Base::IDXGISwapChain1*) m_pSwapChain; }

	STDMETHOD(GetDesc1)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc);
        
	STDMETHOD(GetFullscreenDesc)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc);
        
	STDMETHOD(GetHwnd)(THIS_
            /* [annotation][out] */ 
            _Out_  HWND *pHwnd);
        
	STDMETHOD(GetCoreWindow)(THIS_
            /* [annotation][in] */ 
            _In_  REFIID refiid,
            /* [annotation][out] */ 
            _Out_  void **ppUnk);
        
	STDMETHOD(Present1)(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT PresentFlags,
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_PRESENT_PARAMETERS *pPresentParameters);
        
	STDMETHOD_(BOOL, IsTemporaryMonoSupported)(THIS);
        
	STDMETHOD(GetRestrictToOutput)(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput);
        
	STDMETHOD(SetBackgroundColor)(THIS_
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_RGBA *pColor);
        
	STDMETHOD(GetBackgroundColor)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_RGBA *pColor);
        
	STDMETHOD(SetRotation)(THIS_
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_MODE_ROTATION Rotation);
        
	STDMETHOD(GetRotation)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_MODE_ROTATION *pRotation);
};
