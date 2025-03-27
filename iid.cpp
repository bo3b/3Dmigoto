// Must include this before including any of the d3d headers to have
// DEFINE_GUID work for the IIDs:
#include <initguid.h>
#include <sdkddkver.h>

#ifdef NTDDI_WIN10_RS3
    // SDK 10.0.16299.0 or higher
    // Haven't checked if this was also in RS2
    #include <dxgi1_6.h>
#endif

#ifdef NTDDI_WIN10
    // FIXME: Check if any of these require a minimum 10.x SDK version
    #include <d3d11_4.h>
    #include <d3d12.h>
    #include <dxgi1_5.h>
#endif

#ifdef NTDDI_WINBLUE
    // SDK 8.1 or higher
    #include <d3d11_2.h>
    #include <dxgi1_3.h>
#endif

#include "log.h"

#include <d3d10_1.h>
#include <d3d11_1.h>
#include <d3d9.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>

// For 3DMigoto IIDs:
#include "DirectX11/FrameAnalysis.hpp"
#include "DirectX11/HackerContext.hpp"
#include "DirectX11/HackerDevice.hpp"

using std::string;
using std::wstring;

// -----------------------------------------------------------------------------

struct IID_name
{
    IID   iid;
    char* name;
};

#define IID(name)         \
    {                     \
        IID_##name, #name \
    }

// IIDs used to identify if a known third party tool is sitting between us and
// DirectX (for informational / debugging purposes only):
DEFINE_GUID(IID_ReShadeD3D10Device, 0x88399375, 0x734F, 0x4892, 0xA9, 0x5F, 0x70, 0xDD, 0x42, 0xCE, 0x7C, 0xDD);
DEFINE_GUID(IID_ReShadeD3D11Device, 0x72299288, 0x2C68, 0x4AD8, 0x94, 0x5D, 0x2B, 0xFB, 0x5A, 0xA9, 0xC6, 0x09);
DEFINE_GUID(IID_ReShadeD3D11DeviceContext, 0x27B0246B, 0x2152, 0x4D42, 0xAD, 0x11, 0x32, 0x48, 0x94, 0x72, 0x23, 0x8F);
DEFINE_GUID(IID_ReShadeDXGIDevice, 0xCB285C3B, 0x3677, 0x4332, 0x98, 0xC7, 0xD6, 0x33, 0x9B, 0x97, 0x82, 0xB1);
DEFINE_GUID(IID_ReShadeDXGISwapChain, 0x1F445F9F, 0x9887, 0x4C4C, 0x90, 0x55, 0x4E, 0x3B, 0xAD, 0xAF, 0xCC, 0xA8);
DEFINE_GUID(IID_SpecialKD3D11DeviceContext, 0xe8a22a3f, 0x1405, 0x424c, 0xae, 0x99, 0x0d, 0x3e, 0x9d, 0x54, 0x7c, 0x32);  // Returns the unwrapped device

