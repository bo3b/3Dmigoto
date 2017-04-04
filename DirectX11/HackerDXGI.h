// For this class, we are using the C++ hierarchy to avoid having to duplicate
// a bunch of boilerplate code.
//
// The HackerDXGIFactory descends from HackerDXGIObject for example, and gets
// the benefit of all of the base class's methods.  I verified that the subclass
// gets called correctly, even in this odd COM object world.  The COM interface
// will not support this, but as long as we stay in C++, we get the benefit of
// inheritance here.
//
// That means that any call to Release or Add for example, will call through
// to the original base class of HackerUnknown, for any of the sub-objects.

#pragma once

#include <dxgi1_2.h>

#include "HackerDevice.h"
#include "HackerContext.h"
#include "Overlay.h"


// Forward references required because of circular references from the
// other 'Hacker' objects.

class HackerDevice;
class HackerContext;
class Overlay;


void ForceDisplayParams(DXGI_SWAP_CHAIN_DESC *pDesc);

// -----------------------------------------------------------------------------

class HackerUnknown : public IUnknown
{
private:
	IUnknown *mOrigUnknown;

public:
	HackerUnknown(IUnknown *pUnknown);


	STDMETHOD(QueryInterface)(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	STDMETHOD_(ULONG, AddRef)(THIS);

	STDMETHOD_(ULONG, Release)(THIS);
};


// -----------------------------------------------------------------------------

class HackerDXGIObject : public HackerUnknown
{
private:
	IDXGIObject *mOrigObject;

public:
	HackerDXGIObject(IDXGIObject *pObject);


	STDMETHOD(SetPrivateData)(
		/* [annotation][in] */
		_In_  REFGUID Name,
		/* [in] */ UINT DataSize,
		/* [annotation][in] */
		_In_reads_bytes_(DataSize)  const void *pData);

	STDMETHOD(SetPrivateDataInterface)(
		/* [annotation][in] */
		_In_  REFGUID Name,
		/* [annotation][in] */
		_In_  const IUnknown *pUnknown);

	STDMETHOD(GetPrivateData)(
		/* [annotation][in] */
		_In_  REFGUID Name,
		/* [annotation][out][in] */
		_Inout_  UINT *pDataSize,
		/* [annotation][out] */
		_Out_writes_bytes_(*pDataSize)  void *pData);

	STDMETHOD(GetParent)(
		/* [annotation][in] */
		_In_  REFIID riid,
		/* [annotation][retval][out] */
		_Out_  void **ppParent);
};

			  
// -----------------------------------------------------------------------------

class HackerDXGIAdapter : public HackerDXGIObject
{
private:
	IDXGIAdapter *mOrigAdapter;

public:
	HackerDXGIAdapter(IDXGIAdapter *pAdapter);


	// Specifically override the QueryInterface here so that we can return a 
	// wrapped HackerDXGIAdapter1 for a COM type-coercion.
	STDMETHOD(QueryInterface)(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override;

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
	HackerDXGIAdapter1(IDXGIAdapter1 *pAdapter1);


	STDMETHOD(GetDesc1)(THIS_
		/* [annotation][out] */
		__out  DXGI_ADAPTER_DESC1 *pDesc);
};

// -----------------------------------------------------------------------------

// Requires platform update for Win7.  

class HackerDXGIAdapter2 : public HackerDXGIAdapter1
{
private:
	IDXGIAdapter2 *mOrigAdapter2;

public:
	HackerDXGIAdapter2(IDXGIAdapter2 *pAdapter2);


	STDMETHOD(GetDesc2)(THIS_
		/* [annotation][out] */
		__out  DXGI_ADAPTER_DESC2 *pDesc);
};


// -----------------------------------------------------------------------------

class HackerDXGIDevice : public HackerDXGIObject
{
private:
	IDXGIDevice *mOrigDXGIDevice;

	HackerDevice *mHackerDevice;

public:
	HackerDXGIDevice(IDXGIDevice *pDXGIDevice, HackerDevice *pDevice);

	IDXGIDevice *GetOrigDXGIDevice();
	HackerDevice *GetHackerDevice();


	STDMETHOD(GetAdapter)(
		/* [annotation][out] */
		_Out_  IDXGIAdapter **pAdapter);

