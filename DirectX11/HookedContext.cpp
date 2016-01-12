// Hooked version of D3D11DeviceContext. The goal in this file is just to
// manage the business of hooking the calls and calling the original functions.
// We still call into the HackerContext for all the hard work - so as not to
// duplicate any functional code.
//
// Since the HackerContext will need to call into the original context and not
// have those calls hooked we take care of that too - we create a trampoline
// interface that will call the original functions with the original this
// pointer and pass that back to the HackerContext for it to use in place of
// the original context.
//
// This is all necessary for MGSV:GZ, which doesn't like us passing it a
// deferred context (device and immediate context are fine) with an unusual
// vtable - changes to IUnknown functions seem ok, but not to ID3D11DeviceChild
// or ID3D11DeviceContext functions. Quite bizarre that it is so picky.


// We are defining CINTERFACE here to get the C declarations of the
// ID3D11DeviceContext, which uses a struct with function pointers for the
// vtable. That avoids some nasty casting which we would have to do with the
// C++ interface and as an added bonus gives us a convenient structure to keep
// the pointers of the original functions.
//
// We also then have to define D3D11_NO_HELPERS (or undef __cplusplus)
// otherwise the header will try to define a bunch of helper functions that
// depend on the C++ API and will fail to compile
//
// The downside of this is it makes it basically impossible to include any
// header files that directly or indirectly depend on the C++ interface, which
// includes HackerDevice.h and HackerContext.h. As tempted as I am to just rip
// the vtable definition out into it's own header to get around that, but
// perhaps this will help decouple this from the rest of 3DMigoto.

#define CINTERFACE
#define D3D11_NO_HELPERS
#include <D3D11.h>
#undef D3D11_NO_HELPERS
#undef CINTERFACE

#include <unordered_map>

#include "HookedContext.h"
#include "DLLMainHook.h"
#include "log.h"


// Change this to 1 to enable debug logging of hooks and the trampolines back
// to the original context. Disabled by default as debug logging will already
// log most of these calls in the HackerDevice, so this would triple the noise
// and we probably won't need it very often:
#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif


// A map to look up the hacker context from the original context:
typedef std::unordered_map<ID3D11DeviceContext *, ID3D11DeviceContext *> ContextMap;
static ContextMap context_map;

// Holds all the function pointers that we need to call into the real original
// context:
static struct ID3D11DeviceContextVtbl orig_vtable;



// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(ID3D11DeviceContext *This, REFIID riid, void **ppvObject)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::QueryInterface()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->QueryInterface(i->second, riid, ppvObject);

	return orig_vtable.QueryInterface(This, riid, ppvObject);
}

static ULONG STDMETHODCALLTYPE AddRef(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::AddRef()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->AddRef(i->second);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Release()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Release(i->second);

	return orig_vtable.Release(This);
}

// ID3D11DeviceChild
static void STDMETHODCALLTYPE GetDevice(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11Device **ppDevice)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetDevice()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetDevice(i->second, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt( *pDataSize )  void *pData)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetPrivateData()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetPrivateData(i->second, guid, pDataSize, pData);

	return orig_vtable.GetPrivateData(This, guid, pDataSize, pData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateData(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt( DataSize )  const void *pData)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::SetPrivateData()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->SetPrivateData(i->second, guid, DataSize, pData);

	return orig_vtable.SetPrivateData(This, guid, DataSize, pData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::SetPrivateDataInterface()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->SetPrivateDataInterface(i->second, guid, pData);

	return orig_vtable.SetPrivateDataInterface(This, guid, pData);
}

// ID3D11DeviceContext
static void STDMETHODCALLTYPE VSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSSetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSSetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.VSSetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE PSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSSetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSSetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.PSSetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE PSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11PixelShader *pPixelShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSSetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSSetShader(i->second,  pPixelShader, ppClassInstances, NumClassInstances);

	return orig_vtable.PSSetShader(This,  pPixelShader, ppClassInstances, NumClassInstances);
}

static void STDMETHODCALLTYPE PSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSSetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSSetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.PSSetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE VSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11VertexShader *pVertexShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSSetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSSetShader(i->second,  pVertexShader, ppClassInstances, NumClassInstances);

	return orig_vtable.VSSetShader(This,  pVertexShader, ppClassInstances, NumClassInstances);
}

static void STDMETHODCALLTYPE DrawIndexed(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT IndexCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DrawIndexed()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DrawIndexed(i->second,  IndexCount, StartIndexLocation, BaseVertexLocation);

	return orig_vtable.DrawIndexed(This,  IndexCount, StartIndexLocation, BaseVertexLocation);
}

static void STDMETHODCALLTYPE Draw(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT VertexCount,
		/* [annotation] */
		__in  UINT StartVertexLocation)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Draw()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Draw(i->second,  VertexCount, StartVertexLocation);

	return orig_vtable.Draw(This,  VertexCount, StartVertexLocation);
}

static HRESULT STDMETHODCALLTYPE Map(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource,
		/* [annotation] */
		__in  D3D11_MAP MapType,
		/* [annotation] */
		__in  UINT MapFlags,
		/* [annotation] */
		__out  D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Map()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Map(i->second,  pResource, Subresource, MapType, MapFlags, pMappedResource);

	return orig_vtable.Map(This,  pResource, Subresource, MapType, MapFlags, pMappedResource);
}

static void STDMETHODCALLTYPE Unmap(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Unmap()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Unmap(i->second,  pResource, Subresource);

	return orig_vtable.Unmap(This,  pResource, Subresource);
}

static void STDMETHODCALLTYPE PSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSSetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSSetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.PSSetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE IASetInputLayout(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11InputLayout *pInputLayout)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IASetInputLayout()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IASetInputLayout(i->second,  pInputLayout);

	return orig_vtable.IASetInputLayout(This,  pInputLayout);
}

static void STDMETHODCALLTYPE IASetVertexBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  const UINT *pStrides,
		/* [annotation] */
		__in_ecount(NumBuffers)  const UINT *pOffsets)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IASetVertexBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IASetVertexBuffers(i->second,  StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

	return orig_vtable.IASetVertexBuffers(This,  StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

static void STDMETHODCALLTYPE IASetIndexBuffer(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11Buffer *pIndexBuffer,
		/* [annotation] */
		__in  DXGI_FORMAT Format,
		/* [annotation] */
		__in  UINT Offset)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IASetIndexBuffer()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IASetIndexBuffer(i->second,  pIndexBuffer, Format, Offset);

	return orig_vtable.IASetIndexBuffer(This,  pIndexBuffer, Format, Offset);
}

static void STDMETHODCALLTYPE DrawIndexedInstanced(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT IndexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DrawIndexedInstanced()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DrawIndexedInstanced(i->second,  IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

	return orig_vtable.DrawIndexedInstanced(This,  IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

static void STDMETHODCALLTYPE DrawInstanced(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT VertexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DrawInstanced()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DrawInstanced(i->second,  VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

	return orig_vtable.DrawInstanced(This,  VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

static void STDMETHODCALLTYPE GSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSSetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSSetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.GSSetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE GSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11GeometryShader *pShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSSetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSSetShader(i->second,  pShader, ppClassInstances, NumClassInstances);

	return orig_vtable.GSSetShader(This,  pShader, ppClassInstances, NumClassInstances);
}

static void STDMETHODCALLTYPE IASetPrimitiveTopology(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IASetPrimitiveTopology()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IASetPrimitiveTopology(i->second,  Topology);

	return orig_vtable.IASetPrimitiveTopology(This,  Topology);
}

static void STDMETHODCALLTYPE VSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSSetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSSetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.VSSetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE VSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSSetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSSetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.VSSetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE Begin(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Begin()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Begin(i->second,  pAsync);

	return orig_vtable.Begin(This,  pAsync);
}

static void STDMETHODCALLTYPE End(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::End()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->End(i->second,  pAsync);

	return orig_vtable.End(This,  pAsync);
}

static HRESULT STDMETHODCALLTYPE GetData(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync,
		/* [annotation] */
		__out_bcount_opt( DataSize )  void *pData,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in  UINT GetDataFlags)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetData()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetData(i->second,  pAsync, pData, DataSize, GetDataFlags);

	return orig_vtable.GetData(This,  pAsync, pData, DataSize, GetDataFlags);
}

static void STDMETHODCALLTYPE SetPredication(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11Predicate *pPredicate,
		/* [annotation] */
		__in  BOOL PredicateValue)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::SetPredication()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->SetPredication(i->second,  pPredicate, PredicateValue);

	return orig_vtable.SetPredication(This,  pPredicate, PredicateValue);
}

static void STDMETHODCALLTYPE GSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSSetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSSetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.GSSetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE GSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSSetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSSetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.GSSetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE OMSetRenderTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */
		__in_ecount_opt(NumViews)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */
		__in_opt  ID3D11DepthStencilView *pDepthStencilView)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMSetRenderTargets()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMSetRenderTargets(i->second,  NumViews, ppRenderTargetViews, pDepthStencilView);

	return orig_vtable.OMSetRenderTargets(This,  NumViews, ppRenderTargetViews, pDepthStencilView);
}

static void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT NumRTVs,
		/* [annotation] */
		__in_ecount_opt(NumRTVs)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */
		__in_opt  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
		/* [annotation] */
		__in  UINT NumUAVs,
		/* [annotation] */
		__in_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */
		__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMSetRenderTargetsAndUnorderedAccessViews()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMSetRenderTargetsAndUnorderedAccessViews(i->second,  NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);

	return orig_vtable.OMSetRenderTargetsAndUnorderedAccessViews(This,  NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

static void STDMETHODCALLTYPE OMSetBlendState(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11BlendState *pBlendState,
		/* [annotation] */
		__in_opt  const FLOAT BlendFactor[ 4 ],
		/* [annotation] */
		__in  UINT SampleMask)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMSetBlendState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMSetBlendState(i->second,  pBlendState, BlendFactor, SampleMask);

	return orig_vtable.OMSetBlendState(This,  pBlendState, BlendFactor, SampleMask);
}

static void STDMETHODCALLTYPE OMSetDepthStencilState(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11DepthStencilState *pDepthStencilState,
		/* [annotation] */
		__in  UINT StencilRef)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMSetDepthStencilState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMSetDepthStencilState(i->second,  pDepthStencilState, StencilRef);

	return orig_vtable.OMSetDepthStencilState(This,  pDepthStencilState, StencilRef);
}

static void STDMETHODCALLTYPE SOSetTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
		/* [annotation] */
		__in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::SOSetTargets()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->SOSetTargets(i->second,  NumBuffers, ppSOTargets, pOffsets);

	return orig_vtable.SOSetTargets(This,  NumBuffers, ppSOTargets, pOffsets);
}
static void STDMETHODCALLTYPE DrawAuto(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DrawAuto()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DrawAuto(i->second);

	return orig_vtable.DrawAuto(This);
}

