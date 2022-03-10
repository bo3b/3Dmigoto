#pragma once

#include <d3d11_1.h>
#include <INITGUID.h>

#include "DrawCallInfo.h"

#include "CommandList.hpp"

#include "HackerDevice.hpp"
//#include "ResourceHash.hpp"
#include "Globals.h"

// {A3046B1E-336B-4D90-9FD6-234BC09B8687}
DEFINE_GUID(IID_HackerContext, 0xa3046b1e, 0x336b, 0x4d90, 0x9f, 0xd6, 0x23, 0x4b, 0xc0, 0x9b, 0x86, 0x87);

// Self forward reference for the factory interface.
class HackerContext;

HackerContext* HackerContextFactory(ID3D11Device1* device1, ID3D11DeviceContext1* context1);

// Forward declaration to allow circular reference between HackerContext and HackerDevice.
// We need this to allow each to reference the other as needed.

class HackerDevice;

enum class FrameAnalysisOptions;
struct ShaderOverride;

struct draw_context
{
    float               old_separation;
    ID3D11PixelShader*  old_pixel_shader;
    ID3D11VertexShader* old_vertex_shader;
    CommandList*        post_commands[5];
    DrawCallInfo        call_info;

    draw_context(DrawCall type, UINT vertex_count, UINT index_count, UINT instance_count, UINT first_vertex, UINT first_index, UINT first_instance, ID3D11Buffer** indirect_buffer, UINT args_offset) :
        old_separation(FLT_MAX),
        old_pixel_shader(nullptr),
        old_vertex_shader(nullptr),
        call_info(type, vertex_count, index_count, instance_count, first_vertex, first_index, first_instance, indirect_buffer, args_offset)
    {
        memset(post_commands, 0, sizeof(post_commands));
    }
};

struct dispatch_context
{
    CommandList* post_commands;
    DrawCallInfo call_info;

    dispatch_context(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) :
        post_commands(nullptr),
        call_info(DrawCall::Dispatch, 0, 0, 0, 0, 0, 0, nullptr, 0, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ)
    {}

    dispatch_context(ID3D11Buffer** indirect_buffer, UINT args_offset) :
        post_commands(nullptr),
        call_info(DrawCall::DispatchIndirect, 0, 0, 0, 0, 0, 0, indirect_buffer, args_offset)
    {}
};

// These are per-context so we shouldn't need locks
struct mapped_resource_info
{
    D3D11_MAPPED_SUBRESOURCE map;
    bool                     mapped_writable;
    void*                    orig_pData;
    size_t                   size;

    mapped_resource_info() :
        map(),
        mapped_writable(false),
        orig_pData(nullptr),
        size(0)
    {}
};

// 1-6-18:  Current approach will be to only create one level of wrapping,
// specifically HackerDevice and HackerContext, based on the ID3D11Device1,
// and ID3D11DeviceContext1.  ID3D11Device1/ID3D11DeviceContext1 is supported
// on Win7+platform_update, and thus is a superset of what we need.  By
// using the highest level object supported, we can kill off a lot of conditional
// code that just complicates things.
//
// The ID3D11DeviceContext1 will be supported on all OS except Win7 minus the
// platform_update.  In that scenario, we will save a reference to the
// ID3D11DeviceContext object instead, but store it and wrap it in HackerContext.
//
// Specifically decided to not name everything *1, because frankly that is
// was an awful choice on Microsoft's part to begin with.  Meaningless number
// completely unrelated to version/revision or functionality.  Bad.
// We will use the *1 notation for object names that are specific types,
// like the origContext1 to avoid misleading types.
//
// Any HackerDevice will be the superset object ID3D11DeviceContext1 in all cases
// except for Win7 missing the evil platform_update.

// Hierarchy:
//  HackerContext <- ID3D11DeviceContext1 <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

class HackerContext : public ID3D11DeviceContext1
{
private:
    ID3D11Device1*        origDevice1;
    ID3D11DeviceContext1* origContext1;
    ID3D11DeviceContext1* realOrigContext1;
    HackerDevice*         hackerDevice;

    // These are per-context, moved from globals.h:
    uint32_t                     currentVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    uint32_t                     currentIndexBuffer;  // Only valid while hunting=1
    std::vector<ID3D11Resource*> currentRenderTargets;
    ID3D11Resource*              currentDepthTarget;
    UINT                         currentPSUAVStartSlot;
    UINT                         currentPSNumUAVs;