static const struct IID_name known_interfaces[] = {
    IID(IUnknown),

    // 3DMigoto interfaces:
    IID(HackerDevice),
    IID(HackerContext),
    IID(FrameAnalysisContext),

    // Third party tools:
    IID(ReShadeD3D10Device),
    IID(ReShadeD3D11Device),
    IID(ReShadeD3D11DeviceContext),
    IID(ReShadeDXGIDevice),
    IID(ReShadeDXGISwapChain),
    IID(SpecialKD3D11DeviceContext),

    // DXGI Interfaces https://msdn.microsoft.com/en-us/library/windows/desktop/ff471322(v=vs.85).aspx
    IID(IDXGIAdapter),
    IID(IDXGIAdapter1),
    IID(IDXGIAdapter2),
    IID(IDXGIDebug),
    IID(IDXGIDevice),
    IID(IDXGIDevice1),
    IID(IDXGIDevice2),
    IID(IDXGIDeviceSubObject),
    IID(IDXGIDisplayControl),
    IID(IDXGIFactory),
    IID(IDXGIFactory1),
    IID(IDXGIFactory2),
    // FIXME: IID_IDXGIFactory6, Supposed to be in dxgi1_6.h, but not present in SDK 10.0.16299.0
    IID(IDXGIInfoQueue),
    IID(IDXGIKeyedMutex),
    IID(IDXGIObject),
    IID(IDXGIOutput),
    IID(IDXGIOutput1),
    IID(IDXGIOutputDuplication),
    IID(IDXGIResource),
    IID(IDXGIResource1),
    IID(IDXGISurface),
    IID(IDXGISurface1),
    IID(IDXGISurface2),
    IID(IDXGISwapChain),
    IID(IDXGISwapChain1),

    // D3D9 https://msdn.microsoft.com/en-us/library/windows/desktop/ff471470(v=vs.85).aspx
    // ??? IID(ID3DXFile),
    // ??? IID(ID3DXFileData),
    // ??? IID(ID3DXFileEnumObject),
    // ??? IID(ID3DXFileSaveData),
    // ??? IID(ID3DXFileSaveObject),
    IID(IDirect3D9),
    IID(IDirect3DBaseTexture9),
    IID(IDirect3DCubeTexture9),
    IID(IDirect3DDevice9),
    IID(IDirect3DIndexBuffer9),
    IID(IDirect3DPixelShader9),
    IID(IDirect3DQuery9),
    IID(IDirect3DResource9),
    IID(IDirect3DStateBlock9),
    IID(IDirect3DSurface9),
    IID(IDirect3DSwapChain9),
    IID(IDirect3DTexture9),
    IID(IDirect3DVertexBuffer9),
    IID(IDirect3DVertexDeclaration9),
    IID(IDirect3DVertexShader9),
    IID(IDirect3DVolume9),
    IID(IDirect3DVolumeTexture9),
    // D3D9Ex
    IID(IDirect3D9Ex),
    IID(IDirect3D9ExOverlayExtension),
    IID(IDirect3DAuthenticatedChannel9),
    IID(IDirect3DCryptoSession9),
    IID(IDirect3DDevice9Ex),
    IID(IDirect3DDevice9Video),
    IID(IDirect3DSwapChain9Ex),

    // D3D10 https://msdn.microsoft.com/en-us/library/windows/desktop/bb205152(v=vs.85).aspx
    IID(ID3D10BlendState),
    IID(ID3D10BlendState1),
    IID(ID3D10DepthStencilState),
    IID(ID3D10InputLayout),
    IID(ID3D10RasterizerState),
    IID(ID3D10SamplerState),
    IID(ID3D10Asynchronous),
    IID(ID3D10Blob),
    IID(ID3D10Counter),
    IID(ID3D10Debug),
    IID(ID3D10Device),
    IID(ID3D10Device1),
    IID(ID3D10DeviceChild),
    // IID(ID3D10Include), // Renamed and moved to d3dcommon.h, but where's the IID GUID?
    IID(ID3D10InfoQueue),
    IID(ID3D10Multithread),
    IID(ID3D10Predicate),
    IID(ID3D10Query),
    IID(ID3D10StateBlock),
    IID(ID3D10SwitchToRef),

    // D3D11 https://msdn.microsoft.com/en-us/library/windows/desktop/ff476154(v=vs.85).aspx
    IID(ID3D11Asynchronous),
    IID(ID3D11BlendState),
    IID(ID3D11BlendState1),
    IID(ID3D11CommandList),
    IID(ID3D11Counter),
    IID(ID3D11DepthStencilState),
    IID(ID3D11Device),
    IID(ID3D11Device1),
    IID(ID3D11DeviceChild),
    IID(ID3D11DeviceContext),
    IID(ID3D11DeviceContext1),
    IID(ID3DDeviceContextState),
    IID(ID3D11InputLayout),
    IID(ID3D11Predicate),
    IID(ID3D11Query),
    IID(ID3D11RasterizerState),
    IID(ID3D11RasterizerState1),
    IID(ID3D11SamplerState),

#ifdef NTDDI_WINBLUE
    // Win 8.1 SDK or higher
    IID(IDXGIDebug1),
    IID(IDXGIDecodeSwapChain),
    IID(IDXGIDevice3),
    IID(IDXGIFactory3),
    IID(IDXGIFactoryMedia),
    IID(IDXGIOutput2),
    IID(IDXGIOutput3),
    IID(IDXGISwapChain2),
    IID(IDXGISwapChainMedia),
    IID(ID3D11Device2),
    IID(ID3D11DeviceContext2),
#endif

#ifdef NTDDI_WIN10
    // Win 10.x SDK. Haven't checked which specific SDK versions introduced these
    IID(IDXGIAdapter3),
    IID(IDXGIDevice4),
    IID(IDXGIFactory4),
    IID(IDXGIFactory5),
    IID(IDXGIOutput4),
    IID(IDXGIOutput5),
    IID(IDXGISwapChain3),
    IID(IDXGISwapChain4),
    IID(ID3D11Device3),
    IID(ID3D11Device4),
    IID(ID3D11DeviceContext3),
    IID(ID3D11Multithread),
    IID(ID3D11Query1),
    IID(ID3D11RasterizerState2),

    // D3D12 https://msdn.microsoft.com/en-us/library/windows/desktop/dn770457(v=vs.85).aspx
    IID(ID3D12CommandAllocator),
    IID(ID3D12CommandList),
    IID(ID3D12CommandQueue),
    IID(ID3D12CommandSignature),
    IID(ID3D12DescriptorHeap),
    IID(ID3D12Device),
    IID(ID3D12Device1),
    IID(ID3D12DeviceChild),
    IID(ID3D12Fence),
    IID(ID3D12GraphicsCommandList),
    IID(ID3D12Heap),
    IID(ID3D12Object),
    IID(ID3D12Pageable),
    IID(ID3D12PipelineLibrary),
    IID(ID3D12PipelineState),
    IID(ID3D12QueryHeap),
    IID(ID3D12Resource),
    IID(ID3D12RootSignature),
    IID(ID3D12RootSignatureDeserializer),
    IID(ID3D12VersionedRootSignatureDeserializer),
#endif

#ifdef NTDDI_WIN10_RS3
    //   Not in SDK 10.0.14393.0 / NTDDI_WIN10_RS1
    //             Haven't checked NTDDI_WIN10_RS2
    // Found in SDK 10.0.16299.0 / NTDDI_WIN10_RS3
    IID(IDXGIAdapter4),
    IID(IDXGIOutput6),
    IID(ID3D11Device5),
    IID(ID3D11DeviceContext4),
    IID(ID3D11Fence),
    IID(ID3D12Device2),
    IID(ID3D12Device3),
    IID(ID3D12Fence1),
    IID(ID3D12GraphicsCommandList1),
    IID(ID3D12GraphicsCommandList2),
    IID(ID3D12PipelineLibrary1),
    IID(ID3D12Tools),
#endif
};

