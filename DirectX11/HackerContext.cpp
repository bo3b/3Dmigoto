// Wrapper for the ID3D11DeviceContext.
// This gives us access to every D3D11 call for a Context, and override the pieces needed.
// Superclass access is directly through ID3D11DeviceContext interfaces.
// Hierarchy:
//  HackerContext <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

// Object				OS				D3D11 version	Feature level
// ID3D11DeviceContext	Win7			11.0			11.0
// ID3D11DeviceContext1	Platform update	11.1			11.1
// ID3D11DeviceContext2	Win8.1			11.2
// ID3D11DeviceContext3					11.3

#include "HackerContext.h"
#include "HookedContext.h"

#include "log.h"
#include "HackerDevice.h"
#include "D3D11Wrapper.h"
#include "Globals.h"
#include "ResourceHash.h"
#include "Override.h"
#include "ShaderRegex.h"

// -----------------------------------------------------------------------------------------------

HackerContext::HackerContext(ID3D11Device *pDevice, ID3D11DeviceContext *pContext)
	: ID3D11DeviceContext()
{
	mOrigDevice = pDevice;
	mOrigContext = pContext;
	mRealOrigContext = pContext;
	mHackerDevice = NULL;

	mCurrentIndexBuffer = 0;
	mCurrentVertexShader = 0;
	mCurrentVertexShaderHandle = NULL;
	mCurrentPixelShader = 0;
	mCurrentPixelShaderHandle = NULL;
	mCurrentComputeShader = 0;
	mCurrentComputeShaderHandle = NULL;
	mCurrentGeometryShader = 0;
	mCurrentGeometryShaderHandle = NULL;
	mCurrentDomainShader = 0;
	mCurrentDomainShaderHandle = NULL;
	mCurrentHullShader = 0;
	mCurrentHullShaderHandle = NULL;
	mCurrentDepthTarget = NULL;

	analyse_options = FrameAnalysisOptions::INVALID;
	frame_analysis_log = NULL;
}


// Save the corresponding HackerDevice, as we need to use it periodically to get
// access to the StereoParams.

void HackerContext::SetHackerDevice(HackerDevice *pDevice)
{
	mHackerDevice = pDevice;
}

// Returns the "real" DirectX object. Note that if hooking is enabled calls
// through this object will go back into 3DMigoto, which would then subject
// them to extra logging and any processing 3DMigoto applies, which may be
// undesirable in some cases. This used to cause a crash if a command list
// issued a draw call, since that would then trigger the command list and
// recurse until the stack ran out:
ID3D11DeviceContext* HackerContext::GetOrigContext(void)
{
	return mRealOrigContext;
}

// Use this one when you specifically don't want calls through this object to
// ever go back into 3DMigoto. If hooking is disabled this is identical to the
// above, but when hooking this will be the trampoline object instead:
ID3D11DeviceContext* HackerContext::GetPassThroughOrigContext(void)
{
	return mOrigContext;
}

void HackerContext::HookContext()
{
	// This will install hooks in the original context (if they have not
	// already been installed from a prior context) which will call the
	// equivalent function in this HackerContext. It returns a trampoline
	// interface which we use in place of mOrigContext to call the real
	// original context, thereby side stepping the problem that calling the
	// old mOrigContext would be hooked and call back into us endlessly:
	mOrigContext = hook_context(mOrigContext, this, G->enable_hooks);
}

// -----------------------------------------------------------------------------


// Records the hash of this shader resource view for later lookup. Returns the
// handle to the resource, but be aware that it no longer has a reference and
// should only be used for map lookups.
ID3D11Resource* HackerContext::RecordResourceViewStats(ID3D11ShaderResourceView *view)
{
	ID3D11Resource *resource = NULL;
	uint32_t orig_hash = 0;

	if (!view)
		return NULL;

	view->GetResource(&resource);
	if (!resource)
		return NULL;

	EnterCriticalSection(&G->mCriticalSection);

		// We are using the original resource hash for stat collection - things
		// get tricky otherwise
		orig_hash = GetOrigResourceHash(resource);

		resource->Release();

		if (orig_hash)
			G->mShaderResourceInfo.insert(orig_hash);

	LeaveCriticalSection(&G->mCriticalSection);

	return resource;
}

template <void (__stdcall ID3D11DeviceContext::*GetShaderResources)(THIS_
		UINT StartSlot,
		UINT NumViews,
		ID3D11ShaderResourceView **ppShaderResourceViews)>
void HackerContext::RecordShaderResourceUsage(ShaderInfoData *shader_info)
{
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	ID3D11Resource *resource;
	int i;

	(mOrigContext->*GetShaderResources)(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (views[i]) {
			resource = RecordResourceViewStats(views[i]);
			if (resource)
				shader_info->ResourceRegisters[i].insert(resource);
			views[i]->Release();
		}
	}
}

void HackerContext::RecordPeerShaders(std::set<UINT64> *PeerShaders, UINT64 this_shader_hash)
{
	if (mCurrentVertexShader && mCurrentVertexShader != this_shader_hash)
		PeerShaders->insert(mCurrentVertexShader);

	if (mCurrentHullShader && mCurrentHullShader != this_shader_hash)
		PeerShaders->insert(mCurrentHullShader);

	if (mCurrentDomainShader && mCurrentDomainShader != this_shader_hash)
		PeerShaders->insert(mCurrentDomainShader);

	if (mCurrentGeometryShader && mCurrentGeometryShader != this_shader_hash)
		PeerShaders->insert(mCurrentGeometryShader);

	if (mCurrentPixelShader && mCurrentPixelShader != this_shader_hash)
		PeerShaders->insert(mCurrentPixelShader);
}

void HackerContext::RecordGraphicsShaderStats()
{
	UINT selectedRenderTargetPos;
	ShaderInfoData *info;

	if (mCurrentVertexShader) {
		info = &G->mVertexShaderInfo[mCurrentVertexShader];
		RecordShaderResourceUsage<&ID3D11DeviceContext::VSGetShaderResources>(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentVertexShader);
	}

	if (mCurrentHullShader) {
		info = &G->mHullShaderInfo[mCurrentHullShader];
		RecordShaderResourceUsage<&ID3D11DeviceContext::HSGetShaderResources>(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentHullShader);
	}

	if (mCurrentDomainShader) {
		info = &G->mDomainShaderInfo[mCurrentDomainShader];
		RecordShaderResourceUsage<&ID3D11DeviceContext::DSGetShaderResources>(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentDomainShader);
	}

	if (mCurrentGeometryShader) {
		info = &G->mGeometryShaderInfo[mCurrentGeometryShader];
		RecordShaderResourceUsage<&ID3D11DeviceContext::GSGetShaderResources>(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentGeometryShader);
	}

	if (mCurrentPixelShader) {
		info = &G->mPixelShaderInfo[mCurrentPixelShader];
		RecordShaderResourceUsage<&ID3D11DeviceContext::PSGetShaderResources>(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentPixelShader);

		for (selectedRenderTargetPos = 0; selectedRenderTargetPos < mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
			if (selectedRenderTargetPos >= info->RenderTargets.size())
				info->RenderTargets.push_back(std::set<ID3D11Resource *>());

			info->RenderTargets[selectedRenderTargetPos].insert(mCurrentRenderTargets[selectedRenderTargetPos]);
		}

		if (mCurrentDepthTarget)
			info->DepthTargets.insert(mCurrentDepthTarget);
	}
}

void HackerContext::RecordComputeShaderStats()
{
	RecordShaderResourceUsage<&ID3D11DeviceContext::CSGetShaderResources>(&G->mComputeShaderInfo[mCurrentComputeShader]);

	// TODO: Collect stats on assigned UAVs
}

void HackerContext::RecordRenderTargetInfo(ID3D11RenderTargetView *target, UINT view_num)
{
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	ID3D11Resource *resource = NULL;
	uint32_t orig_hash = 0;

	target->GetDesc(&desc);

	LogDebug("  View #%d, Format = %d, Is2D = %d\n",
		view_num, desc.Format, D3D11_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension);

	target->GetResource(&resource);
	if (!resource)
		return;

	EnterCriticalSection(&G->mCriticalSection);

		// We are using the original resource hash for stat collection - things
		// get tricky otherwise
		orig_hash = GetOrigResourceHash((ID3D11Texture2D *)resource);

		resource->Release();

		if (!resource)
			goto out_unlock;

		mCurrentRenderTargets.push_back(resource);
		G->mVisitedRenderTargets.insert(resource);
		G->mRenderTargetInfo.insert(orig_hash);

out_unlock:
	LeaveCriticalSection(&G->mCriticalSection);
}

void HackerContext::RecordDepthStencil(ID3D11DepthStencilView *target)
{
	D3D11_DEPTH_STENCIL_VIEW_DESC desc;
	ID3D11Resource *resource = NULL;
	uint32_t orig_hash = 0;

	if (!target)
		return;

	target->GetResource(&resource);
	if (!resource)
		return;

	target->GetDesc(&desc);

	EnterCriticalSection(&G->mCriticalSection);

		// We are using the original resource hash for stat collection - things
		// get tricky otherwise
		orig_hash = GetOrigResourceHash(resource);

		resource->Release();

		mCurrentDepthTarget = resource;
		G->mDepthTargetInfo.insert(orig_hash);

	LeaveCriticalSection(&G->mCriticalSection);
}

ID3D11VertexShader* HackerContext::SwitchVSShader(ID3D11VertexShader *shader)
{

	ID3D11VertexShader *pVertexShader;
	ID3D11ClassInstance *pClassInstances;
	UINT NumClassInstances = 0, i;

	// We can possibly save the need to get the current shader by saving the ClassInstances
	mOrigContext->VSGetShader(&pVertexShader, &pClassInstances, &NumClassInstances);
	mOrigContext->VSSetShader(shader, &pClassInstances, NumClassInstances);

	for (i = 0; i < NumClassInstances; i++)
		pClassInstances[i].Release();

	return pVertexShader;
}

ID3D11PixelShader* HackerContext::SwitchPSShader(ID3D11PixelShader *shader)
{

	ID3D11PixelShader *pPixelShader;
	ID3D11ClassInstance *pClassInstances;
	UINT NumClassInstances = 0, i;

	// We can possibly save the need to get the current shader by saving the ClassInstances
	mOrigContext->PSGetShader(&pPixelShader, &pClassInstances, &NumClassInstances);
	mOrigContext->PSSetShader(shader, &pClassInstances, NumClassInstances);

	for (i = 0; i < NumClassInstances; i++)
		pClassInstances[i].Release();

	return pPixelShader;
}

