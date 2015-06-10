#pragma once

#include <d3d11_1.h>

#include "HackerDevice.h"
#include "Globals.h"

struct DrawContext
{
	bool skip;
	bool override;
	float oldSeparation;
	float oldConvergence;
	ID3D11PixelShader *oldPixelShader;
	ID3D11VertexShader *oldVertexShader;

	DrawContext() :
		skip(false),
		override(false),
		oldSeparation(FLT_MAX),
		oldConvergence(FLT_MAX),
		oldVertexShader(NULL),
		oldPixelShader(NULL)
	{}
};


// Forward declaration to allow circular reference between HackerContext and HackerDevice. 
// We need this to allow each to reference the other as needed.

class HackerDevice;
class HackerDevice1;


// Hierarchy:
//  HackerContext <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

// devicechild: MIDL_INTERFACE("1841e5c8-16b0-489b-bcc8-44cfb0d5deae")
// MIDL_INTERFACE("c0bfa96c-e089-44fb-8eaf-26f8796190da")
class HackerContext : public ID3D11DeviceContext 
{
private:
	ID3D11Device *mOrigDevice;
	ID3D11DeviceContext *mOrigContext;
	HackerDevice *mHackerDevice;

	// These are per-context, moved from globals.h:
	UINT64 mCurrentIndexBuffer;
	UINT64 mCurrentVertexShader;
	ID3D11VertexShader *mCurrentVertexShaderHandle;
	UINT64 mCurrentPixelShader;
	ID3D11PixelShader *mCurrentPixelShaderHandle;
	std::vector<void *> mCurrentRenderTargets;
	void *mCurrentDepthTarget;

	// These private methods are utility routines for HackerContext.
	DrawContext BeforeDraw();
	void AfterDraw(DrawContext &data);
	void ProcessShaderOverride(ShaderOverride *shaderOverride, bool isPixelShader,
		DrawContext *data,float *separationValue, float *convergenceValue);
	void DumpStereoResource(ID3D11Texture2D *resource, wchar_t *fullName);
	void DumpResource(ID3D11Resource *resource, int idx);
	void DumpRenderTargets();
	void FrameAnalysisClearRT(ID3D11RenderTargetView *target);
	ID3D11PixelShader* SwitchPSShader(ID3D11PixelShader *shader);
	ID3D11VertexShader* SwitchVSShader(ID3D11VertexShader *shader);
	void RecordDepthStencil(ID3D11DepthStencilView *target);
	void RecordShaderResourceUsage();
	void RecordRenderTargetInfo(ID3D11RenderTargetView *target, UINT view_num);
	void* RecordResourceViewStats(ID3D11ShaderResourceView *view);
	UINT64 GetTexture2DHash(ID3D11Texture2D *texture,
		bool log_new, struct ResourceInfo *resource_info);
	UINT64 GetTexture3DHash(ID3D11Texture3D *texture,
		bool log_new, struct ResourceInfo *resource_info);

public:
	HackerContext(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);

	void SetHackerDevice(HackerDevice *pDevice);
	ID3D11DeviceContext* GetOrigContext();


	//static D3D11Wrapper::ID3D11DeviceContext* GetDirect3DDeviceContext(ID3D11DeviceContext *pContext);
	//__forceinline ID3D11DeviceContext *GetD3D11DeviceContext() { return (ID3D11DeviceContext*) m_pUnk; }

	/*** IUnknown methods ***/
	//STDMETHOD_(ULONG, AddRef)(THIS);
	//STDMETHOD_(ULONG, Release)(THIS);

	HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	ULONG STDMETHODCALLTYPE AddRef(void);

	ULONG STDMETHODCALLTYPE Release(void);

	// ******************* ID3D11DeviceChild interface

	STDMETHOD_(void, GetDevice)(THIS_
		/* [annotation] */
		__out  ID3D11Device **ppDevice);

	STDMETHOD(GetPrivateData)(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt(*pDataSize)  void *pData);

	STDMETHOD(SetPrivateData)(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt(DataSize)  const void *pData);

	STDMETHOD(SetPrivateDataInterface)(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData);

	// ******************* ID3D11DeviceContext interface

	STDMETHOD_(void, VSSetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, PSSetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, PSSetShader)(THIS_
		/* [annotation] */
		__in_opt ID3D11PixelShader *pPixelShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, PSSetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, VSSetShader)(THIS_
		/* [annotation] */
		__in_opt ID3D11VertexShader *pVertexShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, DrawIndexed)(THIS_
		/* [annotation] */
		__in  UINT IndexCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation);