// NOTE: This releases the interface it returns.
static IUnknown* _check_interface(
    IUnknown* unknown,
    REFIID    riid)
{
    IUnknown* test = nullptr;

    if (SUCCEEDED(unknown->QueryInterface(riid, reinterpret_cast<void**>(&test))))
    {
        test->Release();
        return test;
    }

    return nullptr;
}

bool check_interface_supported(
    IUnknown* unknown,
    REFIID    riid)
{
    return !!_check_interface(unknown, riid);
}

static void check_interface(
    IUnknown* unknown,
    REFIID    riid,
    char*     iid_name,
    IUnknown* canonical)
{
    IUnknown* test = _check_interface(unknown, riid);
    IUnknown* canonical_test;

    if (test)
    {
        // Check for violations of the COM identity rule, as that may
        // interfere with our ability to locate our HackerObjects:
        // https://docs.microsoft.com/en-gb/windows/desktop/com/rules-for-implementing-queryinterface
        canonical_test = _check_interface(test, IID_IUnknown);
        if (canonical_test == canonical)
            LOG_INFO("  Supports %s: %p\n", iid_name, test);
        else
            LOG_INFO("  Supports %s: %p (COM identity violation: %p)\n", iid_name, test, canonical_test);
    }
    else
    {
        LOG_DEBUG("  %s not supported\n", iid_name);
    }
}