#define ENABLE_LEGACY_FILTERS 1
void HackerContext::ProcessShaderOverride(ShaderOverride *shaderOverride, bool isPixelShader, DrawContext *data)
{
	bool use_orig = false;

	LogDebug("  override found for shader\n");

	// We really want to start deprecating all the old filters and switch
	// to using the command list for much greater flexibility. This if()
	// will be optimised out by the compiler, but is here to remind anyone
	// looking at this that we don't want to extend this code further.
	if (ENABLE_LEGACY_FILTERS) {
		// Deprecated: The texture filtering support in the command
		// list can match oD for the depth buffer, which will return
		// negative zero -0.0 if no depth buffer is assigned.
		if (shaderOverride->depth_filter != DepthBufferFilter::NONE) {
			ID3D11DepthStencilView *pDepthStencilView = NULL;

			mOrigContext->OMGetRenderTargets(0, NULL, &pDepthStencilView);

			// Remember - we are NOT switching to the original shader when the condition is true
			if (shaderOverride->depth_filter == DepthBufferFilter::DEPTH_ACTIVE && !pDepthStencilView) {
				use_orig = true;
			}
			else if (shaderOverride->depth_filter == DepthBufferFilter::DEPTH_INACTIVE && pDepthStencilView) {
				use_orig = true;
			}

			if (pDepthStencilView)
				pDepthStencilView->Release();

			// TODO: Add alternate filter type where the depth
			// buffer state is passed as an input to the shader
		}

		// Deprecated: Partner filtering can already be achieved with
		// the command list with far more flexibility than this allows
		if (shaderOverride->partner_hash) {
			if (isPixelShader) {
				if (mCurrentVertexShader != shaderOverride->partner_hash)
					use_orig = true;
			}
			else {
				if (mCurrentPixelShader != shaderOverride->partner_hash)
					use_orig = true;
			}
		}
	}

	RunCommandList(mHackerDevice, this, &shaderOverride->command_list, &data->call_info, false);

	if (ENABLE_LEGACY_FILTERS) {
		// Deprecated since the logic can be moved into the shaders with far more flexibility
		if (use_orig) {
			if (isPixelShader) {
				ShaderReplacementMap::iterator i = G->mOriginalShaders.find(mCurrentPixelShaderHandle);
				if (i != G->mOriginalShaders.end())
					data->oldPixelShader = SwitchPSShader((ID3D11PixelShader*)i->second);
			}
			else {
				ShaderReplacementMap::iterator i = G->mOriginalShaders.find(mCurrentVertexShaderHandle);
				if (i != G->mOriginalShaders.end())
					data->oldVertexShader = SwitchVSShader((ID3D11VertexShader*)i->second);
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
template <class ID3D11Shader,
	void (__stdcall ID3D11DeviceContext::*GetShaderVS2013BUGWORKAROUND)(ID3D11Shader**, ID3D11ClassInstance**, UINT*),
	void (__stdcall ID3D11DeviceContext::*SetShaderVS2013BUGWORKAROUND)(ID3D11Shader*, ID3D11ClassInstance*const*, UINT),
	HRESULT (__stdcall ID3D11Device::*CreateShader)(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11Shader**)
>
void HackerContext::DeferredShaderReplacement(ID3D11DeviceChild *shader, UINT64 hash, wchar_t *shader_type)
{
	ID3D11Shader *orig_shader = NULL, *patched_shader = NULL;
	ID3D11ClassInstance *class_instances[256];
	OriginalShaderInfo *orig_info = NULL;
	UINT num_instances = 0;
	string asm_text;
	bool patch_regex = false;
	HRESULT hr;
	unsigned i;
	wstring tagline(L"//");

	try {
		orig_info = &G->mReloadedShaders.at(shader);
	} catch (std::out_of_range) {
		return;
	}

	if (!orig_info->deferred_replacement_candidate || orig_info->deferred_replacement_processed)
		return;

	LogInfo("Performing deferred shader analysis on %S %016I64x...\n", shader_type, hash);

	// Remember that we have analysed this one so we don't check it again
	// (until config reload) regardless of whether we patch it or not:
	orig_info->deferred_replacement_processed = true;

	asm_text = BinaryToAsmText(orig_info->byteCode->GetBufferPointer(),
			orig_info->byteCode->GetBufferSize());
	if (asm_text.empty())
		return;

	try {
		patch_regex = apply_shader_regex_groups(&asm_text, &orig_info->shaderModel, hash, &tagline);
	} catch (...) {
		LogInfo("    *** Exception while patching shader\n");
		return;
	}

	if (!patch_regex) {
		LogInfo("Patch did not apply\n");
		return;
	}

	LogInfo("Patched Shader:\n%s\n", asm_text.c_str());

	vector<char> asm_vector(asm_text.begin(), asm_text.end());
	vector<byte> patched_bytecode;

	hr = AssembleFluganWithSignatureParsing(&asm_vector, &patched_bytecode);
	if (FAILED(hr)) {
		LogInfo("    *** Assembling patched shader failed\n");
		return;
	}

	hr = (mOrigDevice->*CreateShader)(patched_bytecode.data(), patched_bytecode.size(),
			orig_info->linkage, &patched_shader);
	CleanupShaderMaps(patched_shader);
	if (FAILED(hr)) {
		LogInfo("    *** Creating replacement shader failed\n");
		return;
	}

	// Update replacement map so we don't have to repeat this process.
	// Not updating the bytecode in the replaced shader map - we do that
	// elsewhere, but I think that is a bug. Need to untangle that first.
	if (orig_info->replacement)
		orig_info->replacement->Release();
	orig_info->replacement = patched_shader;
	orig_info->infoText = tagline;

	// And bind the replaced shader in time for this draw call:
	// VSBUGWORKAROUND: VS2013 toolchain has a bug that mistakes a member
	// pointer called "SetShader" for the SetShader we have in
	// HackerContext, even though the member pointer we were passed very
	// clearly points to a member function of ID3D11DeviceContext. VS2015
	// toolchain does not suffer from this bug.
	(mOrigContext->*GetShaderVS2013BUGWORKAROUND)(&orig_shader, class_instances, &num_instances);
	(mOrigContext->*SetShaderVS2013BUGWORKAROUND)(patched_shader, class_instances, num_instances);
	if (orig_shader)
		orig_shader->Release();
	for (i = 0; i < num_instances; i++) {
		if (class_instances[i])
			class_instances[i]->Release();
	}
}

void HackerContext::DeferredShaderReplacementBeforeDraw()
{
	if (shader_regex_groups.empty())
		return;

	EnterCriticalSection(&G->mCriticalSection);

		if (mCurrentVertexShaderHandle) {
			DeferredShaderReplacement<ID3D11VertexShader,
				&ID3D11DeviceContext::VSGetShader,
				&ID3D11DeviceContext::VSSetShader,
				&ID3D11Device::CreateVertexShader>
				(mCurrentVertexShaderHandle, mCurrentVertexShader, L"vs");
		}
		if (mCurrentHullShaderHandle) {
			DeferredShaderReplacement<ID3D11HullShader,
				&ID3D11DeviceContext::HSGetShader,
				&ID3D11DeviceContext::HSSetShader,
				&ID3D11Device::CreateHullShader>
				(mCurrentHullShaderHandle, mCurrentHullShader, L"hs");
		}
		if (mCurrentDomainShaderHandle) {
			DeferredShaderReplacement<ID3D11DomainShader,
				&ID3D11DeviceContext::DSGetShader,
				&ID3D11DeviceContext::DSSetShader,
				&ID3D11Device::CreateDomainShader>
				(mCurrentDomainShaderHandle, mCurrentDomainShader, L"ds");
		}
		if (mCurrentGeometryShaderHandle) {
			DeferredShaderReplacement<ID3D11GeometryShader,
				&ID3D11DeviceContext::GSGetShader,
				&ID3D11DeviceContext::GSSetShader,
				&ID3D11Device::CreateGeometryShader>
				(mCurrentGeometryShaderHandle, mCurrentGeometryShader, L"gs");
		}
		if (mCurrentPixelShaderHandle) {
			DeferredShaderReplacement<ID3D11PixelShader,
				&ID3D11DeviceContext::PSGetShader,
				&ID3D11DeviceContext::PSSetShader,
				&ID3D11Device::CreatePixelShader>
				(mCurrentPixelShaderHandle, mCurrentPixelShader, L"ps");
		}

	LeaveCriticalSection(&G->mCriticalSection);
}

void HackerContext::DeferredShaderReplacementBeforeDispatch()
{
	if (shader_regex_groups.empty())
		return;

	if (!mCurrentComputeShaderHandle)
		return;

	EnterCriticalSection(&G->mCriticalSection);

		DeferredShaderReplacement<ID3D11ComputeShader,
			&ID3D11DeviceContext::CSGetShader,
			&ID3D11DeviceContext::CSSetShader,
			&ID3D11Device::CreateComputeShader>
			(mCurrentComputeShaderHandle, mCurrentComputeShader, L"cs");

	LeaveCriticalSection(&G->mCriticalSection);
}


void HackerContext::BeforeDraw(DrawContext &data)
{
	// If we are not hunting shaders, we should skip all of this shader management for a performance bump.
	if (G->hunting == HUNTING_MODE_ENABLED)
	{
		UINT selectedRenderTargetPos;
		EnterCriticalSection(&G->mCriticalSection);
		{
			// In some cases stat collection can have a significant
			// performance impact or may result in a runaway
			// memory leak, so only do it if dump_usage is enabled:
			if (G->DumpUsage)
				RecordGraphicsShaderStats();

			// Selection
			for (selectedRenderTargetPos = 0; selectedRenderTargetPos < mCurrentRenderTargets.size(); ++selectedRenderTargetPos)
				if (mCurrentRenderTargets[selectedRenderTargetPos] == G->mSelectedRenderTarget) break;
			if (mCurrentIndexBuffer == G->mSelectedIndexBuffer ||
				mCurrentVertexShader == G->mSelectedVertexShader ||
				mCurrentPixelShader == G->mSelectedPixelShader ||
				mCurrentGeometryShader == G->mSelectedGeometryShader ||
				mCurrentDomainShader == G->mSelectedDomainShader ||
				mCurrentHullShader == G->mSelectedHullShader ||
				selectedRenderTargetPos < mCurrentRenderTargets.size())
			{
				LogDebug("  Skipping selected operation. CurrentIndexBuffer = %08lx, CurrentVertexShader = %016I64x, CurrentPixelShader = %016I64x\n",
					mCurrentIndexBuffer, mCurrentVertexShader, mCurrentPixelShader);

				// Snapshot render target list.
				if (G->mSelectedRenderTargetSnapshot != G->mSelectedRenderTarget)
				{
					G->mSelectedRenderTargetSnapshotList.clear();
					G->mSelectedRenderTargetSnapshot = G->mSelectedRenderTarget;
				}
				G->mSelectedRenderTargetSnapshotList.insert(mCurrentRenderTargets.begin(), mCurrentRenderTargets.end());
				// Snapshot info.
				if (mCurrentIndexBuffer == G->mSelectedIndexBuffer)
				{
					G->mSelectedIndexBuffer_VertexShader.insert(mCurrentVertexShader);
					G->mSelectedIndexBuffer_PixelShader.insert(mCurrentPixelShader);
				}
				if (mCurrentVertexShader == G->mSelectedVertexShader)
					G->mSelectedVertexShader_IndexBuffer.insert(mCurrentIndexBuffer);
				if (mCurrentPixelShader == G->mSelectedPixelShader)
					G->mSelectedPixelShader_IndexBuffer.insert(mCurrentIndexBuffer);
				if (G->marking_mode == MARKING_MODE_MONO && mHackerDevice->mStereoHandle)
				{
					LogDebug("  setting separation=0 for hunting\n");

					if (NVAPI_OK != NvAPI_Stereo_GetSeparation(mHackerDevice->mStereoHandle, &data.oldSeparation))
						LogDebug("    Stereo_GetSeparation failed.\n");

					NvAPIOverride();
					if (NVAPI_OK != NvAPI_Stereo_SetSeparation(mHackerDevice->mStereoHandle, 0))
						LogDebug("    Stereo_SetSeparation failed.\n");
				}
				else if (G->marking_mode == MARKING_MODE_SKIP)
				{
					data.call_info.skip = true;

					// If we have transferred the draw call to a custom shader via "handling =
					// skip" and "draw = from_caller" we still want a way to skip it for hunting.
					// We can't reuse call_info.skip for that, as that is also set by
					// "handling=skip", which may happen before the "draw=from_caller", so we
					// use a second skip flag specifically for hunting:
					data.call_info.hunting_skip = true;
				}
				else if (G->marking_mode == MARKING_MODE_PINK)
				{
					if (G->mPinkingShader)
						data.oldPixelShader = SwitchPSShader(G->mPinkingShader);
				}
			}
		}
		LeaveCriticalSection(&G->mCriticalSection);
	}

	if (!G->fix_enabled)
		return;

	DeferredShaderReplacementBeforeDraw();

	// Override settings?
	if (!G->mShaderOverrideMap.empty()) {
		ShaderOverrideMap::iterator i;

		i = G->mShaderOverrideMap.find(mCurrentVertexShader);
		if (i != G->mShaderOverrideMap.end()) {
			data.post_commands[0] = &i->second.post_command_list;
			ProcessShaderOverride(&i->second, false, &data);
		}

		if (mCurrentHullShader) {
			i = G->mShaderOverrideMap.find(mCurrentHullShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[1] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data);
			}
		}

		if (mCurrentDomainShader) {
			i = G->mShaderOverrideMap.find(mCurrentDomainShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[2] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data);
			}
		}

		if (mCurrentGeometryShader) {
			i = G->mShaderOverrideMap.find(mCurrentGeometryShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[3] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data);
			}
		}

		i = G->mShaderOverrideMap.find(mCurrentPixelShader);
		if (i != G->mShaderOverrideMap.end()) {
			data.post_commands[4] = &i->second.post_command_list;
			ProcessShaderOverride(&i->second, true, &data);
		}
	}
}

void HackerContext::AfterDraw(DrawContext &data)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (data.post_commands[i]) {
			RunCommandList(mHackerDevice, this, data.post_commands[i], &data.call_info, true);
		}
	}

	if (G->analyse_frame)
		FrameAnalysisAfterDraw(false, &data.call_info);

	if (mHackerDevice->mStereoHandle && data.oldSeparation != FLT_MAX) {
		NvAPIOverride();
		if (NVAPI_OK != NvAPI_Stereo_SetSeparation(mHackerDevice->mStereoHandle, data.oldSeparation))
			LogDebug("    Stereo_SetSeparation failed.\n");
	}

	if (data.oldVertexShader) {
		ID3D11VertexShader *ret;
		ret = SwitchVSShader(data.oldVertexShader);
		data.oldVertexShader->Release();
		if (ret)
			ret->Release();
	}
	if (data.oldPixelShader) {
		ID3D11PixelShader *ret;
		ret = SwitchPSShader(data.oldPixelShader);
		data.oldPixelShader->Release();
		if (ret)
			ret->Release();
	}
}