    // Used for deny_cpu_read, track_texture_updates and constant buffer matching
    typedef std::unordered_map<ID3D11Resource*, mapped_resource_info> MappedResources;
    MappedResources                                                   mappedResources;

    // These private methods are utility routines for HackerContext.
    void BeforeDraw(draw_context& data);
    void AfterDraw(draw_context& data);
    bool BeforeDispatch(dispatch_context* context);
    void AfterDispatch(dispatch_context* context);

    template <
        class ID3D11Shader,
        void (__stdcall ID3D11DeviceContext::*GetShaderVS2013BUGWORKAROUND)(ID3D11Shader**, ID3D11ClassInstance**, UINT*),
        void (__stdcall ID3D11DeviceContext::*SetShaderVS2013BUGWORKAROUND)(ID3D11Shader*, ID3D11ClassInstance* const*, UINT),
        HRESULT (__stdcall ID3D11Device::*CreateShader)(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11Shader**)>
    void DeferredShaderReplacement(ID3D11DeviceChild* shader, UINT64 hash, wchar_t* shader_type);

    void DeferredShaderReplacementBeforeDraw();
    void DeferredShaderReplacementBeforeDispatch();

    bool ExpandRegionCopy(ID3D11Resource* dst_resource, UINT DstX, UINT DstY, ID3D11Resource* src_resource, const D3D11_BOX* src_box, UINT* replace_DstX, D3D11_BOX* replace_box);
    bool MapDenyCPURead(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
    void TrackAndDivertMap(HRESULT map_hr, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
    void TrackAndDivertUnmap(ID3D11Resource* pResource, UINT Subresource);

    void                ProcessShaderOverride(ShaderOverride* shader_override, bool is_pixel_shader, draw_context* data);
    ID3D11PixelShader*  SwitchPSShader(ID3D11PixelShader* shader);
    ID3D11VertexShader* SwitchVSShader(ID3D11VertexShader* shader);
    void                RecordDepthStencil(ID3D11DepthStencilView* target);

    template <
        void (__stdcall ID3D11DeviceContext::*GetShaderResources)(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)>
    void RecordShaderResourceUsage(std::map<UINT64, ShaderInfoData>& shader_info, UINT64 current_shader);

    void            _RecordShaderResourceUsage(ShaderInfoData* shader_info, ID3D11ShaderResourceView* views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]);
    void            RecordGraphicsShaderStats();
    void            RecordComputeShaderStats();
    void            RecordPeerShaders(std::set<UINT64>* peer_shaders, UINT64 shader_hash);
    void            RecordRenderTargetInfo(ID3D11RenderTargetView* target, UINT view_num);
    ID3D11Resource* RecordResourceViewStats(ID3D11View* view, std::set<uint32_t>* resource_info);

    // Templates to reduce duplicated code:
    template <
        class ID3D11Shader,
        void (__stdcall ID3D11DeviceContext::*OrigSetShader)(ID3D11Shader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)>
    void STDMETHODCALLTYPE SetShader(
        _In_opt_ ID3D11Shader*                                        pShader,
        _In_reads_opt_(NumClassInstances) ID3D11ClassInstance* const* ppClassInstances,
        UINT                                                          NumClassInstances,
        std::set<UINT64>*                                             visited_shaders,
        UINT64                                                        selected_shader,
        UINT64*                                                       current_shader_hash,
        ID3D11Shader**                                                current_shader_handle);

    template <
        void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)>
    void BindStereoResources();

    template <
        void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)>
    void SetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);

protected:
    // Allow FrameAnalysisContext access to these as an interim measure
    // until it has been further decoupled from HackerContext. Be wary of
    // relying on these - they will be zero in release mode with no
    // ShaderOverrides / ShaderRegex:
    UINT64 currentVertexShader;
    UINT64 currentHullShader;
    UINT64 currentDomainShader;
    UINT64 currentGeometryShader;
    UINT64 currentPixelShader;
    UINT64 currentComputeShader;

