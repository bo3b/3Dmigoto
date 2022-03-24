// Wrapper for the ID3D11DeviceContext1.
//
// Object                OS               D3D11 version   Feature level
// ID3D11DeviceContext   Win7             11.0            11.0
// ID3D11DeviceContext1  Platform update  11.1            11.1
// ID3D11DeviceContext2  Win8.1           11.2
// ID3D11DeviceContext3                   11.3

// This gives us access to every D3D11 call for a Context, and override the pieces needed.
// Hierarchy:
//  HackerContext <- ID3D11DeviceContext1 <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

#include "HackerContext.hpp"

//#include "HookedContext.h"

#include "log.h"
#include "Globals.h"

#include "HackerDevice.hpp"
#include "D3D11Wrapper.h"
//#include "ResourceHash.hpp"
//#include "Override.hpp"
#include "ShaderRegex.hpp"
#include "FrameAnalysis.hpp"
#include "HookedContext.h"
#include "profiling.hpp"

using namespace std;

// -----------------------------------------------------------------------------------------------

HackerContext* HackerContextFactory(
    ID3D11Device1*        device1,
    ID3D11DeviceContext1* context1)
{
    // We can either create a straight HackerContext, or a souped up
    // FrameAnalysisContext that provides more functionality, at the cost
    // of a slight performance penalty that may marginally (1fps) reduces
    // the framerate in CPU bound games. We can't change this decision
    // later, so we need to decide here and now which it will be, and a
    // game restart will be required to change this.
    //
    // FrameAnalysisContext provides two similar, but separate features -
    // it provides the frame analysis log activated with the F8 key, and
    // can log all calls on the context to the debug log. Therefore, if
    // debug logging is enabled we clearly need the FrameAnalysisContext.
    //
    // Without the debug log we use hunting to decide whether to use the
    // FrameAnalysisContext or not - generally speaking we aim to allow
    // most of the d3dx.ini options to be changed live (where possible) if
    // hunting is enabled (=1 or 2), and that includes enabling and
    // disabling frame analysis on the fly, so if we keyed this off
    // analyse_frame instead of hunting we would be preventing that for no
    // good reason.
    //
    // It's also worth remembering that the frame_analysis key binding only
    // works when hunting=1 (partially as a safety measure, partially
    // because frame analysis resource dumping still has some dependencies
    // on stat collection), so G->hunting is already a pre-requisite for
    // frame analysis:
    if (G->hunting || gLogDebug)
    {
        LOG_INFO("  Creating FrameAnalysisContext\n");
        return new FrameAnalysisContext(device1, context1);
    }

    LOG_INFO("  Creating HackerContext - frame analysis log will not be available\n");
    return new HackerContext(device1, context1);
}

HackerContext::HackerContext(
    ID3D11Device1*        device1,
    ID3D11DeviceContext1* context1)
{
    origDevice1      = device1;
    origContext1     = context1;
    realOrigContext1 = context1;
    hackerDevice     = nullptr;

    memset(currentVertexBuffers, 0, sizeof(currentVertexBuffers));
    currentIndexBuffer          = 0;
    currentVertexShader         = 0;
    currentVertexShaderHandle   = nullptr;
    currentPixelShader          = 0;
    currentPixelShaderHandle    = nullptr;
    currentComputeShader        = 0;
    currentComputeShaderHandle  = nullptr;
    currentGeometryShader       = 0;
    currentGeometryShaderHandle = nullptr;
    currentDomainShader         = 0;
    currentDomainShaderHandle   = nullptr;
    currentHullShader           = 0;
    currentHullShaderHandle     = nullptr;
    currentDepthTarget          = nullptr;
    currentPSUAVStartSlot       = 0;
    currentPSNumUAVs            = 0;
}

// Save the corresponding HackerDevice, as we need to use it periodically to get
// access to the StereoParams.

void HackerContext::SetHackerDevice(
    HackerDevice* hacker_device)
{
    hackerDevice = hacker_device;
}

HackerDevice* HackerContext::GetHackerDevice()
{
    return hackerDevice;
}

// Returns the "real" DirectX object. Note that if hooking is enabled calls
// through this object will go back into 3DMigoto, which would then subject
// them to extra logging and any processing 3DMigoto applies, which may be
// undesirable in some cases. This used to cause a crash if a command list
// issued a draw call, since that would then trigger the command list and
// recurse until the stack ran out:
ID3D11DeviceContext1* HackerContext::GetPossiblyHookedOrigContext1()
{
    return realOrigContext1;
}

// Use this one when you specifically don't want calls through this object to
// ever go back into 3DMigoto. If hooking is disabled this is identical to the
// above, but when hooking this will be the trampoline object instead:
ID3D11DeviceContext1* HackerContext::GetPassThroughOrigContext1()
{
    return origContext1;
}

void HackerContext::HookContext()
{
    // This will install hooks in the original context (if they have not
    // already been installed from a prior context) which will call the
    // equivalent function in this HackerContext. It returns a trampoline
    // interface which we use in place of origContext1 to call the real
    // original context, thereby side stepping the problem that calling the
    // old origContext1 would be hooked and call back into us endlessly:
    origContext1 = hook_context(origContext1, this);
}

// -----------------------------------------------------------------------------

// Records the hash of this shader resource view for later lookup. Returns the
// handle to the resource, but be aware that it no longer has a reference and
// should only be used for map lookups.
ID3D11Resource* HackerContext::RecordResourceViewStats(
    ID3D11View*         view,
    std::set<uint32_t>* resource_info)
{
    ID3D11Resource* resource  = nullptr;
    uint32_t        orig_hash = 0;

    if (!view)
        return nullptr;

    view->GetResource(&resource);
    if (!resource)
        return nullptr;

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        // We are using the original resource hash for stat collection - things
        // get tricky otherwise
        orig_hash = GetOrigResourceHash(resource);

        resource->Release();

        if (orig_hash)
            resource_info->insert(orig_hash);
    }
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);

    return resource;
}

static resource_snapshot snapshot_resource(
    ID3D11Resource* handle)
{
    uint32_t hash = 0, orig_hash = 0;

    ResourceHandleInfo* info = GetResourceHandleInfo(handle);
    if (info)
    {
        hash      = info->hash;
        orig_hash = info->orig_hash;
    }

    return resource_snapshot(handle, hash, orig_hash);
}

void HackerContext::_RecordShaderResourceUsage(
    shader_info_data*         shader_info,
    ID3D11ShaderResourceView* views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT])
{
    ID3D11Resource* resource;
    int             i;

    for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
        if (views[i])
        {
            resource = RecordResourceViewStats(views[i], &G->mShaderResourceInfo);
            if (resource)
                shader_info->ResourceRegisters[i].insert(snapshot_resource(resource));
            views[i]->Release();
        }
    }
}

void HackerContext::RecordPeerShaders(
    std::set<UINT64>* peer_shaders,
    UINT64            shader_hash)
{
    if (currentVertexShader && currentVertexShader != shader_hash)
        peer_shaders->insert(currentVertexShader);

    if (currentHullShader && currentHullShader != shader_hash)
        peer_shaders->insert(currentHullShader);

    if (currentDomainShader && currentDomainShader != shader_hash)
        peer_shaders->insert(currentDomainShader);

    if (currentGeometryShader && currentGeometryShader != shader_hash)
        peer_shaders->insert(currentGeometryShader);

    if (currentPixelShader && currentPixelShader != shader_hash)
        peer_shaders->insert(currentPixelShader);
}

template <
    void (__stdcall ID3D11DeviceContext::*GetShaderResources)(
        UINT                       StartSlot,
        UINT                       NumViews,
        ID3D11ShaderResourceView** ppShaderResourceViews)>
void HackerContext::RecordShaderResourceUsage(
    std::map<UINT64, shader_info_data>& shader_info,
    UINT64                              current_shader)
{
    ID3D11ShaderResourceView* views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    shader_info_data*         info;

    (origContext1->*GetShaderResources)(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        info = &shader_info[current_shader];
        _RecordShaderResourceUsage(info, views);
        RecordPeerShaders(&info->PeerShaders, current_shader);
    }
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
}

void HackerContext::RecordGraphicsShaderStats()
{
    ID3D11UnorderedAccessView* uavs[D3D11_1_UAV_SLOT_COUNT];  // DX11: 8, DX11.1: 64
    UINT                       selected_render_target_pos;
    shader_info_data*          info;
    ID3D11Resource*            resource;
    UINT                       i;
    Profiling::State           profiling_state;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    if (currentVertexShader)
    {
        RecordShaderResourceUsage<&ID3D11DeviceContext::VSGetShaderResources>(G->mVertexShaderInfo, currentVertexShader);
    }

    if (currentHullShader)
    {
        RecordShaderResourceUsage<&ID3D11DeviceContext::HSGetShaderResources>(G->mHullShaderInfo, currentHullShader);
    }

    if (currentDomainShader)
    {
        RecordShaderResourceUsage<&ID3D11DeviceContext::DSGetShaderResources>(G->mDomainShaderInfo, currentDomainShader);
    }

    if (currentGeometryShader)
    {
        RecordShaderResourceUsage<&ID3D11DeviceContext::GSGetShaderResources>(G->mGeometryShaderInfo, currentGeometryShader);
    }

    if (currentPixelShader)
    {
        // This API is poorly designed, because we have to know the
        // current UAV start slot.
        OMGetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, currentPSUAVStartSlot, currentPSNumUAVs, uavs);

        RecordShaderResourceUsage<&ID3D11DeviceContext::PSGetShaderResources>(G->mPixelShaderInfo, currentPixelShader);

        ENTER_CRITICAL_SECTION(&G->mCriticalSection);
        {
            info = &G->mPixelShaderInfo[currentPixelShader];

            for (selected_render_target_pos = 0; selected_render_target_pos < currentRenderTargets.size(); ++selected_render_target_pos)
            {
                if (selected_render_target_pos >= info->RenderTargets.size())
                    info->RenderTargets.push_back(std::set<resource_snapshot>());

                info->RenderTargets[selected_render_target_pos].insert(snapshot_resource(currentRenderTargets[selected_render_target_pos]));
            }

            if (currentDepthTarget)
                info->DepthTargets.insert(snapshot_resource(currentDepthTarget));

            if (currentPSNumUAVs)
            {
                for (i = 0; i < currentPSNumUAVs; i++)
                {
                    if (uavs[i])
                    {
                        resource = RecordResourceViewStats(uavs[i], &G->mUnorderedAccessInfo);
                        if (resource)
                            info->UAVs[i + currentPSUAVStartSlot].insert(snapshot_resource(resource));

                        uavs[i]->Release();
                    }
                }
            }
        }
        LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
    }

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::stat_overhead);
}

void HackerContext::RecordComputeShaderStats()
{
    ID3D11ShaderResourceView*  srvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11UnorderedAccessView* uavs[D3D11_1_UAV_SLOT_COUNT];  // DX11: 8, DX11.1: 64
    shader_info_data*          info;
    D3D_FEATURE_LEVEL          level    = origDevice1->GetFeatureLevel();
    UINT                       num_uavs = (level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT);
    ID3D11Resource*            resource;
    UINT                       i;
    Profiling::State           profiling_state;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    origContext1->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, srvs);
    origContext1->CSGetUnorderedAccessViews(0, num_uavs, uavs);

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        info = &G->mComputeShaderInfo[currentComputeShader];
        _RecordShaderResourceUsage(info, srvs);

        for (i = 0; i < num_uavs; i++)
        {
            if (uavs[i])
            {
                resource = RecordResourceViewStats(uavs[i], &G->mUnorderedAccessInfo);
                if (resource)
                    info->UAVs[i].insert(snapshot_resource(resource));

                uavs[i]->Release();
            }
        }

        if (Profiling::mode == Profiling::Mode::SUMMARY)
            Profiling::end(&profiling_state, &Profiling::stat_overhead);
    }
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
}