	STDMETHOD(CreateSurface)(
		/* [annotation][in] */
		_In_  const DXGI_SURFACE_DESC *pDesc,
		/* [in] */ UINT NumSurfaces,
		/* [in] */ DXGI_USAGE Usage,
		/* [annotation][in] */
		_In_opt_  const DXGI_SHARED_RESOURCE *pSharedResource,
		/* [annotation][out] */
		_Out_  IDXGISurface **ppSurface);

	STDMETHOD(QueryResourceResidency)(
		/* [annotation][size_is][in] */
		_In_reads_(NumResources)  IUnknown *const *ppResources,
		/* [annotation][size_is][out] */
		_Out_writes_(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
		/* [in] */ UINT NumResources);

	STDMETHOD(SetGPUThreadPriority)(
		/* [in] */ INT Priority);

	STDMETHOD(GetGPUThreadPriority)(
		/* [annotation][retval][out] */
		_Out_  INT *pPriority);
};


// -----------------------------------------------------------------------------

class HackerDXGIDevice1 : public HackerDXGIDevice
{
private:
	IDXGIDevice1 *mOrigDXGIDevice1;

public:
	HackerDXGIDevice1(IDXGIDevice1 *pDXGIDevice, HackerDevice *pDevice);

	IDXGIDevice1 *GetOrigDXGIDevice1();


	STDMETHOD(SetMaximumFrameLatency)(
		/* [in] */ UINT MaxLatency);

	STDMETHOD(GetMaximumFrameLatency)(
		/* [annotation][out] */
		_Out_  UINT *pMaxLatency);
};

// -----------------------------------------------------------------------------

// Requires platform update for Win7.

class HackerDXGIDevice2 : public HackerDXGIDevice1
{
private:
	IDXGIDevice2 *mOrigDXGIDevice2;

public:
	HackerDXGIDevice2(IDXGIDevice2 *pDXGIDevice, HackerDevice *pDevice);

	IDXGIDevice2 *GetOrigDXGIDevice2();


	STDMETHOD(OfferResources)(
		/* [annotation][in] */
		_In_  UINT NumResources,
		/* [annotation][size_is][in] */
		_In_reads_(NumResources)  IDXGIResource *const *ppResources,
		/* [annotation][in] */
		_In_  DXGI_OFFER_RESOURCE_PRIORITY Priority);

	STDMETHOD(ReclaimResources)(
		/* [annotation][in] */
		_In_  UINT NumResources,
		/* [annotation][size_is][in] */
		_In_reads_(NumResources)  IDXGIResource *const *ppResources,
		/* [annotation][size_is][out] */
		_Out_writes_all_opt_(NumResources)  BOOL *pDiscarded);

	STDMETHOD(EnqueueSetEvent)(
		/* [annotation][in] */
		_In_  HANDLE hEvent);
};


// -----------------------------------------------------------------------------

class HackerDXGIOutput : public HackerDXGIObject
{
private:
	IDXGIOutput	*mOrigOutput;

public:
	HackerDXGIOutput(IDXGIOutput *pOutput);


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

class HackerDXGIOutput1: public HackerDXGIOutput
{
private:
	IDXGIOutput1	*mOrigOutput1;

public:
	HackerDXGIOutput1(IDXGIOutput1 *pOutput1);


	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList1(
		/* [in] */ DXGI_FORMAT EnumFormat,
		/* [in] */ UINT Flags,
		/* [annotation][out][in] */
		_Inout_  UINT *pNumModes,
		/* [annotation][out] */
		_Out_writes_to_opt_(*pNumModes, *pNumModes)  DXGI_MODE_DESC1 *pDesc);

	virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(
		/* [annotation][in] */
		_In_  const DXGI_MODE_DESC1 *pModeToMatch,
		/* [annotation][out] */
		_Out_  DXGI_MODE_DESC1 *pClosestMatch,
		/* [annotation][in] */
		_In_opt_  IUnknown *pConcernedDevice);

	virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData1(
		/* [annotation][in] */
		_In_  IDXGIResource *pDestination);

	virtual HRESULT STDMETHODCALLTYPE DuplicateOutput(
		/* [annotation][in] */
		_In_  IUnknown *pDevice,
		/* [annotation][out] */
		_Out_  IDXGIOutputDuplication **ppOutputDuplication);
};

// TODO: Remove all the STDMETHOD uses here, and use the interfaces as defined from 
// the header file instead.  Keeps it more consistent with header file, less prone to edit errors.

// -----------------------------------------------------------------------------

class HackerDXGIDeviceSubObject : public HackerDXGIObject
{
private:
	IDXGIDeviceSubObject *mOrigDeviceSubObject;

public:
	HackerDXGIDeviceSubObject(IDXGIDeviceSubObject *pSubObject);