// -----------------------------------------------------------------------------------------------

//HackerContext* HackerContext::GetDirect3DDeviceContext(ID3D11DeviceContext *pOrig)
//{
//	HackerContext* p = (HackerContext*) m_List.GetDataPtr(pOrig);
//	if (!p)
//	{
//		p = new HackerContext(pOrig);
//		if (pOrig) m_List.AddMember(pOrig, p);
//	}
//	return p;
//}

//STDMETHODIMP_(ULONG) HackerContext::AddRef(THIS)
//{
//	++m_ulRef;
//	return m_pUnk->AddRef();
//}

ULONG STDMETHODCALLTYPE HackerContext::AddRef(void)
{
	return mOrigContext->AddRef();
}


// Must set the reference that the HackerDevice uses to null, because otherwise
// we see that dead reference reused in GetImmediateContext, in FC4.

STDMETHODIMP_(ULONG) HackerContext::Release(THIS)
{
	ULONG ulRef = mOrigContext->Release();
	LogDebug("HackerContext::Release counter=%d, this=%p\n", ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("  deleting self\n");

		if (mHackerDevice != nullptr) {
			if (mHackerDevice->GetHackerContext() == this) {
				LogInfo("  clearing mHackerDevice->mHackerContext\n");
				mHackerDevice->SetHackerContext(nullptr);
			}
		} else
			LogInfo("HackerContext::Release - mHackerDevice is NULL\n");

		if (frame_analysis_log)
			fclose(frame_analysis_log);

		delete this;
		return 0L;
	}
	return ulRef;
}

// In a strange case, Mafia 3 calls this hacky interface with the request for
// the ID3D11DeviceContext.  That's right, it calls to get the exact
// same object that it is using to call.  I swear.

HRESULT STDMETHODCALLTYPE HackerContext::QueryInterface(
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerContext::QueryInterface(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(riid).c_str());
		
	HRESULT hr = mOrigContext->QueryInterface(riid, ppvObject);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p\n", hr, ppvObject);
		return hr;
	}

	// To avoid letting the game bypass our hooked object, we need to return the 
	// HackerContext/this in this case.
	if (riid == __uuidof(ID3D11DeviceContext))
	{
		*ppvObject = this;
		LogDebug("  return HackerContext(%s@%p) wrapper of %p\n", type_name(this), this, mOrigContext);
	}
	else if (riid == __uuidof(ID3D11DeviceContext1))
	{
		if (!G->enable_platform_update) 
		{
			LogInfo("***  returns E_NOINTERFACE as error for ID3D11DeviceContext1 (try allow_platform_update=1 if the game refuses to run).\n");
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}

		// If this happens to already be a HackerContext1, we don't want to rewrap it or make a new one.
		if (dynamic_cast<HackerContext1*>(this) != NULL)
		{
			*ppvObject = this;
			LogDebug("  return HackerContext1(%s@%p) wrapper of %p\n", type_name(this), this, ppvObject);
		}

		// For Batman: TellTale games, they call this to fetch the DeviceContext1.
		// We need to return a hooked version as part of fleshing it out for games like this
		// that require the evil-update to run.
		// Not at all positive this is the right approach, the mix of type1 objects and
		// Hacker objects is a bit obscure.

		// If platform_update is allowed, we have to assume that this QueryInterface off
		// the original context will work.
		ID3D11Device1 *origDevice1;
		HRESULT hr = mOrigContext->QueryInterface(IID_PPV_ARGS(&origDevice1));
		if (FAILED(hr))
		{
			LogInfo("  failed QueryInterface for ID3D11Device1 = %x for %p\n", hr, origDevice1);
			LogInfo("***  return error\n");
			return hr;
		}

		// The original QueryInterface will have returned us the legit ID3D11DeviceContext1
		ID3D11DeviceContext1 *origContext1 = static_cast<ID3D11DeviceContext1*>(*ppvObject);

		HackerDevice1 *hackerDeviceWrap1 = new HackerDevice1(origDevice1, origContext1);
		LogDebug("  created HackerDevice1(%s@%p) wrapper of %p\n", type_name(hackerDeviceWrap1), hackerDeviceWrap1, origDevice1);

		HackerContext1 *hackerContextWrap1 = new HackerContext1(origDevice1, origContext1);
		LogDebug("  created HackerContext1(%s@%p) wrapper of %p\n", type_name(hackerContextWrap1), hackerContextWrap1, origContext1);

		hackerDeviceWrap1->SetHackerContext1(hackerContextWrap1);
		hackerContextWrap1->SetHackerDevice1(hackerDeviceWrap1);

		hackerDeviceWrap1->Create3DMigotoResources();
		hackerContextWrap1->Bind3DMigotoResources();

		*ppvObject = hackerContextWrap1;
		LogDebug("  created HackerContext1(%s@%p) wrapper of %p\n", type_name(hackerContextWrap1), hackerContextWrap1, origContext1);
	}

	LogDebug("  returns result = %x for %p\n", hr, ppvObject);
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

STDMETHODIMP_(void) HackerContext::GetDevice(THIS_
	/* [annotation] */
	__out  ID3D11Device **ppDevice)
{
	LogDebug("HackerContext::GetDevice(%s@%p) returns %p\n", type_name(this), this, mHackerDevice);

	// Fix ref counting bug that slowly eats away at the device until we
	// crash. In FC4 this can happen after about 10 minutes, or when
	// running in windowed mode during launch.
	
	// Follow our rule of always calling the original call first to ensure that
	// any side-effects (including ref counting) are activated.
	mOrigContext->GetDevice(ppDevice);

	// Return our wrapped device though.
	if (!(G->enable_hooks & EnableHooks::DEVICE))
		*ppDevice = mHackerDevice;
}