void analyse_iunknown(
    IUnknown* unknown)
{
    IUnknown* canonical;
    int       i;

    if (!unknown)
        return;

    LOG_INFO("Checking what interfaces %p supports...\n", unknown);

    canonical = _check_interface(unknown, IID_IUnknown);

    for (i = 0; i < ARRAYSIZE(known_interfaces); i++)
        check_interface(unknown, known_interfaces[i].iid, known_interfaces[i].name, canonical);

#ifndef NTDDI_WINBLUE
    LOG_INFO("  Win 8.1 interfaces not checked (3DMigoto built with old SDK)\n");
#endif
#ifndef NTDDI_WIN10
    LOG_INFO("  Win 10 & DX12 interfaces not checked (3DMigoto built with old SDK)\n");
#endif
#ifndef NTDDI_WIN10_RS3
    LOG_INFO("  Win 10 RS3 interfaces not checked (3DMigoto built with old SDK)\n");
#endif
}

// -----------------------------------------------------------------------------------------------
// When logging, it's not very helpful to have long sequences of hex instead of
// the actual names of the objects in question.
// e.g.
// DEFINE_GUID(IID_IDXGIFactory,0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69);
//

string name_from_IID(
    IID id)
{
    // Adding every MIDL_INTERFACE from d3d11_1.h to make this reporting complete.
    // Doesn't seem useful to do every object from d3d11.h itself.

    if (__uuidof(IUnknown) == id)
        return "IUnknown";

        // FIXME: We should probably have these IIDs defined regardless of target
        // to catch potential cases where multiple versions of 3DMigoto are
        // coexisting and the devices get mixed up
#if MIGOTO_DX == 11
    if (IID_HackerDevice == id)
        return "HackerDevice";
    if (IID_HackerContext == id)
        return "HackerContext";
#elif MIGOTO_DX == 9
        // FIXME: DX9 GUIDs are not using the correct macros, and need verification
        // that they haven't been copy + pasted
        // if (IID_D3D9Wrapper_IDirect3DDevice9 == id)
        //    return "3DMigotoDevice9";
#endif

#ifdef _D3D9_H_
    if (__uuidof(IDirect3DDevice9) == id)
        return "IDirect3DDevice9";
#endif  // _D3D9_H_

#ifdef __d3d10_h__
    if (__uuidof(ID3D10Multithread) == id)
        return "ID3D10Multithread";
    if (__uuidof(ID3D10Device) == id)
        return "ID3D10Device";
#endif  // __d3d10_h__

#ifdef __d3d11_h__
    if (__uuidof(ID3D11Device) == id)
        return "ID3D11Device";
    if (__uuidof(ID3D11DeviceContext) == id)
        return "ID3D11DeviceContext";
    if (__uuidof(ID3D11DeviceChild) == id)
        return "ID3D11DeviceChild";
    if (__uuidof(ID3D11BlendState) == id)
        return "ID3D11BlendState";
    if (__uuidof(ID3D11RasterizerState) == id)
        return "ID3D11RasterizerState";
    if (__uuidof(ID3D11Texture2D) == id)  // Used to fetch backbuffer
        return "ID3D11Texture2D";
#endif  // __d3d11_h__

#ifdef __d3d11_1_h__
    if (__uuidof(ID3D11BlendState1) == id)
        return "ID3D11BlendState1";
    if (__uuidof(ID3D11Device1) == id)
        return "ID3D11Device1";
    if (__uuidof(ID3D11DeviceContext1) == id)
        return "ID3D11DeviceContext1";
    if (__uuidof(ID3D11RasterizerState1) == id)
        return "ID3D11RasterizerState1";
    if (__uuidof(ID3DDeviceContextState) == id)
        return "ID3DDeviceContextState";
    if (__uuidof(ID3DUserDefinedAnnotation) == id)
        return "ID3DUserDefinedAnnotation";
#endif  // __d3d11_1_h__

    // XXX: From newer Windows SDK than we are using. Defined in util.h for now
    if (__uuidof(ID3D11Device2) == id)  // d3d11_2.h when the time comes
        return "ID3D11Device2";
    if (__uuidof(ID3D11DeviceContext2) == id)  // d3d11_2.h when the time comes
        return "ID3D11DeviceContext2";

#ifdef __d3d11sdklayers_h__
    if (__uuidof(ID3D11InfoQueue) == id)
        return "ID3D11InfoQueue";
#endif

        // All the DXGI interfaces from dxgi.h, and dxgi1_2.h
#ifdef __dxgi_h__
    if (__uuidof(IDXGIAdapter) == id)
        return "IDXGIAdapter";
    if (__uuidof(IDXGIAdapter1) == id)
        return "IDXGIAdapter1";
    if (__uuidof(IDXGIDevice) == id)
        return "IDXGIDevice";
    if (__uuidof(IDXGIDevice1) == id)
        return "IDXGIDevice1";
    if (__uuidof(IDXGIDeviceSubObject) == id)
        return "IDXGIDeviceSubObject";
    if (__uuidof(IDXGIFactory) == id)
        return "IDXGIFactory";
    if (__uuidof(IDXGIFactory1) == id)
        return "IDXGIFactory1";
    if (__uuidof(IDXGIKeyedMutex) == id)
        return "IDXGIKeyedMutex";
    if (__uuidof(IDXGIObject) == id)
        return "IDXGIObject";
    if (__uuidof(IDXGIOutput) == id)
        return "IDXGIOutput";
    if (__uuidof(IDXGIResource) == id)
        return "IDXGIResource";
    if (__uuidof(IDXGISurface) == id)
        return "IDXGISurface";
    if (__uuidof(IDXGISurface1) == id)
        return "IDXGISurface1";
    if (__uuidof(IDXGISwapChain) == id)
        return "IDXGISwapChain";
#endif  // __dxgi_h__

#ifdef __dxgi1_2_h__
    if (__uuidof(IDXGIAdapter2) == id)
        return "IDXGIAdapter2";
    if (__uuidof(IDXGIDevice2) == id)
        return "IDXGIDevice2";
    if (__uuidof(IDXGIDisplayControl) == id)
        return "IDXGIDisplayControl";
    if (__uuidof(IDXGIFactory2) == id)
        return "IDXGIFactory2";
    if (__uuidof(IDXGIOutput1) == id)
        return "IDXGIOutput1";
    if (__uuidof(IDXGIOutputDuplication) == id)
        return "IDXGIOutputDuplication";
    if (__uuidof(IDXGIResource1) == id)
        return "IDXGIResource1";
    if (__uuidof(IDXGISurface2) == id)
        return "IDXGISurface2";
    if (__uuidof(IDXGISwapChain1) == id)
        return "IDXGISwapChain1";
#endif  // __dxgi1_2_h__

    // XXX: From newer Windows SDK than we are using. Defined in util.h for now
    if (__uuidof(IDXGISwapChain2) == id)  // dxgi1_3 A8BE2AC4-199F-4946-B331-79599FB98DE7
        return "IDXGISwapChain2";
    if (__uuidof(IDXGISwapChain3) == id)  // dxgi1_4 94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1
        return "IDXGISwapChain3";
    if (__uuidof(IDXGISwapChain4) == id)  // dxgi1_5 3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB
        return "IDXGISwapChain4";

    // For unknown IIDs lets return the hex string.
    // Converting from wchar_t to string using stackoverflow suggestion.

    string iid_string;
    wchar_t     wiid[128];
    if (SUCCEEDED(StringFromGUID2(id, wiid, 128)))
    {
        wstring convert = wstring(wiid);
        iid_string           = string(convert.begin(), convert.end());
    }
    else
    {
        iid_string = "unknown";
    }

    return iid_string;
}