	STDMETHOD(GetDevice)(
		/* [annotation][in] */
		_In_  REFIID riid,
		/* [annotation][retval][out] */
		_Out_  void **ppDevice);
};


// -----------------------------------------------------------------------------

class HackerDXGIResource : public HackerDXGIDeviceSubObject
{
private:
	IDXGIResource *mOrigResource;

public:
	HackerDXGIResource(IDXGIResource *pResource);


	virtual HRESULT STDMETHODCALLTYPE GetSharedHandle(
		/* [annotation][out] */
		_Out_  HANDLE *pSharedHandle);

	virtual HRESULT STDMETHODCALLTYPE GetUsage(
		/* [annotation][out] */
		_Out_  DXGI_USAGE *pUsage);

	virtual HRESULT STDMETHODCALLTYPE SetEvictionPriority(
		/* [in] */ UINT EvictionPriority);

	virtual HRESULT STDMETHODCALLTYPE GetEvictionPriority(
		/* [annotation][retval][out] */
		_Out_  UINT *pEvictionPriority);
};


// -----------------------------------------------------------------------------

class HackerDXGIResource1 : public HackerDXGIResource
{
private:
	IDXGIResource1 *mOrigResource1;

public:
	HackerDXGIResource1(IDXGIResource1 *pResource1);


	virtual HRESULT STDMETHODCALLTYPE CreateSubresourceSurface(
		UINT index,
		/* [annotation][out] */
		_Out_  IDXGISurface2 **ppSurface);

	virtual HRESULT STDMETHODCALLTYPE CreateSharedHandle(
		/* [annotation][in] */
		_In_opt_  const SECURITY_ATTRIBUTES *pAttributes,
		/* [annotation][in] */
		_In_  DWORD dwAccess,
		/* [annotation][in] */
		_In_opt_  LPCWSTR lpName,
		/* [annotation][out] */
		_Out_  HANDLE *pHandle);
};


// -----------------------------------------------------------------------------

class HackerDXGISwapChain : public HackerDXGIDeviceSubObject
{
private:
	IDXGISwapChain *mOrigSwapChain;

	HackerDevice *mHackerDevice;
	HackerContext *mHackerContext;
	Overlay *mOverlay;

public:

	HackerDXGISwapChain(IDXGISwapChain *pOutput, HackerDevice *pDevice, HackerContext *pContext);

	IDXGISwapChain* GetOrigSwapChain();
	void RunFrameActions();


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

// Requires evil platform update for Win7, so we are specifically not implementing
// this at present.  Just for logging.

class HackerDXGISwapChain1 : public HackerDXGISwapChain
{
private:
	IDXGISwapChain1 *mOrigSwapChain1;

public:
	HackerDXGISwapChain1(IDXGISwapChain1 *pSwapChain, HackerDevice *pDevice, HackerContext *pContext);

	
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

class HackerDXGIFactory : public HackerDXGIObject
{
private:
	IDXGIFactory *mOrigFactory;

public:
	HackerDXGIFactory(IDXGIFactory *pFactory);
	

	// Specifically override the QueryInterface here so that we can return an 
	// error for attempts to create IDXGIFactory2.
	STDMETHOD(QueryInterface)(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override;

	STDMETHOD(EnumAdapters)(THIS_
		/* [in] */ UINT Adapter,
		/* [annotation][out] */
		_Out_  IDXGIAdapter **ppAdapter);

	STDMETHOD(MakeWindowAssociation)(THIS_
		HWND WindowHandle,
		UINT Flags);

	STDMETHOD(GetWindowAssociation)(THIS_
		/* [annotation][out] */
		__out  HWND *pWindowHandle);

	STDMETHOD(CreateSwapChain)(THIS_
		/* [annotation][in] */
		_In_  IUnknown *pDevice,
		/* [annotation][in] */
		_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
		/* [annotation][out] */
		_Out_  IDXGISwapChain **ppSwapChain);

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

// Requires evil platform update for Win7, so we are specifically not implementing
// this at present.  Just for logging.

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