void HackerContext::RecordRenderTargetInfo(
    ID3D11RenderTargetView* target,
    UINT                    view_num)
{
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    ID3D11Resource*               resource  = nullptr;
    uint32_t                      orig_hash = 0;

    target->GetDesc(&desc);

    LOG_DEBUG("  View #%d, Format = %d, Is2D = %d\n", view_num, desc.Format, D3D11_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension);

    target->GetResource(&resource);
    if (!resource)
        return;

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        // We are using the original resource hash for stat collection - things
        // get tricky otherwise
        orig_hash = GetOrigResourceHash((ID3D11Texture2D*)resource);

        resource->Release();

        if (!resource)
            goto out_unlock;

        currentRenderTargets.push_back(resource);
        G->mVisitedRenderTargets.insert(resource);
        G->mRenderTargetInfo.insert(orig_hash);
    }
out_unlock:
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
}

void HackerContext::RecordDepthStencil(
    ID3D11DepthStencilView* target)
{
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    ID3D11Resource*               resource  = nullptr;
    uint32_t                      orig_hash = 0;

    if (!target)
        return;

    target->GetResource(&resource);
    if (!resource)
        return;

    target->GetDesc(&desc);

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        // We are using the original resource hash for stat collection - things
        // get tricky otherwise
        orig_hash = GetOrigResourceHash(resource);

        resource->Release();

        currentDepthTarget = resource;
        G->mDepthTargetInfo.insert(orig_hash);
    }
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
}

ID3D11VertexShader* HackerContext::SwitchVSShader(
    ID3D11VertexShader* shader)
{
    ID3D11VertexShader*  vertex_shader;
    ID3D11ClassInstance* class_instances;
    UINT                 num_class_instances = 0, i;

    // We can possibly save the need to get the current shader by saving the ClassInstances
    origContext1->VSGetShader(&vertex_shader, &class_instances, &num_class_instances);
    origContext1->VSSetShader(shader, &class_instances, num_class_instances);

    for (i = 0; i < num_class_instances; i++)
        class_instances[i].Release();

    return vertex_shader;
}

ID3D11PixelShader* HackerContext::SwitchPSShader(
    ID3D11PixelShader* shader)
{
    ID3D11PixelShader*   pixel_shader;
    ID3D11ClassInstance* class_instances;
    UINT                 num_class_instances = 0, i;

    // We can possibly save the need to get the current shader by saving the ClassInstances
    origContext1->PSGetShader(&pixel_shader, &class_instances, &num_class_instances);
    origContext1->PSSetShader(shader, &class_instances, num_class_instances);

    for (i = 0; i < num_class_instances; i++)
        class_instances[i].Release();

    return pixel_shader;
}

#define ENABLE_LEGACY_FILTERS 1
void HackerContext::ProcessShaderOverride(
    shader_override* shader_override,
    bool             is_pixel_shader,
    draw_context*    data)
{
    bool use_orig = false;

    LOG_DEBUG("  override found for shader\n");

    // We really want to start deprecating all the old filters and switch
    // to using the command list for much greater flexibility. This if()
    // will be optimised out by the compiler, but is here to remind anyone
    // looking at this that we don't want to extend this code further.
    if (ENABLE_LEGACY_FILTERS)
    {
        // Deprecated: The texture filtering support in the command
        // list can match oD for the depth buffer, which will return
        // negative zero -0.0 if no depth buffer is assigned.
        if (shader_override->depth_filter != DepthBufferFilter::NONE)
        {
            ID3D11DepthStencilView* depth_stencil_view = nullptr;

            origContext1->OMGetRenderTargets(0, nullptr, &depth_stencil_view);

            // Remember - we are NOT switching to the original shader when the condition is true
            if (shader_override->depth_filter == DepthBufferFilter::DEPTH_ACTIVE && !depth_stencil_view)
            {
                use_orig = true;
            }
            else if (shader_override->depth_filter == DepthBufferFilter::DEPTH_INACTIVE && depth_stencil_view)
            {
                use_orig = true;
            }

            if (depth_stencil_view)
                depth_stencil_view->Release();

            // TODO: Add alternate filter type where the depth
            // buffer state is passed as an input to the shader
        }

        // Deprecated: Partner filtering can already be achieved with
        // the command list with far more flexibility than this allows
        if (shader_override->partner_hash)
        {
            if (is_pixel_shader)
            {
                if (currentVertexShader != shader_override->partner_hash)
                    use_orig = true;
            }
            else
            {
                if (currentPixelShader != shader_override->partner_hash)
                    use_orig = true;
            }
        }
    }

    run_command_list(hackerDevice, this, &shader_override->command_list, &data->call_info, false);

    if (ENABLE_LEGACY_FILTERS)
    {
        // Deprecated since the logic can be moved into the shaders with far more flexibility
        if (use_orig)
        {
            if (is_pixel_shader)
            {
                ShaderReplacementMap::iterator i = lookup_original_shader(currentPixelShaderHandle);
                if (i != G->mOriginalShaders.end())
                    data->old_pixel_shader = SwitchPSShader(static_cast<ID3D11PixelShader*>(i->second));
            }
            else
            {
                ShaderReplacementMap::iterator i = lookup_original_shader(currentVertexShaderHandle);
                if (i != G->mOriginalShaders.end())
                    data->old_vertex_shader = SwitchVSShader(static_cast<ID3D11VertexShader*>(i->second));
            }
        }
    }
}

// This function will run the ShaderRegex engine to automatically patch shaders
// on the fly and/or insert command lists on the fly. This may seem like a
// slightly unusual place to run this, but the reason is because of another
// upcoming feature that may decide to patch shaders at the last possible
// moment once the pipeline state is known, and this is the best point to do
// that. Later we can look into running the ShaderRegex engine earlier (at
// least on initial load when not hunting), but for now to keep things simpler
// we will do all the auto patching from one place.
//
// We do want to avoid replacing a shader that has already been replaced from
// ShaderFixes, either at shader creation time, or dynamically by the
// mReloadedShaders map - user replaced shaders should always take priority
// over automatically replaced shaders.
template <
    class ID3D11Shader,
    void (__stdcall ID3D11DeviceContext::*GetShaderVS2013BUGWORKAROUND)(ID3D11Shader**, ID3D11ClassInstance**, UINT*),
    void (__stdcall ID3D11DeviceContext::*SetShaderVS2013BUGWORKAROUND)(ID3D11Shader*, ID3D11ClassInstance* const*, UINT),
    HRESULT (__stdcall ID3D11Device::*CreateShader)(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11Shader**)>
void HackerContext::DeferredShaderReplacement(
    ID3D11DeviceChild* shader,
    UINT64             hash,
    wchar_t*           shader_type)
{
    ID3D11Shader*             orig_shader    = nullptr;
    ID3D11Shader*             patched_shader = nullptr;
    ID3D11ClassInstance*      class_instances[256] {};
    ShaderReloadMap::iterator orig_info_i;
    original_shader_info*     orig_info     = nullptr;
    UINT                      num_instances = 0;
    string                    asm_text;
    bool                      patch_regex = false;
    HRESULT                   hr;
    unsigned                  i;
    wstring                   tagline(L"//");
    vector<byte>              patched_bytecode;
    vector<char>              asm_vector;

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        // Faster than catching an out_of_range exception from .at():
        orig_info_i = lookup_reloaded_shader(shader);
        if (orig_info_i == G->mReloadedShaders.end())
            goto out_drop;
        orig_info = &orig_info_i->second;

        if (!orig_info->deferred_replacement_candidate || orig_info->deferred_replacement_processed)
            goto out_drop;

        // Remember that we have analysed this one so we don't check it again
        // (until config reload) regardless of whether we patch it or not:
        orig_info->deferred_replacement_processed = true;

        switch (load_shader_regex_cache(hash, shader_type, &patched_bytecode, &tagline))
        {
            case ShaderRegexCache::NO_MATCH:
                LOG_INFO("%S %016I64x has cached ShaderRegex miss\n", shader_type, hash);
                goto out_drop;
            case ShaderRegexCache::MATCH:
                LOG_INFO("Loaded %S %016I64x command list from ShaderRegex cache\n", shader_type, hash);
                goto out_drop;
            case ShaderRegexCache::PATCH:
                LOG_INFO("Loaded %S %016I64x bytecode from ShaderRegex cache\n", shader_type, hash);
                break;
            case ShaderRegexCache::NO_CACHE:
                LOG_INFO("Performing deferred shader analysis on %S %016I64x...\n", shader_type, hash);

                asm_text = binary_to_asm_text(orig_info->byteCode->GetBufferPointer(), orig_info->byteCode->GetBufferSize(), G->patch_cb_offsets, G->disassemble_undecipherable_custom_data);
                if (asm_text.empty())
                    goto out_drop;

                try
                {
                    patch_regex = apply_shader_regex_groups(&asm_text, shader_type, &orig_info->shaderModel, hash, &tagline);
                }
                catch (...)
                {
                    LOG_INFO("    *** Exception while patching shader\n");
                    goto out_drop;
                }

                if (!patch_regex)
                {
                    LOG_INFO("Patch did not apply\n");
                    goto out_drop;
                }

                // No longer logging this since we can output to ShaderFixes
                // via hunting if marking_actions = regex, or it could be
                // disassembled from the regex cache with cmd_Decompiler
                // LOG_INFO("Patched Shader:\n%s\n", asm_text.c_str());

                asm_vector.assign(asm_text.begin(), asm_text.end());

                try
                {
                    vector<AssemblerParseError> parse_errors;
                    hr = AssembleFluganWithSignatureParsing(&asm_vector, &patched_bytecode, &parse_errors);
                    if (FAILED(hr))
                    {
                        LOG_INFO("    *** Assembling patched shader failed\n");
                        goto out_drop;
                    }
                    // Parse errors are currently being treated as non-fatal on
                    // creation time replacement and ShaderRegex for backwards
                    // compatibility (live shader reload is fatal).
                    for (auto& parse_error : parse_errors)
                        LogOverlay(LOG_NOTICE, "%016I64x-%S %S: %s\n", hash, shader_type, tagline.c_str(), parse_error.what());
                }
                catch (const exception& e)
                {
                    LogOverlay(LOG_WARNING, "Error assembling ShaderRegex patched %016I64x-%S\n%S\n%s\n", hash, shader_type, tagline.c_str(), e.what());
                    goto out_drop;
                }

                save_shader_regex_cache_bin(hash, shader_type, &patched_bytecode);
        }

        hr = (origDevice1->*CreateShader)(patched_bytecode.data(), patched_bytecode.size(), orig_info->linkage, &patched_shader);
        cleanup_shader_maps(patched_shader);
        if (FAILED(hr))
        {
            LOG_INFO("    *** Creating replacement shader failed\n");
            goto out_drop;
        }

        // Update replacement map so we don't have to repeat this process.
        // Not updating the bytecode in the replaced shader map - we do that
        // elsewhere, but I think that is a bug. Need to untangle that first.
        if (orig_info->replacement)
            orig_info->replacement->Release();
        orig_info->replacement = patched_shader;
        orig_info->infoText    = tagline;
    }
    // Now that we've finished updating our data structures we can drop the
    // critical section before calling into DirectX to bind the replacement
    // shader. This was necessary to avoid a deadlock with the resource
    // release tracker, but that now uses a different lock.
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);

    // And bind the replaced shader in time for this draw call:
    // VSBUGWORKAROUND: VS2013 toolchain has a bug that mistakes a member
    // pointer called "SetShader" for the SetShader we have in
    // HackerContext, even though the member pointer we were passed very
    // clearly points to a member function of ID3D11DeviceContext. VS2015
    // toolchain does not suffer from this bug.
    (origContext1->*GetShaderVS2013BUGWORKAROUND)(&orig_shader, class_instances, &num_instances);
    (origContext1->*SetShaderVS2013BUGWORKAROUND)(patched_shader, class_instances, num_instances);
    if (orig_shader)
        orig_shader->Release();
    for (i = 0; i < num_instances; i++)
    {
        if (class_instances[i])
            class_instances[i]->Release();
    }
    return;

out_drop:
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
}