static void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DrawIndexedInstancedIndirect()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DrawIndexedInstancedIndirect(i->second,  pBufferForArgs, AlignedByteOffsetForArgs);

	return orig_vtable.DrawIndexedInstancedIndirect(This,  pBufferForArgs, AlignedByteOffsetForArgs);
}

static void STDMETHODCALLTYPE DrawInstancedIndirect(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DrawInstancedIndirect()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DrawInstancedIndirect(i->second,  pBufferForArgs, AlignedByteOffsetForArgs);

	return orig_vtable.DrawInstancedIndirect(This,  pBufferForArgs, AlignedByteOffsetForArgs);
}

static void STDMETHODCALLTYPE Dispatch(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT ThreadGroupCountX,
		/* [annotation] */
		__in  UINT ThreadGroupCountY,
		/* [annotation] */
		__in  UINT ThreadGroupCountZ)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Dispatch()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Dispatch(i->second,  ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	return orig_vtable.Dispatch(This,  ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

static void STDMETHODCALLTYPE DispatchIndirect(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DispatchIndirect()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DispatchIndirect(i->second,  pBufferForArgs, AlignedByteOffsetForArgs);

	return orig_vtable.DispatchIndirect(This,  pBufferForArgs, AlignedByteOffsetForArgs);
}

static void STDMETHODCALLTYPE RSSetState(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11RasterizerState *pRasterizerState)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::RSSetState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->RSSetState(i->second,  pRasterizerState);

	return orig_vtable.RSSetState(This,  pRasterizerState);
}

static void STDMETHODCALLTYPE RSSetViewports(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		/* [annotation] */
		__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::RSSetViewports()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->RSSetViewports(i->second,  NumViewports, pViewports);

	return orig_vtable.RSSetViewports(This,  NumViewports, pViewports);
}

static void STDMETHODCALLTYPE RSSetScissorRects(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		/* [annotation] */
		__in_ecount_opt(NumRects)  const D3D11_RECT *pRects)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::RSSetScissorRects()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->RSSetScissorRects(i->second,  NumRects, pRects);

	return orig_vtable.RSSetScissorRects(This,  NumRects, pRects);
}

static void STDMETHODCALLTYPE CopySubresourceRegion(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in  UINT DstX,
		/* [annotation] */
		__in  UINT DstY,
		/* [annotation] */
		__in  UINT DstZ,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */
		__in  UINT SrcSubresource,
		/* [annotation] */
		__in_opt  const D3D11_BOX *pSrcBox)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CopySubresourceRegion()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CopySubresourceRegion(i->second,  pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);

	return orig_vtable.CopySubresourceRegion(This,  pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}

static void STDMETHODCALLTYPE CopyResource(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CopyResource()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CopyResource(i->second,  pDstResource, pSrcResource);

	return orig_vtable.CopyResource(This,  pDstResource, pSrcResource);
}

static void STDMETHODCALLTYPE UpdateSubresource(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in_opt  const D3D11_BOX *pDstBox,
		/* [annotation] */
		__in  const void *pSrcData,
		/* [annotation] */
		__in  UINT SrcRowPitch,
		/* [annotation] */
		__in  UINT SrcDepthPitch)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::UpdateSubresource()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->UpdateSubresource(i->second,  pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

	return orig_vtable.UpdateSubresource(This,  pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}

static void STDMETHODCALLTYPE CopyStructureCount(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pDstBuffer,
		/* [annotation] */
		__in  UINT DstAlignedByteOffset,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pSrcView)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CopyStructureCount()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CopyStructureCount(i->second,  pDstBuffer, DstAlignedByteOffset, pSrcView);

	return orig_vtable.CopyStructureCount(This,  pDstBuffer, DstAlignedByteOffset, pSrcView);
}

static void STDMETHODCALLTYPE ClearRenderTargetView(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11RenderTargetView *pRenderTargetView,
		/* [annotation] */
		__in  const FLOAT ColorRGBA[ 4 ])
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ClearRenderTargetView()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ClearRenderTargetView(i->second,  pRenderTargetView, ColorRGBA);

	return orig_vtable.ClearRenderTargetView(This,  pRenderTargetView, ColorRGBA);
}

static void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const UINT Values[ 4 ])
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ClearUnorderedAccessViewUint()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ClearUnorderedAccessViewUint(i->second,  pUnorderedAccessView, Values);

	return orig_vtable.ClearUnorderedAccessViewUint(This,  pUnorderedAccessView, Values);
}

static void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const FLOAT Values[ 4 ])
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ClearUnorderedAccessViewFloat()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ClearUnorderedAccessViewFloat(i->second,  pUnorderedAccessView, Values);

	return orig_vtable.ClearUnorderedAccessViewFloat(This,  pUnorderedAccessView, Values);
}

static void STDMETHODCALLTYPE ClearDepthStencilView(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in  UINT ClearFlags,
		/* [annotation] */
		__in  FLOAT Depth,
		/* [annotation] */
		__in  UINT8 Stencil)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ClearDepthStencilView()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ClearDepthStencilView(i->second,  pDepthStencilView, ClearFlags, Depth, Stencil);

	return orig_vtable.ClearDepthStencilView(This,  pDepthStencilView, ClearFlags, Depth, Stencil);
}

static void STDMETHODCALLTYPE GenerateMips(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GenerateMips()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GenerateMips(i->second,  pShaderResourceView);

	return orig_vtable.GenerateMips(This,  pShaderResourceView);
}

static void STDMETHODCALLTYPE SetResourceMinLOD(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		FLOAT MinLOD)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::SetResourceMinLOD()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->SetResourceMinLOD(i->second,  pResource, MinLOD);

	return orig_vtable.SetResourceMinLOD(This,  pResource, MinLOD);
}

static FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetResourceMinLOD()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetResourceMinLOD(i->second,  pResource);

	return orig_vtable.GetResourceMinLOD(This,  pResource);
}

static void STDMETHODCALLTYPE ResolveSubresource(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */
		__in  UINT SrcSubresource,
		/* [annotation] */
		__in  DXGI_FORMAT Format)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ResolveSubresource()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ResolveSubresource(i->second,  pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);

	return orig_vtable.ResolveSubresource(This,  pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

static void STDMETHODCALLTYPE ExecuteCommandList(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ExecuteCommandList()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ExecuteCommandList(i->second,  pCommandList, RestoreContextState);

	return orig_vtable.ExecuteCommandList(This,  pCommandList, RestoreContextState);
}

static void STDMETHODCALLTYPE HSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSSetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSSetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.HSSetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE HSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11HullShader *pHullShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSSetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSSetShader(i->second,  pHullShader, ppClassInstances, NumClassInstances);

	return orig_vtable.HSSetShader(This,  pHullShader, ppClassInstances, NumClassInstances);
}

static void STDMETHODCALLTYPE HSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSSetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSSetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.HSSetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE HSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSSetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSSetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.HSSetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE DSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSSetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSSetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.DSSetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE DSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11DomainShader *pDomainShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSSetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSSetShader(i->second,  pDomainShader, ppClassInstances, NumClassInstances);

	return orig_vtable.DSSetShader(This,  pDomainShader, ppClassInstances, NumClassInstances);
}