	STDMETHOD_(void, Draw)(THIS_
		/* [annotation] */
		__in  UINT VertexCount,
		/* [annotation] */
		__in  UINT StartVertexLocation);

	STDMETHOD(Map)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource,
		/* [annotation] */
		__in  D3D11_MAP MapType,
		/* [annotation] */
		__in  UINT MapFlags,
		/* [annotation] */
		__out D3D11_MAPPED_SUBRESOURCE *pMappedResource);

	STDMETHOD_(void, Unmap)(THIS_
		/* [annotation] */
		__in ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource);

	STDMETHOD_(void, PSSetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, IASetInputLayout)(THIS_
		/* [annotation] */
		__in_opt ID3D11InputLayout *pInputLayout);

	STDMETHOD_(void, IASetVertexBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  const UINT *pStrides,
		/* [annotation] */
		__in_ecount(NumBuffers)  const UINT *pOffsets);

	STDMETHOD_(void, IASetIndexBuffer)(THIS_
		/* [annotation] */
		__in_opt ID3D11Buffer *pIndexBuffer,
		/* [annotation] */
		__in DXGI_FORMAT Format,
		/* [annotation] */
		__in  UINT Offset);

	STDMETHOD_(void, DrawIndexedInstanced)(THIS_
		/* [annotation] */
		__in  UINT IndexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation);

	STDMETHOD_(void, DrawInstanced)(THIS_
		/* [annotation] */
		__in  UINT VertexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation);

	STDMETHOD_(void, GSSetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, GSSetShader)(THIS_
		/* [annotation] */
		__in_opt ID3D11GeometryShader *pShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, IASetPrimitiveTopology)(THIS_
		/* [annotation] */
		__in D3D11_PRIMITIVE_TOPOLOGY Topology);

	STDMETHOD_(void, VSSetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, VSSetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, Begin)(THIS_
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync);

	STDMETHOD_(void, End)(THIS_
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync);

	STDMETHOD(GetData)(THIS_
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync,
		/* [annotation] */
		__out_bcount_opt(DataSize)  void *pData,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in  UINT GetDataFlags);

	STDMETHOD_(void, SetPredication)(THIS_
		/* [annotation] */
		__in_opt ID3D11Predicate *pPredicate,
		/* [annotation] */
		__in  BOOL PredicateValue);

	STDMETHOD_(void, GSSetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, GSSetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, OMSetRenderTargets)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		/* [annotation] */
		__in_ecount_opt(NumViews) ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */
		__in_opt ID3D11DepthStencilView *pDepthStencilView);

	STDMETHOD_(void, OMSetRenderTargetsAndUnorderedAccessViews)(THIS_
		/* [annotation] */
		__in  UINT NumRTVs,
		/* [annotation] */
		__in_ecount_opt(NumRTVs) ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */
		__in_opt ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
		/* [annotation] */
		__in  UINT NumUAVs,
		/* [annotation] */
		__in_ecount_opt(NumUAVs) ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */
		__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts);

	STDMETHOD_(void, OMSetBlendState)(THIS_
		/* [annotation] */
		__in_opt  ID3D11BlendState *pBlendState,
		/* [annotation] */
		__in_opt  const FLOAT BlendFactor[4],
		/* [annotation] */
		__in  UINT SampleMask);

	STDMETHOD_(void, OMSetDepthStencilState)(THIS_
		/* [annotation] */
		__in_opt  ID3D11DepthStencilState *pDepthStencilState,
		/* [annotation] */
		__in  UINT StencilRef);

	STDMETHOD_(void, SOSetTargets)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
		/* [annotation] */
		__in_ecount_opt(NumBuffers)  const UINT *pOffsets);

	STDMETHOD_(void, DrawAuto)(THIS);

	STDMETHOD_(void, DrawIndexedInstancedIndirect)(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs);

	STDMETHOD_(void, DrawInstancedIndirect)(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs);

	STDMETHOD_(void, Dispatch)(THIS_
		/* [annotation] */
		__in  UINT ThreadGroupCountX,
		/* [annotation] */
		__in  UINT ThreadGroupCountY,
		/* [annotation] */
		__in  UINT ThreadGroupCountZ);

	STDMETHOD_(void, DispatchIndirect)(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs);

	STDMETHOD_(void, RSSetState)(THIS_
		/* [annotation] */
		__in_opt  ID3D11RasterizerState *pRasterizerState);

	STDMETHOD_(void, RSSetViewports)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		/* [annotation] */
		__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports);

	STDMETHOD_(void, RSSetScissorRects)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		/* [annotation] */
		__in_ecount_opt(NumRects)  const D3D11_RECT *pRects);

	STDMETHOD_(void, CopySubresourceRegion)(THIS_
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
		__in_opt  const D3D11_BOX *pSrcBox);

	STDMETHOD_(void, CopyResource)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource);

	STDMETHOD_(void, UpdateSubresource)(THIS_
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
		__in  UINT SrcDepthPitch);

	STDMETHOD_(void, CopyStructureCount)(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pDstBuffer,
		/* [annotation] */
		__in  UINT DstAlignedByteOffset,
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pSrcView);

	STDMETHOD_(void, ClearRenderTargetView)(THIS_
		/* [annotation] */
		__in  ID3D11RenderTargetView *pRenderTargetView,
		/* [annotation] */
		__in  const FLOAT ColorRGBA[4]);

	STDMETHOD_(void, ClearUnorderedAccessViewUint)(THIS_
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const UINT Values[4]);

	STDMETHOD_(void, ClearUnorderedAccessViewFloat)(THIS_
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const FLOAT Values[4]);

	STDMETHOD_(void, ClearDepthStencilView)(THIS_
		/* [annotation] */
		__in  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in  UINT ClearFlags,
		/* [annotation] */
		__in  FLOAT Depth,
		/* [annotation] */
		__in  UINT8 Stencil);

	STDMETHOD_(void, GenerateMips)(THIS_
		/* [annotation] */
		__in  ID3D11ShaderResourceView *pShaderResourceView);

	STDMETHOD_(void, SetResourceMinLOD)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		FLOAT MinLOD);

	STDMETHOD_(FLOAT, GetResourceMinLOD)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource);

	STDMETHOD_(void, ResolveSubresource)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  UINT DstSubresource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */
		__in  UINT SrcSubresource,
		/* [annotation] */
		__in  DXGI_FORMAT Format);

	STDMETHOD_(void, ExecuteCommandList)(THIS_
		/* [annotation] */
		__in  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState);

	STDMETHOD_(void, HSSetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, HSSetShader)(THIS_
		/* [annotation] */
		__in_opt  ID3D11HullShader *pHullShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, HSSetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, HSSetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, DSSetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, DSSetShader)(THIS_
		/* [annotation] */
		__in_opt  ID3D11DomainShader *pDomainShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, DSSetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, DSSetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, CSSetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, CSSetUnorderedAccessViews)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
		/* [annotation] */
		__in_ecount(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */
		__in_ecount(NumUAVs)  const UINT *pUAVInitialCounts);

	STDMETHOD_(void, CSSetShader)(THIS_
		/* [annotation] */
		__in_opt  ID3D11ComputeShader *pComputeShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, CSSetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, CSSetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, VSGetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, PSGetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, PSGetShader)(THIS_
		/* [annotation] */
		__out  ID3D11PixelShader **ppPixelShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, PSGetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, VSGetShader)(THIS_
		/* [annotation] */
		__out  ID3D11VertexShader **ppVertexShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, PSGetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, IAGetInputLayout)(THIS_
		/* [annotation] */
		__out  ID3D11InputLayout **ppInputLayout);

	STDMETHOD_(void, IAGetVertexBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  UINT *pStrides,
		/* [annotation] */
		__out_ecount_opt(NumBuffers)  UINT *pOffsets);

	STDMETHOD_(void, IAGetIndexBuffer)(THIS_
		/* [annotation] */
		__out_opt  ID3D11Buffer **pIndexBuffer,
		/* [annotation] */
		__out_opt  DXGI_FORMAT *Format,
		/* [annotation] */
		__out_opt  UINT *Offset);

	STDMETHOD_(void, GSGetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, GSGetShader)(THIS_
		/* [annotation] */
		__out  ID3D11GeometryShader **ppGeometryShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, IAGetPrimitiveTopology)(THIS_
		/* [annotation] */
		__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology);

	STDMETHOD_(void, VSGetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, VSGetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, GetPredication)(THIS_
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate,
		/* [annotation] */
		__out_opt  BOOL *pPredicateValue);

	STDMETHOD_(void, GSGetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, GSGetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, OMGetRenderTargets)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		/* [annotation] */
		__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView);

	STDMETHOD_(void, OMGetRenderTargetsAndUnorderedAccessViews)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumRTVs,
		/* [annotation] */
		__out_ecount_opt(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView,
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot)  UINT NumUAVs,
		/* [annotation] */
		__out_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

	STDMETHOD_(void, OMGetBlendState)(THIS_
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState,
		/* [annotation] */
		__out_opt  FLOAT BlendFactor[4],
		/* [annotation] */
		__out_opt  UINT *pSampleMask);

	STDMETHOD_(void, OMGetDepthStencilState)(THIS_
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
		/* [annotation] */
		__out_opt  UINT *pStencilRef);

	STDMETHOD_(void, SOGetTargets)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets);

	STDMETHOD_(void, RSGetState)(THIS_
		/* [annotation] */
		__out  ID3D11RasterizerState **ppRasterizerState);

	STDMETHOD_(void, RSGetViewports)(THIS_
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		/* [annotation] */
		__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports);

	STDMETHOD_(void, RSGetScissorRects)(THIS_
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		/* [annotation] */
		__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects);

	STDMETHOD_(void, HSGetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, HSGetShader)(THIS_
		/* [annotation] */
		__out  ID3D11HullShader **ppHullShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, HSGetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, HSGetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, DSGetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, DSGetShader)(THIS_
		/* [annotation] */
		__out  ID3D11DomainShader **ppDomainShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, DSGetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, DSGetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, CSGetShaderResources)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, CSGetUnorderedAccessViews)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
		/* [annotation] */
		__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

	STDMETHOD_(void, CSGetShader)(THIS_
		/* [annotation] */
		__out  ID3D11ComputeShader **ppComputeShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, CSGetSamplers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, CSGetConstantBuffers)(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, ClearState)(THIS);

	STDMETHOD_(void, Flush)(THIS);

	STDMETHOD_(D3D11_DEVICE_CONTEXT_TYPE, GetType)(THIS);

	STDMETHOD_(UINT, GetContextFlags)(THIS);

	STDMETHOD(FinishCommandList)(THIS_
		BOOL RestoreDeferredContextState,
		/* [annotation] */
		__out_opt  ID3D11CommandList **ppCommandList);

};