void HackerContext::DeferredShaderReplacementBeforeDraw()
{
    Profiling::State profiling_state;

    if (shader_regex_groups.empty())
        return;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    if (currentVertexShaderHandle)
    {
        DeferredShaderReplacement<ID3D11VertexShader, &ID3D11DeviceContext::VSGetShader, &ID3D11DeviceContext::VSSetShader, &ID3D11Device::CreateVertexShader>(currentVertexShaderHandle, currentVertexShader, L"vs");
    }
    if (currentHullShaderHandle)
    {
        DeferredShaderReplacement<ID3D11HullShader, &ID3D11DeviceContext::HSGetShader, &ID3D11DeviceContext::HSSetShader, &ID3D11Device::CreateHullShader>(currentHullShaderHandle, currentHullShader, L"hs");
    }
    if (currentDomainShaderHandle)
    {
        DeferredShaderReplacement<ID3D11DomainShader, &ID3D11DeviceContext::DSGetShader, &ID3D11DeviceContext::DSSetShader, &ID3D11Device::CreateDomainShader>(currentDomainShaderHandle, currentDomainShader, L"ds");
    }
    if (currentGeometryShaderHandle)
    {
        DeferredShaderReplacement<ID3D11GeometryShader, &ID3D11DeviceContext::GSGetShader, &ID3D11DeviceContext::GSSetShader, &ID3D11Device::CreateGeometryShader>(currentGeometryShaderHandle, currentGeometryShader, L"gs");
    }
    if (currentPixelShaderHandle)
    {
        DeferredShaderReplacement<ID3D11PixelShader, &ID3D11DeviceContext::PSGetShader, &ID3D11DeviceContext::PSSetShader, &ID3D11Device::CreatePixelShader>(currentPixelShaderHandle, currentPixelShader, L"ps");
    }

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::shaderregex_overhead);
}

void HackerContext::DeferredShaderReplacementBeforeDispatch()
{
    if (shader_regex_groups.empty())
        return;

    if (!currentComputeShaderHandle)
        return;

    DeferredShaderReplacement<ID3D11ComputeShader, &ID3D11DeviceContext::CSGetShader, &ID3D11DeviceContext::CSSetShader, &ID3D11Device::CreateComputeShader>(currentComputeShaderHandle, currentComputeShader, L"cs");
}

void HackerContext::BeforeDraw(
    draw_context& data)
{
    Profiling::State profiling_state;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    // If we are not hunting shaders, we should skip all of this shader management for a performance bump.
    if (G->hunting == HUNTING_MODE_ENABLED)
    {
        UINT selected_vertex_buffer_pos;
        UINT selected_render_target_pos;
        UINT i;

        // In some cases stat collection can have a significant
        // performance impact or may result in a runaway memory leak,
        // so only do it if dump_usage is enabled.
        if (G->DumpUsage)
            RecordGraphicsShaderStats();

        ENTER_CRITICAL_SECTION(&G->mCriticalSection);
        {
            // Selection
            for (selected_vertex_buffer_pos = 0; selected_vertex_buffer_pos < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++selected_vertex_buffer_pos)
            {
                if (currentVertexBuffers[selected_vertex_buffer_pos] == G->mSelectedVertexBuffer)
                    break;
            }
            for (selected_render_target_pos = 0; selected_render_target_pos < currentRenderTargets.size(); ++selected_render_target_pos)
            {
                if (currentRenderTargets[selected_render_target_pos] == G->mSelectedRenderTarget)
                    break;
            }
            if (currentIndexBuffer == G->mSelectedIndexBuffer ||
                currentVertexShader == G->mSelectedVertexShader ||
                currentPixelShader == G->mSelectedPixelShader ||
                currentGeometryShader == G->mSelectedGeometryShader ||
                currentDomainShader == G->mSelectedDomainShader ||
                currentHullShader == G->mSelectedHullShader ||
                selected_vertex_buffer_pos < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT ||
                selected_render_target_pos < currentRenderTargets.size())
            {
                LOG_DEBUG("  Skipping selected operation. CurrentIndexBuffer = %08lx, CurrentVertexShader = %016I64x, CurrentPixelShader = %016I64x\n", currentIndexBuffer, currentVertexShader, currentPixelShader);

                // Snapshot render target list.
                if (G->mSelectedRenderTargetSnapshot != G->mSelectedRenderTarget)
                {
                    G->mSelectedRenderTargetSnapshotList.clear();
                    G->mSelectedRenderTargetSnapshot = G->mSelectedRenderTarget;
                }
                G->mSelectedRenderTargetSnapshotList.insert(currentRenderTargets.begin(), currentRenderTargets.end());
                // Snapshot info.
                for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
                {
                    if (currentVertexBuffers[i] == G->mSelectedVertexBuffer)
                    {
                        G->mSelectedVertexBuffer_VertexShader.insert(currentVertexShader);
                        G->mSelectedVertexBuffer_PixelShader.insert(currentPixelShader);
                    }
                }
                if (currentIndexBuffer == G->mSelectedIndexBuffer)
                {
                    G->mSelectedIndexBuffer_VertexShader.insert(currentVertexShader);
                    G->mSelectedIndexBuffer_PixelShader.insert(currentPixelShader);
                }
                if (currentVertexShader == G->mSelectedVertexShader)
                {
                    for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
                    {
                        if (currentVertexBuffers[i])
                            G->mSelectedVertexShader_VertexBuffer.insert(currentVertexBuffers[i]);
                    }
                    if (currentIndexBuffer)
                        G->mSelectedVertexShader_IndexBuffer.insert(currentIndexBuffer);
                }
                if (currentPixelShader == G->mSelectedPixelShader)
                {
                    for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
                    {
                        if (currentVertexBuffers[i])
                            G->mSelectedVertexShader_VertexBuffer.insert(currentVertexBuffers[i]);
                    }
                    if (currentIndexBuffer)
                        G->mSelectedPixelShader_IndexBuffer.insert(currentIndexBuffer);
                }
                if (G->marking_mode == MarkingMode::MONO && hackerDevice->stereoHandle)
                {
                    LOG_DEBUG("  setting separation=0 for hunting\n");

                    if (NVAPI_OK != Profiling::NvAPI_Stereo_GetSeparation(hackerDevice->stereoHandle, &data.old_separation))
                        LOG_DEBUG("    Stereo_GetSeparation failed.\n");

                    nvapi_override();
                    if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSeparation(hackerDevice->stereoHandle, 0))
                        LOG_DEBUG("    Stereo_SetSeparation failed.\n");
                }
                else if (G->marking_mode == MarkingMode::SKIP)
                {
                    data.call_info.skip = true;

                    // If we have transferred the draw call to a custom shader via "handling =
                    // skip" and "draw = from_caller" we still want a way to skip it for hunting.
                    // We can't reuse call_info.skip for that, as that is also set by
                    // "handling=skip", which may happen before the "draw=from_caller", so we
                    // use a second skip flag specifically for hunting:
                    data.call_info.hunting_skip = true;
                }
                else if (G->marking_mode == MarkingMode::PINK)
                {
                    if (G->mPinkingShader)
                        data.old_pixel_shader = SwitchPSShader(G->mPinkingShader);
                }
            }
        }
        LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
    }

    if (!G->fix_enabled)
        goto out_profile;

    DeferredShaderReplacementBeforeDraw();

    // Override settings?
    if (!G->mShaderOverrideMap.empty())
    {
        ShaderOverrideMap::iterator i;

        i = lookup_shaderoverride(currentVertexShader);
        if (i != G->mShaderOverrideMap.end())
        {
            data.post_commands[0] = &i->second.post_command_list;
            ProcessShaderOverride(&i->second, false, &data);
        }

        if (currentHullShader)
        {
            i = lookup_shaderoverride(currentHullShader);
            if (i != G->mShaderOverrideMap.end())
            {
                data.post_commands[1] = &i->second.post_command_list;
                ProcessShaderOverride(&i->second, false, &data);
            }
        }

        if (currentDomainShader)
        {
            i = lookup_shaderoverride(currentDomainShader);
            if (i != G->mShaderOverrideMap.end())
            {
                data.post_commands[2] = &i->second.post_command_list;
                ProcessShaderOverride(&i->second, false, &data);
            }
        }

        if (currentGeometryShader)
        {
            i = lookup_shaderoverride(currentGeometryShader);
            if (i != G->mShaderOverrideMap.end())
            {
                data.post_commands[3] = &i->second.post_command_list;
                ProcessShaderOverride(&i->second, false, &data);
            }
        }

        i = lookup_shaderoverride(currentPixelShader);
        if (i != G->mShaderOverrideMap.end())
        {
            data.post_commands[4] = &i->second.post_command_list;
            ProcessShaderOverride(&i->second, true, &data);
        }
    }

out_profile:
    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::draw_overhead);
}

void HackerContext::AfterDraw(
    draw_context& data)
{
    int              i;
    Profiling::State profiling_state;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    if (data.call_info.skip)
        Profiling::skipped_draw_calls++;

    for (i = 0; i < 5; i++)
    {
        if (data.post_commands[i])
        {
            run_command_list(hackerDevice, this, data.post_commands[i], &data.call_info, true);
        }
    }

    if (hackerDevice->stereoHandle && data.old_separation != FLT_MAX)
    {
        nvapi_override();
        if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSeparation(hackerDevice->stereoHandle, data.old_separation))
            LOG_DEBUG("    Stereo_SetSeparation failed.\n");
    }

    if (data.old_vertex_shader)
    {
        ID3D11VertexShader* ret;
        ret = SwitchVSShader(data.old_vertex_shader);
        data.old_vertex_shader->Release();
        if (ret)
            ret->Release();
    }
    if (data.old_pixel_shader)
    {
        ID3D11PixelShader* ret;
        ret = SwitchPSShader(data.old_pixel_shader);
        data.old_pixel_shader->Release();
        if (ret)
            ret->Release();
    }

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::draw_overhead);
}

// -----------------------------------------------------------------------------------------------

ULONG STDMETHODCALLTYPE HackerContext::AddRef()
{
    return origContext1->AddRef();
}

// Must set the reference that the HackerDevice uses to null, because otherwise
// we see that dead reference reused in GetImmediateContext, in FC4.

ULONG STDMETHODCALLTYPE HackerContext::Release()
{
    ULONG ul_ref = origContext1->Release();
    LOG_DEBUG("HackerContext::Release counter=%d, this=%p\n", ul_ref, this);

    if (ul_ref <= 0)
    {
        LOG_INFO("  deleting self\n");

        if (hackerDevice != nullptr)
        {
            if (hackerDevice->GetHackerContext() == this)
            {
                LOG_INFO("  clearing hackerDevice->hackerContext\n");
                hackerDevice->SetHackerContext(nullptr);
            }
        }
        else
        {
            LOG_INFO("HackerContext::Release - hackerDevice is NULL\n");
        }

        delete this;
        return 0L;
    }
    return ul_ref;
}

// In a strange case, Mafia 3 calls this hacky interface with the request for
// the ID3D11DeviceContext.  That's right, it calls to get the exact
// same object that it is using to call.  I swear.