static void STDMETHODCALLTYPE DSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSSetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSSetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.DSSetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE DSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSSetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSSetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.DSSetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE CSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSSetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSSetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.CSSetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE CSSetUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
		/* [annotation] */
		__in_ecount(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */
		__in_ecount(NumUAVs)  const UINT *pUAVInitialCounts)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSSetUnorderedAccessViews()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSSetUnorderedAccessViews(i->second,  StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);

	return orig_vtable.CSSetUnorderedAccessViews(This,  StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

static void STDMETHODCALLTYPE CSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11ComputeShader *pComputeShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSSetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSSetShader(i->second,  pComputeShader, ppClassInstances, NumClassInstances);

	return orig_vtable.CSSetShader(This,  pComputeShader, ppClassInstances, NumClassInstances);
}

static void STDMETHODCALLTYPE CSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSSetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSSetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.CSSetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE CSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSSetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSSetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.CSSetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE VSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSGetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSGetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.VSGetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE PSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSGetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSGetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.PSGetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE PSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11PixelShader **ppPixelShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSGetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSGetShader(i->second,  ppPixelShader, ppClassInstances, pNumClassInstances);

	return orig_vtable.PSGetShader(This,  ppPixelShader, ppClassInstances, pNumClassInstances);
}

static void STDMETHODCALLTYPE PSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSGetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSGetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.PSGetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE VSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11VertexShader **ppVertexShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSGetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSGetShader(i->second,  ppVertexShader, ppClassInstances, pNumClassInstances);

	return orig_vtable.VSGetShader(This,  ppVertexShader, ppClassInstances, pNumClassInstances);
}

static void STDMETHODCALLTYPE PSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::PSGetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->PSGetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.PSGetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE IAGetInputLayout(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11InputLayout **ppInputLayout)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IAGetInputLayout()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IAGetInputLayout(i->second,  ppInputLayout);

	return orig_vtable.IAGetInputLayout(This,  ppInputLayout);
}

static void STDMETHODCALLTYPE IAGetVertexBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  UINT *pStrides,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  UINT *pOffsets)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IAGetVertexBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IAGetVertexBuffers(i->second,  StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

	return orig_vtable.IAGetVertexBuffers(This,  StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

static void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11Buffer **pIndexBuffer,
		/* [annotation] */
		__out_opt  DXGI_FORMAT *Format,
		/* [annotation] */
		__out_opt  UINT *Offset)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IAGetIndexBuffer()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IAGetIndexBuffer(i->second,  pIndexBuffer, Format, Offset);

	return orig_vtable.IAGetIndexBuffer(This,  pIndexBuffer, Format, Offset);
}

static void STDMETHODCALLTYPE GSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSGetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSGetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.GSGetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE GSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11GeometryShader **ppGeometryShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSGetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSGetShader(i->second,  ppGeometryShader, ppClassInstances, pNumClassInstances);

	return orig_vtable.GSGetShader(This,  ppGeometryShader, ppClassInstances, pNumClassInstances);
}

static void STDMETHODCALLTYPE IAGetPrimitiveTopology(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::IAGetPrimitiveTopology()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->IAGetPrimitiveTopology(i->second,  pTopology);

	return orig_vtable.IAGetPrimitiveTopology(This,  pTopology);
}

static void STDMETHODCALLTYPE VSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSGetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSGetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.VSGetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE VSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::VSGetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->VSGetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.VSGetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE GetPredication(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate,
		/* [annotation] */
		__out_opt  BOOL *pPredicateValue)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetPredication()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetPredication(i->second,  ppPredicate, pPredicateValue);

	return orig_vtable.GetPredication(This,  ppPredicate, pPredicateValue);
}

static void STDMETHODCALLTYPE GSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSGetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSGetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.GSGetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE GSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GSGetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GSGetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.GSGetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE OMGetRenderTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */
		__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMGetRenderTargets()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMGetRenderTargets(i->second,  NumViews, ppRenderTargetViews, ppDepthStencilView);

	return orig_vtable.OMGetRenderTargets(This,  NumViews, ppRenderTargetViews, ppDepthStencilView);
}

static void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumRTVs,
		/* [annotation] */
		__out_ecount_opt(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot )  UINT NumUAVs,
		/* [annotation] */
		__out_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMGetRenderTargetsAndUnorderedAccessViews()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMGetRenderTargetsAndUnorderedAccessViews(i->second,  NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);

	return orig_vtable.OMGetRenderTargetsAndUnorderedAccessViews(This,  NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}

static void STDMETHODCALLTYPE OMGetBlendState(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState,
		/* [annotation] */
		__out_opt  FLOAT BlendFactor[ 4 ],
		/* [annotation] */
		__out_opt  UINT *pSampleMask)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMGetBlendState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMGetBlendState(i->second,  ppBlendState, BlendFactor, pSampleMask);

	return orig_vtable.OMGetBlendState(This,  ppBlendState, BlendFactor, pSampleMask);
}

static void STDMETHODCALLTYPE OMGetDepthStencilState(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
		/* [annotation] */
		__out_opt  UINT *pStencilRef)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::OMGetDepthStencilState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->OMGetDepthStencilState(i->second,  ppDepthStencilState, pStencilRef);

	return orig_vtable.OMGetDepthStencilState(This,  ppDepthStencilState, pStencilRef);
}

static void STDMETHODCALLTYPE SOGetTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::SOGetTargets()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->SOGetTargets(i->second,  NumBuffers, ppSOTargets);

	return orig_vtable.SOGetTargets(This,  NumBuffers, ppSOTargets);
}

static void STDMETHODCALLTYPE RSGetState(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11RasterizerState **ppRasterizerState)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::RSGetState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->RSGetState(i->second,  ppRasterizerState);

	return orig_vtable.RSGetState(This,  ppRasterizerState);
}

static void STDMETHODCALLTYPE RSGetViewports(ID3D11DeviceContext *This,
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		/* [annotation] */
		__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::RSGetViewports()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->RSGetViewports(i->second,  pNumViewports, pViewports);

	return orig_vtable.RSGetViewports(This,  pNumViewports, pViewports);
}

static void STDMETHODCALLTYPE RSGetScissorRects(ID3D11DeviceContext *This,
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		/* [annotation] */
		__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::RSGetScissorRects()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->RSGetScissorRects(i->second,  pNumRects, pRects);

	return orig_vtable.RSGetScissorRects(This,  pNumRects, pRects);
}

static void STDMETHODCALLTYPE HSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSGetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSGetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.HSGetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE HSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11HullShader **ppHullShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSGetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSGetShader(i->second,  ppHullShader, ppClassInstances, pNumClassInstances);

	return orig_vtable.HSGetShader(This,  ppHullShader, ppClassInstances, pNumClassInstances);
}

static void STDMETHODCALLTYPE HSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSGetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSGetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.HSGetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE HSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::HSGetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->HSGetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.HSGetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE DSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSGetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSGetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.DSGetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE DSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11DomainShader **ppDomainShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSGetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSGetShader(i->second,  ppDomainShader, ppClassInstances, pNumClassInstances);

	return orig_vtable.DSGetShader(This,  ppDomainShader, ppClassInstances, pNumClassInstances);
}

static void STDMETHODCALLTYPE DSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSGetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSGetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.DSGetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE DSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::DSGetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->DSGetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.DSGetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE CSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSGetShaderResources()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSGetShaderResources(i->second,  StartSlot, NumViews, ppShaderResourceViews);

	return orig_vtable.CSGetShaderResources(This,  StartSlot, NumViews, ppShaderResourceViews);
}

static void STDMETHODCALLTYPE CSGetUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
		/* [annotation] */
		__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSGetUnorderedAccessViews()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSGetUnorderedAccessViews(i->second,  StartSlot, NumUAVs, ppUnorderedAccessViews);

	return orig_vtable.CSGetUnorderedAccessViews(This,  StartSlot, NumUAVs, ppUnorderedAccessViews);
}

static void STDMETHODCALLTYPE CSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11ComputeShader **ppComputeShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSGetShader()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSGetShader(i->second,  ppComputeShader, ppClassInstances, pNumClassInstances);

	return orig_vtable.CSGetShader(This,  ppComputeShader, ppClassInstances, pNumClassInstances);
}

static void STDMETHODCALLTYPE CSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSGetSamplers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSGetSamplers(i->second,  StartSlot, NumSamplers, ppSamplers);

	return orig_vtable.CSGetSamplers(This,  StartSlot, NumSamplers, ppSamplers);
}

static void STDMETHODCALLTYPE CSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::CSGetConstantBuffers()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->CSGetConstantBuffers(i->second,  StartSlot, NumBuffers, ppConstantBuffers);

	return orig_vtable.CSGetConstantBuffers(This,  StartSlot, NumBuffers, ppConstantBuffers);
}

static void STDMETHODCALLTYPE ClearState(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::ClearState()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->ClearState(i->second);

	return orig_vtable.ClearState(This);
}

static void STDMETHODCALLTYPE Flush(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::Flush()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->Flush(i->second);

	return orig_vtable.Flush(This);
}

static D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetType()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetType(i->second);

	return orig_vtable.GetType(This);
}

