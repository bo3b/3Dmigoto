// Must include this before including any of the d3d headers to have
// DEFINE_GUID work for the IIDs:
#include <INITGUID.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d9.h>
#include <d3d10_1.h>
#include <d3d10shader.h>
#include <d3d11_4.h>
#include <d3d12.h>

#include "log.h"

struct IID_name {
	IID iid;
	char *name;
};

#define IID(name) { IID_##name, #name }

static const struct IID_name known_interfaces[] = {
	IID(IUnknown),

	// DXGI Interfaces https://msdn.microsoft.com/en-us/library/windows/desktop/ff471322(v=vs.85).aspx
	IID(IDXGIAdapter),
	IID(IDXGIAdapter1),
	IID(IDXGIAdapter2),
	IID(IDXGIAdapter3),
	IID(IDXGIAdapter4),
	IID(IDXGIDebug),
	IID(IDXGIDebug1),
	IID(IDXGIDecodeSwapChain),
	IID(IDXGIDevice),
	IID(IDXGIDevice1),
	IID(IDXGIDevice2),
	IID(IDXGIDevice3),
	IID(IDXGIDevice4),
	IID(IDXGIDeviceSubObject),
	IID(IDXGIDisplayControl),
	IID(IDXGIFactory),
	IID(IDXGIFactory1),
	IID(IDXGIFactory2),
	IID(IDXGIFactory3),
	IID(IDXGIFactory4),
	IID(IDXGIFactory5),
	// FIXME: IID_IDXGIFactory6, Supposed to be in dxgi1_6.h, but not present in SDK 10.0.16299.0
	IID(IDXGIFactoryMedia),
	IID(IDXGIInfoQueue),
	IID(IDXGIKeyedMutex),
	IID(IDXGIObject),
	IID(IDXGIOutput),
	IID(IDXGIOutput1),
	IID(IDXGIOutput2),
	IID(IDXGIOutput3),
	IID(IDXGIOutput4),
	IID(IDXGIOutput5),
	IID(IDXGIOutput6),
	IID(IDXGIOutputDuplication),
	IID(IDXGIResource),
	IID(IDXGIResource1),
	IID(IDXGISurface),
	IID(IDXGISurface1),
	IID(IDXGISurface2),
	IID(IDXGISwapChain),
	IID(IDXGISwapChain1),
	IID(IDXGISwapChain2),
	IID(IDXGISwapChain3),
	IID(IDXGISwapChain4),
	IID(IDXGISwapChainMedia),

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
	IID(ID3D11Device2),
	IID(ID3D11Device3),
	IID(ID3D11Device4),
	IID(ID3D11Device5),
	IID(ID3D11DeviceChild),
	IID(ID3D11DeviceContext),
	IID(ID3D11DeviceContext1),
	IID(ID3D11DeviceContext2),
	IID(ID3D11DeviceContext3),
	IID(ID3D11DeviceContext4),
	IID(ID3DDeviceContextState),
	IID(ID3D11Fence),
	IID(ID3D11InputLayout),
	IID(ID3D11Multithread),
	IID(ID3D11Predicate),
	IID(ID3D11Query),
	IID(ID3D11Query1),
	IID(ID3D11RasterizerState),
	IID(ID3D11RasterizerState1),
	IID(ID3D11RasterizerState2),
	IID(ID3D11SamplerState),

	// D3D12 https://msdn.microsoft.com/en-us/library/windows/desktop/dn770457(v=vs.85).aspx
	IID(ID3D12CommandAllocator),
	IID(ID3D12CommandList),
	IID(ID3D12CommandQueue),
	IID(ID3D12CommandSignature),
	IID(ID3D12DescriptorHeap),
	IID(ID3D12Device),
	IID(ID3D12Device1),
	IID(ID3D12Device2),
	IID(ID3D12Device3),
	IID(ID3D12DeviceChild),
	IID(ID3D12Fence),
	IID(ID3D12Fence1),
	IID(ID3D12GraphicsCommandList),
	IID(ID3D12GraphicsCommandList1),
	IID(ID3D12GraphicsCommandList2),
	IID(ID3D12Heap),
	IID(ID3D12Object),
	IID(ID3D12Pageable),
	IID(ID3D12PipelineLibrary),
	IID(ID3D12PipelineLibrary1),
	IID(ID3D12PipelineState),
	IID(ID3D12QueryHeap),
	IID(ID3D12Resource),
	IID(ID3D12RootSignature),
	IID(ID3D12RootSignatureDeserializer),
	IID(ID3D12Tools),
	IID(ID3D12VersionedRootSignatureDeserializer),
};

static void check_interface(IUnknown *unknown, REFIID riid, char *iid_name)
{
	IUnknown *test;

	if (SUCCEEDED(unknown->QueryInterface(riid, (void**)&test))) {
		LogInfo("  Supports %s: %p\n", iid_name, test);
		test->Release();
	} else {
		LogDebug("  %s not supported\n", iid_name);
	}
}

void analyse_iunknown(IUnknown *unknown)
{
	int i;

	LogInfo("Checking what interfaces %p supports...\n", unknown);

	for (i = 0; i < ARRAYSIZE(known_interfaces); i++)
		check_interface(unknown, known_interfaces[i].iid, known_interfaces[i].name);
}