// -----------------------------------------------------------------------------

class HackerContext1: public HackerContext
{
private:
	ID3D11Device1 *mOrigDevice1;
	ID3D11DeviceContext1 *mOrigContext1;

public:
	HackerContext1(ID3D11Device1 *pDevice, ID3D11DeviceContext1 *pContext1);


	void STDMETHODCALLTYPE CopySubresourceRegion1(
		/* [annotation] */
		_In_  ID3D11Resource *pDstResource,
		/* [annotation] */
		_In_  UINT DstSubresource,
		/* [annotation] */
		_In_  UINT DstX,
		/* [annotation] */
		_In_  UINT DstY,
		/* [annotation] */
		_In_  UINT DstZ,
		/* [annotation] */
		_In_  ID3D11Resource *pSrcResource,
		/* [annotation] */
		_In_  UINT SrcSubresource,
		/* [annotation] */
		_In_opt_  const D3D11_BOX *pSrcBox,
		/* [annotation] */
		_In_  UINT CopyFlags);

	void STDMETHODCALLTYPE UpdateSubresource1(
		/* [annotation] */
		_In_  ID3D11Resource *pDstResource,
		/* [annotation] */
		_In_  UINT DstSubresource,
		/* [annotation] */
		_In_opt_  const D3D11_BOX *pDstBox,
		/* [annotation] */
		_In_  const void *pSrcData,
		/* [annotation] */
		_In_  UINT SrcRowPitch,
		/* [annotation] */
		_In_  UINT SrcDepthPitch,
		/* [annotation] */
		_In_  UINT CopyFlags);

	void STDMETHODCALLTYPE DiscardResource(
		/* [annotation] */
		_In_  ID3D11Resource *pResource);

	void STDMETHODCALLTYPE DiscardView(
		/* [annotation] */
		_In_  ID3D11View *pResourceView);

	void STDMETHODCALLTYPE VSSetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE HSSetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE DSSetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE GSSetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE PSSetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE CSSetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		/* [annotation] */
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE VSGetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE HSGetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE DSGetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE GSGetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE PSGetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE CSGetConstantBuffers1(
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		/* [annotation] */
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE SwapDeviceContextState(
		/* [annotation] */
		_In_  ID3DDeviceContextState *pState,
		/* [annotation] */
		_Out_opt_  ID3DDeviceContextState **ppPreviousState);

	void STDMETHODCALLTYPE ClearView(
		/* [annotation] */
		_In_  ID3D11View *pView,
		/* [annotation] */
		_In_  const FLOAT Color[4],
		/* [annotation] */
		_In_reads_opt_(NumRects)  const D3D11_RECT *pRect,
		UINT NumRects);

	void STDMETHODCALLTYPE DiscardView1(
		/* [annotation] */
		_In_  ID3D11View *pResourceView,
		/* [annotation] */
		_In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
		UINT NumRects);
};
