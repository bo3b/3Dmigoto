#pragma once

#include "EnumNames.hpp"

#include <d3d11_1.h>
#include <dxgi1_2.h>

enum class GetResolutionFrom
{
    INVALID = -1,
    SWAP_CHAIN,
    DEPTH_STENCIL,
};
static Enum_Name_t<const wchar_t*, GetResolutionFrom> GetResolutionFromNames[] = {
    { L"swap_chain", GetResolutionFrom::SWAP_CHAIN },
    { L"depth_stencil", GetResolutionFrom::DEPTH_STENCIL },
    { NULL, GetResolutionFrom::INVALID }  // End of list marker
};

// Forward references required because of circular references from the
// other 'Hacker' objects.

class HackerDevice;
class HackerContext;
class Overlay;

// Non member functions referenced in HookedDXGI
void install_SetWindowPos_hook();
void force_display_mode(DXGI_MODE_DESC* buffer_desc);

// -----------------------------------------------------------------------------
// Hierarchy:
//    HackerSwapChain -> IDXGISwapChain1 -> IDXGISwapChain -> IDXGIDeviceSubObject -> IDXGIObject -> IUnknown

class HackerSwapChain : public IDXGISwapChain1
{
protected:
    IDXGISwapChain1* origSwapChain1;
    HackerDevice*    hackerDevice;
    HackerContext*   hackerContext;

public:
    HackerSwapChain(IDXGISwapChain1* swap_chain, HackerDevice* device, HackerContext* context);

    IDXGISwapChain1* GetOrigSwapChain1();
    void             UpdateStereoParams();
    void             RunFrameActions();
    Overlay*         overlay;

    // clang-format off
    // For all the override function definitions, let's disable the clang-format, in order to keep the
    // original formatting from the original Microsoft DX11 functions.

    /** IUnknown **/

    HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();


    /** IDXGIObject **/

