#pragma once

#include <dxgi1_2.h>

// -----------------------------------------------------------------------------

class HackerDXGIOutput : public IDXGIOutput
{
private:
	IDXGIOutput		*mOrigOutput;

public:
	HackerDXGIOutput(IDXGIOutput *pOutput);


	// ******************* IDirect3DUnknown methods 

	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	// ******************* IDXGIObject methods

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

	// ******************* IDXGIOutput methods 
	STDMETHOD(GetDesc)(THIS_
		/* [annotation][out] */
		__out  DXGI_OUTPUT_DESC *pDesc);

	STDMETHOD(GetDisplayModeList)(THIS_
		/* [in] */ DXGI_FORMAT EnumFormat,
		/* [in] */ UINT Flags,
		/* [annotation][out][in] */
		__inout  UINT *pNumModes,
		/* [annotation][out] */
		__out_ecount_part_opt(*pNumModes, *pNumModes)  DXGI_MODE_DESC *pDesc);

	STDMETHOD(FindClosestMatchingMode)(THIS_
		/* [annotation][in] */
		__in  const DXGI_MODE_DESC *pModeToMatch,
		/* [annotation][out] */
		__out  DXGI_MODE_DESC *pClosestMatch,
		/* [annotation][in] */
		__in_opt  IUnknown *pConcernedDevice);

	STDMETHOD(WaitForVBlank)(THIS_);

	STDMETHOD(TakeOwnership)(THIS_
		/* [annotation][in] */
		__in  IUnknown *pDevice,
		BOOL Exclusive);

	virtual void STDMETHODCALLTYPE ReleaseOwnership(void);

	STDMETHOD(GetGammaControlCapabilities)(THIS_
		/* [annotation][out] */
		__out  DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps);

	STDMETHOD(SetGammaControl)(THIS_
		/* [annotation][in] */
		__in  const DXGI_GAMMA_CONTROL *pArray);

	STDMETHOD(GetGammaControl)(THIS_
		/* [annotation][out] */
		__out  DXGI_GAMMA_CONTROL *pArray);

	STDMETHOD(SetDisplaySurface)(THIS_
		/* [annotation][in] */
		__in  IDXGISurface *pScanoutSurface);

	STDMETHOD(GetDisplaySurfaceData)(THIS_
		/* [annotation][in] */
		__in  IDXGISurface *pDestination);

	STDMETHOD(GetFrameStatistics)(THIS_
		/* [annotation][out] */
		__out  DXGI_FRAME_STATISTICS *pStats);
};


// -----------------------------------------------------------------------------

class HackerDXGIAdapter : public IDXGIAdapter
{
private:
	IDXGIAdapter *mOrigAdapter;

public:
	HackerDXGIAdapter(IDXGIAdapter *pAdapter);

    // ******************* IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	// ******************* IDXGIObject methods
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

    // ******************* IDXGIFactory methods 
	STDMETHOD(EnumOutputs)(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput);
        
    STDMETHOD(GetDesc)(THIS_
            /* [annotation][out] */ 
            __out DXGI_ADAPTER_DESC *pDesc);
        
    STDMETHOD(CheckInterfaceSupport)(THIS_
            /* [annotation][in] */ 
            __in  REFGUID InterfaceName,
            /* [annotation][out] */ 
            __out  LARGE_INTEGER *pUMDVersion);

};


// -----------------------------------------------------------------------------

class HackerDXGIAdapter1 : public HackerDXGIAdapter
{
private:
	IDXGIAdapter1 *mOrigAdapter1;

public:
	HackerDXGIAdapter1(IDXGIAdapter1 *pAdapter);

    STDMETHOD(GetDesc1)(THIS_ 
	        /* [annotation][out] */ 
            __out  DXGI_ADAPTER_DESC1 *pDesc);
};


// -----------------------------------------------------------------------------

// MIDL_INTERFACE("310d36a0-d2e7-4c0a-aa04-6a9d23b8886a")
class HackerDXGISwapChain : public IDXGISwapChain
{
private:
	IDXGISwapChain *mOrigSwapChain;

public:

	HackerDXGISwapChain(IDXGISwapChain *pOutput);

    // ******************* IDirect3DUnknown methods 
	
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	// ******************* IDXGIDeviceSubObject methods

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
            _Out_  DXGI_SWAP_CHAIN_DESC *pDesc);
        
    STDMETHOD(ResizeBuffers)(THIS_
            /* [in] */ UINT BufferCount,
            /* [in] */ UINT Width,
            /* [in] */ UINT Height,
            /* [in] */ DXGI_FORMAT NewFormat,
            /* [in] */ UINT SwapChainFlags);
        
    STDMETHOD(ResizeTarget)(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC *pNewTargetParameters);
        
    STDMETHOD(GetContainingOutput)(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput);
        
    STDMETHOD(GetFrameStatistics)(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_FRAME_STATISTICS *pStats);
        
    STDMETHOD(GetLastPresentCount)(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount);
        
};


// -----------------------------------------------------------------------------

class HackerDXGISwapChain1 : public HackerDXGISwapChain
{
private:
	IDXGISwapChain1 *mOrigSwapChain1;

public:
	HackerDXGISwapChain1(IDXGISwapChain1 *pSwapChain);