static UINT STDMETHODCALLTYPE GetContextFlags(ID3D11DeviceContext *This)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::GetContextFlags()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->GetContextFlags(i->second);

	return orig_vtable.GetContextFlags(This);
}

static HRESULT STDMETHODCALLTYPE FinishCommandList(ID3D11DeviceContext *This,
		BOOL RestoreDeferredContextState,
		/* [annotation] */
		__out_opt  ID3D11CommandList **ppCommandList)
{
	ContextMap::iterator i;

	HookDebug("HookedContext::FinishCommandList()\n");

	i = context_map.find(This);
	if (i != context_map.end())
		return i->second->lpVtbl->FinishCommandList(i->second, RestoreDeferredContextState, ppCommandList);

	return orig_vtable.FinishCommandList(This, RestoreDeferredContextState, ppCommandList);
}

static void install_hooks(ID3D11DeviceContext *context)
{
	static bool hooks_installed = false;
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	hooks_installed = true;

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface,                            context->lpVtbl->QueryInterface,                            QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef,                                    context->lpVtbl->AddRef,                                    AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release,                                   context->lpVtbl->Release,                                   Release);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice,                                 context->lpVtbl->GetDevice,                                 GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData,                            context->lpVtbl->GetPrivateData,                            GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData,                            context->lpVtbl->SetPrivateData,                            SetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateDataInterface,                   context->lpVtbl->SetPrivateDataInterface,                   SetPrivateDataInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSSetConstantBuffers,                      context->lpVtbl->VSSetConstantBuffers,                      VSSetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSSetShaderResources,                      context->lpVtbl->PSSetShaderResources,                      PSSetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSSetShader,                               context->lpVtbl->PSSetShader,                               PSSetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSSetSamplers,                             context->lpVtbl->PSSetSamplers,                             PSSetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSSetShader,                               context->lpVtbl->VSSetShader,                               VSSetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawIndexed,                               context->lpVtbl->DrawIndexed,                               DrawIndexed);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Draw,                                      context->lpVtbl->Draw,                                      Draw);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Map,                                       context->lpVtbl->Map,                                       Map);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Unmap,                                     context->lpVtbl->Unmap,                                     Unmap);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSSetConstantBuffers,                      context->lpVtbl->PSSetConstantBuffers,                      PSSetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IASetInputLayout,                          context->lpVtbl->IASetInputLayout,                          IASetInputLayout);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IASetVertexBuffers,                        context->lpVtbl->IASetVertexBuffers,                        IASetVertexBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IASetIndexBuffer,                          context->lpVtbl->IASetIndexBuffer,                          IASetIndexBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawIndexedInstanced,                      context->lpVtbl->DrawIndexedInstanced,                      DrawIndexedInstanced);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawInstanced,                             context->lpVtbl->DrawInstanced,                             DrawInstanced);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSSetConstantBuffers,                      context->lpVtbl->GSSetConstantBuffers,                      GSSetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSSetShader,                               context->lpVtbl->GSSetShader,                               GSSetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IASetPrimitiveTopology,                    context->lpVtbl->IASetPrimitiveTopology,                    IASetPrimitiveTopology);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSSetShaderResources,                      context->lpVtbl->VSSetShaderResources,                      VSSetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSSetSamplers,                             context->lpVtbl->VSSetSamplers,                             VSSetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Begin,                                     context->lpVtbl->Begin,                                     Begin);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.End,                                       context->lpVtbl->End,                                       End);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetData,                                   context->lpVtbl->GetData,                                   GetData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPredication,                            context->lpVtbl->SetPredication,                            SetPredication);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSSetShaderResources,                      context->lpVtbl->GSSetShaderResources,                      GSSetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSSetSamplers,                             context->lpVtbl->GSSetSamplers,                             GSSetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMSetRenderTargets,                        context->lpVtbl->OMSetRenderTargets,                        OMSetRenderTargets);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMSetRenderTargetsAndUnorderedAccessViews, context->lpVtbl->OMSetRenderTargetsAndUnorderedAccessViews, OMSetRenderTargetsAndUnorderedAccessViews);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMSetBlendState,                           context->lpVtbl->OMSetBlendState,                           OMSetBlendState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMSetDepthStencilState,                    context->lpVtbl->OMSetDepthStencilState,                    OMSetDepthStencilState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SOSetTargets,                              context->lpVtbl->SOSetTargets,                              SOSetTargets);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawAuto,                                  context->lpVtbl->DrawAuto,                                  DrawAuto);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawIndexedInstancedIndirect,              context->lpVtbl->DrawIndexedInstancedIndirect,              DrawIndexedInstancedIndirect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawInstancedIndirect,                     context->lpVtbl->DrawInstancedIndirect,                     DrawInstancedIndirect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Dispatch,                                  context->lpVtbl->Dispatch,                                  Dispatch);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DispatchIndirect,                          context->lpVtbl->DispatchIndirect,                          DispatchIndirect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RSSetState,                                context->lpVtbl->RSSetState,                                RSSetState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RSSetViewports,                            context->lpVtbl->RSSetViewports,                            RSSetViewports);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RSSetScissorRects,                         context->lpVtbl->RSSetScissorRects,                         RSSetScissorRects);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CopySubresourceRegion,                     context->lpVtbl->CopySubresourceRegion,                     CopySubresourceRegion);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CopyResource,                              context->lpVtbl->CopyResource,                              CopyResource);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UpdateSubresource,                         context->lpVtbl->UpdateSubresource,                         UpdateSubresource);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CopyStructureCount,                        context->lpVtbl->CopyStructureCount,                        CopyStructureCount);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ClearRenderTargetView,                     context->lpVtbl->ClearRenderTargetView,                     ClearRenderTargetView);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ClearUnorderedAccessViewUint,              context->lpVtbl->ClearUnorderedAccessViewUint,              ClearUnorderedAccessViewUint);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ClearUnorderedAccessViewFloat,             context->lpVtbl->ClearUnorderedAccessViewFloat,             ClearUnorderedAccessViewFloat);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ClearDepthStencilView,                     context->lpVtbl->ClearDepthStencilView,                     ClearDepthStencilView);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GenerateMips,                              context->lpVtbl->GenerateMips,                              GenerateMips);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetResourceMinLOD,                         context->lpVtbl->SetResourceMinLOD,                         SetResourceMinLOD);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetResourceMinLOD,                         context->lpVtbl->GetResourceMinLOD,                         GetResourceMinLOD);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ResolveSubresource,                        context->lpVtbl->ResolveSubresource,                        ResolveSubresource);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ExecuteCommandList,                        context->lpVtbl->ExecuteCommandList,                        ExecuteCommandList);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSSetShaderResources,                      context->lpVtbl->HSSetShaderResources,                      HSSetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSSetShader,                               context->lpVtbl->HSSetShader,                               HSSetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSSetSamplers,                             context->lpVtbl->HSSetSamplers,                             HSSetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSSetConstantBuffers,                      context->lpVtbl->HSSetConstantBuffers,                      HSSetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSSetShaderResources,                      context->lpVtbl->DSSetShaderResources,                      DSSetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSSetShader,                               context->lpVtbl->DSSetShader,                               DSSetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSSetSamplers,                             context->lpVtbl->DSSetSamplers,                             DSSetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSSetConstantBuffers,                      context->lpVtbl->DSSetConstantBuffers,                      DSSetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSSetShaderResources,                      context->lpVtbl->CSSetShaderResources,                      CSSetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSSetUnorderedAccessViews,                 context->lpVtbl->CSSetUnorderedAccessViews,                 CSSetUnorderedAccessViews);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSSetShader,                               context->lpVtbl->CSSetShader,                               CSSetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSSetSamplers,                             context->lpVtbl->CSSetSamplers,                             CSSetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSSetConstantBuffers,                      context->lpVtbl->CSSetConstantBuffers,                      CSSetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSGetConstantBuffers,                      context->lpVtbl->VSGetConstantBuffers,                      VSGetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSGetShaderResources,                      context->lpVtbl->PSGetShaderResources,                      PSGetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSGetShader,                               context->lpVtbl->PSGetShader,                               PSGetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSGetSamplers,                             context->lpVtbl->PSGetSamplers,                             PSGetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSGetShader,                               context->lpVtbl->VSGetShader,                               VSGetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PSGetConstantBuffers,                      context->lpVtbl->PSGetConstantBuffers,                      PSGetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IAGetInputLayout,                          context->lpVtbl->IAGetInputLayout,                          IAGetInputLayout);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IAGetVertexBuffers,                        context->lpVtbl->IAGetVertexBuffers,                        IAGetVertexBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IAGetIndexBuffer,                          context->lpVtbl->IAGetIndexBuffer,                          IAGetIndexBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSGetConstantBuffers,                      context->lpVtbl->GSGetConstantBuffers,                      GSGetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSGetShader,                               context->lpVtbl->GSGetShader,                               GSGetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.IAGetPrimitiveTopology,                    context->lpVtbl->IAGetPrimitiveTopology,                    IAGetPrimitiveTopology);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSGetShaderResources,                      context->lpVtbl->VSGetShaderResources,                      VSGetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.VSGetSamplers,                             context->lpVtbl->VSGetSamplers,                             VSGetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPredication,                            context->lpVtbl->GetPredication,                            GetPredication);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSGetShaderResources,                      context->lpVtbl->GSGetShaderResources,                      GSGetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GSGetSamplers,                             context->lpVtbl->GSGetSamplers,                             GSGetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMGetRenderTargets,                        context->lpVtbl->OMGetRenderTargets,                        OMGetRenderTargets);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMGetRenderTargetsAndUnorderedAccessViews, context->lpVtbl->OMGetRenderTargetsAndUnorderedAccessViews, OMGetRenderTargetsAndUnorderedAccessViews);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMGetBlendState,                           context->lpVtbl->OMGetBlendState,                           OMGetBlendState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OMGetDepthStencilState,                    context->lpVtbl->OMGetDepthStencilState,                    OMGetDepthStencilState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SOGetTargets,                              context->lpVtbl->SOGetTargets,                              SOGetTargets);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RSGetState,                                context->lpVtbl->RSGetState,                                RSGetState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RSGetViewports,                            context->lpVtbl->RSGetViewports,                            RSGetViewports);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RSGetScissorRects,                         context->lpVtbl->RSGetScissorRects,                         RSGetScissorRects);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSGetShaderResources,                      context->lpVtbl->HSGetShaderResources,                      HSGetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSGetShader,                               context->lpVtbl->HSGetShader,                               HSGetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSGetSamplers,                             context->lpVtbl->HSGetSamplers,                             HSGetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.HSGetConstantBuffers,                      context->lpVtbl->HSGetConstantBuffers,                      HSGetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSGetShaderResources,                      context->lpVtbl->DSGetShaderResources,                      DSGetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSGetShader,                               context->lpVtbl->DSGetShader,                               DSGetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSGetSamplers,                             context->lpVtbl->DSGetSamplers,                             DSGetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DSGetConstantBuffers,                      context->lpVtbl->DSGetConstantBuffers,                      DSGetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSGetShaderResources,                      context->lpVtbl->CSGetShaderResources,                      CSGetShaderResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSGetUnorderedAccessViews,                 context->lpVtbl->CSGetUnorderedAccessViews,                 CSGetUnorderedAccessViews);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSGetShader,                               context->lpVtbl->CSGetShader,                               CSGetShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSGetSamplers,                             context->lpVtbl->CSGetSamplers,                             CSGetSamplers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CSGetConstantBuffers,                      context->lpVtbl->CSGetConstantBuffers,                      CSGetConstantBuffers);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ClearState,                                context->lpVtbl->ClearState,                                ClearState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Flush,                                     context->lpVtbl->Flush,                                     Flush);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType,                                   context->lpVtbl->GetType,                                   GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetContextFlags,                           context->lpVtbl->GetContextFlags,                           GetContextFlags);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FinishCommandList,                         context->lpVtbl->FinishCommandList,                         FinishCommandList);
}