STDMETHODIMP HackerContext::GetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__inout  UINT *pDataSize,
	/* [annotation] */
	__out_bcount_opt(*pDataSize)  void *pData)
{
	LogInfo("HackerContext::GetPrivateData(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(guid).c_str());

	HRESULT hr = mOrigContext->GetPrivateData(guid, pDataSize, pData);
	LogInfo("  returns result = %x, DataSize = %d\n", hr, *pDataSize);

	return hr;
}

STDMETHODIMP HackerContext::SetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in  UINT DataSize,
	/* [annotation] */
	__in_bcount_opt(DataSize)  const void *pData)
{
	LogInfo("HackerContext::SetPrivateData(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(guid).c_str());
	LogInfo("  DataSize = %d\n", DataSize);

	HRESULT hr = mOrigContext->SetPrivateData(guid, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP HackerContext::SetPrivateDataInterface(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in_opt  const IUnknown *pData)
{
	LogInfo("HackerContext::SetPrivateDataInterface(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(guid).c_str());

	HRESULT hr = mOrigContext->SetPrivateDataInterface(guid, pData);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

// -----------------------------------------------------------------------------------------------

// ******************* ID3D11DeviceContext interface

// These first routines all the boilerplate ones that just pass through to the original context.
// They need to be here in order to pass along the calls, since there is no proper object where
// it would normally go to the superclass. 

STDMETHODIMP_(void) HackerContext::VSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers)
{
	FrameAnalysisLog("VSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

bool HackerContext::MapDenyCPURead(
	ID3D11Resource *pResource,
	UINT Subresource,
	D3D11_MAP MapType,
	UINT MapFlags,
	D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	uint32_t hash;
	TextureOverrideMap::iterator i;

	// Currently only replacing first subresource to simplify map type, and
	// only on read access as it is unclear how to handle a read/write access.
	if (Subresource != 0)
		return false;

	if (G->mTextureOverrideMap.empty())
		return false;

	EnterCriticalSection(&G->mCriticalSection);
		hash = GetResourceHash(pResource);
	LeaveCriticalSection(&G->mCriticalSection);

	i = G->mTextureOverrideMap.find(hash);
	if (i == G->mTextureOverrideMap.end())
		return false;

	return i->second.deny_cpu_read;
}

void HackerContext::TrackAndDivertMap(HRESULT map_hr, ID3D11Resource *pResource,
		UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
		D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	D3D11_RESOURCE_DIMENSION dim;
	ID3D11Buffer *buf = NULL;
	ID3D11Texture1D *tex1d = NULL;
	ID3D11Texture2D *tex2d = NULL;
	ID3D11Texture3D *tex3d = NULL;
	D3D11_BUFFER_DESC buf_desc;
	D3D11_TEXTURE1D_DESC tex1d_desc;
	D3D11_TEXTURE2D_DESC tex2d_desc;
	D3D11_TEXTURE3D_DESC tex3d_desc;
	MappedResourceInfo *map_info = NULL;
	void *replace = NULL;
	bool divertable = false, divert = false, track = false;
	bool write = false, read = false, deny = false;

	if (FAILED(map_hr) || !pResource || !pMappedResource || !pMappedResource->pData)
		return;

	switch (MapType) {
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
			divert = track = MapTrackResourceHashUpdate(pResource, Subresource);
			break;

		case D3D11_MAP_READ:
			read = divertable = true;
			divert = deny = MapDenyCPURead(pResource, Subresource, MapType, MapFlags, pMappedResource);
			break;
	}

	if (!track && !divert)
		return;

	map_info = &mMappedResources[pResource];
	map_info->mapped_writable = write;
	memcpy(&map_info->map, pMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	if (!divertable || !divert)
		return;

	pResource->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			buf = (ID3D11Buffer*)pResource;
			buf->GetDesc(&buf_desc);
			map_info->size = buf_desc.ByteWidth;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			tex1d = (ID3D11Texture1D*)pResource;
			tex1d->GetDesc(&tex1d_desc);
			map_info->size = dxgi_format_size(tex1d_desc.Format) * tex1d_desc.Width;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex2d = (ID3D11Texture2D*)pResource;
			tex2d->GetDesc(&tex2d_desc);
			map_info->size = pMappedResource->RowPitch * tex2d_desc.Height;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex3d = (ID3D11Texture3D*)pResource;
			tex3d->GetDesc(&tex3d_desc);
			map_info->size = pMappedResource->DepthPitch * tex3d_desc.Depth;
			break;
		default:
			return;
	}

	replace = malloc(map_info->size);
	if (!replace) {
		LogInfo("TrackAndDivertMap out of memory\n");
		return;
	}

	if (read && !deny)
		memcpy(replace, pMappedResource->pData, map_info->size);
	else
		memset(replace, 0, map_info->size);

	map_info->orig_pData = pMappedResource->pData;
	map_info->map.pData = replace;
	pMappedResource->pData = replace;
}

void HackerContext::TrackAndDivertUnmap(ID3D11Resource *pResource, UINT Subresource)
{
	MappedResources::iterator i;
	MappedResourceInfo *map_info = NULL;

	if (mMappedResources.empty())
		return;

	i = mMappedResources.find(pResource);
	if (i == mMappedResources.end())
		return;
	map_info = &i->second;

	if (G->track_texture_updates && Subresource == 0 && map_info->mapped_writable)
		UpdateResourceHashFromCPU(pResource, map_info->map.pData, map_info->map.RowPitch, map_info->map.DepthPitch);

	if (map_info->orig_pData) {
		// TODO: Measure performance vs. not diverting:
		if (map_info->mapped_writable)
			memcpy(map_info->orig_pData, map_info->map.pData, map_info->size);

		free(map_info->map.pData);
	}

	mMappedResources.erase(i);
}

STDMETHODIMP HackerContext::Map(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	/* [annotation] */
	__in  UINT Subresource,
	/* [annotation] */
	__in  D3D11_MAP MapType,
	/* [annotation] */
	__in  UINT MapFlags,
	/* [annotation] */
	__out D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	HRESULT hr;

	FrameAnalysisLogNoNL("Map(pResource:0x%p, Subresource:%u, MapType:%u, MapFlags:%u, pMappedResource:0x%p)",
			pResource, Subresource, MapType, MapFlags, pMappedResource);
	FrameAnalysisLogResourceHash(pResource);

	hr = mOrigContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);

	TrackAndDivertMap(hr, pResource, Subresource, MapType, MapFlags, pMappedResource);

	return hr;
}

STDMETHODIMP_(void) HackerContext::Unmap(THIS_
	/* [annotation] */
	__in ID3D11Resource *pResource,
	/* [annotation] */
	__in  UINT Subresource)
{
	FrameAnalysisLogNoNL("Unmap(pResource:0x%p, Subresource:%u)",
			pResource, Subresource);
	FrameAnalysisLogResourceHash(pResource);

	TrackAndDivertUnmap(pResource, Subresource);
	mOrigContext->Unmap(pResource, Subresource);

	if (G->analyse_frame)
		FrameAnalysisAfterUnmap(pResource);
}

STDMETHODIMP_(void) HackerContext::PSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers)
{
	FrameAnalysisLog("PSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	 mOrigContext->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::IASetInputLayout(THIS_
	/* [annotation] */
	__in_opt ID3D11InputLayout *pInputLayout)
{
	FrameAnalysisLog("IASetInputLayout(pInputLayout:0x%p)\n",
			pInputLayout);

	 mOrigContext->IASetInputLayout(pInputLayout);
}

STDMETHODIMP_(void) HackerContext::IASetVertexBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  const UINT *pStrides,
	/* [annotation] */
	__in_ecount(NumBuffers)  const UINT *pOffsets)
{
	FrameAnalysisLog("IASetVertexBuffers(StartSlot:%u, NumBuffers:%u, ppVertexBuffers:0x%p, pStrides:0x%p, pOffsets:0x%p)\n",
			StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppVertexBuffers);

	 mOrigContext->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

STDMETHODIMP_(void) HackerContext::GSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers)
{
	FrameAnalysisLog("GSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	 mOrigContext->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::GSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11GeometryShader *pShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	SetShader<ID3D11GeometryShader, &ID3D11DeviceContext::GSSetShader>
		(pShader, ppClassInstances, NumClassInstances,
		 &G->mVisitedGeometryShaders,
		 G->mSelectedGeometryShader,
		 &mCurrentGeometryShader,
		 &mCurrentGeometryShaderHandle);

	FrameAnalysisLog("GSSetShader(pShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pShader, ppClassInstances, NumClassInstances, mCurrentGeometryShader);
}

STDMETHODIMP_(void) HackerContext::IASetPrimitiveTopology(THIS_
	/* [annotation] */
	__in D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	FrameAnalysisLog("IASetPrimitiveTopology(Topology:%u)\n",
			Topology);

	 mOrigContext->IASetPrimitiveTopology(Topology);
}

STDMETHODIMP_(void) HackerContext::VSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers)
{
	FrameAnalysisLog("VSSetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);

	 mOrigContext->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::PSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers)
{
	FrameAnalysisLog("PSSetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);

	mOrigContext->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::Begin(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	FrameAnalysisLogNoNL("Begin(pAsync:0x%p)", pAsync);
	FrameAnalysisLogAsyncQuery(pAsync);

	mOrigContext->Begin(pAsync);
}

STDMETHODIMP_(void) HackerContext::End(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	FrameAnalysisLogNoNL("End(pAsync:0x%p)", pAsync);
	FrameAnalysisLogAsyncQuery(pAsync);

	 mOrigContext->End(pAsync);
}

STDMETHODIMP HackerContext::GetData(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync,
	/* [annotation] */
	__out_bcount_opt(DataSize)  void *pData,
	/* [annotation] */
	__in  UINT DataSize,
	/* [annotation] */
	__in  UINT GetDataFlags)
{
	HRESULT ret = mOrigContext->GetData(pAsync, pData, DataSize, GetDataFlags);

	FrameAnalysisLogNoNL("GetData(pAsync:0x%p, pData:0x%p, DataSize:%u, GetDataFlags:%u) = %u",
			pAsync, pData, DataSize, GetDataFlags, ret);
	FrameAnalysisLogAsyncQuery(pAsync);
	if (SUCCEEDED(ret))
		FrameAnalysisLogData(pData, DataSize);

	return ret;
}

STDMETHODIMP_(void) HackerContext::SetPredication(THIS_
	/* [annotation] */
	__in_opt ID3D11Predicate *pPredicate,
	/* [annotation] */
	__in  BOOL PredicateValue)
{
	FrameAnalysisLogNoNL("SetPredication(pPredicate:0x%p, PredicateValue:%s)",
			pPredicate, PredicateValue ? "true" : "false");
	FrameAnalysisLogAsyncQuery(pPredicate);

	return mOrigContext->SetPredication(pPredicate, PredicateValue);
}

STDMETHODIMP_(void) HackerContext::GSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	FrameAnalysisLog("GSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

	SetShaderResources<&ID3D11DeviceContext::GSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::GSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers)
{
	FrameAnalysisLog("GSSetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);

	 mOrigContext->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::OMSetBlendState(THIS_
	/* [annotation] */
	__in_opt  ID3D11BlendState *pBlendState,
	/* [annotation] */
	__in_opt  const FLOAT BlendFactor[4],
	/* [annotation] */
	__in  UINT SampleMask)
{
	FrameAnalysisLog("OMSetBlendState(pBlendState:0x%p, BlendFactor:0x%p, SampleMask:%u)\n",
			pBlendState, BlendFactor, SampleMask); // Beware dereferencing optional BlendFactor

	 mOrigContext->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

STDMETHODIMP_(void) HackerContext::OMSetDepthStencilState(THIS_
	/* [annotation] */
	__in_opt  ID3D11DepthStencilState *pDepthStencilState,
	/* [annotation] */
	__in  UINT StencilRef)
{
	FrameAnalysisLog("OMSetDepthStencilState(pDepthStencilState:0x%p, StencilRef:%u)\n",
			pDepthStencilState, StencilRef);

	 mOrigContext->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}

STDMETHODIMP_(void) HackerContext::SOSetTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
	/* [annotation] */
	__in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	FrameAnalysisLog("SOSetTargets(NumBuffers:%u, ppSOTargets:0x%p, pOffsets:0x%p)\n",
			NumBuffers, ppSOTargets, pOffsets);
	FrameAnalysisLogResourceArray(0, NumBuffers, (ID3D11Resource *const *)ppSOTargets);

	 mOrigContext->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}

bool HackerContext::BeforeDispatch(DispatchContext *context)
{
	if (G->hunting == HUNTING_MODE_ENABLED) {
		RecordComputeShaderStats();

		if (mCurrentComputeShader == G->mSelectedComputeShader) {
			if (G->marking_mode == MARKING_MODE_SKIP)
				return false;
		}
	}

	if (!G->fix_enabled)
		return true;

	DeferredShaderReplacementBeforeDispatch();

	// Override settings?
	if (!G->mShaderOverrideMap.empty()) {
		ShaderOverrideMap::iterator i;

		i = G->mShaderOverrideMap.find(mCurrentComputeShader);
		if (i != G->mShaderOverrideMap.end()) {
			context->post_commands = &i->second.post_command_list;
			// XXX: Not using ProcessShaderOverride() as a
			// lot of it's logic doesn't really apply to
			// compute shaders. The main thing we care
			// about is the command list, so just run that:
			RunCommandList(mHackerDevice, this, &i->second.command_list, NULL, false);
		}
	}

	return true;
}

void HackerContext::AfterDispatch(DispatchContext *context)
{
	if (context->post_commands)
		RunCommandList(mHackerDevice, this, context->post_commands, NULL, true);

	if (G->analyse_frame)
		FrameAnalysisAfterDraw(true, NULL);
}

STDMETHODIMP_(void) HackerContext::Dispatch(THIS_
	/* [annotation] */
	__in  UINT ThreadGroupCountX,
	/* [annotation] */
	__in  UINT ThreadGroupCountY,
	/* [annotation] */
	__in  UINT ThreadGroupCountZ)
{
	DispatchContext context;

	if (BeforeDispatch(&context))
		mOrigContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	FrameAnalysisLog("Dispatch(ThreadGroupCountX:%u, ThreadGroupCountY:%u, ThreadGroupCountZ:%u)\n",
			ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	AfterDispatch(&context);
}

STDMETHODIMP_(void) HackerContext::DispatchIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	DispatchContext context;


	if (BeforeDispatch(&context))
		mOrigContext->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);

	FrameAnalysisLog("DispatchIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

	AfterDispatch(&context);
}

STDMETHODIMP_(void) HackerContext::RSSetState(THIS_
	/* [annotation] */
	__in_opt  ID3D11RasterizerState *pRasterizerState)
{
	FrameAnalysisLog("RSSetState(pRasterizerState:0x%p)\n",
			pRasterizerState);

	 mOrigContext->RSSetState(pRasterizerState);
}

STDMETHODIMP_(void) HackerContext::RSSetViewports(THIS_
	/* [annotation] */
	__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
	/* [annotation] */
	__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports)
{
	FrameAnalysisLog("RSSetViewports(NumViewports:%u, pViewports:0x%p)\n",
			NumViewports, pViewports);

	// In the 3D Vision Direct Mode, we need to double the width of any ViewPorts
	// We specifically modify the input, so that the game is using full 2x width.
	// Modifying every ViewPort rect seems wrong, so we are only doing those that
	// match the screen resolution. 
	if (G->gForceStereo == 2)
	{
		for (size_t i = 0; i < NumViewports; i++)
		{
			if (pViewports[i].Width == G->mResolutionInfo.width)
			{
				const_cast<D3D11_VIEWPORT *>(pViewports)[i].Width *= 2;
				LogInfo("-> forced 2x width for Direct Mode: %.0f\n", pViewports[i].Width);
			}
		}
	}

	 mOrigContext->RSSetViewports(NumViewports, pViewports);
}

STDMETHODIMP_(void) HackerContext::RSSetScissorRects(THIS_
	/* [annotation] */
	__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
	/* [annotation] */
	__in_ecount_opt(NumRects)  const D3D11_RECT *pRects)
{
	FrameAnalysisLog("RSSetScissorRects(NumRects:%u, pRects:0x%p)\n",
			NumRects, pRects);

	 mOrigContext->RSSetScissorRects(NumRects, pRects);
}

/*
 * Used for CryEngine games like Lichdom that copy a 2D rectangle from the
 * colour render target to a texture as an input for transparent refraction
 * effects. Expands the rectange to the full width.
 */
bool HackerContext::ExpandRegionCopy(ID3D11Resource *pDstResource, UINT DstX,
		UINT DstY, ID3D11Resource *pSrcResource, const D3D11_BOX *pSrcBox,
		UINT *replaceDstX, D3D11_BOX *replaceBox)
{
	ID3D11Texture2D *srcTex = (ID3D11Texture2D*)pSrcResource;
	ID3D11Texture2D *dstTex = (ID3D11Texture2D*)pDstResource;
	D3D11_TEXTURE2D_DESC srcDesc, dstDesc;
	D3D11_RESOURCE_DIMENSION srcDim, dstDim;
	uint32_t srcHash, dstHash;
	TextureOverrideMap::iterator i;

	if (!pSrcResource || !pDstResource || !pSrcBox)
		return false;

	pSrcResource->GetType(&srcDim);
	pDstResource->GetType(&dstDim);
	if (srcDim != dstDim || srcDim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return false;

	srcTex->GetDesc(&srcDesc);
	dstTex->GetDesc(&dstDesc);
	EnterCriticalSection(&G->mCriticalSection);
		srcHash = GetResourceHash(srcTex);
		dstHash = GetResourceHash(dstTex);
	LeaveCriticalSection(&G->mCriticalSection);

	LogDebug("CopySubresourceRegion %08lx (%u:%u x %u:%u / %u x %u) -> %08lx (%u x %u / %u x %u)\n",
			srcHash, pSrcBox->left, pSrcBox->right, pSrcBox->top, pSrcBox->bottom, srcDesc.Width, srcDesc.Height, 
			dstHash, DstX, DstY, dstDesc.Width, dstDesc.Height);

	i = G->mTextureOverrideMap.find(dstHash);
	if (i == G->mTextureOverrideMap.end())
		return false;

	if (!i->second.expand_region_copy)
		return false;

	memcpy(replaceBox, pSrcBox, sizeof(D3D11_BOX));
	*replaceDstX = 0;
	replaceBox->left = 0;
	replaceBox->right = dstDesc.Width;

	return true;
}

STDMETHODIMP_(void) HackerContext::CopySubresourceRegion(THIS_
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
	D3D11_BOX replaceSrcBox;
	UINT replaceDstX = DstX;

	FrameAnalysisLog("CopySubresourceRegion(pDstResource:0x%p, DstSubresource:%u, DstX:%u, DstY:%u, DstZ:%u, pSrcResource:0x%p, SrcSubresource:%u, pSrcBox:0x%p)\n",
			pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
	FrameAnalysisLogResource(-1, "Src", pSrcResource);
	FrameAnalysisLogResource(-1, "Dst", pDstResource);

	if (G->hunting) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pDstResource, DstSubresource, pSrcResource, SrcSubresource, 'S', DstX, DstY, DstZ, pSrcBox);
	}

	if (ExpandRegionCopy(pDstResource, DstX, DstY, pSrcResource, pSrcBox, &replaceDstX, &replaceSrcBox))
		pSrcBox = &replaceSrcBox;

	 mOrigContext->CopySubresourceRegion(pDstResource, DstSubresource, replaceDstX, DstY, DstZ,
		pSrcResource, SrcSubresource, pSrcBox);

	// We only update the destination resource hash when the entire
	// subresource 0 is updated and pSrcBox is NULL. We could check if the
	// pSrcBox fills the entire resource, but if the game is using pSrcBox
	// it stands to reason that it won't always fill the entire resource
	// and the hashes might be less predictable. Possibly something to
	// enable as an option in the future if there is a proven need.
	if (G->track_texture_updates && DstSubresource == 0 && DstX == 0 && DstY == 0 && DstZ == 0 && pSrcBox == NULL)
		PropagateResourceHash(pDstResource, pSrcResource);
}

STDMETHODIMP_(void) HackerContext::CopyResource(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pDstResource,
	/* [annotation] */
	__in  ID3D11Resource *pSrcResource)
{
	FrameAnalysisLog("CopyResource(pDstResource:0x%p, pSrcResource:0x%p)\n",
			pDstResource, pSrcResource);
	FrameAnalysisLogResource(-1, "Src", pSrcResource);
	FrameAnalysisLogResource(-1, "Dst", pDstResource);

	if (G->hunting) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pDstResource, 0, pSrcResource, 0, 'C', 0, 0, 0, NULL);
	}

	 mOrigContext->CopyResource(pDstResource, pSrcResource);

	if (G->track_texture_updates)
		PropagateResourceHash(pDstResource, pSrcResource);
}

STDMETHODIMP_(void) HackerContext::UpdateSubresource(THIS_
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
	FrameAnalysisLogNoNL("UpdateSubresource(pDstResource:0x%p, DstSubresource:%u, pDstBox:0x%p, pSrcData:0x%p, SrcRowPitch:%u, SrcDepthPitch:%u)",
			pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
	FrameAnalysisLogResourceHash(pDstResource);

	if (G->hunting) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pDstResource, DstSubresource, NULL, 0, 'U', 0, 0, 0, NULL);
	}

	 mOrigContext->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
		SrcDepthPitch);

	// We only update the destination resource hash when the entire
	// subresource 0 is updated and pDstBox is NULL. We could check if the
	// pDstBox fills the entire resource, but if the game is using pDstBox
	// it stands to reason that it won't always fill the entire resource
	// and the hashes might be less predictable. Possibly something to
	// enable as an option in the future if there is a proven need.
	if (G->track_texture_updates && DstSubresource == 0 && pDstBox == NULL)
		UpdateResourceHashFromCPU(pDstResource, pSrcData, SrcRowPitch, SrcDepthPitch);

	if (G->analyse_frame)
		FrameAnalysisAfterUpdate(pDstResource);
}

STDMETHODIMP_(void) HackerContext::CopyStructureCount(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pDstBuffer,
	/* [annotation] */
	__in  UINT DstAlignedByteOffset,
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pSrcView)
{
	FrameAnalysisLog("CopyStructureCount(pDstBuffer:0x%p, DstAlignedByteOffset:%u, pSrcView:0x%p)\n",
			pDstBuffer, DstAlignedByteOffset, pSrcView);
	FrameAnalysisLogView(-1, "Src", (ID3D11View*)pSrcView);
	FrameAnalysisLogResource(-1, "Dst", pDstBuffer);

	 mOrigContext->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}

STDMETHODIMP_(void) HackerContext::ClearUnorderedAccessViewUint(THIS_
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const UINT Values[4])
{
	FrameAnalysisLog("ClearUnorderedAccessViewUint(pUnorderedAccessView:0x%p, Values:0x%p\n)",
			pUnorderedAccessView, Values);
	FrameAnalysisLogView(-1, NULL, pUnorderedAccessView);

	RunViewCommandList(mHackerDevice, this, &G->clear_uav_uint_command_list, pUnorderedAccessView, false);
	mOrigContext->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
	RunViewCommandList(mHackerDevice, this, &G->clear_uav_uint_command_list, pUnorderedAccessView, true);
}

STDMETHODIMP_(void) HackerContext::ClearUnorderedAccessViewFloat(THIS_
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const FLOAT Values[4])
{
	FrameAnalysisLog("ClearUnorderedAccessViewFloat(pUnorderedAccessView:0x%p, Values:0x%p\n)",
			pUnorderedAccessView, Values);
	FrameAnalysisLogView(-1, NULL, pUnorderedAccessView);

	RunViewCommandList(mHackerDevice, this, &G->clear_uav_float_command_list, pUnorderedAccessView, false);
	mOrigContext->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
	RunViewCommandList(mHackerDevice, this, &G->clear_uav_float_command_list, pUnorderedAccessView, true);
}

STDMETHODIMP_(void) HackerContext::ClearDepthStencilView(THIS_
	/* [annotation] */
	__in  ID3D11DepthStencilView *pDepthStencilView,
	/* [annotation] */
	__in  UINT ClearFlags,
	/* [annotation] */
	__in  FLOAT Depth,
	/* [annotation] */
	__in  UINT8 Stencil)
{
	FrameAnalysisLog("ClearDepthStencilView(pDepthStencilView:0x%p, ClearFlags:%u, Depth:%f, Stencil:%u\n)",
			pDepthStencilView, ClearFlags, Depth, Stencil);
	FrameAnalysisLogView(-1, NULL, pDepthStencilView);

	RunViewCommandList(mHackerDevice, this, &G->clear_dsv_command_list, pDepthStencilView, false);
	mOrigContext->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
	RunViewCommandList(mHackerDevice, this, &G->clear_dsv_command_list, pDepthStencilView, true);
}

STDMETHODIMP_(void) HackerContext::GenerateMips(THIS_
	/* [annotation] */
	__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	FrameAnalysisLog("GenerateMips(pShaderResourceView:0x%p\n)",
			pShaderResourceView);
	FrameAnalysisLogView(-1, NULL, pShaderResourceView);

	 mOrigContext->GenerateMips(pShaderResourceView);
}

STDMETHODIMP_(void) HackerContext::SetResourceMinLOD(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	FLOAT MinLOD)
{
	FrameAnalysisLogNoNL("SetResourceMinLOD(pResource:0x%p)",
			pResource);
	FrameAnalysisLogResourceHash(pResource);

	 mOrigContext->SetResourceMinLOD(pResource, MinLOD);
}

STDMETHODIMP_(FLOAT) HackerContext::GetResourceMinLOD(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource)
{
	FLOAT ret = mOrigContext->GetResourceMinLOD(pResource);

	FrameAnalysisLogNoNL("GetResourceMinLOD(pResource:0x%p) = %f",
			pResource, ret);
	FrameAnalysisLogResourceHash(pResource);
	return ret;
}

STDMETHODIMP_(void) HackerContext::ResolveSubresource(THIS_
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
	FrameAnalysisLog("ResolveSubresource(pDstResource:0x%p, DstSubresource:%u, pSrcResource:0x%p, SrcSubresource:%u, Format:%u)\n",
			pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
	FrameAnalysisLogResource(-1, "Src", pSrcResource);
	FrameAnalysisLogResource(-1, "Dst", pDstResource);

	 mOrigContext->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource,
		Format);
}

STDMETHODIMP_(void) HackerContext::ExecuteCommandList(THIS_
	/* [annotation] */
	__in  ID3D11CommandList *pCommandList,
	BOOL RestoreContextState)
{
	FrameAnalysisLog("ExecuteCommandList(pCommandList:0x%p, RestoreContextState:%s)\n",
			pCommandList, RestoreContextState ? "true" : "false");

	if (G->deferred_contexts_enabled)
		mOrigContext->ExecuteCommandList(pCommandList, RestoreContextState);
}

STDMETHODIMP_(void) HackerContext::HSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	FrameAnalysisLog("HSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

	SetShaderResources<&ID3D11DeviceContext::HSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::HSSetShader(THIS_
	/* [annotation] */
	__in_opt  ID3D11HullShader *pHullShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	SetShader<ID3D11HullShader, &ID3D11DeviceContext::HSSetShader>
		(pHullShader, ppClassInstances, NumClassInstances,
		 &G->mVisitedHullShaders,
		 G->mSelectedHullShader,
		 &mCurrentHullShader,
		 &mCurrentHullShaderHandle);

	FrameAnalysisLog("HSSetShader(pHullShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pHullShader, ppClassInstances, NumClassInstances, mCurrentHullShader);
}

STDMETHODIMP_(void) HackerContext::HSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	FrameAnalysisLog("HSSetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);

	 mOrigContext->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::HSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	FrameAnalysisLog("HSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	 mOrigContext->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::DSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	FrameAnalysisLog("DSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

	SetShaderResources<&ID3D11DeviceContext::DSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::DSSetShader(THIS_
	/* [annotation] */
	__in_opt  ID3D11DomainShader *pDomainShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	SetShader<ID3D11DomainShader, &ID3D11DeviceContext::DSSetShader>
		(pDomainShader, ppClassInstances, NumClassInstances,
		 &G->mVisitedDomainShaders,
		 G->mSelectedDomainShader,
		 &mCurrentDomainShader,
		 &mCurrentDomainShaderHandle);

	FrameAnalysisLog("DSSetShader(pDomainShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pDomainShader, ppClassInstances, NumClassInstances, mCurrentDomainShader);
}

STDMETHODIMP_(void) HackerContext::DSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	FrameAnalysisLog("DSSetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);

	 mOrigContext->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::DSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	FrameAnalysisLog("DSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	 mOrigContext->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::CSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	FrameAnalysisLog("CSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

	SetShaderResources<&ID3D11DeviceContext::CSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::CSSetUnorderedAccessViews(THIS_
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
	/* [annotation] */
	__in_ecount(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
	/* [annotation] */
	__in_ecount(NumUAVs)  const UINT *pUAVInitialCounts)
{
	FrameAnalysisLog("CSSetUnorderedAccessViews(StartSlot:%u, NumUAVs:%u, ppUnorderedAccessViews:0x%p, pUAVInitialCounts:0x%p)\n",
			StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
	FrameAnalysisLogViewArray(StartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);

	if (ppUnorderedAccessViews) {
		// TODO: Record stats on unordered access view usage
		for (UINT i = 0; i < NumUAVs; ++i) {
			if (!ppUnorderedAccessViews[i])
				continue;
			// TODO: Record stats
			FrameAnalysisClearUAV(ppUnorderedAccessViews[i]);
		}
	}

	mOrigContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}


// C++ function template of common code shared by all XXSetShader functions:
template <class ID3D11Shader,
	 void (__stdcall ID3D11DeviceContext::*OrigSetShader)(THIS_
			 ID3D11Shader *pShader,
			 ID3D11ClassInstance *const *ppClassInstances,
			 UINT NumClassInstances)
	 >
STDMETHODIMP_(void) HackerContext::SetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11Shader *pShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances,
	std::set<UINT64> *visitedShaders,
	UINT64 selectedShader,
	UINT64 *currentShaderHash,
	ID3D11Shader **currentShaderHandle)
{
	ID3D11Shader *repl_shader = pShader;

	// Always update the current shader handle no matter what so we can
	// reliably check if a shader of a given type is bound and for certain
	// types of old style filtering:
	*currentShaderHandle = pShader;

	if (pShader) {
		// Store as current shader. Need to do this even while
		// not hunting for ShaderOverride section in BeforeDraw
		// We also set the current shader hash, but as an optimization,
		// we skip the lookup if there are no ShaderOverride The
		// lookup/find takes measurable amounts of CPU time.
		//
		// grumble grumble this optimisation caught me out grumble grumble -DSS
		if (!G->mShaderOverrideMap.empty() || (G->hunting == HUNTING_MODE_ENABLED)) {
			ShaderMap::iterator i = G->mShaders.find(pShader);
			if (i != G->mShaders.end()) {
				*currentShaderHash = i->second;
				LogDebug("  shader found: handle = %p, hash = %016I64x\n", *currentShaderHandle, *currentShaderHash);

				if ((G->hunting == HUNTING_MODE_ENABLED) && visitedShaders) {
					EnterCriticalSection(&G->mCriticalSection);
					visitedShaders->insert(i->second);
					LeaveCriticalSection(&G->mCriticalSection);
				}
			}
			else
				LogDebug("  shader %p not found\n", pShader);
		} else {
			// Not accurate, but if we have a bug where we
			// reference this at least make sure we don't use the
			// *wrong* hash
			*currentShaderHash = 0;
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		// No longer conditional on G->hunting now that hunting may be soft enabled via key binding
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pShader);
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL) {
			LogDebug("  shader replaced by: %p\n", it->second.replacement);

			// It might make sense to Release() the original shader, to recover memory on GPU
			//   -Bo3b
			// No - we're already not incrementing the refcount since we don't bind it, and if we
			// released the original it would mean the game has an invalid pointer and can crash.
			// I wouldn't worry too much about GPU memory usage beyond leaks - the driver has a
			// full virtual memory system and can swap rarely used resources out to system memory.
			// If we did want to do better here we could return a wrapper object when the game
			// creates the original shader, and manage original/replaced/reverted/etc from there.
			//   -DSS
			repl_shader = (ID3D11Shader*)it->second.replacement;
		}

		if (G->hunting == HUNTING_MODE_ENABLED) {
			// Replacement map.
			if (G->marking_mode == MARKING_MODE_ORIGINAL || !G->fix_enabled) {
				ShaderReplacementMap::iterator j = G->mOriginalShaders.find(pShader);
				if ((selectedShader == *currentShaderHash || !G->fix_enabled) && j != G->mOriginalShaders.end()) {
					repl_shader = (ID3D11Shader*)j->second;
				}
			}
			if (G->marking_mode == MARKING_MODE_ZERO) {
				ShaderReplacementMap::iterator j = G->mZeroShaders.find(pShader);
				if (selectedShader == *currentShaderHash && j != G->mZeroShaders.end()) {
					repl_shader = (ID3D11Shader*)j->second;
				}
			}
		}

	} else {
		*currentShaderHash = 0;
	}

	// Call through to original XXSetShader, but pShader may have been replaced.
	(mOrigContext->*OrigSetShader)(repl_shader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) HackerContext::CSSetShader(THIS_
	/* [annotation] */
	__in_opt  ID3D11ComputeShader *pComputeShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	SetShader<ID3D11ComputeShader, &ID3D11DeviceContext::CSSetShader>
		(pComputeShader, ppClassInstances, NumClassInstances,
		 &G->mVisitedComputeShaders,
		 G->mSelectedComputeShader,
		 &mCurrentComputeShader,
		 &mCurrentComputeShaderHandle);

	FrameAnalysisLog("CSSetShader(pComputeShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pComputeShader, ppClassInstances, NumClassInstances, mCurrentComputeShader);
}

STDMETHODIMP_(void) HackerContext::CSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	FrameAnalysisLog("CSSetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);

	 mOrigContext->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::CSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	FrameAnalysisLog("CSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	 mOrigContext->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::VSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("VSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::PSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("PSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::PSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11PixelShader **ppPixelShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLog("PSGetShader(ppPixelShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p) hash=%016I64x\n",
			ppPixelShader, ppClassInstances, pNumClassInstances, mCurrentPixelShader);
}

STDMETHODIMP_(void) HackerContext::PSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("PSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) HackerContext::VSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11VertexShader **ppVertexShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);

	// Todo: At GetShader, we need to return the original shader if it's been reloaded.
	FrameAnalysisLog("VSGetShader(ppVertexShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p) hash=%016I64x\n",
			ppVertexShader, ppClassInstances, pNumClassInstances, mCurrentVertexShader);
}

STDMETHODIMP_(void) HackerContext::PSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("PSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::IAGetInputLayout(THIS_
	/* [annotation] */
	__out  ID3D11InputLayout **ppInputLayout)
{
	 mOrigContext->IAGetInputLayout(ppInputLayout);

	FrameAnalysisLog("IAGetInputLayout(ppInputLayout:0x%p)\n",
			ppInputLayout);
}

STDMETHODIMP_(void) HackerContext::IAGetVertexBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount_opt(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
	/* [annotation] */
	__out_ecount_opt(NumBuffers)  UINT *pStrides,
	/* [annotation] */
	__out_ecount_opt(NumBuffers)  UINT *pOffsets)
{
	 mOrigContext->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

	FrameAnalysisLog("IAGetVertexBuffers(StartSlot:%u, NumBuffers:%u, ppVertexBuffers:0x%p, pStrides:0x%p, pOffsets:0x%p)\n",
			StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppVertexBuffers);
}

STDMETHODIMP_(void) HackerContext::IAGetIndexBuffer(THIS_
	/* [annotation] */
	__out_opt  ID3D11Buffer **pIndexBuffer,
	/* [annotation] */
	__out_opt  DXGI_FORMAT *Format,
	/* [annotation] */
	__out_opt  UINT *Offset)
{
	 mOrigContext->IAGetIndexBuffer(pIndexBuffer, Format, Offset);

	FrameAnalysisLog("IAGetIndexBuffer(pIndexBuffer:0x%p, Format:0x%p, Offset:0x%p)\n",
			pIndexBuffer, Format, Offset);
}

STDMETHODIMP_(void) HackerContext::GSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("GSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::GSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11GeometryShader **ppGeometryShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
	FrameAnalysisLog("GSGetShader(ppGeometryShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p) hash=%016I64x\n",
			ppGeometryShader, ppClassInstances, pNumClassInstances, mCurrentGeometryShader);
}

STDMETHODIMP_(void) HackerContext::IAGetPrimitiveTopology(THIS_
	/* [annotation] */
	__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	 mOrigContext->IAGetPrimitiveTopology(pTopology);

	FrameAnalysisLog("IAGetPrimitiveTopology(pTopology:0x%p)\n",
			pTopology);
}

STDMETHODIMP_(void) HackerContext::VSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("VSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::VSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("VSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) HackerContext::GetPredication(THIS_
	/* [annotation] */
	__out_opt  ID3D11Predicate **ppPredicate,
	/* [annotation] */
	__out_opt  BOOL *pPredicateValue)
{
	 mOrigContext->GetPredication(ppPredicate, pPredicateValue);

	FrameAnalysisLogNoNL("GetPredication(ppPredicate:0x%p, pPredicateValue:0x%p)",
			ppPredicate, pPredicateValue);
	FrameAnalysisLogAsyncQuery(ppPredicate ? *ppPredicate : NULL);
}

STDMETHODIMP_(void) HackerContext::GSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("GSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::GSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("GSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) HackerContext::OMGetRenderTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
	/* [annotation] */
	__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
	/* [annotation] */
	__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	 mOrigContext->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);

	FrameAnalysisLog("OMGetRenderTargets(NumViews:%u, ppRenderTargetViews:0x%p, ppDepthStencilView:0x%p)\n",
			NumViews, ppRenderTargetViews, ppDepthStencilView);
	FrameAnalysisLogViewArray(0, NumViews, (ID3D11View *const *)ppRenderTargetViews);
	if (ppDepthStencilView)
		FrameAnalysisLogView(-1, "D", *ppDepthStencilView);
}

STDMETHODIMP_(void) HackerContext::OMGetRenderTargetsAndUnorderedAccessViews(THIS_
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
	__out_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	 mOrigContext->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews);

	FrameAnalysisLog("OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs:%i, ppRenderTargetViews:0x%p, ppDepthStencilView:0x%p, UAVStartSlot:%i, NumUAVs:%u, ppUnorderedAccessViews:0x%p)\n",
			NumRTVs, ppRenderTargetViews, ppDepthStencilView,
			UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
	FrameAnalysisLogViewArray(0, NumRTVs, (ID3D11View *const *)ppRenderTargetViews);
	if (ppDepthStencilView)
		FrameAnalysisLogView(-1, "D", *ppDepthStencilView);
	FrameAnalysisLogViewArray(UAVStartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);
}

STDMETHODIMP_(void) HackerContext::OMGetBlendState(THIS_
	/* [annotation] */
	__out_opt  ID3D11BlendState **ppBlendState,
	/* [annotation] */
	__out_opt  FLOAT BlendFactor[4],
	/* [annotation] */
	__out_opt  UINT *pSampleMask)
{
	 mOrigContext->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);

	FrameAnalysisLog("OMGetBlendState(ppBlendState:0x%p, BlendFactor:0x%p, pSampleMask:0x%p)\n",
			ppBlendState, BlendFactor, pSampleMask);
}

STDMETHODIMP_(void) HackerContext::OMGetDepthStencilState(THIS_
	/* [annotation] */
	__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
	/* [annotation] */
	__out_opt  UINT *pStencilRef)
{
	 mOrigContext->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);

	FrameAnalysisLog("OMGetDepthStencilState(ppDepthStencilState:0x%p, pStencilRef:0x%p)\n",
			ppDepthStencilState, pStencilRef);
}

STDMETHODIMP_(void) HackerContext::SOGetTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets)
{
	 mOrigContext->SOGetTargets(NumBuffers, ppSOTargets);

	FrameAnalysisLog("SOGetTargets(NumBuffers:%u, ppSOTargets:0x%p)\n",
			NumBuffers, ppSOTargets);
	FrameAnalysisLogResourceArray(0, NumBuffers, (ID3D11Resource *const *)ppSOTargets);
}

STDMETHODIMP_(void) HackerContext::RSGetState(THIS_
	/* [annotation] */
	__out  ID3D11RasterizerState **ppRasterizerState)
{
	 mOrigContext->RSGetState(ppRasterizerState);

	FrameAnalysisLog("RSGetState(ppRasterizerState:0x%p)\n",
			ppRasterizerState);
}

STDMETHODIMP_(void) HackerContext::RSGetViewports(THIS_
	/* [annotation] */
	__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
	/* [annotation] */
	__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports)
{
	 mOrigContext->RSGetViewports(pNumViewports, pViewports);

	FrameAnalysisLog("RSGetViewports(pNumViewports:0x%p, pViewports:0x%p)\n",
			pNumViewports, pViewports);
}

STDMETHODIMP_(void) HackerContext::RSGetScissorRects(THIS_
	/* [annotation] */
	__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
	/* [annotation] */
	__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects)
{
	 mOrigContext->RSGetScissorRects(pNumRects, pRects);

	FrameAnalysisLog("RSGetScissorRects(pNumRects:0x%p, pRects:0x%p)\n",
			pNumRects, pRects);
}

STDMETHODIMP_(void) HackerContext::HSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("HSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::HSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11HullShader **ppHullShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
	FrameAnalysisLog("HSGetShader(ppHullShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p) hash=%016I64x\n",
			ppHullShader, ppClassInstances, pNumClassInstances, mCurrentHullShader);
}

STDMETHODIMP_(void) HackerContext::HSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("HSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) HackerContext::HSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("HSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::DSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("DSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::DSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11DomainShader **ppDomainShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLog("DSGetShader(ppDomainShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p) hash=%016I64x\n",
			ppDomainShader, ppClassInstances, pNumClassInstances, mCurrentDomainShader);
}

STDMETHODIMP_(void) HackerContext::DSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("DSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) HackerContext::DSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("DSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::CSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("CSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::CSGetUnorderedAccessViews(THIS_
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
	/* [annotation] */
	__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	 mOrigContext->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);

	FrameAnalysisLog("CSGetUnorderedAccessViews(StartSlot:%u, NumUAVs:%u, ppUnorderedAccessViews:0x%p)\n",
			StartSlot, NumUAVs, ppUnorderedAccessViews);
	FrameAnalysisLogViewArray(StartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);
}

STDMETHODIMP_(void) HackerContext::CSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11ComputeShader **ppComputeShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLog("CSGetShader(ppComputeShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p) hash=%016I64x\n",
			ppComputeShader, ppClassInstances, pNumClassInstances, mCurrentComputeShader);
}

STDMETHODIMP_(void) HackerContext::CSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("CSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) HackerContext::CSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("CSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::ClearState(THIS)
{
	FrameAnalysisLog("ClearState()\n");

	 mOrigContext->ClearState();

	 // ClearState() will unbind StereoParams and IniParams, so we need to
	 // rebind them now:
	 Bind3DMigotoResources();
}

STDMETHODIMP_(void) HackerContext::Flush(THIS)
{
	FrameAnalysisLog("Flush()\n");

	 mOrigContext->Flush();
}

STDMETHODIMP_(D3D11_DEVICE_CONTEXT_TYPE) HackerContext::GetType(THIS)
{
	D3D11_DEVICE_CONTEXT_TYPE ret = mOrigContext->GetType();

	FrameAnalysisLog("GetType() = %u\n", ret);
	return ret;
}

STDMETHODIMP_(UINT) HackerContext::GetContextFlags(THIS)
{
	UINT ret = mOrigContext->GetContextFlags();

	FrameAnalysisLog("GetContextFlags() = %u\n", ret);
	return ret;
}

STDMETHODIMP HackerContext::FinishCommandList(THIS_
	BOOL RestoreDeferredContextState,
	/* [annotation] */
	__out_opt  ID3D11CommandList **ppCommandList)
{
	HRESULT ret = mOrigContext->FinishCommandList(RestoreDeferredContextState, ppCommandList);

	if (!RestoreDeferredContextState) {
		// This is equivalent to calling ClearState() afterwards, so we
		// need to rebind the 3DMigoto resources now
		Bind3DMigotoResources();
	}

	FrameAnalysisLog("FinishCommandList(ppCommandList:0x%p -> 0x%p) = %u\n", ppCommandList, ppCommandList ? *ppCommandList : NULL, ret);
	return ret;
}


// -----------------------------------------------------------------------------------------------

template <void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(THIS_
		UINT StartSlot,
		UINT NumViews,
		ID3D11ShaderResourceView *const *ppShaderResourceViews)>
void HackerContext::BindStereoResources()
{
	if (!mHackerDevice) {
		LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
		return;
	}

	// Set NVidia stereo texture.
	if (mHackerDevice->mStereoResourceView && G->StereoParamsReg >= 0) {
		LogDebug("  adding NVidia stereo parameter texture to shader resources in slot %i.\n", G->StereoParamsReg);

		(mOrigContext->*OrigSetShaderResources)(G->StereoParamsReg, 1, &mHackerDevice->mStereoResourceView);
	}

	// Set constants from ini file if they exist
	if (mHackerDevice->mIniResourceView && G->IniParamsReg >= 0) {
		LogDebug("  adding ini constants as texture to shader resources in slot %i.\n", G->IniParamsReg);

		(mOrigContext->*OrigSetShaderResources)(G->IniParamsReg, 1, &mHackerDevice->mIniResourceView);
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
	// do this after the SetHackerDevice call because we need mHackerDevice
	BindStereoResources<&ID3D11DeviceContext::VSSetShaderResources>();
	BindStereoResources<&ID3D11DeviceContext::HSSetShaderResources>();
	BindStereoResources<&ID3D11DeviceContext::DSSetShaderResources>();
	BindStereoResources<&ID3D11DeviceContext::GSSetShaderResources>();
	BindStereoResources<&ID3D11DeviceContext::PSSetShaderResources>();
	BindStereoResources<&ID3D11DeviceContext::CSSetShaderResources>();
}

// This function makes sure that the StereoParams and IniParams resources
// remain pinned whenver the game assigns shader resources:
template <void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(THIS_
		UINT StartSlot,
		UINT NumViews,
		ID3D11ShaderResourceView *const *ppShaderResourceViews)>
void HackerContext::SetShaderResources(UINT StartSlot, UINT NumViews,
		ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	ID3D11ShaderResourceView **override_srvs = NULL;

	if (!mHackerDevice)
		return;

	if (mHackerDevice->mStereoResourceView && G->StereoParamsReg >= 0) {
		if (NumViews > G->StereoParamsReg - StartSlot) {
			LogDebug("  Game attempted to unbind StereoParams, pinning in slot %i\n", G->StereoParamsReg);
			override_srvs = new ID3D11ShaderResourceView*[NumViews];
			memcpy(override_srvs, ppShaderResourceViews, sizeof(ID3D11ShaderResourceView*) * NumViews);
			override_srvs[G->StereoParamsReg - StartSlot] = mHackerDevice->mStereoResourceView;
		}
	}

	if (mHackerDevice->mIniResourceView && G->IniParamsReg >= 0) {
		if (NumViews > G->IniParamsReg - StartSlot) {
			LogDebug("  Game attempted to unbind IniParams, pinning in slot %i\n", G->IniParamsReg);
			if (!override_srvs) {
				override_srvs = new ID3D11ShaderResourceView*[NumViews];
				memcpy(override_srvs, ppShaderResourceViews, sizeof(ID3D11ShaderResourceView*) * NumViews);
			}
			override_srvs[G->IniParamsReg - StartSlot] = mHackerDevice->mIniResourceView;
		}
	}

	if (override_srvs) {
		(mOrigContext->*OrigSetShaderResources)(StartSlot, NumViews, override_srvs);
		delete [] override_srvs;
	} else {
		(mOrigContext->*OrigSetShaderResources)(StartSlot, NumViews, ppShaderResourceViews);
	}
}

// The rest of these methods are all the primary code for the tool, Direct3D calls that we override
// in order to replace or modify shaders.

STDMETHODIMP_(void) HackerContext::VSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11VertexShader *pVertexShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	SetShader<ID3D11VertexShader, &ID3D11DeviceContext::VSSetShader>
		(pVertexShader, ppClassInstances, NumClassInstances,
		 &G->mVisitedVertexShaders,
		 G->mSelectedVertexShader,
		 &mCurrentVertexShader,
		 &mCurrentVertexShaderHandle);

	FrameAnalysisLog("VSSetShader(pVertexShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pVertexShader, ppClassInstances, NumClassInstances, mCurrentVertexShader);
}

STDMETHODIMP_(void) HackerContext::PSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	FrameAnalysisLog("PSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

	SetShaderResources<&ID3D11DeviceContext::PSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::PSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11PixelShader *pPixelShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	SetShader<ID3D11PixelShader, &ID3D11DeviceContext::PSSetShader>
		(pPixelShader, ppClassInstances, NumClassInstances,
		 &G->mVisitedPixelShaders,
		 G->mSelectedPixelShader,
		 &mCurrentPixelShader,
		 &mCurrentPixelShaderHandle);

	FrameAnalysisLog("PSSetShader(pPixelShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pPixelShader, ppClassInstances, NumClassInstances, mCurrentPixelShader);

	if (pPixelShader) {
		// Set custom depth texture.
		if (mHackerDevice->mZBufferResourceView)
		{
			LogDebug("  adding Z buffer to shader resources in slot 126.\n");

			mOrigContext->PSSetShaderResources(126, 1, &mHackerDevice->mZBufferResourceView);
		}
	}
}

STDMETHODIMP_(void) HackerContext::DrawIndexed(THIS_
	/* [annotation] */
	__in  UINT IndexCount,
	/* [annotation] */
	__in  UINT StartIndexLocation,
	/* [annotation] */
	__in  INT BaseVertexLocation)
{
	DrawContext c = DrawContext(0, IndexCount, 0, BaseVertexLocation, StartIndexLocation, 0, NULL, 0, false);
	BeforeDraw(c);

	FrameAnalysisLog("DrawIndexed(IndexCount:%u, StartIndexLocation:%u, BaseVertexLocation:%u)\n",
			IndexCount, StartIndexLocation, BaseVertexLocation);

	if (!c.call_info.skip)
		 mOrigContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::Draw(THIS_
	/* [annotation] */
	__in  UINT VertexCount,
	/* [annotation] */
	__in  UINT StartVertexLocation)
{
	DrawContext c = DrawContext(VertexCount, 0, 0, StartVertexLocation, 0, 0, NULL, 0, false);
	BeforeDraw(c);

	FrameAnalysisLog("Draw(VertexCount:%u, StartVertexLocation:%u)\n",
			VertexCount, StartVertexLocation);

	if (!c.call_info.skip)
		 mOrigContext->Draw(VertexCount, StartVertexLocation);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::IASetIndexBuffer(THIS_
	/* [annotation] */
	__in_opt ID3D11Buffer *pIndexBuffer,
	/* [annotation] */
	__in DXGI_FORMAT Format,
	/* [annotation] */
	__in  UINT Offset)
{
	FrameAnalysisLogNoNL("IASetIndexBuffer(pIndexBuffer:0x%p, Format:%u, Offset:%u)",
			pIndexBuffer, Format, Offset);
	FrameAnalysisLogResourceHash(pIndexBuffer);

	mOrigContext->IASetIndexBuffer(pIndexBuffer, Format, Offset);

	// This is only used for index buffer hunting nowadays since the
	// command list checks the hash on demand only when it is needed
	mCurrentIndexBuffer = 0;
	if (pIndexBuffer && G->hunting == HUNTING_MODE_ENABLED) {
		mCurrentIndexBuffer = GetResourceHash(pIndexBuffer);
		if (mCurrentIndexBuffer) {
			// When hunting, save this as a visited index buffer to cycle through.
			EnterCriticalSection(&G->mCriticalSection);
			G->mVisitedIndexBuffers.insert(mCurrentIndexBuffer);
			LeaveCriticalSection(&G->mCriticalSection);
		}
	}
}

STDMETHODIMP_(void) HackerContext::DrawIndexedInstanced(THIS_
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
	DrawContext c = DrawContext(0, IndexCountPerInstance, InstanceCount, BaseVertexLocation, StartIndexLocation, StartInstanceLocation, NULL, 0, false);
	BeforeDraw(c);

	FrameAnalysisLog("DrawIndexedInstanced(IndexCountPerInstance:%u, InstanceCount:%u, StartIndexLocation:%u, BaseVertexLocation:%i, StartInstanceLocation:%u)\n",
			IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

	if (!c.call_info.skip)
		mOrigContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
		BaseVertexLocation, StartInstanceLocation);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawInstanced(THIS_
	/* [annotation] */
	__in  UINT VertexCountPerInstance,
	/* [annotation] */
	__in  UINT InstanceCount,
	/* [annotation] */
	__in  UINT StartVertexLocation,
	/* [annotation] */
	__in  UINT StartInstanceLocation)
{
	DrawContext c = DrawContext(VertexCountPerInstance, 0, InstanceCount, StartVertexLocation, 0, StartInstanceLocation, NULL, 0, false);
	BeforeDraw(c);

	FrameAnalysisLog("DrawInstanced(VertexCountPerInstance:%u, InstanceCount:%u, StartVertexLocation:%u, StartInstanceLocation:%u)\n",
			VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

	if (!c.call_info.skip)
		mOrigContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::VSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	FrameAnalysisLog("VSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

	SetShaderResources<&ID3D11DeviceContext::VSSetShaderResources>(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::OMSetRenderTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
	/* [annotation] */
	__in_ecount_opt(NumViews) ID3D11RenderTargetView *const *ppRenderTargetViews,
	/* [annotation] */
	__in_opt ID3D11DepthStencilView *pDepthStencilView)
{
	FrameAnalysisLog("OMSetRenderTargets(NumViews:%u, ppRenderTargetViews:0x%p, pDepthStencilView:0x%p)\n",
			NumViews, ppRenderTargetViews, pDepthStencilView);
	FrameAnalysisLogViewArray(0, NumViews, (ID3D11View *const *)ppRenderTargetViews);
	FrameAnalysisLogView(-1, "D", pDepthStencilView);

	if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);
			mCurrentRenderTargets.clear();
			mCurrentDepthTarget = NULL;
		LeaveCriticalSection(&G->mCriticalSection);

		if (ppRenderTargetViews) {
			for (UINT i = 0; i < NumViews; ++i) {
				if (!ppRenderTargetViews[i])
					continue;
				if (G->DumpUsage)
					RecordRenderTargetInfo(ppRenderTargetViews[i], i);
				FrameAnalysisClearRT(ppRenderTargetViews[i]);
			}
		}

		if (G->DumpUsage)
			RecordDepthStencil(pDepthStencilView);
	}

	mOrigContext->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}

STDMETHODIMP_(void) HackerContext::OMSetRenderTargetsAndUnorderedAccessViews(THIS_
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
	__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts)
{
	FrameAnalysisLog("OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs:%i, ppRenderTargetViews:0x%p, pDepthStencilView:0x%p, UAVStartSlot:%i, NumUAVs:%u, ppUnorderedAccessViews:0x%p, pUAVInitialCounts:0x%p)\n",
			NumRTVs, ppRenderTargetViews, pDepthStencilView,
			UAVStartSlot, NumUAVs, ppUnorderedAccessViews,
			pUAVInitialCounts);
	FrameAnalysisLogViewArray(0, NumRTVs, (ID3D11View *const *)ppRenderTargetViews);
	FrameAnalysisLogView(-1, "D", pDepthStencilView);
	FrameAnalysisLogViewArray(UAVStartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);

	if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);
			mCurrentRenderTargets.clear();
			mCurrentDepthTarget = NULL;
		LeaveCriticalSection(&G->mCriticalSection);

		if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
			if (ppRenderTargetViews) {
				for (UINT i = 0; i < NumRTVs; ++i) {
					if (!ppRenderTargetViews[i])
						continue;
					if (G->DumpUsage)
						RecordRenderTargetInfo(ppRenderTargetViews[i], i);
					FrameAnalysisClearRT(ppRenderTargetViews[i]);
				}
			}

			if (G->DumpUsage)
				RecordDepthStencil(pDepthStencilView);
		}

		if (ppUnorderedAccessViews && (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)) {
			for (UINT i = 0; i < NumUAVs; ++i) {
				if (!ppUnorderedAccessViews[i])
					continue;
				// TODO: Record stats
				FrameAnalysisClearUAV(ppUnorderedAccessViews[i]);
			}
		}
	}

	mOrigContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

STDMETHODIMP_(void) HackerContext::DrawAuto(THIS)
{
	DrawContext c = DrawContext(0, 0, 0, 0, 0, 0, NULL, 0, false);
	BeforeDraw(c);

	FrameAnalysisLog("DrawAuto()\n");

	if (!c.call_info.skip)
		mOrigContext->DrawAuto();
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawIndexedInstancedIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	DrawContext c = DrawContext(0, 0, 0, 0, 0, 0, pBufferForArgs, AlignedByteOffsetForArgs, false);
	BeforeDraw(c);

	FrameAnalysisLog("DrawIndexedInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

	if (!c.call_info.skip)
		mOrigContext->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawInstancedIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	DrawContext c = DrawContext(0, 0, 0, 0, 0, 0, pBufferForArgs, AlignedByteOffsetForArgs, true);
	BeforeDraw(c);

	FrameAnalysisLog("DrawInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

	if (!c.call_info.skip)
		mOrigContext->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::ClearRenderTargetView(THIS_
	/* [annotation] */
	__in  ID3D11RenderTargetView *pRenderTargetView,
	/* [annotation] */
	__in  const FLOAT ColorRGBA[4])
{
	FrameAnalysisLog("ClearRenderTargetView(pRenderTargetView:0x%p, ColorRGBA:0x%p)\n",
			pRenderTargetView, ColorRGBA);
	FrameAnalysisLogView(-1, NULL, pRenderTargetView);

	RunViewCommandList(mHackerDevice, this, &G->clear_rtv_command_list, pRenderTargetView, false);
	mOrigContext->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
	RunViewCommandList(mHackerDevice, this, &G->clear_rtv_command_list, pRenderTargetView, true);
}


// -----------------------------------------------------------------------------
// HackerContext1
//	Requires Win7 Platform Update

// Hierarchy:
//  HackerContext1 <- HackerContext <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

HackerContext1::HackerContext1(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext)
	: HackerContext(pDevice1, pContext)
{
	mOrigDevice1 = pDevice1;
	mOrigContext1 = pContext;
}

void HackerContext1::SetHackerDevice1(HackerDevice1 *pDevice)
{
	mHackerDevice1 = pDevice;

	// Pass this along to the superclass, in case games improperly use the non1 versions.
	SetHackerDevice(pDevice);
}


void STDMETHODCALLTYPE HackerContext1::CopySubresourceRegion1(
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
	_In_  UINT CopyFlags)
{
	FrameAnalysisLog("CopySubresourceRegion1(pDstResource:0x%p, DstSubresource:%u, DstX:%u, DstY:%u, DstZ:%u, pSrcResource:0x%p, SrcSubresource:%u, pSrcBox:0x%p, CopyFlags:%u)\n",
			pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
	FrameAnalysisLogResource(-1, "Src", pSrcResource);
	FrameAnalysisLogResource(-1, "Dst", pDstResource);

	mOrigContext1->CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
}

void STDMETHODCALLTYPE HackerContext1::UpdateSubresource1(
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
	_In_  UINT CopyFlags)
{
	FrameAnalysisLogNoNL("UpdateSubresource1(pDstResource:0x%p, DstSubresource:%u, pDstBox:0x%p, pSrcData:0x%p, SrcRowPitch:%u, SrcDepthPitch:%u, CopyFlags:%u)",
			pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
	FrameAnalysisLogResourceHash(pDstResource);

	mOrigContext1->UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);

	// TODO: Track resource hash updates
}


void STDMETHODCALLTYPE HackerContext1::DiscardResource(
	/* [annotation] */
	_In_  ID3D11Resource *pResource)
{
	FrameAnalysisLogNoNL("DiscardResource(pResource:0x%p)",
			pResource);
	FrameAnalysisLogResourceHash(pResource);

	mOrigContext1->DiscardResource(pResource);
}

void STDMETHODCALLTYPE HackerContext1::DiscardView(
	/* [annotation] */
	_In_  ID3D11View *pResourceView)
{
	FrameAnalysisLog("DiscardView(pResourceView:0x%p)\n",
			pResourceView);
	FrameAnalysisLogView(-1, NULL, pResourceView);

	mOrigContext1->DiscardView(pResourceView);
}


void STDMETHODCALLTYPE HackerContext1::VSSetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pNumConstants)
{
	FrameAnalysisLog("VSSetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}


void STDMETHODCALLTYPE HackerContext1::HSSetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pNumConstants)
{
	FrameAnalysisLog("HSSetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}


void STDMETHODCALLTYPE HackerContext1::DSSetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pNumConstants)
{
	FrameAnalysisLog("DSSetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext1::GSSetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pNumConstants)
{
	FrameAnalysisLog("GSSetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext1::PSSetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pNumConstants)
{
	FrameAnalysisLog("PSSetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext1::CSSetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
	/* [annotation] */
	_In_reads_opt_(NumBuffers)  const UINT *pNumConstants)
{
	FrameAnalysisLog("CSSetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

	mOrigContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext1::VSGetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pNumConstants)
{
	mOrigContext1->VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("VSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}



void STDMETHODCALLTYPE HackerContext1::HSGetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pNumConstants)
{
	mOrigContext1->HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("HSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}



void STDMETHODCALLTYPE HackerContext1::DSGetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pNumConstants)
{
	mOrigContext1->DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("DSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}


void STDMETHODCALLTYPE HackerContext1::GSGetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pNumConstants)
{
	mOrigContext1->GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("GSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}


void STDMETHODCALLTYPE HackerContext1::PSGetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pNumConstants)
{
	mOrigContext1->PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("PSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}


void STDMETHODCALLTYPE HackerContext1::CSGetConstantBuffers1(
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
	/* [annotation] */
	_Out_writes_opt_(NumBuffers)  UINT *pNumConstants)
{
	mOrigContext1->CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("CSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}


void STDMETHODCALLTYPE HackerContext1::SwapDeviceContextState(
	/* [annotation] */
	_In_  ID3DDeviceContextState *pState,
	/* [annotation] */
	_Out_opt_  ID3DDeviceContextState **ppPreviousState)
{
	FrameAnalysisLog("SwapDeviceContextState(pState:0x%p, ppPreviousState:0x%p)\n",
			pState, ppPreviousState);

	mOrigContext1->SwapDeviceContextState(pState, ppPreviousState);

	// If a game or overlay creates separate context state objects we won't
	// have had a chance to bind the 3DMigoto resources when it was
	// first created, so do so now:
	Bind3DMigotoResources();
}


void STDMETHODCALLTYPE HackerContext1::ClearView(
	/* [annotation] */
	_In_  ID3D11View *pView,
	/* [annotation] */
	_In_  const FLOAT Color[4],
	/* [annotation] */
	_In_reads_opt_(NumRects)  const D3D11_RECT *pRect,
	UINT NumRects)
{
	FrameAnalysisLog("ClearView(pView:0x%p, Color:0x%p, pRect:0x%p)\n",
			pView, Color, pRect);
	FrameAnalysisLogView(-1, NULL, pView);

	// TODO: Add a command list here, but we probably actualy want to call
	// the existing RTV / DSV / UAV clear command lists instead for
	// compatibility with engines that might use this if the feature level
	// is high enough, and the others if it is not.
	mOrigContext1->ClearView(pView, Color, pRect, NumRects);
}


void STDMETHODCALLTYPE HackerContext1::DiscardView1(
	/* [annotation] */
	_In_  ID3D11View *pResourceView,
	/* [annotation] */
	_In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
	UINT NumRects)
{
	FrameAnalysisLog("DiscardView1(pResourceView:0x%p, pRects:0x%p)\n",
			pResourceView, pRects);
	FrameAnalysisLogView(-1, NULL, pResourceView);

	mOrigContext1->DiscardView1(pResourceView, pRects, NumRects);
}