	STDMETHOD(GetDesc1)(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc);
        
	STDMETHOD(GetFullscreenDesc)(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc);
        
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
            _In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters);
        
	STDMETHOD_(BOOL, IsTemporaryMonoSupported)(THIS);
        
	STDMETHOD(GetRestrictToOutput)(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput);
        
	STDMETHOD(SetBackgroundColor)(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_RGBA *pColor);
        
	STDMETHOD(GetBackgroundColor)(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_RGBA *pColor);
        
	STDMETHOD(SetRotation)(THIS_
            /* [annotation][in] */ 
            _In_  DXGI_MODE_ROTATION Rotation);
        
	STDMETHOD(GetRotation)(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_ROTATION *pRotation);
};

// -----------------------------------------------------------------------------

class HackerDXGIFactory : public IDXGIFactory
{
private:
	IDXGIFactory *mOrigFactory;

public:
	HackerDXGIFactory(IDXGIFactory *pFactory);

	// ******************* IDirect3DUnknown methods

	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);
	STDMETHOD(QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject));

	// ******************* IDXGIObject methods

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

	// ******************* IDXGIFactory methods 

	STDMETHOD(EnumAdapters)(THIS_
		/* [in] */ UINT Adapter,
		/* [annotation][out] */
		__out IDXGIAdapter **ppAdapter);

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
		__in  DXGI_SWAP_CHAIN_DESC *pDesc,
		/* [annotation][out] */
		__out  IDXGISwapChain **ppSwapChain);

	STDMETHOD(CreateSoftwareAdapter)(THIS_
		/* [in] */ HMODULE Module,
		/* [annotation][out] */
		__out  IDXGIAdapter **ppAdapter);

};

// -----------------------------------------------------------------------------

class HackerDXGIFactory1 : public HackerDXGIFactory
{
private:
	IDXGIFactory1 *mOrigFactory1;

public:
	HackerDXGIFactory1(IDXGIFactory1 *pFactory);

	STDMETHOD(EnumAdapters1)(THIS_
		/* [in] */ UINT Adapter,
		/* [annotation][out] */
		__out IDXGIAdapter1 **ppAdapter);

	STDMETHOD_(BOOL, IsCurrent)(THIS);
};


// -----------------------------------------------------------------------------

class HackerDXGIFactory2 : public HackerDXGIFactory1
{
private:
	IDXGIFactory2 *mOrigFactory2;

public:
	HackerDXGIFactory2(IDXGIFactory2 *pFactory);

	STDMETHOD_(BOOL, IsWindowedStereoEnabled)(THIS);

	STDMETHOD(CreateSwapChainForHwnd)(
		/* [annotation][in] */
		_In_  IUnknown *pDevice,
		/* [annotation][in] */
		_In_  HWND hWnd,
		/* [annotation][in] */
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */
		_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
		/* [annotation][in] */
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */
		_Out_  IDXGISwapChain1 **ppSwapChain);

	STDMETHOD(CreateSwapChainForCoreWindow)(
		/* [annotation][in] */
		_In_  IUnknown *pDevice,
		/* [annotation][in] */
		_In_  IUnknown *pWindow,
		/* [annotation][in] */
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */
		_Out_  IDXGISwapChain1 **ppSwapChain);

	STDMETHOD(GetSharedResourceAdapterLuid)(
		/* [annotation] */
		_In_  HANDLE hResource,
		/* [annotation] */
		_Out_  LUID *pLuid);

	STDMETHOD(RegisterStereoStatusWindow)(
		/* [annotation][in] */
		_In_  HWND WindowHandle,
		/* [annotation][in] */
		_In_  UINT wMsg,
		/* [annotation][out] */
		_Out_  DWORD *pdwCookie);

	STDMETHOD(RegisterStereoStatusEvent)(
		/* [annotation][in] */
		_In_  HANDLE hEvent,
		/* [annotation][out] */
		_Out_  DWORD *pdwCookie);

	STDMETHOD_(void, STDMETHODCALLTYPE UnregisterStereoStatus)(
		/* [annotation][in] */
		_In_  DWORD dwCookie);

	STDMETHOD(RegisterOcclusionStatusWindow)(
		/* [annotation][in] */
		_In_  HWND WindowHandle,
		/* [annotation][in] */
		_In_  UINT wMsg,
		/* [annotation][out] */
		_Out_  DWORD *pdwCookie);

	STDMETHOD(RegisterOcclusionStatusEvent)(
		/* [annotation][in] */
		_In_  HANDLE hEvent,
		/* [annotation][out] */
		_Out_  DWORD *pdwCookie);

	STDMETHOD_(void, STDMETHODCALLTYPE UnregisterOcclusionStatus)(
		/* [annotation][in] */
		_In_  DWORD dwCookie);

	STDMETHOD(CreateSwapChainForComposition)(
		/* [annotation][in] */
		_In_  IUnknown *pDevice,
		/* [annotation][in] */
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */
		_Outptr_  IDXGISwapChain1 **ppSwapChain);
};