HRESULT STDMETHODCALLTYPE HackerContext::QueryInterface(
    REFIID riid,
    void** ppvObject)
{
    LOG_DEBUG("HackerContext::QueryInterface(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(riid).c_str());

    if (ppvObject && IsEqualIID(riid, IID_HackerContext))
    {
        // This is a special case - only 3DMigoto itself should know
        // this IID, so this is us checking if it has a HackerContext.
        // There's no need to call through to DX for this one.
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    HRESULT hr = origContext1->QueryInterface(riid, ppvObject);
    if (FAILED(hr))
    {
        LOG_DEBUG("  failed result = %x for %p\n", hr, ppvObject);
        return hr;
    }

    // To avoid letting the game bypass our hooked object, we need to return the
    // HackerContext/this in this case.
    if (riid == __uuidof(ID3D11DeviceContext))
    {
        *ppvObject = this;
        LOG_DEBUG("  return HackerContext(%s@%p) wrapper of %p\n", type_name(this), this, origContext1);
    }
    else if (riid == __uuidof(ID3D11DeviceContext1))
    {
        if (!G->enable_platform_update)
        {
            LOG_INFO("***  returns E_NOINTERFACE as error for ID3D11DeviceContext1 (try allow_platform_update=1 if the game refuses to run).\n");
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        // For Batman: TellTale games, they call this to fetch the DeviceContext1.
        // We need to return a hooked version as part of fleshing it out for games like this
        // that require the evil-update to run.

        // In this case, we are already an ID3D11DeviceContext1, so just return self.
        *ppvObject = this;
        LOG_DEBUG("  return HackerContext(%s@%p) wrapper of %p\n", type_name(this), this, ppvObject);
    }

    LOG_DEBUG("  returns result = %x for %p\n", hr, ppvObject);
    return hr;
}

// -----------------------------------------------------------------------------------------------

// ******************* ID3D11DeviceChild interface

// Returns our subclassed version of the Device.
// But the method signature cannot be altered.
// Since we can't alter what the object has stored, this returns just
// the superclass call.  If the object was created correctly, that should
// be a HackerDevice object.
// The previous version of this call would fetch the HackerDevice from a list
// and thus this new approach may be broken.

void STDMETHODCALLTYPE HackerContext::GetDevice(
    ID3D11Device** ppDevice)
{
    LOG_DEBUG("HackerContext::GetDevice(%s@%p) returns %p\n", type_name(this), this, hackerDevice);

    // Fix ref counting bug that slowly eats away at the device until we
    // crash. In FC4 this can happen after about 10 minutes, or when
    // running in windowed mode during launch.

    // Follow our rule of always calling the original call first to ensure that
    // any side-effects (including ref counting) are activated.
    origContext1->GetDevice(ppDevice);

    // Return our wrapped device though.
    if (!(G->enable_hooks & EnableHooks::DEVICE))
        *ppDevice = hackerDevice;
}

HRESULT STDMETHODCALLTYPE HackerContext::GetPrivateData(
    REFGUID guid,
    UINT*   pDataSize,
    void*   pData)
{
    LOG_DEBUG("HackerContext::GetPrivateData(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(guid).c_str());

    HRESULT hr = origContext1->GetPrivateData(guid, pDataSize, pData);
    LOG_DEBUG("  returns result = %x, DataSize = %d\n", hr, *pDataSize);

    return hr;
}

HRESULT STDMETHODCALLTYPE HackerContext::SetPrivateData(
    REFGUID     guid,
    UINT        DataSize,
    const void* pData)
{
    LOG_INFO("HackerContext::SetPrivateData(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(guid).c_str());
    LOG_INFO("  DataSize = %d\n", DataSize);

    HRESULT hr = origContext1->SetPrivateData(guid, DataSize, pData);
    LOG_INFO("  returns result = %x\n", hr);

    return hr;
}

HRESULT STDMETHODCALLTYPE HackerContext::SetPrivateDataInterface(
    REFGUID         guid,
    const IUnknown* pData)
{
    LOG_INFO("HackerContext::SetPrivateDataInterface(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(guid).c_str());

    HRESULT hr = origContext1->SetPrivateDataInterface(guid, pData);
    LOG_INFO("  returns result = %x\n", hr);

    return hr;
}

// -----------------------------------------------------------------------------------------------

// ******************* ID3D11DeviceContext interface

// These first routines all the boilerplate ones that just pass through to the original context.
// They need to be here in order to pass along the calls, since there is no proper object where
// it would normally go to the superclass.

void STDMETHODCALLTYPE HackerContext::VSSetConstantBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers)
{
    origContext1->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

bool HackerContext::MapDenyCPURead(
    ID3D11Resource*           resource,
    UINT                      subresource,
    D3D11_MAP                 map_type,
    UINT                      map_flags,
    D3D11_MAPPED_SUBRESOURCE* mapped_resource)
{
    uint32_t                     hash;
    TextureOverrideMap::iterator i;

    // Currently only replacing first subresource to simplify map type, and
    // only on read access as it is unclear how to handle a read/write access.
    if (subresource != 0)
        return false;

    if (G->mTextureOverrideMap.empty())
        return false;

    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        hash = GetResourceHash(resource);
    }
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);

    i = lookup_textureoverride(hash);
    if (i == G->mTextureOverrideMap.end())
        return false;

    return i->second.begin()->deny_cpu_read;
}

void HackerContext::TrackAndDivertMap(
    HRESULT                   map_hr,
    ID3D11Resource*           resource,
    UINT                      subresource,
    D3D11_MAP                 map_type,
    UINT                      map_flags,
    D3D11_MAPPED_SUBRESOURCE* mapped_resource)
{
    D3D11_RESOURCE_DIMENSION dim;
    ID3D11Buffer*            buf   = nullptr;
    ID3D11Texture1D*         tex1d = nullptr;
    ID3D11Texture2D*         tex2d = nullptr;
    ID3D11Texture3D*         tex3d = nullptr;
    D3D11_BUFFER_DESC        buf_desc;
    D3D11_TEXTURE1D_DESC     tex1d_desc;
    D3D11_TEXTURE2D_DESC     tex2d_desc;
    D3D11_TEXTURE3D_DESC     tex3d_desc;
    mapped_resource_info*    map_info   = nullptr;
    void*                    replace    = nullptr;
    bool                     divertable = false, divert = false, track = false;
    bool                     write = false, read = false, deny = false;
    Profiling::State         profiling_state;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    if (FAILED(map_hr) || !resource || !mapped_resource || !mapped_resource->pData)
        goto out_profile;

    switch (map_type)
    {
        case D3D11_MAP_READ_WRITE:
            read = true;
            // Fall through
        case D3D11_MAP_WRITE_DISCARD:
            divertable = true;
            // Fall through
        case D3D11_MAP_WRITE:
        case D3D11_MAP_WRITE_NO_OVERWRITE:
            write = true;
            // We can't divert these last two since we have no way
            // to know which addresses the application wrote to,
            // and trying anyway crashes FC4. We still need the
            // hash tracking code to run on these though (necessary
            // for FC4), so we still go ahead and track the
            // mapping. We might actually be able to get rid of
            // diverting altogether for all these and only use
            // tracking - seems like it might be safe to read from
            // all these IO mapped addresses, but not sure about
            // performance or if there might be any unintended
            // consequences like uninitialised data:
            divert = track = MapTrackResourceHashUpdate(resource, subresource);
            break;

        case D3D11_MAP_READ:
            read = divertable = true;
            divert = deny = MapDenyCPURead(resource, subresource, map_type, map_flags, mapped_resource);
            break;
    }

    if (!track && !divert)
        goto out_profile;

    map_info                  = &mappedResources[resource];
    map_info->mapped_writable = write;
    memcpy(&map_info->map, mapped_resource, sizeof(D3D11_MAPPED_SUBRESOURCE));

    if (!divertable || !divert)
        goto out_profile;

    resource->GetType(&dim);
    switch (dim)
    {
        case D3D11_RESOURCE_DIMENSION_BUFFER:
            buf = static_cast<ID3D11Buffer*>(resource);
            buf->GetDesc(&buf_desc);
            map_info->size = buf_desc.ByteWidth;
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            tex1d = static_cast<ID3D11Texture1D*>(resource);
            tex1d->GetDesc(&tex1d_desc);
            map_info->size = dxgi_format_size(tex1d_desc.Format) * tex1d_desc.Width;
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            tex2d = static_cast<ID3D11Texture2D*>(resource);
            tex2d->GetDesc(&tex2d_desc);
            map_info->size = mapped_resource->RowPitch * tex2d_desc.Height;
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            tex3d = static_cast<ID3D11Texture3D*>(resource);
            tex3d->GetDesc(&tex3d_desc);
            map_info->size = mapped_resource->DepthPitch * tex3d_desc.Depth;
            break;
        default:
            goto out_profile;
    }

    replace = malloc(map_info->size);
    if (!replace)
    {
        LOG_INFO("TrackAndDivertMap out of memory\n");
        goto out_profile;
    }

    if (read && !deny)
        memcpy(replace, mapped_resource->pData, map_info->size);
    else
        memset(replace, 0, map_info->size);

    map_info->orig_pData   = mapped_resource->pData;
    map_info->map.pData    = replace;
    mapped_resource->pData = replace;

out_profile:
    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::map_overhead);
}

void HackerContext::TrackAndDivertUnmap(
    ID3D11Resource* resource,
    UINT            subresource)
{
    MappedResources::iterator i;
    mapped_resource_info*     map_info = nullptr;
    Profiling::State          profiling_state;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    if (mappedResources.empty())
        goto out_profile;

    i = mappedResources.find(resource);
    if (i == mappedResources.end())
        goto out_profile;
    map_info = &i->second;

    if (G->track_texture_updates == 1 && subresource == 0 && map_info->mapped_writable)
        UpdateResourceHashFromCPU(resource, map_info->map.pData, map_info->map.RowPitch, map_info->map.DepthPitch);

    if (map_info->orig_pData)
    {
        // TODO: Measure performance vs. not diverting:
        if (map_info->mapped_writable)
            memcpy(map_info->orig_pData, map_info->map.pData, map_info->size);

        free(map_info->map.pData);
    }

    mappedResources.erase(i);

out_profile:
    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::map_overhead);
}

HRESULT STDMETHODCALLTYPE HackerContext::Map(
    ID3D11Resource*           pResource,
    UINT                      Subresource,
    D3D11_MAP                 MapType,
    UINT                      MapFlags,
    D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
    HRESULT hr;

    hr = origContext1->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);

    TrackAndDivertMap(hr, pResource, Subresource, MapType, MapFlags, pMappedResource);

    return hr;
}

void STDMETHODCALLTYPE HackerContext::Unmap(
    ID3D11Resource* pResource,
    UINT            Subresource)
{
    TrackAndDivertUnmap(pResource, Subresource);
    origContext1->Unmap(pResource, Subresource);
}

void STDMETHODCALLTYPE HackerContext::PSSetConstantBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers)
{
    origContext1->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::IASetInputLayout(
    ID3D11InputLayout* pInputLayout)
{
    origContext1->IASetInputLayout(pInputLayout);
}

void STDMETHODCALLTYPE HackerContext::IASetVertexBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppVertexBuffers,
    const UINT*          pStrides,
    const UINT*          pOffsets)
{
    origContext1->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

    if (G->hunting == HUNTING_MODE_ENABLED)
    {
        ENTER_CRITICAL_SECTION(&G->mCriticalSection);
        {
            for (UINT i = StartSlot; (i < StartSlot + NumBuffers) && (i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT); i++)
            {
                if (ppVertexBuffers && ppVertexBuffers[i])
                {
                    currentVertexBuffers[i] = GetResourceHash(ppVertexBuffers[i]);
                    G->mVisitedVertexBuffers.insert(currentVertexBuffers[i]);
                }
                else
                {
                    currentVertexBuffers[i] = 0;
                }
            }
        }
        LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
    }
}

void STDMETHODCALLTYPE HackerContext::GSSetConstantBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers)
{
    origContext1->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::GSSetShader(
    ID3D11GeometryShader*       pShader,
    ID3D11ClassInstance* const* ppClassInstances,
    UINT                        NumClassInstances)
{
    SetShader<ID3D11GeometryShader, &ID3D11DeviceContext::GSSetShader>(pShader, ppClassInstances, NumClassInstances, &G->mVisitedGeometryShaders, G->mSelectedGeometryShader, &currentGeometryShader, &currentGeometryShaderHandle);
}

void STDMETHODCALLTYPE HackerContext::IASetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY Topology)
{
    origContext1->IASetPrimitiveTopology(Topology);
}