    HRESULT STDMETHODCALLTYPE SetPrivateData(
        /* [annotation][in] */
        _In_  REFGUID Name,
        /* [in] */ UINT DataSize,
        /* [annotation][in] */
        _In_reads_bytes_(DataSize)  const void *pData);

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        /* [annotation][in] */
        _In_  REFGUID Name,
        /* [annotation][in] */
        _In_  const IUnknown *pUnknown);

    HRESULT STDMETHODCALLTYPE GetPrivateData(
        /* [annotation][in] */
        _In_  REFGUID Name,
        /* [annotation][out][in] */
        _Inout_  UINT *pDataSize,
        /* [annotation][out] */
        _Out_writes_bytes_(*pDataSize)  void *pData);

    HRESULT STDMETHODCALLTYPE GetParent(
        /* [annotation][in] */
        _In_  REFIID riid,
        /* [annotation][retval][out] */
        _Out_  void **ppParent);


    /** IDXGIDeviceSubObject **/

    HRESULT STDMETHODCALLTYPE GetDevice(
        /* [annotation][in] */
        _In_  REFIID riid,
        /* [annotation][retval][out] */
        _Out_  void **ppDevice);


    /** IDXGISwapChain **/

    HRESULT STDMETHODCALLTYPE Present(
        /* [in] */ UINT SyncInterval,
        /* [in] */ UINT Flags);

    HRESULT STDMETHODCALLTYPE GetBuffer(
        /* [in] */ UINT Buffer,
        /* [annotation][in] */
        _In_  REFIID riid,
        /* [annotation][out][in] */
        _Out_  void **ppSurface);

    HRESULT STDMETHODCALLTYPE SetFullscreenState(
        /* [in] */ BOOL Fullscreen,
        /* [annotation][in] */
        _In_opt_  IDXGIOutput *pTarget);

    HRESULT STDMETHODCALLTYPE GetFullscreenState(
        /* [annotation][out] */
        _Out_opt_  BOOL *pFullscreen,
        /* [annotation][out] */
        _Out_opt_  IDXGIOutput **ppTarget);

    HRESULT STDMETHODCALLTYPE GetDesc(
        /* [annotation][out] */
        _Out_  DXGI_SWAP_CHAIN_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE ResizeBuffers(
        /* [in] */ UINT BufferCount,
        /* [in] */ UINT Width,
        /* [in] */ UINT Height,
        /* [in] */ DXGI_FORMAT NewFormat,
        /* [in] */ UINT SwapChainFlags);

    HRESULT STDMETHODCALLTYPE ResizeTarget(
        /* [annotation][in] */
        _In_  const DXGI_MODE_DESC *pNewTargetParameters);

    HRESULT STDMETHODCALLTYPE GetContainingOutput(
        /* [annotation][out] */
        _Out_  IDXGIOutput **ppOutput);

    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
        /* [annotation][out] */
        _Out_  DXGI_FRAME_STATISTICS *pStats);

    HRESULT STDMETHODCALLTYPE GetLastPresentCount(
        /* [annotation][out] */
        _Out_  UINT *pLastPresentCount);
        

    /** IDXGISwapChain1 **/

    HRESULT STDMETHODCALLTYPE GetDesc1(
        /* [annotation][out] */
        _Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc);

    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
        /* [annotation][out] */
        _Out_  DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE GetHwnd(
        /* [annotation][out] */
        _Out_  HWND *pHwnd);

    HRESULT STDMETHODCALLTYPE GetCoreWindow(
        /* [annotation][in] */
        _In_  REFIID refiid,
        /* [annotation][out] */
        _Out_  void **ppUnk);

    HRESULT STDMETHODCALLTYPE Present1(
        /* [in] */ UINT SyncInterval,
        /* [in] */ UINT PresentFlags,
        /* [annotation][in] */
        _In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters);

    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported();

    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
        /* [annotation][out] */
        _Out_  IDXGIOutput **ppRestrictToOutput);

    HRESULT STDMETHODCALLTYPE SetBackgroundColor(
        /* [annotation][in] */
        _In_  const DXGI_RGBA *pColor);

    HRESULT STDMETHODCALLTYPE GetBackgroundColor(
        /* [annotation][out] */
        _Out_  DXGI_RGBA *pColor);

    HRESULT STDMETHODCALLTYPE SetRotation(
        /* [annotation][in] */
        _In_  DXGI_MODE_ROTATION Rotation);

    HRESULT STDMETHODCALLTYPE GetRotation(
        /* [annotation][out] */
        _Out_  DXGI_MODE_ROTATION *pRotation);
};

// clang-format on

// -----------------------------------------------------------------------------

class HackerUpscalingSwapChain : public HackerSwapChain
{
private:
    IDXGISwapChain1* fakeSwapChain1;
    ID3D11Texture2D* fakeBackBuffer;

    UINT width;
    UINT height;

public:
    HackerUpscalingSwapChain(IDXGISwapChain1* swap_chain, HackerDevice* hacker_device, HackerContext* hacker_context, DXGI_SWAP_CHAIN_DESC* fake_swap_chain_desc, UINT new_width, UINT new_height);
    ~HackerUpscalingSwapChain();

private:
    void CreateRenderTarget(DXGI_SWAP_CHAIN_DESC* fake_swap_chain_desc);

public:
    // clang-format off

    HRESULT STDMETHODCALLTYPE GetBuffer(
        /* [in] */ UINT Buffer,
        /* [annotation][in] */
        _In_  REFIID riid,
        /* [annotation][out][in] */
        _Out_  void **ppSurface);

    HRESULT STDMETHODCALLTYPE SetFullscreenState(
        /* [in] */ BOOL Fullscreen,
        /* [annotation][in] */
        _In_opt_  IDXGIOutput *pTarget);

    HRESULT STDMETHODCALLTYPE GetDesc(
        /* [annotation][out] */
        _Out_  DXGI_SWAP_CHAIN_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE ResizeBuffers(
        /* [in] */ UINT BufferCount,
        /* [in] */ UINT Width,
        /* [in] */ UINT Height,
        /* [in] */ DXGI_FORMAT NewFormat,
        /* [in] */ UINT SwapChainFlags);

    HRESULT STDMETHODCALLTYPE ResizeTarget(
        /* [annotation][in] */
        _In_  const DXGI_MODE_DESC *pNewTargetParameters);

};

// clang-format on