public:
    HackerContext(ID3D11Device1* device1, ID3D11DeviceContext1* context1);

    void                  SetHackerDevice(HackerDevice* hacker_device);
    HackerDevice*         GetHackerDevice();
    void                  Bind3DMigotoResources();
    void                  InitIniParams();
    ID3D11DeviceContext1* GetPossiblyHookedOrigContext1();
    ID3D11DeviceContext1* GetPassThroughOrigContext1();
    void                  HookContext();

    // public to allow CommandList access
    virtual void FrameAnalysisLog(char* fmt, ...) {};
    virtual void FrameAnalysisTrigger(FrameAnalysisOptions new_options) {};
    virtual void FrameAnalysisDump(ID3D11Resource* resource, FrameAnalysisOptions options, const wchar_t* target, DXGI_FORMAT format, UINT stride, UINT offset) {};

    // These are the shaders the game has set, which may be different from
    // the ones we have bound to the pipeline:
    ID3D11VertexShader*   currentVertexShaderHandle;
    ID3D11PixelShader*    currentPixelShaderHandle;
    ID3D11ComputeShader*  currentComputeShaderHandle;
    ID3D11GeometryShader* currentGeometryShaderHandle;
    ID3D11DomainShader*   currentDomainShaderHandle;
    ID3D11HullShader*     currentHullShaderHandle;

    // clang-format off
    // For all the override function definitions, let's disable the clang-format, in order to keep the
    // original formatting from the original Microsoft DX11 functions.

    /*** IUnknown methods ***/

    HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

    ULONG STDMETHODCALLTYPE AddRef(void);

    ULONG STDMETHODCALLTYPE Release(void);


    /** ID3D11DeviceChild **/

    void STDMETHODCALLTYPE GetDevice(
        /* [annotation] */
        _Out_  ID3D11Device **ppDevice);

    HRESULT STDMETHODCALLTYPE GetPrivateData(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _Inout_  UINT *pDataSize,
        /* [annotation] */
        _Out_writes_bytes_opt_(*pDataSize)  void *pData);

    HRESULT STDMETHODCALLTYPE SetPrivateData(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_reads_bytes_opt_(DataSize)  const void *pData);

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        /* [annotation] */
        _In_  REFGUID guid,
        /* [annotation] */
        _In_opt_  const IUnknown *pData);


    /** ID3D11DeviceContext **/

    void STDMETHODCALLTYPE VSSetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

    void STDMETHODCALLTYPE PSSetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

    void STDMETHODCALLTYPE PSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11PixelShader *pPixelShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);

    void STDMETHODCALLTYPE PSSetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

    void STDMETHODCALLTYPE VSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11VertexShader *pVertexShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);

    void STDMETHODCALLTYPE DrawIndexed(
        /* [annotation] */
        _In_  UINT IndexCount,
        /* [annotation] */
        _In_  UINT StartIndexLocation,
        /* [annotation] */
        _In_  INT BaseVertexLocation);

    void STDMETHODCALLTYPE Draw(
        /* [annotation] */
        _In_  UINT VertexCount,
        /* [annotation] */
        _In_  UINT StartVertexLocation);

    HRESULT STDMETHODCALLTYPE Map(
        /* [annotation] */
        _In_  ID3D11Resource *pResource,
        /* [annotation] */
        _In_  UINT Subresource,
        /* [annotation] */
        _In_  D3D11_MAP MapType,
        /* [annotation] */
        _In_  UINT MapFlags,
        /* [annotation] */
        _Out_  D3D11_MAPPED_SUBRESOURCE *pMappedResource);

    void STDMETHODCALLTYPE Unmap(
        /* [annotation] */
        _In_  ID3D11Resource *pResource,
        /* [annotation] */
        _In_  UINT Subresource);

    void STDMETHODCALLTYPE PSSetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

    void STDMETHODCALLTYPE IASetInputLayout(
        /* [annotation] */
        _In_opt_  ID3D11InputLayout *pInputLayout);

    void STDMETHODCALLTYPE IASetVertexBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  const UINT *pStrides,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  const UINT *pOffsets);

    void STDMETHODCALLTYPE IASetIndexBuffer(
        /* [annotation] */
        _In_opt_  ID3D11Buffer *pIndexBuffer,
        /* [annotation] */
        _In_  DXGI_FORMAT Format,
        /* [annotation] */
        _In_  UINT Offset);

    void STDMETHODCALLTYPE DrawIndexedInstanced(
        /* [annotation] */
        _In_  UINT IndexCountPerInstance,
        /* [annotation] */
        _In_  UINT InstanceCount,
        /* [annotation] */
        _In_  UINT StartIndexLocation,
        /* [annotation] */
        _In_  INT BaseVertexLocation,
        /* [annotation] */
        _In_  UINT StartInstanceLocation);

    void STDMETHODCALLTYPE DrawInstanced(
        /* [annotation] */
        _In_  UINT VertexCountPerInstance,
        /* [annotation] */
        _In_  UINT InstanceCount,
        /* [annotation] */
        _In_  UINT StartVertexLocation,
        /* [annotation] */
        _In_  UINT StartInstanceLocation);

    void STDMETHODCALLTYPE GSSetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

    void STDMETHODCALLTYPE GSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11GeometryShader *pShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);

    void STDMETHODCALLTYPE IASetPrimitiveTopology(
        /* [annotation] */
        _In_  D3D11_PRIMITIVE_TOPOLOGY Topology);

    void STDMETHODCALLTYPE VSSetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

    void STDMETHODCALLTYPE VSSetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

    void STDMETHODCALLTYPE Begin(
        /* [annotation] */
        _In_  ID3D11Asynchronous *pAsync);

    void STDMETHODCALLTYPE End(
        /* [annotation] */
        _In_  ID3D11Asynchronous *pAsync);

    HRESULT STDMETHODCALLTYPE GetData(
        /* [annotation] */
        _In_  ID3D11Asynchronous *pAsync,
        /* [annotation] */
        _Out_writes_bytes_opt_(DataSize)  void *pData,
        /* [annotation] */
        _In_  UINT DataSize,
        /* [annotation] */
        _In_  UINT GetDataFlags);

    void STDMETHODCALLTYPE SetPredication(
        /* [annotation] */
        _In_opt_  ID3D11Predicate *pPredicate,
        /* [annotation] */
        _In_  BOOL PredicateValue);

    void STDMETHODCALLTYPE GSSetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

    void STDMETHODCALLTYPE GSSetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

    void STDMETHODCALLTYPE OMSetRenderTargets(
        /* [annotation] */
        _In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */
        _In_opt_  ID3D11DepthStencilView *pDepthStencilView);

    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
        /* [annotation] */
        _In_  UINT NumRTVs,
        /* [annotation] */
        _In_reads_opt_(NumRTVs)  ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */
        _In_opt_  ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */
        _In_range_(0, D3D11_1_UAV_SLOT_COUNT - 1)  UINT UAVStartSlot,
        /* [annotation] */
        _In_  UINT NumUAVs,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  const UINT *pUAVInitialCounts);

    void STDMETHODCALLTYPE OMSetBlendState(
        /* [annotation] */
        _In_opt_  ID3D11BlendState *pBlendState,
        /* [annotation] */
        _In_opt_  const FLOAT BlendFactor[4],
        /* [annotation] */
        _In_  UINT SampleMask);

    void STDMETHODCALLTYPE OMSetDepthStencilState(
        /* [annotation] */
        _In_opt_  ID3D11DepthStencilState *pDepthStencilState,
        /* [annotation] */
        _In_  UINT StencilRef);

    void STDMETHODCALLTYPE SOSetTargets(
        /* [annotation] */
        _In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  const UINT *pOffsets);

    void STDMETHODCALLTYPE DrawAuto(void);

    void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
        /* [annotation] */
        _In_  ID3D11Buffer *pBufferForArgs,
        /* [annotation] */
        _In_  UINT AlignedByteOffsetForArgs);

    void STDMETHODCALLTYPE DrawInstancedIndirect(
        /* [annotation] */
        _In_  ID3D11Buffer *pBufferForArgs,
        /* [annotation] */
        _In_  UINT AlignedByteOffsetForArgs);

    void STDMETHODCALLTYPE Dispatch(
        /* [annotation] */
        _In_  UINT ThreadGroupCountX,
        /* [annotation] */
        _In_  UINT ThreadGroupCountY,
        /* [annotation] */
        _In_  UINT ThreadGroupCountZ);

    void STDMETHODCALLTYPE DispatchIndirect(
        /* [annotation] */
        _In_  ID3D11Buffer *pBufferForArgs,
        /* [annotation] */
        _In_  UINT AlignedByteOffsetForArgs);

    void STDMETHODCALLTYPE RSSetState(
        /* [annotation] */
        _In_opt_  ID3D11RasterizerState *pRasterizerState);

    void STDMETHODCALLTYPE RSSetViewports(
        /* [annotation] */
        _In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
        /* [annotation] */
        _In_reads_opt_(NumViewports)  const D3D11_VIEWPORT *pViewports);

    void STDMETHODCALLTYPE RSSetScissorRects(
        /* [annotation] */
        _In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
        /* [annotation] */
        _In_reads_opt_(NumRects)  const D3D11_RECT *pRects);

    void STDMETHODCALLTYPE CopySubresourceRegion(
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
        _In_opt_  const D3D11_BOX *pSrcBox);

    void STDMETHODCALLTYPE CopyResource(
        /* [annotation] */
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */
        _In_  ID3D11Resource *pSrcResource);

    void STDMETHODCALLTYPE UpdateSubresource(
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
        _In_  UINT SrcDepthPitch);

    void STDMETHODCALLTYPE CopyStructureCount(
        /* [annotation] */
        _In_  ID3D11Buffer *pDstBuffer,
        /* [annotation] */
        _In_  UINT DstAlignedByteOffset,
        /* [annotation] */
        _In_  ID3D11UnorderedAccessView *pSrcView);

    void STDMETHODCALLTYPE ClearRenderTargetView(
        /* [annotation] */
        _In_  ID3D11RenderTargetView *pRenderTargetView,
        /* [annotation] */
        _In_  const FLOAT ColorRGBA[4]);

    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
        /* [annotation] */
        _In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */
        _In_  const UINT Values[4]);

    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
        /* [annotation] */
        _In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */
        _In_  const FLOAT Values[4]);

    void STDMETHODCALLTYPE ClearDepthStencilView(
        /* [annotation] */
        _In_  ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */
        _In_  UINT ClearFlags,
        /* [annotation] */
        _In_  FLOAT Depth,
        /* [annotation] */
        _In_  UINT8 Stencil);

    void STDMETHODCALLTYPE GenerateMips(
        /* [annotation] */
        _In_  ID3D11ShaderResourceView *pShaderResourceView);

    void STDMETHODCALLTYPE SetResourceMinLOD(
        /* [annotation] */
        _In_  ID3D11Resource *pResource,
        FLOAT MinLOD);

    FLOAT STDMETHODCALLTYPE GetResourceMinLOD(
        /* [annotation] */
        _In_  ID3D11Resource *pResource);

    void STDMETHODCALLTYPE ResolveSubresource(
        /* [annotation] */
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */
        _In_  UINT DstSubresource,
        /* [annotation] */
        _In_  ID3D11Resource *pSrcResource,
        /* [annotation] */
        _In_  UINT SrcSubresource,
        /* [annotation] */
        _In_  DXGI_FORMAT Format);

    void STDMETHODCALLTYPE ExecuteCommandList(
        /* [annotation] */
        _In_  ID3D11CommandList *pCommandList,
        BOOL RestoreContextState);

    void STDMETHODCALLTYPE HSSetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

    void STDMETHODCALLTYPE HSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11HullShader *pHullShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);

    void STDMETHODCALLTYPE HSSetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

    void STDMETHODCALLTYPE HSSetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

    void STDMETHODCALLTYPE DSSetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

    void STDMETHODCALLTYPE DSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11DomainShader *pDomainShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);

    void STDMETHODCALLTYPE DSSetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

    void STDMETHODCALLTYPE DSSetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

    void STDMETHODCALLTYPE CSSetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
        /* [annotation] */
        _In_range_(0, D3D11_1_UAV_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_1_UAV_SLOT_COUNT - StartSlot)  UINT NumUAVs,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */
        _In_reads_opt_(NumUAVs)  const UINT *pUAVInitialCounts);

    void STDMETHODCALLTYPE CSSetShader(
        /* [annotation] */
        _In_opt_  ID3D11ComputeShader *pComputeShader,
        /* [annotation] */
        _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);

    void STDMETHODCALLTYPE CSSetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

    void STDMETHODCALLTYPE CSSetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

    void STDMETHODCALLTYPE VSGetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

    void STDMETHODCALLTYPE PSGetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

    void STDMETHODCALLTYPE PSGetShader(
        /* [annotation] */
        _Out_  ID3D11PixelShader **ppPixelShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances);

    void STDMETHODCALLTYPE PSGetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

    void STDMETHODCALLTYPE VSGetShader(
        /* [annotation] */
        _Out_  ID3D11VertexShader **ppVertexShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances);

    void STDMETHODCALLTYPE PSGetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

    void STDMETHODCALLTYPE IAGetInputLayout(
        /* [annotation] */
        _Out_  ID3D11InputLayout **ppInputLayout);

    void STDMETHODCALLTYPE IAGetVertexBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  UINT *pStrides,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  UINT *pOffsets);

    void STDMETHODCALLTYPE IAGetIndexBuffer(
        /* [annotation] */
        _Out_opt_  ID3D11Buffer **pIndexBuffer,
        /* [annotation] */
        _Out_opt_  DXGI_FORMAT *Format,
        /* [annotation] */
        _Out_opt_  UINT *Offset);

    void STDMETHODCALLTYPE GSGetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

    void STDMETHODCALLTYPE GSGetShader(
        /* [annotation] */
        _Out_  ID3D11GeometryShader **ppGeometryShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances);

    void STDMETHODCALLTYPE IAGetPrimitiveTopology(
        /* [annotation] */
        _Out_  D3D11_PRIMITIVE_TOPOLOGY *pTopology);

    void STDMETHODCALLTYPE VSGetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

    void STDMETHODCALLTYPE VSGetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

    void STDMETHODCALLTYPE GetPredication(
        /* [annotation] */
        _Out_opt_  ID3D11Predicate **ppPredicate,
        /* [annotation] */
        _Out_opt_  BOOL *pPredicateValue);

    void STDMETHODCALLTYPE GSGetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

    void STDMETHODCALLTYPE GSGetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

    void STDMETHODCALLTYPE OMGetRenderTargets(
        /* [annotation] */
        _In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */
        _Out_opt_  ID3D11DepthStencilView **ppDepthStencilView);

    void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
        /* [annotation] */
        _In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumRTVs,
        /* [annotation] */
        _Out_writes_opt_(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */
        _Out_opt_  ID3D11DepthStencilView **ppDepthStencilView,
        /* [annotation] */
        _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot)  UINT NumUAVs,
        /* [annotation] */
        _Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

    void STDMETHODCALLTYPE OMGetBlendState(
        /* [annotation] */
        _Out_opt_  ID3D11BlendState **ppBlendState,
        /* [annotation] */
        _Out_opt_  FLOAT BlendFactor[4],
        /* [annotation] */
        _Out_opt_  UINT *pSampleMask);

    void STDMETHODCALLTYPE OMGetDepthStencilState(
        /* [annotation] */
        _Out_opt_  ID3D11DepthStencilState **ppDepthStencilState,
        /* [annotation] */
        _Out_opt_  UINT *pStencilRef);

    void STDMETHODCALLTYPE SOGetTargets(
        /* [annotation] */
        _In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppSOTargets);

    void STDMETHODCALLTYPE RSGetState(
        /* [annotation] */
        _Out_  ID3D11RasterizerState **ppRasterizerState);

    void STDMETHODCALLTYPE RSGetViewports(
        /* [annotation] */
        _Inout_ /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
        /* [annotation] */
        _Out_writes_opt_(*pNumViewports)  D3D11_VIEWPORT *pViewports);

    void STDMETHODCALLTYPE RSGetScissorRects(
        /* [annotation] */
        _Inout_ /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
        /* [annotation] */
        _Out_writes_opt_(*pNumRects)  D3D11_RECT *pRects);

    void STDMETHODCALLTYPE HSGetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

    void STDMETHODCALLTYPE HSGetShader(
        /* [annotation] */
        _Out_  ID3D11HullShader **ppHullShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances);

    void STDMETHODCALLTYPE HSGetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

    void STDMETHODCALLTYPE HSGetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

    void STDMETHODCALLTYPE DSGetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

    void STDMETHODCALLTYPE DSGetShader(
        /* [annotation] */
        _Out_  ID3D11DomainShader **ppDomainShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances);

    void STDMETHODCALLTYPE DSGetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

    void STDMETHODCALLTYPE DSGetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

    void STDMETHODCALLTYPE CSGetShaderResources(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
        /* [annotation] */
        _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
        /* [annotation] */
        _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
        /* [annotation] */
        _Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

    void STDMETHODCALLTYPE CSGetShader(
        /* [annotation] */
        _Out_  ID3D11ComputeShader **ppComputeShader,
        /* [annotation] */
        _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */
        _Inout_opt_  UINT *pNumClassInstances);

    void STDMETHODCALLTYPE CSGetSamplers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
        /* [annotation] */
        _Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

    void STDMETHODCALLTYPE CSGetConstantBuffers(
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
        /* [annotation] */
        _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
        /* [annotation] */
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

    void STDMETHODCALLTYPE ClearState(void);

    void STDMETHODCALLTYPE Flush(void);

    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType(void);

    UINT STDMETHODCALLTYPE GetContextFlags(void);

    HRESULT STDMETHODCALLTYPE FinishCommandList(
        BOOL RestoreDeferredContextState,
        /* [annotation] */
        _Out_opt_  ID3D11CommandList **ppCommandList);


    /** ID3D11DeviceContext1 **/

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

// clang-format on