void STDMETHODCALLTYPE HackerContext::VSSetSamplers(
    UINT                       StartSlot,
    UINT                       NumSamplers,
    ID3D11SamplerState* const* ppSamplers)
{
    origContext1->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::PSSetSamplers(
    UINT                       StartSlot,
    UINT                       NumSamplers,
    ID3D11SamplerState* const* ppSamplers)
{
    origContext1->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::Begin(
    ID3D11Asynchronous* pAsync)
{
    origContext1->Begin(pAsync);
}

void STDMETHODCALLTYPE HackerContext::End(
    ID3D11Asynchronous* pAsync)
{
    origContext1->End(pAsync);
}

HRESULT STDMETHODCALLTYPE HackerContext::GetData(
    ID3D11Asynchronous* pAsync,
    void*               pData,
    UINT                DataSize,
    UINT                GetDataFlags)
{
    return origContext1->GetData(pAsync, pData, DataSize, GetDataFlags);
}

void STDMETHODCALLTYPE HackerContext::SetPredication(
    ID3D11Predicate* pPredicate,
    BOOL             PredicateValue)
{
    return origContext1->SetPredication(pPredicate, PredicateValue);
}

void STDMETHODCALLTYPE HackerContext::GSSetShaderResources(
    UINT                             StartSlot,
    UINT                             NumViews,
    ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
    SetShaderResources<&ID3D11DeviceContext::GSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::GSSetSamplers(
    UINT                       StartSlot,
    UINT                       NumSamplers,
    ID3D11SamplerState* const* ppSamplers)
{
    origContext1->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::OMSetBlendState(
    ID3D11BlendState* pBlendState,
    const FLOAT       BlendFactor[4],
    UINT              SampleMask)
{
    origContext1->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

void STDMETHODCALLTYPE HackerContext::OMSetDepthStencilState(
    ID3D11DepthStencilState* pDepthStencilState,
    UINT                     StencilRef)
{
    origContext1->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}

void STDMETHODCALLTYPE HackerContext::SOSetTargets(
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppSOTargets,
    const UINT*          pOffsets)
{
    origContext1->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}

bool HackerContext::BeforeDispatch(
    dispatch_context* context)
{
    if (G->hunting == HUNTING_MODE_ENABLED)
    {
        if (G->DumpUsage)
            RecordComputeShaderStats();

        if (currentComputeShader == G->mSelectedComputeShader)
        {
            if (G->marking_mode == MarkingMode::SKIP)
                return false;
        }
    }

    if (!G->fix_enabled)
        return true;

    DeferredShaderReplacementBeforeDispatch();

    // Override settings?
    if (!G->mShaderOverrideMap.empty())
    {
        ShaderOverrideMap::iterator i;

        i = lookup_shaderoverride(currentComputeShader);
        if (i != G->mShaderOverrideMap.end())
        {
            context->post_commands = &i->second.post_command_list;
            // XXX: Not using ProcessShaderOverride() as a
            // lot of it's logic doesn't really apply to
            // compute shaders. The main thing we care
            // about is the command list, so just run that:
            run_command_list(hackerDevice, this, &i->second.command_list, &context->call_info, false);
            return !context->call_info.skip;
        }
    }

    return true;
}

void HackerContext::AfterDispatch(
    dispatch_context* context)
{
    if (context->post_commands)
        run_command_list(hackerDevice, this, context->post_commands, &context->call_info, true);
}

void STDMETHODCALLTYPE HackerContext::Dispatch(
    UINT ThreadGroupCountX,
    UINT ThreadGroupCountY,
    UINT ThreadGroupCountZ)
{
    dispatch_context context { ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ };

    if (BeforeDispatch(&context))
        origContext1->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
    else
        Profiling::skipped_draw_calls++;

    AfterDispatch(&context);
}

void STDMETHODCALLTYPE HackerContext::DispatchIndirect(
    ID3D11Buffer* pBufferForArgs,
    UINT          AlignedByteOffsetForArgs)
{
    dispatch_context context { &pBufferForArgs, AlignedByteOffsetForArgs };

    if (BeforeDispatch(&context))
        origContext1->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    else
        Profiling::skipped_draw_calls++;

    AfterDispatch(&context);
}

void STDMETHODCALLTYPE HackerContext::RSSetState(
    ID3D11RasterizerState* pRasterizerState)
{
    origContext1->RSSetState(pRasterizerState);
}

void STDMETHODCALLTYPE HackerContext::RSSetViewports(
    UINT                  NumViewports,
    const D3D11_VIEWPORT* pViewports)
{
    origContext1->RSSetViewports(NumViewports, pViewports);
}

void STDMETHODCALLTYPE HackerContext::RSSetScissorRects(
    UINT              NumRects,
    const D3D11_RECT* pRects)
{
    origContext1->RSSetScissorRects(NumRects, pRects);
}

/*
 * Used for CryEngine games like Lichdom that copy a 2D rectangle from the
 * colour render target to a texture as an input for transparent refraction
 * effects. Expands the rectange to the full width.
 */
bool HackerContext::ExpandRegionCopy(
    ID3D11Resource*  dst_resource,
    UINT             dst_x,
    UINT             dst_y,
    ID3D11Resource*  src_resource,
    const D3D11_BOX* src_box,
    UINT*            replace_dst_x,
    D3D11_BOX*       replace_box)
{
    ID3D11Texture2D*             src_tex = static_cast<ID3D11Texture2D*>(src_resource);
    ID3D11Texture2D*             dst_tex = static_cast<ID3D11Texture2D*>(dst_resource);
    D3D11_TEXTURE2D_DESC         src_desc, dst_desc;
    D3D11_RESOURCE_DIMENSION     src_dim, dst_dim;
    uint32_t                     src_hash, dst_hash;
    TextureOverrideMap::iterator i;

    if (!src_resource || !dst_resource || !src_box)
        return false;

    src_resource->GetType(&src_dim);
    dst_resource->GetType(&dst_dim);
    if (src_dim != dst_dim || src_dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        return false;

    src_tex->GetDesc(&src_desc);
    dst_tex->GetDesc(&dst_desc);
    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
    {
        src_hash = GetResourceHash(src_tex);
        dst_hash = GetResourceHash(dst_tex);
    }
    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);

    LOG_DEBUG("CopySubresourceRegion %08lx (%u:%u x %u:%u / %u x %u) -> %08lx (%u x %u / %u x %u)\n", src_hash, src_box->left, src_box->right, src_box->top, src_box->bottom, src_desc.Width, src_desc.Height, dst_hash, dst_x, dst_y, dst_desc.Width, dst_desc.Height);

    i = lookup_textureoverride(dst_hash);
    if (i == G->mTextureOverrideMap.end())
        return false;

    if (!i->second.begin()->expand_region_copy)
        return false;

    memcpy(replace_box, src_box, sizeof(D3D11_BOX));
    *replace_dst_x     = 0;
    replace_box->left  = 0;
    replace_box->right = dst_desc.Width;

    return true;
}

void STDMETHODCALLTYPE HackerContext::CopySubresourceRegion(
    ID3D11Resource*  pDstResource,
    UINT             DstSubresource,
    UINT             DstX,
    UINT             DstY,
    UINT             DstZ,
    ID3D11Resource*  pSrcResource,
    UINT             SrcSubresource,
    const D3D11_BOX* pSrcBox)
{
    D3D11_BOX replace_src_box;
    UINT      replace_DstX = DstX;

    if (G->hunting && G->track_texture_updates != 2)
    {  // Any hunting mode - want to catch hash contamination even while soft disabled
        MarkResourceHashContaminated(pDstResource, DstSubresource, pSrcResource, SrcSubresource, 'S', DstX, DstY, DstZ, pSrcBox);
    }

    if (ExpandRegionCopy(pDstResource, DstX, DstY, pSrcResource, pSrcBox, &replace_DstX, &replace_src_box))
        pSrcBox = &replace_src_box;

    origContext1->CopySubresourceRegion(pDstResource, DstSubresource, replace_DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);

    // We only update the destination resource hash when the entire
    // subresource 0 is updated and pSrcBox is NULL. We could check if the
    // pSrcBox fills the entire resource, but if the game is using pSrcBox
    // it stands to reason that it won't always fill the entire resource
    // and the hashes might be less predictable. Possibly something to
    // enable as an option in the future if there is a proven need.
    if (G->track_texture_updates == 1 && DstSubresource == 0 && DstX == 0 && DstY == 0 && DstZ == 0 && pSrcBox == nullptr)
        PropagateResourceHash(pDstResource, pSrcResource);
}

void STDMETHODCALLTYPE HackerContext::CopyResource(
    ID3D11Resource* pDstResource,
    ID3D11Resource* pSrcResource)
{
    if (G->hunting && G->track_texture_updates != 2)
    {  // Any hunting mode - want to catch hash contamination even while soft disabled
        MarkResourceHashContaminated(pDstResource, 0, pSrcResource, 0, 'C', 0, 0, 0, nullptr);
    }

    origContext1->CopyResource(pDstResource, pSrcResource);

    if (G->track_texture_updates == 1)
        PropagateResourceHash(pDstResource, pSrcResource);
}

void STDMETHODCALLTYPE HackerContext::UpdateSubresource(
    ID3D11Resource*  pDstResource,
    UINT             DstSubresource,
    const D3D11_BOX* pDstBox,
    const void*      pSrcData,
    UINT             SrcRowPitch,
    UINT             SrcDepthPitch)
{
    if (G->hunting && G->track_texture_updates != 2)
    {  // Any hunting mode - want to catch hash contamination even while soft disabled
        MarkResourceHashContaminated(pDstResource, DstSubresource, nullptr, 0, 'U', 0, 0, 0, nullptr);
    }

    origContext1->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

    // We only update the destination resource hash when the entire
    // subresource 0 is updated and pDstBox is NULL. We could check if the
    // pDstBox fills the entire resource, but if the game is using pDstBox
    // it stands to reason that it won't always fill the entire resource
    // and the hashes might be less predictable. Possibly something to
    // enable as an option in the future if there is a proven need.
    if (G->track_texture_updates == 1 && DstSubresource == 0 && pDstBox == nullptr)
        UpdateResourceHashFromCPU(pDstResource, pSrcData, SrcRowPitch, SrcDepthPitch);
}

void STDMETHODCALLTYPE HackerContext::CopyStructureCount(
    ID3D11Buffer*              pDstBuffer,
    UINT                       DstAlignedByteOffset,
    ID3D11UnorderedAccessView* pSrcView)
{
    origContext1->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}

void STDMETHODCALLTYPE HackerContext::ClearUnorderedAccessViewUint(
    ID3D11UnorderedAccessView* pUnorderedAccessView,
    const UINT                 Values[4])
{
    run_view_command_list(hackerDevice, this, &G->clear_uav_uint_command_list, pUnorderedAccessView, false);
    origContext1->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
    run_view_command_list(hackerDevice, this, &G->post_clear_uav_uint_command_list, pUnorderedAccessView, true);
}

void STDMETHODCALLTYPE HackerContext::ClearUnorderedAccessViewFloat(
    ID3D11UnorderedAccessView* pUnorderedAccessView,
    const FLOAT                Values[4])
{
    run_view_command_list(hackerDevice, this, &G->clear_uav_float_command_list, pUnorderedAccessView, false);
    origContext1->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
    run_view_command_list(hackerDevice, this, &G->post_clear_uav_float_command_list, pUnorderedAccessView, true);
}

void STDMETHODCALLTYPE HackerContext::ClearDepthStencilView(
    ID3D11DepthStencilView* pDepthStencilView,
    UINT                    ClearFlags,
    FLOAT                   Depth,
    UINT8                   Stencil)
{
    run_view_command_list(hackerDevice, this, &G->clear_dsv_command_list, pDepthStencilView, false);
    origContext1->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
    run_view_command_list(hackerDevice, this, &G->post_clear_dsv_command_list, pDepthStencilView, true);
}

void STDMETHODCALLTYPE HackerContext::GenerateMips(
    ID3D11ShaderResourceView* pShaderResourceView)
{
    origContext1->GenerateMips(pShaderResourceView);
}

void STDMETHODCALLTYPE HackerContext::SetResourceMinLOD(
    ID3D11Resource* pResource,
    FLOAT           MinLOD)
{
    origContext1->SetResourceMinLOD(pResource, MinLOD);
}

FLOAT STDMETHODCALLTYPE HackerContext::GetResourceMinLOD(
    ID3D11Resource* pResource)
{
    FLOAT ret = origContext1->GetResourceMinLOD(pResource);

    return ret;
}

void STDMETHODCALLTYPE HackerContext::ResolveSubresource(
    ID3D11Resource* pDstResource,
    UINT            DstSubresource,
    ID3D11Resource* pSrcResource,
    UINT            SrcSubresource,
    DXGI_FORMAT     Format)
{
    origContext1->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

void STDMETHODCALLTYPE HackerContext::ExecuteCommandList(
    ID3D11CommandList* pCommandList,
    BOOL               RestoreContextState)
{
    if (G->deferred_contexts_enabled)
        origContext1->ExecuteCommandList(pCommandList, RestoreContextState);

    if (!RestoreContextState)
    {
        // This is equivalent to calling ClearState() afterwards, so we
        // need to rebind the 3DMigoto resources now. See also
        // FinishCommandList's RestoreDeferredContextState:
        Bind3DMigotoResources();
    }
}

void STDMETHODCALLTYPE HackerContext::HSSetShaderResources(
    UINT                             StartSlot,
    UINT                             NumViews,
    ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
    SetShaderResources<&ID3D11DeviceContext::HSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::HSSetShader(
    ID3D11HullShader*           pHullShader,
    ID3D11ClassInstance* const* ppClassInstances,
    UINT                        NumClassInstances)
{
    SetShader<ID3D11HullShader, &ID3D11DeviceContext::HSSetShader>(pHullShader, ppClassInstances, NumClassInstances, &G->mVisitedHullShaders, G->mSelectedHullShader, &currentHullShader, &currentHullShaderHandle);
}

void STDMETHODCALLTYPE HackerContext::HSSetSamplers(
    UINT                       StartSlot,
    UINT                       NumSamplers,
    ID3D11SamplerState* const* ppSamplers)
{
    origContext1->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::HSSetConstantBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers)
{
    origContext1->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::DSSetShaderResources(
    UINT                             StartSlot,
    UINT                             NumViews,
    ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
    SetShaderResources<&ID3D11DeviceContext::DSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::DSSetShader(
    ID3D11DomainShader*         pDomainShader,
    ID3D11ClassInstance* const* ppClassInstances,
    UINT                        NumClassInstances)
{
    SetShader<ID3D11DomainShader, &ID3D11DeviceContext::DSSetShader>(pDomainShader, ppClassInstances, NumClassInstances, &G->mVisitedDomainShaders, G->mSelectedDomainShader, &currentDomainShader, &currentDomainShaderHandle);
}

void STDMETHODCALLTYPE HackerContext::DSSetSamplers(
    UINT                       StartSlot,
    UINT                       NumSamplers,
    ID3D11SamplerState* const* ppSamplers)
{
    origContext1->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::DSSetConstantBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers)
{
    origContext1->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::CSSetShaderResources(
    UINT                             StartSlot,
    UINT                             NumViews,
    ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
    SetShaderResources<&ID3D11DeviceContext::CSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::CSSetUnorderedAccessViews(
    UINT                              StartSlot,
    UINT                              NumUAVs,
    ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                       pUAVInitialCounts)
{
    if (ppUnorderedAccessViews)
    {
        // TODO: Record stats on unordered access view usage
        for (UINT i = 0; i < NumUAVs; ++i)
        {
            if (!ppUnorderedAccessViews[i])
                continue;
            // TODO: Record stats
        }
    }

    origContext1->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

// C++ function template of common code shared by all XXSetShader functions:
template <
    class ID3D11Shader,
    void (__stdcall ID3D11DeviceContext::*OrigSetShader)(
        ID3D11Shader*               pShader,
        ID3D11ClassInstance* const* ppClassInstances,
        UINT                        NumClassInstances)>
void STDMETHODCALLTYPE HackerContext::SetShader(
    ID3D11Shader*               shader,
    ID3D11ClassInstance* const* class_instances,
    UINT                        num_class_instances,
    std::set<UINT64>*           visited_shaders,
    UINT64                      selected_shader,
    UINT64*                     current_shader_hash,
    ID3D11Shader**              current_shader_handle)
{
    ID3D11Shader* repl_shader = shader;

    // Always update the current shader handle no matter what so we can
    // reliably check if a shader of a given type is bound and for certain
    // types of old style filtering:
    *current_shader_handle = shader;

    if (shader)
    {
        // Store as current shader. Need to do this even while
        // not hunting for ShaderOverride section in BeforeDraw
        // We also set the current shader hash, but as an optimization,
        // we skip the lookup if there are no ShaderOverride The
        // lookup/find takes measurable amounts of CPU time.
        //
        // grumble grumble this optimisation caught me out *TWICE* grumble grumble -DSS
        if (!G->mShaderOverrideMap.empty() || !shader_regex_groups.empty() || (G->hunting == HUNTING_MODE_ENABLED))
        {
            ShaderMap::iterator i = lookup_shader_hash(shader);
            if (i != G->mShaders.end())
            {
                *current_shader_hash = i->second;
                LOG_DEBUG("  shader found: handle = %p, hash = %016I64x\n", *current_shader_handle, *current_shader_hash);

                if ((G->hunting == HUNTING_MODE_ENABLED) && visited_shaders)
                {
                    ENTER_CRITICAL_SECTION(&G->mCriticalSection);
                    {
                        visited_shaders->insert(i->second);
                    }
                    LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
                }
            }
            else
            {
                LOG_DEBUG("  shader %p not found\n", shader);
            }
        }
        else
        {
            // Not accurate, but if we have a bug where we
            // reference this at least make sure we don't use the
            // *wrong* hash
            *current_shader_hash = 0;
        }

        // If the shader has been live reloaded from ShaderFixes, use the new one
        // No longer conditional on G->hunting now that hunting may be soft enabled via key binding
        ShaderReloadMap::iterator it = lookup_reloaded_shader(shader);
        if (it != G->mReloadedShaders.end() && it->second.replacement != nullptr)
        {
            LOG_DEBUG("  shader replaced by: %p\n", it->second.replacement);

            // It might make sense to Release() the original shader, to recover memory on GPU
            //   -Bo3b
            // No - we're already not incrementing the refcount since we don't bind it, and if we
            // released the original it would mean the game has an invalid pointer and can crash.
            // I wouldn't worry too much about GPU memory usage beyond leaks - the driver has a
            // full virtual memory system and can swap rarely used resources out to system memory.
            // If we did want to do better here we could return a wrapper object when the game
            // creates the original shader, and manage original/replaced/reverted/etc from there.
            //   -DSS
            repl_shader = static_cast<ID3D11Shader*>(it->second.replacement);
        }

        if (G->hunting == HUNTING_MODE_ENABLED)
        {
            // Replacement map.
            if (G->marking_mode == MarkingMode::ORIGINAL || !G->fix_enabled)
            {
                ShaderReplacementMap::iterator j = lookup_original_shader(shader);
                if ((selected_shader == *current_shader_hash || !G->fix_enabled) && j != G->mOriginalShaders.end())
                {
                    repl_shader = static_cast<ID3D11Shader*>(j->second);
                }
            }
        }
    }
    else
    {
        *current_shader_hash = 0;
    }

    // Call through to original XXSetShader, but pShader may have been replaced.
    (origContext1->*OrigSetShader)(repl_shader, class_instances, num_class_instances);
}

void STDMETHODCALLTYPE HackerContext::CSSetShader(
    ID3D11ComputeShader*        pComputeShader,
    ID3D11ClassInstance* const* ppClassInstances,
    UINT                        NumClassInstances)
{
    SetShader<ID3D11ComputeShader, &ID3D11DeviceContext::CSSetShader>(pComputeShader, ppClassInstances, NumClassInstances, &G->mVisitedComputeShaders, G->mSelectedComputeShader, &currentComputeShader, &currentComputeShaderHandle);
}

void STDMETHODCALLTYPE HackerContext::CSSetSamplers(
    UINT                       StartSlot,
    UINT                       NumSamplers,
    ID3D11SamplerState* const* ppSamplers)
{
    origContext1->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::CSSetConstantBuffers(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers)
{
    origContext1->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::VSGetConstantBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers)
{
    origContext1->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::PSGetShaderResources(
    UINT                       StartSlot,
    UINT                       NumViews,
    ID3D11ShaderResourceView** ppShaderResourceViews)
{
    origContext1->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::PSGetShader(
    ID3D11PixelShader**   ppPixelShader,
    ID3D11ClassInstance** ppClassInstances,
    UINT*                 pNumClassInstances)
{
    origContext1->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
}

void STDMETHODCALLTYPE HackerContext::PSGetSamplers(
    UINT                 StartSlot,
    UINT                 NumSamplers,
    ID3D11SamplerState** ppSamplers)
{
    origContext1->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::VSGetShader(
    ID3D11VertexShader**  ppVertexShader,
    ID3D11ClassInstance** ppClassInstances,
    UINT*                 pNumClassInstances)
{
    origContext1->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);

    // Todo: At GetShader, we need to return the original shader if it's been reloaded.
}

void STDMETHODCALLTYPE HackerContext::PSGetConstantBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers)
{
    origContext1->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::IAGetInputLayout(
    ID3D11InputLayout** ppInputLayout)
{
    origContext1->IAGetInputLayout(ppInputLayout);
}

void STDMETHODCALLTYPE HackerContext::IAGetVertexBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppVertexBuffers,
    UINT*          pStrides,
    UINT*          pOffsets)
{
    origContext1->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

void STDMETHODCALLTYPE HackerContext::IAGetIndexBuffer(
    ID3D11Buffer** pIndexBuffer,
    DXGI_FORMAT*   Format,
    UINT*          Offset)
{
    origContext1->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}

void STDMETHODCALLTYPE HackerContext::GSGetConstantBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers)
{
    origContext1->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::GSGetShader(
    ID3D11GeometryShader** ppGeometryShader,
    ID3D11ClassInstance**  ppClassInstances,
    UINT*                  pNumClassInstances)
{
    origContext1->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
}

void STDMETHODCALLTYPE HackerContext::IAGetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY* pTopology)
{
    origContext1->IAGetPrimitiveTopology(pTopology);
}

void STDMETHODCALLTYPE HackerContext::VSGetShaderResources(
    UINT                       StartSlot,
    UINT                       NumViews,
    ID3D11ShaderResourceView** ppShaderResourceViews)
{
    origContext1->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::VSGetSamplers(
    UINT                 StartSlot,
    UINT                 NumSamplers,
    ID3D11SamplerState** ppSamplers)
{
    origContext1->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::GetPredication(
    ID3D11Predicate** ppPredicate,
    BOOL*             pPredicateValue)
{
    origContext1->GetPredication(ppPredicate, pPredicateValue);
}

void STDMETHODCALLTYPE HackerContext::GSGetShaderResources(
    UINT                       StartSlot,
    UINT                       NumViews,
    ID3D11ShaderResourceView** ppShaderResourceViews)
{
    origContext1->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::GSGetSamplers(
    UINT                 StartSlot,
    UINT                 NumSamplers,
    ID3D11SamplerState** ppSamplers)
{
    origContext1->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::OMGetRenderTargets(
    UINT                     NumViews,
    ID3D11RenderTargetView** ppRenderTargetViews,
    ID3D11DepthStencilView** ppDepthStencilView)
{
    origContext1->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}

void STDMETHODCALLTYPE HackerContext::OMGetRenderTargetsAndUnorderedAccessViews(
    UINT                        NumRTVs,
    ID3D11RenderTargetView**    ppRenderTargetViews,
    ID3D11DepthStencilView**    ppDepthStencilView,
    UINT                        UAVStartSlot,
    UINT                        NumUAVs,
    ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
    origContext1->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}

void STDMETHODCALLTYPE HackerContext::OMGetBlendState(
    ID3D11BlendState** ppBlendState,
    FLOAT              BlendFactor[4],
    UINT*              pSampleMask)
{
    origContext1->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}

void STDMETHODCALLTYPE HackerContext::OMGetDepthStencilState(
    ID3D11DepthStencilState** ppDepthStencilState,
    UINT*                     pStencilRef)
{
    origContext1->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}

void STDMETHODCALLTYPE HackerContext::SOGetTargets(
    UINT           NumBuffers,
    ID3D11Buffer** ppSOTargets)
{
    origContext1->SOGetTargets(NumBuffers, ppSOTargets);
}

void STDMETHODCALLTYPE HackerContext::RSGetState(
    ID3D11RasterizerState** ppRasterizerState)
{
    origContext1->RSGetState(ppRasterizerState);
}

void STDMETHODCALLTYPE HackerContext::RSGetViewports(
    UINT*           pNumViewports,
    D3D11_VIEWPORT* pViewports)
{
    origContext1->RSGetViewports(pNumViewports, pViewports);
}

void STDMETHODCALLTYPE HackerContext::RSGetScissorRects(
    UINT*       pNumRects,
    D3D11_RECT* pRects)
{
    origContext1->RSGetScissorRects(pNumRects, pRects);
}

void STDMETHODCALLTYPE HackerContext::HSGetShaderResources(
    UINT                       StartSlot,
    UINT                       NumViews,
    ID3D11ShaderResourceView** ppShaderResourceViews)
{
    origContext1->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::HSGetShader(
    ID3D11HullShader**    ppHullShader,
    ID3D11ClassInstance** ppClassInstances,
    UINT*                 pNumClassInstances)
{
    origContext1->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
}

void STDMETHODCALLTYPE HackerContext::HSGetSamplers(
    UINT                 StartSlot,
    UINT                 NumSamplers,
    ID3D11SamplerState** ppSamplers)
{
    origContext1->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::HSGetConstantBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers)
{
    origContext1->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::DSGetShaderResources(
    UINT                       StartSlot,
    UINT                       NumViews,
    ID3D11ShaderResourceView** ppShaderResourceViews)
{
    origContext1->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::DSGetShader(
    ID3D11DomainShader**  ppDomainShader,
    ID3D11ClassInstance** ppClassInstances,
    UINT*                 pNumClassInstances)
{
    origContext1->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
}

void STDMETHODCALLTYPE HackerContext::DSGetSamplers(
    UINT                 StartSlot,
    UINT                 NumSamplers,
    ID3D11SamplerState** ppSamplers)
{
    origContext1->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::DSGetConstantBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers)
{
    origContext1->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::CSGetShaderResources(
    UINT                       StartSlot,
    UINT                       NumViews,
    ID3D11ShaderResourceView** ppShaderResourceViews)
{
    origContext1->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::CSGetUnorderedAccessViews(
    UINT                        StartSlot,
    UINT                        NumUAVs,
    ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
    origContext1->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
}

void STDMETHODCALLTYPE HackerContext::CSGetShader(
    ID3D11ComputeShader** ppComputeShader,
    ID3D11ClassInstance** ppClassInstances,
    UINT*                 pNumClassInstances)
{
    origContext1->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
}

void STDMETHODCALLTYPE HackerContext::CSGetSamplers(
    UINT                 StartSlot,
    UINT                 NumSamplers,
    ID3D11SamplerState** ppSamplers)
{
    origContext1->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE HackerContext::CSGetConstantBuffers(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers)
{
    origContext1->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE HackerContext::ClearState()
{
    origContext1->ClearState();

    // ClearState() will unbind StereoParams and IniParams, so we need to
    // rebind them now:
    Bind3DMigotoResources();
}

void STDMETHODCALLTYPE HackerContext::Flush()
{
    origContext1->Flush();
}

D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE HackerContext::GetType()
{
    return origContext1->GetType();
}

UINT STDMETHODCALLTYPE HackerContext::GetContextFlags()
{
    return origContext1->GetContextFlags();
}

HRESULT STDMETHODCALLTYPE HackerContext::FinishCommandList(
    BOOL                RestoreDeferredContextState,
    ID3D11CommandList** ppCommandList)
{
    BOOL ret = origContext1->FinishCommandList(RestoreDeferredContextState, ppCommandList);

    if (!RestoreDeferredContextState)
    {
        // This is equivalent to calling ClearState() afterwards, so we
        // need to rebind the 3DMigoto resources now. See also
        // ExecuteCommandList's RestoreContextState:
        Bind3DMigotoResources();
    }

    return ret;
}

// -----------------------------------------------------------------------------------------------

template <
    void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(
        UINT                             StartSlot,
        UINT                             NumViews,
        ID3D11ShaderResourceView* const* ppShaderResourceViews)>
void HackerContext::BindStereoResources()
{
    if (!hackerDevice)
    {
        LOG_INFO("  error querying device. Can't set NVidia stereo parameter texture.\n");
        return;
    }

    // Set NVidia stereo texture.
    if (hackerDevice->stereoResourceView && G->StereoParamsReg >= 0)
    {
        LOG_DEBUG("  adding NVidia stereo parameter texture to shader resources in slot %i.\n", G->StereoParamsReg);

        (origContext1->*OrigSetShaderResources)(G->StereoParamsReg, 1, &hackerDevice->stereoResourceView);
    }

    // Set constants from ini file if they exist
    if (hackerDevice->iniResourceView && G->IniParamsReg >= 0)
    {
        LOG_DEBUG("  adding ini constants as texture to shader resources in slot %i.\n", G->IniParamsReg);

        (origContext1->*OrigSetShaderResources)(G->IniParamsReg, 1, &hackerDevice->iniResourceView);
    }
}

void HackerContext::Bind3DMigotoResources()
{
    // Third generation of binding the stereo resources. We used to do this
    // in the SetShader calls, but that was problematic in certain games
    // like Akiba's Trip that would unbind all resources between then and
    // the following draw call. We then did this in the draw/dispatch
    // calls, which was very succesful, but cost a few percent CPU time
    // which can add up to a significant drop in framerate in CPU bound
    // games.
    //
    // Our new strategy is to bind them when the context is created, then
    // make sure that they stay bound in the SetShaderResource() calls. We
    // do this after the SetHackerDevice call because we need hackerDevice
    BindStereoResources<&ID3D11DeviceContext::VSSetShaderResources>();
    BindStereoResources<&ID3D11DeviceContext::HSSetShaderResources>();
    BindStereoResources<&ID3D11DeviceContext::DSSetShaderResources>();
    BindStereoResources<&ID3D11DeviceContext::GSSetShaderResources>();
    BindStereoResources<&ID3D11DeviceContext::PSSetShaderResources>();
    BindStereoResources<&ID3D11DeviceContext::CSSetShaderResources>();
}

void HackerContext::InitIniParams()
{
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT                  hr;

    // Only the immediate context is allowed to perform [Constants]
    // initialisation, as otherwise creating a deferred context could
    // clobber any changes since then. The only exception I can think of is
    // if a deferred context is created before the immediate context, but
    // an immediate context will have to be created before the frame ends
    // to execute that deferred context, so the worst case is that a
    // deferred context may run for one frame without the constants command
    // list being run. This situation is unlikely, and even if it does
    // happen is unlikely to cause any issues in practice, so let's not try
    // to do anything heroic to deal with it.
    if (origContext1->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
    {
        LOG_INFO("BUG: InitIniParams called on a deferred context\n");
        double_beep_exit();
    }

    // The command list only changes ini params that are defined, but for
    // consistency we want all other ini params to be initialised as well:
    memset(G->iniParams.data(), 0, sizeof(DirectX::XMFLOAT4) * G->iniParams.size());

    // Update the IniParams resource on the GPU before executing the
    // [Constants] command list. This ensures that it does get updated,
    // even if the [Constants] command list doesn't initialise any IniParam
    // (to non-zero), and we do this first in case [Constants] runs any
    // custom shaders that may check IniParams. This is a bit wasteful
    // since in most cases we will update the resource twice in a row, and
    // the alternative is setting a flag to force update_params in the
    // [Constants] command list, but this is a cold path so a little extra
    // overhead won't matter and I don't want to forget about this if
    // further command list optimisations cause [Constants] to bail out
    // early and not consider update_params at all.
    if (hackerDevice->iniTexture)
    {
        hr = origContext1->Map(hackerDevice->iniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped_resource.pData, G->iniParams.data(), sizeof(DirectX::XMFLOAT4) * G->iniParams.size());
            origContext1->Unmap(hackerDevice->iniTexture, 0);
            Profiling::iniparams_updates++;
        }
        else
        {
            LOG_INFO("InitIniParams: Map failed\n");
        }
    }

    // The command list will take care of initialising any non-zero values:
    run_command_list(hackerDevice, this, &G->constants_command_list, nullptr, false);
    // We don't consider persistent globals set in the [Constants] pre
    // command list as making the user config file dirty, because this
    // command list includes the user config file's [Constants] itself.
    // We clear only the low bit here, so that this may be overridden if
    // an invalid value is found that is scheduled to be removed:
    G->user_config_dirty &= ~1;
    run_command_list(hackerDevice, this, &G->post_constants_command_list, nullptr, true);

    // Only want to run [Constants] on initial load and config reload. In
    // some games we see additional DirectX devices & contexts being
    // created (e.g. this happens in DOAXVV when displaying any news) and
    // we don't want to re-run Constants at that time since it may reset
    // variables and resources back to default.
    //
    // This may also happen in some game's init paths if they create some
    // throwaway devices (e.g. to probe the system's capabilities) or
    // potentially due to some overlays or other 3rd party tools. In most
    // of those cases running [Constants] once at launch still should be
    // the preferred option so that variables don't get reset unexpectedly,
    // though it is conceivable that in some cases we might want to reinit
    // custom resources, shaders, discard caches and re-run [Constants] at
    // that time. That could simplify cases where we create a custom
    // resource on one device and later need it on another (which we now do
    // through on-demand inter-device transfers - which has some
    // limitations), but I might wait until we have a proven need for that.
    G->constants_run = true;
}

// This function makes sure that the StereoParams and IniParams resources
// remain pinned whenever the game assigns shader resources:
template <
    void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(
        UINT                             StartSlot,
        UINT                             NumViews,
        ID3D11ShaderResourceView* const* ppShaderResourceViews)>
void HackerContext::SetShaderResources(
    UINT                             start_slot,
    UINT                             num_views,
    ID3D11ShaderResourceView* const* shader_resource_views)
{
    ID3D11ShaderResourceView** override_srvs = nullptr;

    if (!hackerDevice)
        return;

    if (hackerDevice->stereoResourceView && G->StereoParamsReg >= 0)
    {
        if (num_views > G->StereoParamsReg - start_slot)
        {
            LOG_DEBUG("  Game attempted to unbind StereoParams, pinning in slot %i\n", G->StereoParamsReg);
            override_srvs = new ID3D11ShaderResourceView*[num_views];
            memcpy(override_srvs, shader_resource_views, sizeof(ID3D11ShaderResourceView*) * num_views);
            override_srvs[G->StereoParamsReg - start_slot] = hackerDevice->stereoResourceView;
        }
    }

    if (hackerDevice->iniResourceView && G->IniParamsReg >= 0)
    {
        if (num_views > G->IniParamsReg - start_slot)
        {
            LOG_DEBUG("  Game attempted to unbind IniParams, pinning in slot %i\n", G->IniParamsReg);
            if (!override_srvs)
            {
                override_srvs = new ID3D11ShaderResourceView*[num_views];
                memcpy(override_srvs, shader_resource_views, sizeof(ID3D11ShaderResourceView*) * num_views);
            }
            override_srvs[G->IniParamsReg - start_slot] = hackerDevice->iniResourceView;
        }
    }

    if (override_srvs)
    {
        (origContext1->*OrigSetShaderResources)(start_slot, num_views, override_srvs);
        delete[] override_srvs;
    }
    else
    {
        (origContext1->*OrigSetShaderResources)(start_slot, num_views, shader_resource_views);
    }
}

// The rest of these methods are all the primary code for the tool, Direct3D calls that we override
// in order to replace or modify shaders.

void STDMETHODCALLTYPE HackerContext::VSSetShader(
    ID3D11VertexShader*         pVertexShader,
    ID3D11ClassInstance* const* ppClassInstances,
    UINT                        NumClassInstances)
{
    SetShader<ID3D11VertexShader, &ID3D11DeviceContext::VSSetShader>(pVertexShader, ppClassInstances, NumClassInstances, &G->mVisitedVertexShaders, G->mSelectedVertexShader, &currentVertexShader, &currentVertexShaderHandle);
}

void STDMETHODCALLTYPE HackerContext::PSSetShaderResources(
    UINT                             StartSlot,
    UINT                             NumViews,
    ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
    SetShaderResources<&ID3D11DeviceContext::PSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::PSSetShader(
    ID3D11PixelShader*          pPixelShader,
    ID3D11ClassInstance* const* ppClassInstances,
    UINT                        NumClassInstances)
{
    SetShader<ID3D11PixelShader, &ID3D11DeviceContext::PSSetShader>(pPixelShader, ppClassInstances, NumClassInstances, &G->mVisitedPixelShaders, G->mSelectedPixelShader, &currentPixelShader, &currentPixelShaderHandle);

    if (pPixelShader)
    {
        // Set custom depth texture.
        if (hackerDevice->zBufferResourceView)
        {
            LOG_DEBUG("  adding Z buffer to shader resources in slot 126.\n");

            origContext1->PSSetShaderResources(126, 1, &hackerDevice->zBufferResourceView);
        }
    }
}

void STDMETHODCALLTYPE HackerContext::DrawIndexed(
    UINT IndexCount,
    UINT StartIndexLocation,
    INT  BaseVertexLocation)
{
    draw_context c = draw_context(DrawCall::DrawIndexed, 0, IndexCount, 0, BaseVertexLocation, StartIndexLocation, 0, nullptr, 0);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::Draw(
    UINT VertexCount,
    UINT StartVertexLocation)
{
    draw_context c = draw_context(DrawCall::Draw, VertexCount, 0, 0, StartVertexLocation, 0, 0, nullptr, 0);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->Draw(VertexCount, StartVertexLocation);
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::IASetIndexBuffer(
    ID3D11Buffer* pIndexBuffer,
    DXGI_FORMAT   Format,
    UINT          Offset)
{
    origContext1->IASetIndexBuffer(pIndexBuffer, Format, Offset);

    // This is only used for index buffer hunting nowadays since the
    // command list checks the hash on demand only when it is needed
    currentIndexBuffer = 0;
    if (pIndexBuffer && G->hunting == HUNTING_MODE_ENABLED)
    {
        currentIndexBuffer = GetResourceHash(pIndexBuffer);
        if (currentIndexBuffer)
        {
            // When hunting, save this as a visited index buffer to cycle through.
            ENTER_CRITICAL_SECTION(&G->mCriticalSection);
            {
                G->mVisitedIndexBuffers.insert(currentIndexBuffer);
            }
            LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
        }
    }
}

void STDMETHODCALLTYPE HackerContext::DrawIndexedInstanced(
    UINT IndexCountPerInstance,
    UINT InstanceCount,
    UINT StartIndexLocation,
    INT  BaseVertexLocation,
    UINT StartInstanceLocation)
{
    draw_context c = draw_context(DrawCall::DrawIndexedInstanced, 0, IndexCountPerInstance, InstanceCount, BaseVertexLocation, StartIndexLocation, StartInstanceLocation, nullptr, 0);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::DrawInstanced(
    UINT VertexCountPerInstance,
    UINT InstanceCount,
    UINT StartVertexLocation,
    UINT StartInstanceLocation)
{
    draw_context c = draw_context(DrawCall::DrawInstanced, VertexCountPerInstance, 0, InstanceCount, StartVertexLocation, 0, StartInstanceLocation, nullptr, 0);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::VSSetShaderResources(
    UINT                             StartSlot,
    UINT                             NumViews,
    ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
    SetShaderResources<&ID3D11DeviceContext::VSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE HackerContext::OMSetRenderTargets(
    UINT                           NumViews,
    ID3D11RenderTargetView* const* ppRenderTargetViews,
    ID3D11DepthStencilView*        pDepthStencilView)
{
    Profiling::State profiling_state;

    if (G->hunting == HUNTING_MODE_ENABLED)
    {
        ENTER_CRITICAL_SECTION(&G->mCriticalSection);
        {
            currentRenderTargets.clear();
            currentDepthTarget = nullptr;
            currentPSNumUAVs   = 0;
        }
        LEAVE_CRITICAL_SECTION(&G->mCriticalSection);

        if (G->DumpUsage)
        {
            if (Profiling::mode == Profiling::Mode::SUMMARY)
                Profiling::start(&profiling_state);

            if (ppRenderTargetViews)
            {
                for (UINT i = 0; i < NumViews; ++i)
                {
                    if (!ppRenderTargetViews[i])
                        continue;
                    RecordRenderTargetInfo(ppRenderTargetViews[i], i);
                }
            }

            RecordDepthStencil(pDepthStencilView);

            if (Profiling::mode == Profiling::Mode::SUMMARY)
                Profiling::end(&profiling_state, &Profiling::stat_overhead);
        }
    }

    origContext1->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}

void STDMETHODCALLTYPE HackerContext::OMSetRenderTargetsAndUnorderedAccessViews(
    UINT                              NumRTVs,
    ID3D11RenderTargetView* const*    ppRenderTargetViews,
    ID3D11DepthStencilView*           pDepthStencilView,
    UINT                              UAVStartSlot,
    UINT                              NumUAVs,
    ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                       pUAVInitialCounts)
{
    Profiling::State profiling_state;

    if (G->hunting == HUNTING_MODE_ENABLED)
    {
        ENTER_CRITICAL_SECTION(&G->mCriticalSection);
        {
            if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
            {
                currentRenderTargets.clear();
                currentDepthTarget = nullptr;
                if (G->DumpUsage)
                {
                    if (Profiling::mode == Profiling::Mode::SUMMARY)
                        Profiling::start(&profiling_state);

                    if (ppRenderTargetViews)
                    {
                        for (UINT i = 0; i < NumRTVs; ++i)
                        {
                            if (ppRenderTargetViews[i])
                                RecordRenderTargetInfo(ppRenderTargetViews[i], i);
                        }
                    }
                    RecordDepthStencil(pDepthStencilView);

                    if (Profiling::mode == Profiling::Mode::SUMMARY)
                        Profiling::end(&profiling_state, &Profiling::stat_overhead);
                }
            }

            if (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
            {
                currentPSUAVStartSlot = UAVStartSlot;
                currentPSNumUAVs      = NumUAVs;
                // TODO: Record UAV stats
            }
        }
        LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
    }

    origContext1->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

void STDMETHODCALLTYPE HackerContext::DrawAuto()
{
    draw_context c = draw_context(DrawCall::DrawAuto, 0, 0, 0, 0, 0, 0, nullptr, 0);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->DrawAuto();
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::DrawIndexedInstancedIndirect(
    ID3D11Buffer* pBufferForArgs,
    UINT          AlignedByteOffsetForArgs)
{
    draw_context c = draw_context(DrawCall::DrawIndexedInstancedIndirect, 0, 0, 0, 0, 0, 0, &pBufferForArgs, AlignedByteOffsetForArgs);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::DrawInstancedIndirect(
    ID3D11Buffer* pBufferForArgs,
    UINT          AlignedByteOffsetForArgs)
{
    draw_context c = draw_context(DrawCall::DrawInstancedIndirect, 0, 0, 0, 0, 0, 0, &pBufferForArgs, AlignedByteOffsetForArgs);
    BeforeDraw(c);

    if (!c.call_info.skip)
        origContext1->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
    AfterDraw(c);
}

void STDMETHODCALLTYPE HackerContext::ClearRenderTargetView(
    ID3D11RenderTargetView* pRenderTargetView,
    const FLOAT             ColorRGBA[4])
{
    run_view_command_list(hackerDevice, this, &G->clear_rtv_command_list, pRenderTargetView, false);
    origContext1->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
    run_view_command_list(hackerDevice, this, &G->post_clear_rtv_command_list, pRenderTargetView, true);
}

// -----------------------------------------------------------------------------
// Sort of HackerContext1
//    Requires Win7 Platform Update

// Hierarchy:
//  HackerContext <- ID3D11DeviceContext1 <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

void STDMETHODCALLTYPE HackerContext::CopySubresourceRegion1(
    ID3D11Resource*  pDstResource,
    UINT             DstSubresource,
    UINT             DstX,
    UINT             DstY,
    UINT             DstZ,
    ID3D11Resource*  pSrcResource,
    UINT             SrcSubresource,
    const D3D11_BOX* pSrcBox,
    UINT             CopyFlags)
{
    origContext1->CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
}

void STDMETHODCALLTYPE HackerContext::UpdateSubresource1(
    ID3D11Resource*  pDstResource,
    UINT             DstSubresource,
    const D3D11_BOX* pDstBox,
    const void*      pSrcData,
    UINT             SrcRowPitch,
    UINT             SrcDepthPitch,
    UINT             CopyFlags)
{
    origContext1->UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);

    // TODO: Track resource hash updates
}

void STDMETHODCALLTYPE HackerContext::DiscardResource(
    ID3D11Resource* pResource)
{
    origContext1->DiscardResource(pResource);
}

void STDMETHODCALLTYPE HackerContext::DiscardView(
    ID3D11View* pResourceView)
{
    origContext1->DiscardView(pResourceView);
}

void STDMETHODCALLTYPE HackerContext::VSSetConstantBuffers1(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers,
    const UINT*          pFirstConstant,
    const UINT*          pNumConstants)
{
    origContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::HSSetConstantBuffers1(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers,
    const UINT*          pFirstConstant,
    const UINT*          pNumConstants)
{
    origContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::DSSetConstantBuffers1(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers,
    const UINT*          pFirstConstant,
    const UINT*          pNumConstants)
{
    origContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::GSSetConstantBuffers1(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers,
    const UINT*          pFirstConstant,
    const UINT*          pNumConstants)
{
    origContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::PSSetConstantBuffers1(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers,
    const UINT*          pFirstConstant,
    const UINT*          pNumConstants)
{
    origContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::CSSetConstantBuffers1(
    UINT                 StartSlot,
    UINT                 NumBuffers,
    ID3D11Buffer* const* ppConstantBuffers,
    const UINT*          pFirstConstant,
    const UINT*          pNumConstants)
{
    origContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::VSGetConstantBuffers1(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers,
    UINT*          pFirstConstant,
    UINT*          pNumConstants)
{
    origContext1->VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::HSGetConstantBuffers1(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers,
    UINT*          pFirstConstant,
    UINT*          pNumConstants)
{
    origContext1->HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::DSGetConstantBuffers1(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers,
    UINT*          pFirstConstant,
    UINT*          pNumConstants)
{
    origContext1->DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::GSGetConstantBuffers1(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers,
    UINT*          pFirstConstant,
    UINT*          pNumConstants)
{
    origContext1->GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::PSGetConstantBuffers1(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers,
    UINT*          pFirstConstant,
    UINT*          pNumConstants)
{
    origContext1->PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::CSGetConstantBuffers1(
    UINT           StartSlot,
    UINT           NumBuffers,
    ID3D11Buffer** ppConstantBuffers,
    UINT*          pFirstConstant,
    UINT*          pNumConstants)
{
    origContext1->CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE HackerContext::SwapDeviceContextState(
    ID3DDeviceContextState*  pState,
    ID3DDeviceContextState** ppPreviousState)
{
    origContext1->SwapDeviceContextState(pState, ppPreviousState);

    // If a game or overlay creates separate context state objects we won't
    // have had a chance to bind the 3DMigoto resources when it was
    // first created, so do so now:
    Bind3DMigotoResources();
}

void STDMETHODCALLTYPE HackerContext::ClearView(
    ID3D11View*       pView,
    const FLOAT       Color[4],
    const D3D11_RECT* pRect,
    UINT              NumRects)
{
    // TODO: Add a command list here, but we probably actualy want to call
    // the existing RTV / DSV / UAV clear command lists instead for
    // compatibility with engines that might use this if the feature level
    // is high enough, and the others if it is not.
    origContext1->ClearView(pView, Color, pRect, NumRects);
}

void STDMETHODCALLTYPE HackerContext::DiscardView1(
    ID3D11View*       pResourceView,
    const D3D11_RECT* pRects,
    UINT              NumRects)
{
    origContext1->DiscardView1(pResourceView, pRects, NumRects);
}