// This provides another ID3D11DeviceContext interface for calling the original
// functions in orig_vtable. This replaces mOrigContext in the HackerContext
// and elsewhere and gives us a way to call back into the game with minimal
// code changes.
typedef struct ID3D11DeviceContextTrampoline {
	CONST_VTBL struct ID3D11DeviceContextVtbl *lpVtbl;
	ID3D11DeviceContext *orig_this;
} ID3D11DeviceContextTrampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(ID3D11DeviceContext *This, REFIID riid, void **ppvObject)
{
	HookDebug("TrampolineContext::QueryInterface()\n");
	return orig_vtable.QueryInterface(((ID3D11DeviceContextTrampoline*)This)->orig_this, riid, ppvObject);
}
static ULONG STDMETHODCALLTYPE TrampolineAddRef(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::AddRef()\n");
	return orig_vtable.AddRef(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
static ULONG STDMETHODCALLTYPE TrampolineRelease(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::Release()\n");
	return orig_vtable.Release(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
// ID3D11DeviceChild
static void STDMETHODCALLTYPE TrampolineGetDevice(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11Device **ppDevice)
{
	HookDebug("TrampolineContext::GetDevice()\n");
	return orig_vtable.GetDevice(((ID3D11DeviceContextTrampoline*)This)->orig_this, ppDevice);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt( *pDataSize )  void *pData)
{
	HookDebug("TrampolineContext::GetPrivateData()\n");
	return orig_vtable.GetPrivateData(((ID3D11DeviceContextTrampoline*)This)->orig_this, guid, pDataSize, pData);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt( DataSize )  const void *pData)
{
	HookDebug("TrampolineContext::SetPrivateData()\n");
	return orig_vtable.SetPrivateData(((ID3D11DeviceContextTrampoline*)This)->orig_this, guid, DataSize, pData);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateDataInterface(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData)
{
	HookDebug("TrampolineContext::SetPrivateDataInterface()\n");
	return orig_vtable.SetPrivateDataInterface(((ID3D11DeviceContextTrampoline*)This)->orig_this, guid, pData);
}
// ID3D11DeviceContext
static void STDMETHODCALLTYPE TrampolineVSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	HookDebug("TrampolineContext::VSSetConstantBuffers()\n");
	return orig_vtable.VSSetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolinePSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	HookDebug("TrampolineContext::PSSetShaderResources()\n");
	return orig_vtable.PSSetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolinePSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11PixelShader *pPixelShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HookDebug("TrampolineContext::PSSetShader()\n");
	return orig_vtable.PSSetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pPixelShader, ppClassInstances, NumClassInstances);
}
static void STDMETHODCALLTYPE TrampolinePSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	HookDebug("TrampolineContext::PSSetSamplers()\n");
	return orig_vtable.PSSetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineVSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11VertexShader *pVertexShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HookDebug("TrampolineContext::VSSetShader()\n");
	return orig_vtable.VSSetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pVertexShader, ppClassInstances, NumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineDrawIndexed(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT IndexCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation)
{
	HookDebug("TrampolineContext::DrawIndexed()\n");
	return orig_vtable.DrawIndexed(((ID3D11DeviceContextTrampoline*)This)->orig_this,  IndexCount, StartIndexLocation, BaseVertexLocation);
}
static void STDMETHODCALLTYPE TrampolineDraw(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT VertexCount,
		/* [annotation] */
		__in  UINT StartVertexLocation)
{
	HookDebug("TrampolineContext::Draw()\n");
	return orig_vtable.Draw(((ID3D11DeviceContextTrampoline*)This)->orig_this,  VertexCount, StartVertexLocation);
}
static HRESULT STDMETHODCALLTYPE TrampolineMap(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource,
		/* [annotation] */
		__in  D3D11_MAP MapType,
		/* [annotation] */
		__in  UINT MapFlags,
		/* [annotation] */
		__out  D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	HookDebug("TrampolineContext::Map()\n");
	return orig_vtable.Map(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pResource, Subresource, MapType, MapFlags, pMappedResource);
}
static void STDMETHODCALLTYPE TrampolineUnmap(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource)
{
	HookDebug("TrampolineContext::Unmap()\n");
	return orig_vtable.Unmap(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pResource, Subresource);
}
static void STDMETHODCALLTYPE TrampolinePSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	HookDebug("TrampolineContext::PSSetConstantBuffers()\n");
	return orig_vtable.PSSetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineIASetInputLayout(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11InputLayout *pInputLayout)
{
	HookDebug("TrampolineContext::IASetInputLayout()\n");
	return orig_vtable.IASetInputLayout(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pInputLayout);
}
static void STDMETHODCALLTYPE TrampolineIASetVertexBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  const UINT *pStrides,
		/* [annotation] */
		__in_ecount(NumBuffers)  const UINT *pOffsets)
{
	HookDebug("TrampolineContext::IASetVertexBuffers()\n");
	return orig_vtable.IASetVertexBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
static void STDMETHODCALLTYPE TrampolineIASetIndexBuffer(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11Buffer *pIndexBuffer,
		/* [annotation] */
		__in  DXGI_FORMAT Format,
		/* [annotation] */
		__in  UINT Offset)
{
	HookDebug("TrampolineContext::IASetIndexBuffer()\n");
	return orig_vtable.IASetIndexBuffer(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pIndexBuffer, Format, Offset);
}
static void STDMETHODCALLTYPE TrampolineDrawIndexedInstanced(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT IndexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation)
{
	HookDebug("TrampolineContext::DrawIndexedInstanced()\n");
	return orig_vtable.DrawIndexedInstanced(((ID3D11DeviceContextTrampoline*)This)->orig_this,  IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
static void STDMETHODCALLTYPE TrampolineDrawInstanced(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT VertexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation)
{
	HookDebug("TrampolineContext::DrawInstanced()\n");
	return orig_vtable.DrawInstanced(((ID3D11DeviceContextTrampoline*)This)->orig_this,  VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
static void STDMETHODCALLTYPE TrampolineGSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	HookDebug("TrampolineContext::GSSetConstantBuffers()\n");
	return orig_vtable.GSSetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineGSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11GeometryShader *pShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HookDebug("TrampolineContext::GSSetShader()\n");
	return orig_vtable.GSSetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pShader, ppClassInstances, NumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineIASetPrimitiveTopology(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	HookDebug("TrampolineContext::IASetPrimitiveTopology()\n");
	return orig_vtable.IASetPrimitiveTopology(((ID3D11DeviceContextTrampoline*)This)->orig_this,  Topology);
}
static void STDMETHODCALLTYPE TrampolineVSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	HookDebug("TrampolineContext::VSSetShaderResources()\n");
	return orig_vtable.VSSetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineVSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	HookDebug("TrampolineContext::VSSetSamplers()\n");
	return orig_vtable.VSSetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineBegin(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync)
{
	HookDebug("TrampolineContext::Begin()\n");
	return orig_vtable.Begin(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pAsync);
}
static void STDMETHODCALLTYPE TrampolineEnd(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync)
{
	HookDebug("TrampolineContext::End()\n");
	return orig_vtable.End(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pAsync);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetData(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync,
		/* [annotation] */
		__out_bcount_opt( DataSize )  void *pData,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in  UINT GetDataFlags)
{
	HookDebug("TrampolineContext::GetData()\n");
	return orig_vtable.GetData(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pAsync, pData, DataSize, GetDataFlags);
}
static void STDMETHODCALLTYPE TrampolineSetPredication(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11Predicate *pPredicate,
		/* [annotation] */
		__in  BOOL PredicateValue)
{
	HookDebug("TrampolineContext::SetPredication()\n");
	return orig_vtable.SetPredication(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pPredicate, PredicateValue);
}
static void STDMETHODCALLTYPE TrampolineGSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	HookDebug("TrampolineContext::GSSetShaderResources()\n");
	return orig_vtable.GSSetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineGSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	HookDebug("TrampolineContext::GSSetSamplers()\n");
	return orig_vtable.GSSetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineOMSetRenderTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */
		__in_ecount_opt(NumViews)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */
		__in_opt  ID3D11DepthStencilView *pDepthStencilView)
{
	HookDebug("TrampolineContext::OMSetRenderTargets()\n");
	return orig_vtable.OMSetRenderTargets(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumViews, ppRenderTargetViews, pDepthStencilView);
}
static void STDMETHODCALLTYPE TrampolineOMSetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT NumRTVs,
		/* [annotation] */
		__in_ecount_opt(NumRTVs)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */
		__in_opt  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
		/* [annotation] */
		__in  UINT NumUAVs,
		/* [annotation] */
		__in_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */
		__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts)
{
	HookDebug("TrampolineContext::OMSetRenderTargetsAndUnorderedAccessViews()\n");
	return orig_vtable.OMSetRenderTargetsAndUnorderedAccessViews(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
static void STDMETHODCALLTYPE TrampolineOMSetBlendState(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11BlendState *pBlendState,
		/* [annotation] */
		__in_opt  const FLOAT BlendFactor[ 4 ],
		/* [annotation] */
		__in  UINT SampleMask)
{
	HookDebug("TrampolineContext::OMSetBlendState()\n");
	return orig_vtable.OMSetBlendState(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pBlendState, BlendFactor, SampleMask);
}
static void STDMETHODCALLTYPE TrampolineOMSetDepthStencilState(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11DepthStencilState *pDepthStencilState,
		/* [annotation] */
		__in  UINT StencilRef)
{
	HookDebug("TrampolineContext::OMSetDepthStencilState()\n");
	return orig_vtable.OMSetDepthStencilState(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDepthStencilState, StencilRef);
}
static void STDMETHODCALLTYPE TrampolineSOSetTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
		/* [annotation] */
		__in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	HookDebug("TrampolineContext::SOSetTargets()\n");
	return orig_vtable.SOSetTargets(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumBuffers, ppSOTargets, pOffsets);
}
static void STDMETHODCALLTYPE TrampolineDrawAuto(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::DrawAuto()\n");
	return orig_vtable.DrawAuto(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
static void STDMETHODCALLTYPE TrampolineDrawIndexedInstancedIndirect(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	HookDebug("TrampolineContext::DrawIndexedInstancedIndirect()\n");
	return orig_vtable.DrawIndexedInstancedIndirect(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pBufferForArgs, AlignedByteOffsetForArgs);
}
static void STDMETHODCALLTYPE TrampolineDrawInstancedIndirect(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	HookDebug("TrampolineContext::DrawInstancedIndirect()\n");
	return orig_vtable.DrawInstancedIndirect(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pBufferForArgs, AlignedByteOffsetForArgs);
}
static void STDMETHODCALLTYPE TrampolineDispatch(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  UINT ThreadGroupCountX,
		/* [annotation] */
		__in  UINT ThreadGroupCountY,
		/* [annotation] */
		__in  UINT ThreadGroupCountZ)
{
	HookDebug("TrampolineContext::Dispatch()\n");
	return orig_vtable.Dispatch(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
static void STDMETHODCALLTYPE TrampolineDispatchIndirect(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	HookDebug("TrampolineContext::DispatchIndirect()\n");
	return orig_vtable.DispatchIndirect(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pBufferForArgs, AlignedByteOffsetForArgs);
}
static void STDMETHODCALLTYPE TrampolineRSSetState(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11RasterizerState *pRasterizerState)
{
	HookDebug("TrampolineContext::RSSetState()\n");
	return orig_vtable.RSSetState(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pRasterizerState);
}
static void STDMETHODCALLTYPE TrampolineRSSetViewports(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		/* [annotation] */
		__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports)
{
	HookDebug("TrampolineContext::RSSetViewports()\n");
	return orig_vtable.RSSetViewports(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumViewports, pViewports);
}
static void STDMETHODCALLTYPE TrampolineRSSetScissorRects(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		/* [annotation] */
		__in_ecount_opt(NumRects)  const D3D11_RECT *pRects)
{
	HookDebug("TrampolineContext::RSSetScissorRects()\n");
	return orig_vtable.RSSetScissorRects(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumRects, pRects);
}
static void STDMETHODCALLTYPE TrampolineCopySubresourceRegion(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in  UINT DstX,
		/* [annotation] */
		__in  UINT DstY,
		/* [annotation] */
		__in  UINT DstZ,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */
		__in  UINT SrcSubresource,
		/* [annotation] */
		__in_opt  const D3D11_BOX *pSrcBox)
{
	HookDebug("TrampolineContext::CopySubresourceRegion()\n");
	return orig_vtable.CopySubresourceRegion(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}
static void STDMETHODCALLTYPE TrampolineCopyResource(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource)
{
	HookDebug("TrampolineContext::CopyResource()\n");
	return orig_vtable.CopyResource(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDstResource, pSrcResource);
}
static void STDMETHODCALLTYPE TrampolineUpdateSubresource(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in_opt  const D3D11_BOX *pDstBox,
		/* [annotation] */
		__in  const void *pSrcData,
		/* [annotation] */
		__in  UINT SrcRowPitch,
		/* [annotation] */
		__in  UINT SrcDepthPitch)
{
	HookDebug("TrampolineContext::UpdateSubresource()\n");
	return orig_vtable.UpdateSubresource(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
static void STDMETHODCALLTYPE TrampolineCopyStructureCount(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Buffer *pDstBuffer,
		/* [annotation] */
		__in  UINT DstAlignedByteOffset,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pSrcView)
{
	HookDebug("TrampolineContext::CopyStructureCount()\n");
	return orig_vtable.CopyStructureCount(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDstBuffer, DstAlignedByteOffset, pSrcView);
}
static void STDMETHODCALLTYPE TrampolineClearRenderTargetView(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11RenderTargetView *pRenderTargetView,
		/* [annotation] */
		__in  const FLOAT ColorRGBA[ 4 ])
{
	HookDebug("TrampolineContext::ClearRenderTargetView()\n");
	return orig_vtable.ClearRenderTargetView(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pRenderTargetView, ColorRGBA);
}
static void STDMETHODCALLTYPE TrampolineClearUnorderedAccessViewUint(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const UINT Values[ 4 ])
{
	HookDebug("TrampolineContext::ClearUnorderedAccessViewUint()\n");
	return orig_vtable.ClearUnorderedAccessViewUint(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pUnorderedAccessView, Values);
}
static void STDMETHODCALLTYPE TrampolineClearUnorderedAccessViewFloat(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const FLOAT Values[ 4 ])
{
	HookDebug("TrampolineContext::ClearUnorderedAccessViewFloat()\n");
	return orig_vtable.ClearUnorderedAccessViewFloat(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pUnorderedAccessView, Values);
}
static void STDMETHODCALLTYPE TrampolineClearDepthStencilView(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in  UINT ClearFlags,
		/* [annotation] */
		__in  FLOAT Depth,
		/* [annotation] */
		__in  UINT8 Stencil)
{
	HookDebug("TrampolineContext::ClearDepthStencilView()\n");
	return orig_vtable.ClearDepthStencilView(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDepthStencilView, ClearFlags, Depth, Stencil);
}
static void STDMETHODCALLTYPE TrampolineGenerateMips(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	HookDebug("TrampolineContext::GenerateMips()\n");
	return orig_vtable.GenerateMips(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pShaderResourceView);
}
static void STDMETHODCALLTYPE TrampolineSetResourceMinLOD(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		FLOAT MinLOD)
{
	HookDebug("TrampolineContext::SetResourceMinLOD()\n");
	return orig_vtable.SetResourceMinLOD(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pResource, MinLOD);
}
static FLOAT STDMETHODCALLTYPE TrampolineGetResourceMinLOD(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pResource)
{
	HookDebug("TrampolineContext::GetResourceMinLOD()\n");
	return orig_vtable.GetResourceMinLOD(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pResource);
}
static void STDMETHODCALLTYPE TrampolineResolveSubresource(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */
		__in  UINT SrcSubresource,
		/* [annotation] */
		__in  DXGI_FORMAT Format)
{
	HookDebug("TrampolineContext::ResolveSubresource()\n");
	return orig_vtable.ResolveSubresource(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
static void STDMETHODCALLTYPE TrampolineExecuteCommandList(ID3D11DeviceContext *This,
		/* [annotation] */
		__in  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState)
{
	HookDebug("TrampolineContext::ExecuteCommandList()\n");
	return orig_vtable.ExecuteCommandList(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pCommandList, RestoreContextState);
}
static void STDMETHODCALLTYPE TrampolineHSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	HookDebug("TrampolineContext::HSSetShaderResources()\n");
	return orig_vtable.HSSetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineHSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11HullShader *pHullShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HookDebug("TrampolineContext::HSSetShader()\n");
	return orig_vtable.HSSetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pHullShader, ppClassInstances, NumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineHSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	HookDebug("TrampolineContext::HSSetSamplers()\n");
	return orig_vtable.HSSetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineHSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	HookDebug("TrampolineContext::HSSetConstantBuffers()\n");
	return orig_vtable.HSSetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineDSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	HookDebug("TrampolineContext::DSSetShaderResources()\n");
	return orig_vtable.DSSetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineDSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11DomainShader *pDomainShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HookDebug("TrampolineContext::DSSetShader()\n");
	return orig_vtable.DSSetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pDomainShader, ppClassInstances, NumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineDSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	HookDebug("TrampolineContext::DSSetSamplers()\n");
	return orig_vtable.DSSetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineDSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	HookDebug("TrampolineContext::DSSetConstantBuffers()\n");
	return orig_vtable.DSSetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineCSSetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	HookDebug("TrampolineContext::CSSetShaderResources()\n");
	return orig_vtable.CSSetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineCSSetUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
		/* [annotation] */
		__in_ecount(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */
		__in_ecount(NumUAVs)  const UINT *pUAVInitialCounts)
{
	HookDebug("TrampolineContext::CSSetUnorderedAccessViews()\n");
	return orig_vtable.CSSetUnorderedAccessViews(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
static void STDMETHODCALLTYPE TrampolineCSSetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_opt  ID3D11ComputeShader *pComputeShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HookDebug("TrampolineContext::CSSetShader()\n");
	return orig_vtable.CSSetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pComputeShader, ppClassInstances, NumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineCSSetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	HookDebug("TrampolineContext::CSSetSamplers()\n");
	return orig_vtable.CSSetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineCSSetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	HookDebug("TrampolineContext::CSSetConstantBuffers()\n");
	return orig_vtable.CSSetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineVSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HookDebug("TrampolineContext::VSGetConstantBuffers()\n");
	return orig_vtable.VSGetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolinePSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HookDebug("TrampolineContext::PSGetShaderResources()\n");
	return orig_vtable.PSGetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolinePSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11PixelShader **ppPixelShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HookDebug("TrampolineContext::PSGetShader()\n");
	return orig_vtable.PSGetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppPixelShader, ppClassInstances, pNumClassInstances);
}
static void STDMETHODCALLTYPE TrampolinePSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HookDebug("TrampolineContext::PSGetSamplers()\n");
	return orig_vtable.PSGetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineVSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11VertexShader **ppVertexShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HookDebug("TrampolineContext::VSGetShader()\n");
	return orig_vtable.VSGetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppVertexShader, ppClassInstances, pNumClassInstances);
}
static void STDMETHODCALLTYPE TrampolinePSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HookDebug("TrampolineContext::PSGetConstantBuffers()\n");
	return orig_vtable.PSGetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineIAGetInputLayout(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11InputLayout **ppInputLayout)
{
	HookDebug("TrampolineContext::IAGetInputLayout()\n");
	return orig_vtable.IAGetInputLayout(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppInputLayout);
}
static void STDMETHODCALLTYPE TrampolineIAGetVertexBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  UINT *pStrides,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  UINT *pOffsets)
{
	HookDebug("TrampolineContext::IAGetVertexBuffers()\n");
	return orig_vtable.IAGetVertexBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
static void STDMETHODCALLTYPE TrampolineIAGetIndexBuffer(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11Buffer **pIndexBuffer,
		/* [annotation] */
		__out_opt  DXGI_FORMAT *Format,
		/* [annotation] */
		__out_opt  UINT *Offset)
{
	HookDebug("TrampolineContext::IAGetIndexBuffer()\n");
	return orig_vtable.IAGetIndexBuffer(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pIndexBuffer, Format, Offset);
}
static void STDMETHODCALLTYPE TrampolineGSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HookDebug("TrampolineContext::GSGetConstantBuffers()\n");
	return orig_vtable.GSGetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineGSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11GeometryShader **ppGeometryShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HookDebug("TrampolineContext::GSGetShader()\n");
	return orig_vtable.GSGetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppGeometryShader, ppClassInstances, pNumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineIAGetPrimitiveTopology(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	HookDebug("TrampolineContext::IAGetPrimitiveTopology()\n");
	return orig_vtable.IAGetPrimitiveTopology(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pTopology);
}
static void STDMETHODCALLTYPE TrampolineVSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HookDebug("TrampolineContext::VSGetShaderResources()\n");
	return orig_vtable.VSGetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineVSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HookDebug("TrampolineContext::VSGetSamplers()\n");
	return orig_vtable.VSGetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineGetPredication(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate,
		/* [annotation] */
		__out_opt  BOOL *pPredicateValue)
{
	HookDebug("TrampolineContext::GetPredication()\n");
	return orig_vtable.GetPredication(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppPredicate, pPredicateValue);
}
static void STDMETHODCALLTYPE TrampolineGSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HookDebug("TrampolineContext::GSGetShaderResources()\n");
	return orig_vtable.GSGetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineGSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HookDebug("TrampolineContext::GSGetSamplers()\n");
	return orig_vtable.GSGetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineOMGetRenderTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */
		__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	HookDebug("TrampolineContext::OMGetRenderTargets()\n");
	return orig_vtable.OMGetRenderTargets(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumViews, ppRenderTargetViews, ppDepthStencilView);
}
static void STDMETHODCALLTYPE TrampolineOMGetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumRTVs,
		/* [annotation] */
		__out_ecount_opt(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot )  UINT NumUAVs,
		/* [annotation] */
		__out_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	HookDebug("TrampolineContext::OMGetRenderTargetsAndUnorderedAccessViews()\n");
	return orig_vtable.OMGetRenderTargetsAndUnorderedAccessViews(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}
static void STDMETHODCALLTYPE TrampolineOMGetBlendState(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState,
		/* [annotation] */
		__out_opt  FLOAT BlendFactor[ 4 ],
		/* [annotation] */
		__out_opt  UINT *pSampleMask)
{
	HookDebug("TrampolineContext::OMGetBlendState()\n");
	return orig_vtable.OMGetBlendState(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppBlendState, BlendFactor, pSampleMask);
}
static void STDMETHODCALLTYPE TrampolineOMGetDepthStencilState(ID3D11DeviceContext *This,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
		/* [annotation] */
		__out_opt  UINT *pStencilRef)
{
	HookDebug("TrampolineContext::OMGetDepthStencilState()\n");
	return orig_vtable.OMGetDepthStencilState(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppDepthStencilState, pStencilRef);
}
static void STDMETHODCALLTYPE TrampolineSOGetTargets(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets)
{
	HookDebug("TrampolineContext::SOGetTargets()\n");
	return orig_vtable.SOGetTargets(((ID3D11DeviceContextTrampoline*)This)->orig_this,  NumBuffers, ppSOTargets);
}
static void STDMETHODCALLTYPE TrampolineRSGetState(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11RasterizerState **ppRasterizerState)
{
	HookDebug("TrampolineContext::RSGetState()\n");
	return orig_vtable.RSGetState(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppRasterizerState);
}
static void STDMETHODCALLTYPE TrampolineRSGetViewports(ID3D11DeviceContext *This,
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		/* [annotation] */
		__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports)
{
	HookDebug("TrampolineContext::RSGetViewports()\n");
	return orig_vtable.RSGetViewports(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pNumViewports, pViewports);
}
static void STDMETHODCALLTYPE TrampolineRSGetScissorRects(ID3D11DeviceContext *This,
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		/* [annotation] */
		__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects)
{
	HookDebug("TrampolineContext::RSGetScissorRects()\n");
	return orig_vtable.RSGetScissorRects(((ID3D11DeviceContextTrampoline*)This)->orig_this,  pNumRects, pRects);
}
static void STDMETHODCALLTYPE TrampolineHSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HookDebug("TrampolineContext::HSGetShaderResources()\n");
	return orig_vtable.HSGetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineHSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11HullShader **ppHullShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HookDebug("TrampolineContext::HSGetShader()\n");
	return orig_vtable.HSGetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppHullShader, ppClassInstances, pNumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineHSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HookDebug("TrampolineContext::HSGetSamplers()\n");
	return orig_vtable.HSGetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineHSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HookDebug("TrampolineContext::HSGetConstantBuffers()\n");
	return orig_vtable.HSGetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineDSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HookDebug("TrampolineContext::DSGetShaderResources()\n");
	return orig_vtable.DSGetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineDSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11DomainShader **ppDomainShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HookDebug("TrampolineContext::DSGetShader()\n");
	return orig_vtable.DSGetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppDomainShader, ppClassInstances, pNumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineDSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HookDebug("TrampolineContext::DSGetSamplers()\n");
	return orig_vtable.DSGetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineDSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HookDebug("TrampolineContext::DSGetConstantBuffers()\n");
	return orig_vtable.DSGetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineCSGetShaderResources(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HookDebug("TrampolineContext::CSGetShaderResources()\n");
	return orig_vtable.CSGetShaderResources(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumViews, ppShaderResourceViews);
}
static void STDMETHODCALLTYPE TrampolineCSGetUnorderedAccessViews(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
		/* [annotation] */
		__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	HookDebug("TrampolineContext::CSGetUnorderedAccessViews()\n");
	return orig_vtable.CSGetUnorderedAccessViews(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumUAVs, ppUnorderedAccessViews);
}
static void STDMETHODCALLTYPE TrampolineCSGetShader(ID3D11DeviceContext *This,
		/* [annotation] */
		__out  ID3D11ComputeShader **ppComputeShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HookDebug("TrampolineContext::CSGetShader()\n");
	return orig_vtable.CSGetShader(((ID3D11DeviceContextTrampoline*)This)->orig_this,  ppComputeShader, ppClassInstances, pNumClassInstances);
}
static void STDMETHODCALLTYPE TrampolineCSGetSamplers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HookDebug("TrampolineContext::CSGetSamplers()\n");
	return orig_vtable.CSGetSamplers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumSamplers, ppSamplers);
}
static void STDMETHODCALLTYPE TrampolineCSGetConstantBuffers(ID3D11DeviceContext *This,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HookDebug("TrampolineContext::CSGetConstantBuffers()\n");
	return orig_vtable.CSGetConstantBuffers(((ID3D11DeviceContextTrampoline*)This)->orig_this,  StartSlot, NumBuffers, ppConstantBuffers);
}
static void STDMETHODCALLTYPE TrampolineClearState(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::ClearState()\n");
	return orig_vtable.ClearState(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
static void STDMETHODCALLTYPE TrampolineFlush(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::Flush()\n");
	return orig_vtable.Flush(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
static D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE TrampolineGetType(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::GetType()\n");
	return orig_vtable.GetType(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
static UINT STDMETHODCALLTYPE TrampolineGetContextFlags(ID3D11DeviceContext *This)
{
	HookDebug("TrampolineContext::GetContextFlags()\n");
	return orig_vtable.GetContextFlags(((ID3D11DeviceContextTrampoline*)This)->orig_this);
}
static HRESULT STDMETHODCALLTYPE TrampolineFinishCommandList(ID3D11DeviceContext *This,
		BOOL RestoreDeferredContextState,
		/* [annotation] */
		__out_opt  ID3D11CommandList **ppCommandList)
{
	HookDebug("TrampolineContext::FinishCommandList()\n");
	return orig_vtable.FinishCommandList(((ID3D11DeviceContextTrampoline*)This)->orig_this, RestoreDeferredContextState, ppCommandList);
}

static CONST_VTBL struct ID3D11DeviceContextVtbl trampoline_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineGetPrivateData,
	TrampolineSetPrivateData,
	TrampolineSetPrivateDataInterface,
	TrampolineVSSetConstantBuffers,
	TrampolinePSSetShaderResources,
	TrampolinePSSetShader,
	TrampolinePSSetSamplers,
	TrampolineVSSetShader,
	TrampolineDrawIndexed,
	TrampolineDraw,
	TrampolineMap,
	TrampolineUnmap,
	TrampolinePSSetConstantBuffers,
	TrampolineIASetInputLayout,
	TrampolineIASetVertexBuffers,
	TrampolineIASetIndexBuffer,
	TrampolineDrawIndexedInstanced,
	TrampolineDrawInstanced,
	TrampolineGSSetConstantBuffers,
	TrampolineGSSetShader,
	TrampolineIASetPrimitiveTopology,
	TrampolineVSSetShaderResources,
	TrampolineVSSetSamplers,
	TrampolineBegin,
	TrampolineEnd,
	TrampolineGetData,
	TrampolineSetPredication,
	TrampolineGSSetShaderResources,
	TrampolineGSSetSamplers,
	TrampolineOMSetRenderTargets,
	TrampolineOMSetRenderTargetsAndUnorderedAccessViews,
	TrampolineOMSetBlendState,
	TrampolineOMSetDepthStencilState,
	TrampolineSOSetTargets,
	TrampolineDrawAuto,
	TrampolineDrawIndexedInstancedIndirect,
	TrampolineDrawInstancedIndirect,
	TrampolineDispatch,
	TrampolineDispatchIndirect,
	TrampolineRSSetState,
	TrampolineRSSetViewports,
	TrampolineRSSetScissorRects,
	TrampolineCopySubresourceRegion,
	TrampolineCopyResource,
	TrampolineUpdateSubresource,
	TrampolineCopyStructureCount,
	TrampolineClearRenderTargetView,
	TrampolineClearUnorderedAccessViewUint,
	TrampolineClearUnorderedAccessViewFloat,
	TrampolineClearDepthStencilView,
	TrampolineGenerateMips,
	TrampolineSetResourceMinLOD,
	TrampolineGetResourceMinLOD,
	TrampolineResolveSubresource,
	TrampolineExecuteCommandList,
	TrampolineHSSetShaderResources,
	TrampolineHSSetShader,
	TrampolineHSSetSamplers,
	TrampolineHSSetConstantBuffers,
	TrampolineDSSetShaderResources,
	TrampolineDSSetShader,
	TrampolineDSSetSamplers,
	TrampolineDSSetConstantBuffers,
	TrampolineCSSetShaderResources,
	TrampolineCSSetUnorderedAccessViews,
	TrampolineCSSetShader,
	TrampolineCSSetSamplers,
	TrampolineCSSetConstantBuffers,
	TrampolineVSGetConstantBuffers,
	TrampolinePSGetShaderResources,
	TrampolinePSGetShader,
	TrampolinePSGetSamplers,
	TrampolineVSGetShader,
	TrampolinePSGetConstantBuffers,
	TrampolineIAGetInputLayout,
	TrampolineIAGetVertexBuffers,
	TrampolineIAGetIndexBuffer,
	TrampolineGSGetConstantBuffers,
	TrampolineGSGetShader,
	TrampolineIAGetPrimitiveTopology,
	TrampolineVSGetShaderResources,
	TrampolineVSGetSamplers,
	TrampolineGetPredication,
	TrampolineGSGetShaderResources,
	TrampolineGSGetSamplers,
	TrampolineOMGetRenderTargets,
	TrampolineOMGetRenderTargetsAndUnorderedAccessViews,
	TrampolineOMGetBlendState,
	TrampolineOMGetDepthStencilState,
	TrampolineSOGetTargets,
	TrampolineRSGetState,
	TrampolineRSGetViewports,
	TrampolineRSGetScissorRects,
	TrampolineHSGetShaderResources,
	TrampolineHSGetShader,
	TrampolineHSGetSamplers,
	TrampolineHSGetConstantBuffers,
	TrampolineDSGetShaderResources,
	TrampolineDSGetShader,
	TrampolineDSGetSamplers,
	TrampolineDSGetConstantBuffers,
	TrampolineCSGetShaderResources,
	TrampolineCSGetUnorderedAccessViews,
	TrampolineCSGetShader,
	TrampolineCSGetSamplers,
	TrampolineCSGetConstantBuffers,
	TrampolineClearState,
	TrampolineFlush,
	TrampolineGetType,
	TrampolineGetContextFlags,
	TrampolineFinishCommandList,
};

ID3D11DeviceContext* hook_context(ID3D11DeviceContext *orig_context, ID3D11DeviceContext *hacker_context)
{
	ID3D11DeviceContextTrampoline *trampoline_context = new ID3D11DeviceContextTrampoline();
	trampoline_context->lpVtbl = &trampoline_vtable;
	trampoline_context->orig_this = orig_context;

	install_hooks(orig_context);
	context_map[orig_context] = hacker_context;

	return (ID3D11DeviceContext*)trampoline_context;
}
