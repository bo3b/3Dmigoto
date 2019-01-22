// Wrapper for the ID3D11DeviceContext1.
//
// Object				OS				D3D11 version	Feature level
// ID3D11DeviceContext	Win7			11.0			11.0
// ID3D11DeviceContext1	Platform update	11.1			11.1
// ID3D11DeviceContext2	Win8.1			11.2
// ID3D11DeviceContext3					11.3

// This gives us access to every D3D11 call for a Context, and override the pieces needed.
// Hierarchy:
//  HackerContext <- ID3D11DeviceContext1 <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

#include "HackerContext.h"

//#include "HookedContext.h"

#include "log.h"
#include "Globals.h"

#include "HackerDevice.h"
#include "D3D11Wrapper.h"
//#include "ResourceHash.h"
//#include "Override.h"
#include "ShaderRegex.h"
#include "FrameAnalysis.h"
#include "profiling.h"
#include "api.h"

// -----------------------------------------------------------------------------------------------

HackerContext* HackerContextFactory(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext1)
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
	if (G->hunting || gLogDebug) {
		LogInfo("  Creating FrameAnalysisContext\n");
		return new FrameAnalysisContext(pDevice1, pContext1);
	}

	LogInfo("  Creating HackerContext - frame analysis log will not be available\n");
	return new HackerContext(pDevice1, pContext1);
}

HackerContext::HackerContext(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext1)
{
	mOrigDevice1 = pDevice1;
	mOrigContext1 = pContext1;
	mRealOrigContext1 = pContext1;
	mHackerDevice = NULL;

	memset(mCurrentVertexBuffers, 0, sizeof(mCurrentVertexBuffers));
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
	mCurrentPSUAVStartSlot = 0;
	mCurrentPSNumUAVs = 0;
}


// Save the corresponding HackerDevice, as we need to use it periodically to get
// access to the StereoParams.

void HackerContext::SetHackerDevice(HackerDevice *pDevice)
{
	mHackerDevice = pDevice;
}

HackerDevice* HackerContext::GetHackerDevice()
{
	return mHackerDevice;
}

// Returns the "real" DirectX object. Note that if hooking is enabled calls
// through this object will go back into 3DMigoto, which would then subject
// them to extra logging and any processing 3DMigoto applies, which may be
// undesirable in some cases. This used to cause a crash if a command list
// issued a draw call, since that would then trigger the command list and
// recurse until the stack ran out:
ID3D11DeviceContext1* HackerContext::GetPossiblyHookedOrigContext1(void)
{
	return mRealOrigContext1;
}

// Use this one when you specifically don't want calls through this object to
// ever go back into 3DMigoto. If hooking is disabled this is identical to the
// above, but when hooking this will be the trampoline object instead:
ID3D11DeviceContext1* HackerContext::GetPassThroughOrigContext1(void)
{
	return mOrigContext1;
}

void HackerContext::HookContext()
{
	// This will install hooks in the original context (if they have not
	// already been installed from a prior context) which will call the
	// equivalent function in this HackerContext. It returns a trampoline
	// interface which we use in place of mOrigContext1 to call the real
	// original context, thereby side stepping the problem that calling the
	// old mOrigContext1 would be hooked and call back into us endlessly:
	mOrigContext1 = hook_context(mOrigContext1, this);
}

// -----------------------------------------------------------------------------


// Records the hash of this shader resource view for later lookup. Returns the
// handle to the resource, but be aware that it no longer has a reference and
// should only be used for map lookups.
ID3D11Resource* HackerContext::RecordResourceViewStats(ID3D11View *view, std::set<uint32_t> *resource_info)
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
			resource_info->insert(orig_hash);

	LeaveCriticalSection(&G->mCriticalSection);

	return resource;
}

static ResourceSnapshot SnapshotResource(ID3D11Resource *handle)
{
	uint32_t hash = 0, orig_hash = 0;

	ResourceHandleInfo *info = GetResourceHandleInfo(handle);
	if (info) {
		hash = info->hash;
		orig_hash = info->orig_hash;
	}

	return ResourceSnapshot(handle, hash, orig_hash);
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

	(mOrigContext1->*GetShaderResources)(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (views[i]) {
			resource = RecordResourceViewStats(views[i], &G->mShaderResourceInfo);
			if (resource)
				shader_info->ResourceRegisters[i].insert(SnapshotResource(resource));
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
	ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT]; // DX11: 8, DX11.1: 64
	UINT selectedRenderTargetPos;
	ShaderInfoData *info;
	ID3D11Resource *resource;
	UINT i;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

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
				info->RenderTargets.push_back(std::set<ResourceSnapshot>());

			info->RenderTargets[selectedRenderTargetPos].insert(SnapshotResource(mCurrentRenderTargets[selectedRenderTargetPos]));
		}

		if (mCurrentDepthTarget)
			info->DepthTargets.insert(SnapshotResource(mCurrentDepthTarget));

		if (mCurrentPSNumUAVs) {
			// This API is poorly designed, because we have to know
			// the current UAV start slot
			OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, mCurrentPSUAVStartSlot, mCurrentPSNumUAVs, uavs);
			for (i = 0; i < mCurrentPSNumUAVs; i++) {
				if (uavs[i]) {
					resource = RecordResourceViewStats(uavs[i], &G->mUnorderedAccessInfo);
					if (resource)
						info->UAVs[i + mCurrentPSUAVStartSlot].insert(SnapshotResource(resource));

					uavs[i]->Release();
				}
			}
		}
	}

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::stat_overhead);
}

void HackerContext::RecordComputeShaderStats()
{
	ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT]; // DX11: 8, DX11.1: 64
	ShaderInfoData *info = &G->mComputeShaderInfo[mCurrentComputeShader];
	D3D_FEATURE_LEVEL level = mOrigDevice1->GetFeatureLevel();
	UINT num_uavs = (level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT);
	ID3D11Resource *resource;
	UINT i;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	RecordShaderResourceUsage<&ID3D11DeviceContext::CSGetShaderResources>(info);

	CSGetUnorderedAccessViews(0, num_uavs, uavs);
	for (i = 0; i < num_uavs; i++) {
		if (uavs[i]) {
			resource = RecordResourceViewStats(uavs[i], &G->mUnorderedAccessInfo);
			if (resource)
				info->UAVs[i].insert(SnapshotResource(resource));

			uavs[i]->Release();
		}
	}

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::stat_overhead);
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
	mOrigContext1->VSGetShader(&pVertexShader, &pClassInstances, &NumClassInstances);
	mOrigContext1->VSSetShader(shader, &pClassInstances, NumClassInstances);

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
	mOrigContext1->PSGetShader(&pPixelShader, &pClassInstances, &NumClassInstances);
	mOrigContext1->PSSetShader(shader, &pClassInstances, NumClassInstances);

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

			mOrigContext1->OMGetRenderTargets(0, NULL, &pDepthStencilView);

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
				ShaderReplacementMap::iterator i = lookup_original_shader(mCurrentPixelShaderHandle);
				if (i != G->mOriginalShaders.end())
					data->oldPixelShader = SwitchPSShader((ID3D11PixelShader*)i->second);
			}
			else {
				ShaderReplacementMap::iterator i = lookup_original_shader(mCurrentVertexShaderHandle);
				if (i != G->mOriginalShaders.end())
					data->oldVertexShader = SwitchVSShader((ID3D11VertexShader*)i->second);
			}
		}
	}
}

static std::unordered_set<unsigned> ue4_patch_cb_offset;

#define NEW_UE4
#define SENUA

static void init_ue4_patch_cb_offset(unsigned offset, unsigned count)
{
	unsigned i;

	for (i = 0; i < count; i++) {
		ue4_patch_cb_offset.insert(offset + i);
	}
}

static void init_ue4_patch_cb_offsets()
{
	static bool init = false;
	unsigned off = 0;

	if (init)
		return;
	init = true;

	init_ue4_patch_cb_offset(0 + off, 4); // FMatrix TranslatedWorldToClip;
	init_ue4_patch_cb_offset(4 + off, 4); // FMatrix WorldToClip;
	// init_ue4_patch_cb_offset(8 + off, 4); // FMatrix TranslatedWorldToView;
	// init_ue4_patch_cb_offset(12 + off, 4); // FMatrix ViewToTranslatedWorld;
	// init_ue4_patch_cb_offset(16 + off, 4); // FMatrix TranslatedWorldToCameraView;
	// init_ue4_patch_cb_offset(20 + off, 4); // FMatrix CameraViewToTranslatedWorld;
	init_ue4_patch_cb_offset(24 + off, 4); // FMatrix ViewToClip;
	init_ue4_patch_cb_offset(28 + off, 4); // FMatrix ClipToView;
	init_ue4_patch_cb_offset(32 + off, 4); // FMatrix ClipToTranslatedWorld;
	init_ue4_patch_cb_offset(36 + off, 4); // FMatrix SVPositionToTranslatedWorld;
	init_ue4_patch_cb_offset(40 + off, 4); // FMatrix ScreenToWorld;
	init_ue4_patch_cb_offset(44 + off, 4); // FMatrix ScreenToTranslatedWorld;

#ifdef NEW_UE4
	off += 2; // HMDViewNoRollUp & HMDViewNoRollRight
#endif

#ifdef SENUA
	off += 1;
#endif

	init_ue4_patch_cb_offset(53 + off, 1); // FVector WorldCameraOrigin;
	init_ue4_patch_cb_offset(54 + off, 1); // FVector TranslatedWorldCameraOrigin;
	init_ue4_patch_cb_offset(55 + off, 1); // FVector WorldViewOrigin;
	// init_ue4_patch_cb_offset(56 + off, 1); // FVector PreViewTranslation;

	init_ue4_patch_cb_offset(57 + off, 4); // FMatrix PrevProjection;
	init_ue4_patch_cb_offset(61 + off, 4); // FMatrix PrevViewProj;
	init_ue4_patch_cb_offset(65 + off, 4); // FMatrix PrevViewRotationProj;
	init_ue4_patch_cb_offset(69 + off, 4); // FMatrix PrevViewToClip;
	init_ue4_patch_cb_offset(73 + off, 4); // FMatrix PrevClipToView;
	init_ue4_patch_cb_offset(77 + off, 4); // FMatrix PrevTranslatedWorldToClip;
	// init_ue4_patch_cb_offset(81 + off, 4); // FMatrix PrevTranslatedWorldToView;
	// init_ue4_patch_cb_offset(85 + off, 4); // FMatrix PrevViewToTranslatedWorld;
	// init_ue4_patch_cb_offset(89 + off, 4); // FMatrix PrevTranslatedWorldToCameraView;
	// init_ue4_patch_cb_offset(93 + off, 4); // FMatrix PrevCameraViewToTranslatedWorld;

	init_ue4_patch_cb_offset(97 + off, 1); // FVector PrevWorldCameraOrigin;
	init_ue4_patch_cb_offset(98 + off, 1); // FVector PrevWorldViewOrigin;
	// init_ue4_patch_cb_offset(99 + off, 1); // FVector PrevPreViewTranslation;

	init_ue4_patch_cb_offset(100 + off, 4); // FMatrix PrevInvViewProj;
	init_ue4_patch_cb_offset(104 + off, 4); // FMatrix PrevScreenToTranslatedWorld;
#ifdef SENUA
	// Senua's Sacrifice specific. Looks like a duplicate of PrevViewProj
	// (or TranslatedWorldToClip). I bet I know what happened - UE4's
	// inconsistent matrix naming scheme confused the devs and they didn't
	// realise that "PrevTranslatedWorldToClip" was already present under
	// the name "PrevViewProj", so they added a second copy of it:
	init_ue4_patch_cb_offset(108 + off, 4);
	off += 4;
#endif
	init_ue4_patch_cb_offset(108 + off, 4); // FMatrix ClipToPrevClip;
}

static bool patch_cb(std::string *asm_text, unsigned cb_reg, size_t *dcl_end, unsigned *tmp_regs, unsigned shader_model_major)
{
	std::unordered_map<unsigned, unsigned> cb_idx_to_tmp_reg;
	std::unordered_map<unsigned, unsigned>::iterator i;
	std::string cb_search, cb_index_str, tmp_reg_str, insert_str;
	size_t cb_pos, cb_idx_start, cb_idx_end;
	bool patched = false;
	unsigned cb_idx, tmp_reg;
	unsigned ld_idx, ld_off;

	cb_search = std::string("cb") + std::to_string(cb_reg) + std::string("[");
	LogInfo("Redirecting cb%d to t%d\n", cb_reg, cb_reg + 100);

	for (
		cb_pos = asm_text->find(cb_search, *dcl_end);
		cb_pos != std::string::npos;
		cb_pos = asm_text->find(cb_search, cb_pos + 1)
	) {
		cb_idx_start = cb_pos + cb_search.length();
		cb_idx_end = asm_text->find("]", cb_idx_start);
		cb_index_str = asm_text->substr(cb_idx_start, cb_idx_end - cb_idx_start);
		if (cb_index_str[0] < '0' || cb_index_str[0] > '9') {
			LogInfo("Cannot patch cb%d[%s]\n", cb_reg, cb_index_str.c_str()); // yet ;-)
			continue;
		}
		cb_idx = stoul(asm_text->substr(cb_idx_start, 4));

		if (!ue4_patch_cb_offset.count(cb_idx)) {
			LogInfo("Skipping cb%d[%s]\n", cb_reg, cb_index_str.c_str());
			continue;
		}

		try {
			tmp_reg = cb_idx_to_tmp_reg.at(cb_idx);
		} catch (std::out_of_range) {
			tmp_reg = (*tmp_regs)++;
			cb_idx_to_tmp_reg[cb_idx] = tmp_reg;
		}

		tmp_reg_str = std::string("r") + std::to_string(tmp_reg);
		LogInfo("Replacing cb%d[%s] with %s\n", cb_reg, cb_index_str.c_str(), tmp_reg_str.c_str());
		asm_text->replace(cb_pos, cb_idx_end - cb_pos + 1, tmp_reg_str);

		patched = true;
	}

	if (!patched)
		return false;

	insert_str = std::string("\ndcl_resource_structured t")
		+ std::to_string(cb_reg + 100)
		+ std::string(", 2048"); // Max allowed stride
	LogInfo("Inserting %s\n", insert_str.substr(1).c_str());
	asm_text->insert(*dcl_end, insert_str);
	*dcl_end += insert_str.length();

	for (i = cb_idx_to_tmp_reg.begin(); i != cb_idx_to_tmp_reg.end(); i++) {
		cb_idx = i->first * 16;
		tmp_reg = i->second;
		ld_idx = cb_idx / 2048;
		ld_off = cb_idx % 2048;

		if (shader_model_major == 5) {
			insert_str = std::string("\nld_structured_indexable(structured_buffer, stride=2048)(mixed,mixed,mixed,mixed) r");
		} else {
			insert_str = std::string("\nld_structured r");
			"ld_structured {reg}.xyzw, l({idx}), l({offset}), t{sb}.xyzw";
		}
		insert_str = insert_str
			+ std::to_string(tmp_reg)
			+ std::string(".xyzw, l(")
			+ std::to_string(ld_idx)
			+ std::string("), l(")
			+ std::to_string(ld_off)
			+ std::string("), t")
			+ std::to_string(cb_reg + 100)
			+ std::string(".xyzw");

		LogInfo("Inserting %s\n", insert_str.substr(1).c_str());
		asm_text->insert(*dcl_end, insert_str);
	}

	return true;
}

static bool patch_asm_redirect_cb(std::string *asm_text,
		bool patch_cbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT],
		int disable_driver_stereo_vs_reg, int disable_driver_stereo_ds_reg)
{
	bool patched = false;
	size_t dcl_start, dcl_end, dcl_temps, dcl_temps_end;
	size_t dcl_cb, shader_model_pos;
	std::string shader_model;
	unsigned shader_model_major, tmp_regs = 0, cb_reg;
	std::string insert_str;
	int disable_driver_stereo_reg = -1;

	init_ue4_patch_cb_offsets();

	LogInfo("Analysing shader constant buffer usage...\n");
	for (
		shader_model_pos = asm_text->find("\n");
		shader_model_pos != std::string::npos && (*asm_text)[shader_model_pos + 1] == '/';
		shader_model_pos = asm_text->find("\n", shader_model_pos + 1)
	) {}
	shader_model = asm_text->substr(shader_model_pos + 1, asm_text->find("\n", shader_model_pos + 1) - shader_model_pos - 1);
	shader_model_major = stoul(asm_text->substr(shader_model_pos + 4, 1));
	LogInfo("Found %s\n", shader_model.c_str());
	if (shader_model_major < 4 || shader_model_major > 5) {
		LogInfo("Unsupported shader model\n");
		return false;
	}

	if (shader_model[0] == 'v')
		disable_driver_stereo_reg = disable_driver_stereo_vs_reg;
	else if (shader_model[0] == 'd')
		disable_driver_stereo_reg = disable_driver_stereo_ds_reg;

	dcl_start = asm_text->find("\ndcl_", shader_model_pos);
	dcl_end = asm_text->rfind("\ndcl_");
	dcl_end = asm_text->find("\n", dcl_end + 1);
	dcl_temps = asm_text->find("\ndcl_temps ", dcl_start);

	if (dcl_temps != std::string::npos) {
		tmp_regs = stoul(asm_text->substr(dcl_temps + 10, 4));
		LogInfo("Found dcl_temps %d\n", tmp_regs);
	}

	for (
		dcl_cb = asm_text->find("dcl_constantbuffer ", dcl_start);
		dcl_cb != std::string::npos;
		dcl_cb = asm_text->find("dcl_constantbuffer ", dcl_cb + 1)
	) {
		cb_reg = stoul(asm_text->substr(dcl_cb + 21, 2));
		LogInfo("Found dcl_constantbuffer cb%d\n", cb_reg);

		if (cb_reg == disable_driver_stereo_reg) {
			LogInfo("!!! WARNING: cb%d conflicts with driver stereo reg - SHADER BROKEN !!!\n", cb_reg);
			// TODO: BeepFailure();
			disable_driver_stereo_reg = -1;
		}

		// There are two constant buffers reserved for system use. They
		// can't be used in HLSL, but could potentially be used in
		// assembly, so we need to check that we are in the expected
		// range before using it to index into patch_cbuffers:
		if (cb_reg >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
			LogInfo("cb%d out of range\n", cb_reg);
			continue;
		}

		if (!patch_cbuffers[cb_reg])
			continue;

		patch_cbuffers[cb_reg] = patch_cb(asm_text, cb_reg, &dcl_end, &tmp_regs, shader_model_major);

		patched = patched || patch_cbuffers[cb_reg];
	}

	// XXX: Here or after disabling the driver stereo cb?
	if (!patched)
		return false;

	if (disable_driver_stereo_reg != -1) {
		insert_str = std::string("\ndcl_constantbuffer cb")
			+ std::to_string(disable_driver_stereo_reg)
			+ std::string("[4], immediateIndexed");
		LogInfo("Disabling driver stereo correction: %s\n", insert_str.substr(1).c_str());
		insert_str = std::string("\n\n// Disables driver stereo correction:") + insert_str;
		asm_text->insert(dcl_end, insert_str);
		dcl_end += insert_str.size();
	}

	insert_str = std::string("\n\n// Constant buffers redirected by DarkStarSword's UE4 autofix:");
	asm_text->insert(dcl_end, insert_str);

	if (dcl_temps != std::string::npos) {
		dcl_temps += 11;
		dcl_temps_end = asm_text->find("\n", dcl_temps);
		LogInfo("Updating dcl_temps %d\n", tmp_regs);
		asm_text->replace(dcl_temps, dcl_temps_end - dcl_temps, std::to_string(tmp_regs));
		// TODO: Fixup dcl_end
	} else {
		insert_str = std::string("\ndcl_temps ") + std::to_string(tmp_regs);
		LogInfo("Inserting dcl_temps %d\n", tmp_regs);
		asm_text->insert(dcl_end, insert_str);
		dcl_end += insert_str.size();
	}
	// NOTE: dcl_end is no longer valid

	return true;
}


// FIXME: Move to engine specific DLL
static std::unordered_set<ID3D11Resource*> tagged_cbuffers;

// This function will replace shaders when the decision to do so has to be
// delayed for as long as possible, to just before the draw/dispatch call when
// any other pipeline state we might need to check is known. e.g. this can be
// used to replace shaders based on what constant buffers they are used with,
// which will be used by an upcoming UE4 autofix tool.
//
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
	void (__stdcall ID3D11DeviceContext::*GetConstantBuffers)(UINT, UINT, ID3D11Buffer**),
	void (__stdcall ID3D11DeviceContext::*GetShaderVS2013BUGWORKAROUND)(ID3D11Shader**, ID3D11ClassInstance**, UINT*),
	void (__stdcall ID3D11DeviceContext::*SetShaderVS2013BUGWORKAROUND)(ID3D11Shader*, ID3D11ClassInstance*const*, UINT),
	HRESULT (__stdcall ID3D11Device::*CreateShader)(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11Shader**)
>
void HackerContext::DeferredShaderReplacement(ID3D11DeviceChild *shader, UINT64 hash, wchar_t *shader_type)
{
	ID3D11Buffer *cbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	bool patch_cbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	ID3D11Shader *orig_shader = NULL, *patched_shader = NULL;
	ShaderOverride *shader_override = NULL;
	ID3D11ClassInstance *class_instances[256];
	ShaderReloadMap::iterator orig_info_i;
	OriginalShaderInfo *orig_info = NULL;
	UINT num_instances = 0;
	string asm_text;
	wchar_t section_name[64];
	wstring dest, src, src_null;
	bool ok = true;
	bool patch_regex = false, patch_cbs = false;
	HRESULT hr;
	unsigned i;
	wstring tagline(L"//");

	// Faster than catching an out_of_range exception from .at():
	orig_info_i = lookup_reloaded_shader(shader);
	if (orig_info_i == G->mReloadedShaders.end())
		return;
	orig_info = &orig_info_i->second;

	if (!orig_info->deferred_replacement_candidate || orig_info->deferred_replacement_processed)
		return;

	LogInfo("Performing deferred shader analysis on %S %016I64x...\n", shader_type, hash);

	// Remember that we have analysed this one so we don't check it
	// again (until reload) regardless of whether we patch it or not:
	orig_info->deferred_replacement_processed = true;

	// TODO: Compare performance vs doing this in XXSetConstantBuffers
	(mOrigContext1->*GetConstantBuffers)(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, cbuffers);

	memset(patch_cbuffers, 0, sizeof(patch_cbuffers));
	for (i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
		if (tagged_cbuffers.count(cbuffers[i])) {
			patch_cbuffers[i] = true;
			patch_cbs = true;
			LogInfo("Tagged constant buffer %p used with %S %016I64x, remapping cb%d -> t%d\n",
					cbuffers[i], shader_type, hash, i, i + 100);
		}
	}

	asm_text = BinaryToAsmText(orig_info->byteCode->GetBufferPointer(),
			orig_info->byteCode->GetBufferSize());
	if (asm_text.empty())
		return;

	try {
		patch_regex = apply_shader_regex_groups(&asm_text, &orig_info->shaderModel, hash, &tagline);
		// FIXME: Get stereo CBs from driver profile
		if (patch_cbs)
			patch_cbs = patch_asm_redirect_cb(&asm_text, patch_cbuffers, 12, 12);
		if (patch_cbs)
			tagline.append(L"Automatically patched by DarkStarSword's UE4 3D Vision tool");
	} catch (...) {
		LogInfo("    *** Exception while patching shader\n");
		return;
	}

	if (!patch_regex && !patch_cbs) {
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

	hr = (mOrigDevice1->*CreateShader)(patched_bytecode.data(), patched_bytecode.size(),
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
	(mOrigContext1->*GetShaderVS2013BUGWORKAROUND)(&orig_shader, class_instances, &num_instances);
	(mOrigContext1->*SetShaderVS2013BUGWORKAROUND)(patched_shader, class_instances, num_instances);
	if (orig_shader)
		orig_shader->Release();
	for (i = 0; i < num_instances; i++) {
		if (class_instances[i])
			class_instances[i]->Release();
	}

	if (patch_cbs) {
		shader_override = &G->mShaderOverrideMap[hash];
		wsprintf(section_name, L"AutoGeneratedShaderOverride%016I64x", hash);
		LogInfo("[%S]\n", section_name);

		// Generate the resource copy directives to assign the stereoised buffer:
		// FIXME: Has to be all lower case - should handle that in the command list parser
		src = wstring(L"resourcefviewuniformshaderparameters_stereo_srv_uav");
		src_null = wstring(L"null");
		for (i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
			if (!patch_cbuffers[i])
				continue;

			dest = wstring(shader_type) + wstring(L"-t") + std::to_wstring(i + 100);

			LogInfo("  %S = %S\n", dest.c_str(), src.c_str());
			LogInfo("  post %S = %S\n", dest.c_str(), src_null.c_str());
			// FIXME: Namespace this
			wstring ns = L"";
			ok = ParseCommandListResourceCopyDirective(section_name, dest.c_str(), &src, &shader_override->command_list, &ns) && ok;
			ok = ParseCommandListResourceCopyDirective(section_name, dest.c_str(), &src_null, &shader_override->post_command_list, &ns) && ok;
			if (!ok) {
				LogInfo("WARNING: Command List failed to parse auto generated "
						"resource copy directives - missing resource definitions?\n");
			}
		}
	}
}

void HackerContext::DeferredShaderReplacementBeforeDraw()
{
	Profiling::State profiling_state;

	if (shader_regex_groups.empty() && tagged_cbuffers.empty())
		return;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	EnterCriticalSection(&G->mCriticalSection);

		if (mCurrentVertexShaderHandle) {
			DeferredShaderReplacement<ID3D11VertexShader,
				&ID3D11DeviceContext::VSGetConstantBuffers,
				&ID3D11DeviceContext::VSGetShader,
				&ID3D11DeviceContext::VSSetShader,
				&ID3D11Device::CreateVertexShader>
				(mCurrentVertexShaderHandle, mCurrentVertexShader, L"vs");
		}
		if (mCurrentHullShaderHandle) {
			DeferredShaderReplacement<ID3D11HullShader,
				&ID3D11DeviceContext::HSGetConstantBuffers,
				&ID3D11DeviceContext::HSGetShader,
				&ID3D11DeviceContext::HSSetShader,
				&ID3D11Device::CreateHullShader>
				(mCurrentHullShaderHandle, mCurrentHullShader, L"hs");
		}
		if (mCurrentDomainShaderHandle) {
			DeferredShaderReplacement<ID3D11DomainShader,
				&ID3D11DeviceContext::DSGetConstantBuffers,
				&ID3D11DeviceContext::DSGetShader,
				&ID3D11DeviceContext::DSSetShader,
				&ID3D11Device::CreateDomainShader>
				(mCurrentDomainShaderHandle, mCurrentDomainShader, L"ds");
		}
		if (mCurrentGeometryShaderHandle) {
			DeferredShaderReplacement<ID3D11GeometryShader,
				&ID3D11DeviceContext::GSGetConstantBuffers,
				&ID3D11DeviceContext::GSGetShader,
				&ID3D11DeviceContext::GSSetShader,
				&ID3D11Device::CreateGeometryShader>
				(mCurrentGeometryShaderHandle, mCurrentGeometryShader, L"gs");
		}
		if (mCurrentPixelShaderHandle) {
			DeferredShaderReplacement<ID3D11PixelShader,
				&ID3D11DeviceContext::PSGetConstantBuffers,
				&ID3D11DeviceContext::PSGetShader,
				&ID3D11DeviceContext::PSSetShader,
				&ID3D11Device::CreatePixelShader>
				(mCurrentPixelShaderHandle, mCurrentPixelShader, L"ps");
		}

	LeaveCriticalSection(&G->mCriticalSection);

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::shaderregex_overhead);
}

void HackerContext::DeferredShaderReplacementBeforeDispatch()
{
	if (shader_regex_groups.empty() && tagged_cbuffers.empty())
		return;

	if (!mCurrentComputeShaderHandle)
		return;

	EnterCriticalSection(&G->mCriticalSection);

		DeferredShaderReplacement<ID3D11ComputeShader,
			&ID3D11DeviceContext::CSGetConstantBuffers,
			&ID3D11DeviceContext::CSGetShader,
			&ID3D11DeviceContext::CSSetShader,
			&ID3D11Device::CreateComputeShader>
			(mCurrentComputeShaderHandle, mCurrentComputeShader, L"cs");

	LeaveCriticalSection(&G->mCriticalSection);
}


void HackerContext::BeforeDraw(DrawContext &data)
{
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	// If we are not hunting shaders, we should skip all of this shader management for a performance bump.
	if (G->hunting == HUNTING_MODE_ENABLED)
	{
		UINT selectedVertexBufferPos;
		UINT selectedRenderTargetPos;
		UINT i;

		EnterCriticalSection(&G->mCriticalSection);
		{
			// In some cases stat collection can have a significant
			// performance impact or may result in a runaway
			// memory leak, so only do it if dump_usage is enabled:
			if (G->DumpUsage)
				RecordGraphicsShaderStats();

			// Selection
			for (selectedVertexBufferPos = 0; selectedVertexBufferPos < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++selectedVertexBufferPos) {
				if (mCurrentVertexBuffers[selectedVertexBufferPos] == G->mSelectedVertexBuffer)
					break;
			}
			for (selectedRenderTargetPos = 0; selectedRenderTargetPos < mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
				if (mCurrentRenderTargets[selectedRenderTargetPos] == G->mSelectedRenderTarget)
					break;
			}
			if (mCurrentIndexBuffer == G->mSelectedIndexBuffer ||
				mCurrentVertexShader == G->mSelectedVertexShader ||
				mCurrentPixelShader == G->mSelectedPixelShader ||
				mCurrentGeometryShader == G->mSelectedGeometryShader ||
				mCurrentDomainShader == G->mSelectedDomainShader ||
				mCurrentHullShader == G->mSelectedHullShader ||
				selectedVertexBufferPos < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT ||
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
				for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
					if (mCurrentVertexBuffers[i] == G->mSelectedVertexBuffer) {
						G->mSelectedVertexBuffer_VertexShader.insert(mCurrentVertexShader);
						G->mSelectedVertexBuffer_PixelShader.insert(mCurrentPixelShader);
					}
				}
				if (mCurrentIndexBuffer == G->mSelectedIndexBuffer)
				{
					G->mSelectedIndexBuffer_VertexShader.insert(mCurrentVertexShader);
					G->mSelectedIndexBuffer_PixelShader.insert(mCurrentPixelShader);
				}
				if (mCurrentVertexShader == G->mSelectedVertexShader) {
					for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
						if (mCurrentVertexBuffers[i])
							G->mSelectedVertexShader_VertexBuffer.insert(mCurrentVertexBuffers[i]);
					}
					if (mCurrentIndexBuffer)
						G->mSelectedVertexShader_IndexBuffer.insert(mCurrentIndexBuffer);
				}
				if (mCurrentPixelShader == G->mSelectedPixelShader) {
					for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
						if (mCurrentVertexBuffers[i])
							G->mSelectedVertexShader_VertexBuffer.insert(mCurrentVertexBuffers[i]);
					}
					if (mCurrentIndexBuffer)
						G->mSelectedPixelShader_IndexBuffer.insert(mCurrentIndexBuffer);
				}
				if (G->marking_mode == MarkingMode::MONO && mHackerDevice->mStereoHandle)
				{
					LogDebug("  setting separation=0 for hunting\n");

					if (NVAPI_OK != Profiling::NvAPI_Stereo_GetSeparation(mHackerDevice->mStereoHandle, &data.oldSeparation))
						LogDebug("    Stereo_GetSeparation failed.\n");

					NvAPIOverride();
					if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSeparation(mHackerDevice->mStereoHandle, 0))
						LogDebug("    Stereo_SetSeparation failed.\n");
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
						data.oldPixelShader = SwitchPSShader(G->mPinkingShader);
				}
			}
		}
		LeaveCriticalSection(&G->mCriticalSection);
	}

	if (!G->fix_enabled)
		goto out_profile;

	DeferredShaderReplacementBeforeDraw();

	// Override settings?
	if (!G->mShaderOverrideMap.empty()) {
		ShaderOverrideMap::iterator i;

		i = lookup_shaderoverride(mCurrentVertexShader);
		if (i != G->mShaderOverrideMap.end()) {
			data.post_commands[0] = &i->second.post_command_list;
			ProcessShaderOverride(&i->second, false, &data);
		}

		if (mCurrentHullShader) {
			i = lookup_shaderoverride(mCurrentHullShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[1] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data);
			}
		}

		if (mCurrentDomainShader) {
			i = lookup_shaderoverride(mCurrentDomainShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[2] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data);
			}
		}

		if (mCurrentGeometryShader) {
			i = lookup_shaderoverride(mCurrentGeometryShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[3] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data);
			}
		}

		i = lookup_shaderoverride(mCurrentPixelShader);
		if (i != G->mShaderOverrideMap.end()) {
			data.post_commands[4] = &i->second.post_command_list;
			ProcessShaderOverride(&i->second, true, &data);
		}
	}

out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::draw_overhead);
}

void HackerContext::AfterDraw(DrawContext &data)
{
	int i;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (data.call_info.skip)
		Profiling::skipped_draw_calls++;

	for (i = 0; i < 5; i++) {
		if (data.post_commands[i]) {
			RunCommandList(mHackerDevice, this, data.post_commands[i], &data.call_info, true);
		}
	}

	if (mHackerDevice->mStereoHandle && data.oldSeparation != FLT_MAX) {
		NvAPIOverride();
		if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSeparation(mHackerDevice->mStereoHandle, data.oldSeparation))
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

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::draw_overhead);
}

// -----------------------------------------------------------------------------------------------

ULONG STDMETHODCALLTYPE HackerContext::AddRef(void)
{
	return mOrigContext1->AddRef();
}


// Must set the reference that the HackerDevice uses to null, because otherwise
// we see that dead reference reused in GetImmediateContext, in FC4.

STDMETHODIMP_(ULONG) HackerContext::Release(THIS)
{
	ULONG ulRef = mOrigContext1->Release();
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

	if (ppvObject && IsEqualIID(riid, IID_HackerContext)) {
		// This is a special case - only 3DMigoto itself should know
		// this IID, so this is us checking if it has a HackerContext.
		// There's no need to call through to DX for this one.
		AddRef();
		*ppvObject = this;
		return S_OK;
	}

	HRESULT hr = mOrigContext1->QueryInterface(riid, ppvObject);
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
		LogDebug("  return HackerContext(%s@%p) wrapper of %p\n", type_name(this), this, mOrigContext1);
	}
	else if (riid == __uuidof(ID3D11DeviceContext1))
	{
		if (!G->enable_platform_update) 
		{
			LogInfo("***  returns E_NOINTERFACE as error for ID3D11DeviceContext1 (try allow_platform_update=1 if the game refuses to run).\n");
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}

		// For Batman: TellTale games, they call this to fetch the DeviceContext1.
		// We need to return a hooked version as part of fleshing it out for games like this
		// that require the evil-update to run.

		// In this case, we are already an ID3D11DeviceContext1, so just return self.
		*ppvObject = this;
		LogDebug("  return HackerContext(%s@%p) wrapper of %p\n", type_name(this), this, ppvObject);
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
	mOrigContext1->GetDevice(ppDevice);

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

	HRESULT hr = mOrigContext1->GetPrivateData(guid, pDataSize, pData);
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

	HRESULT hr = mOrigContext1->SetPrivateData(guid, DataSize, pData);
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

	HRESULT hr = mOrigContext1->SetPrivateDataInterface(guid, pData);
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
	mOrigContext1->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
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

	i = lookup_textureoverride(hash);
	if (i == G->mTextureOverrideMap.end())
		return false;

	return i->second.begin()->deny_cpu_read;
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
	bool write = false, read = false, deny = false, analyse_cb = false;
	UINT size = 0;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (FAILED(map_hr) || !pResource || !pMappedResource || !pMappedResource->pData)
		goto out_profile;

	pResource->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			buf = (ID3D11Buffer*)pResource;
			buf->GetDesc(&buf_desc);
			size = buf_desc.ByteWidth;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			tex1d = (ID3D11Texture1D*)pResource;
			tex1d->GetDesc(&tex1d_desc);
			size = dxgi_format_size(tex1d_desc.Format) * tex1d_desc.Width;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex2d = (ID3D11Texture2D*)pResource;
			tex2d->GetDesc(&tex2d_desc);
			size = pMappedResource->RowPitch * tex2d_desc.Height;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex3d = (ID3D11Texture3D*)pResource;
			tex3d->GetDesc(&tex3d_desc);
			size = pMappedResource->DepthPitch * tex3d_desc.Depth;
			break;
		default:
			return;
	}

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

			if (dim == D3D11_RESOURCE_DIMENSION_BUFFER && buf_desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)
			{
				if (extension_dll_needs_cb(size))
					analyse_cb = divert = track = true;
			}

			break;

		case D3D11_MAP_READ:
			read = divertable = true;
			divert = deny = MapDenyCPURead(pResource, Subresource, MapType, MapFlags, pMappedResource);
			break;
	}

	if (!track && !divert)
		goto out_profile;

	map_info = &mMappedResources[pResource];
	map_info->mapped_writable = write;
	map_info->size = size;
	map_info->analyse_cb = analyse_cb;
	memcpy(&map_info->map, pMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	if (!divertable || !divert)
		goto out_profile;

	replace = malloc(size);
	if (!replace) {
		LogInfo("TrackAndDivertMap out of memory\n");
		goto out_profile;
	}

	if (read && !deny)
		memcpy(replace, pMappedResource->pData, size);
	else
		memset(replace, 0, size);

	map_info->orig_pData = pMappedResource->pData;
	map_info->map.pData = replace;
	pMappedResource->pData = replace;

out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::map_overhead);
}

void HackerContext::TrackAndDivertUnmap(ID3D11Resource *pResource, UINT Subresource)
{
	MappedResources::iterator i;
	MappedResourceInfo *map_info = NULL;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (mMappedResources.empty())
		goto out_profile;

	i = mMappedResources.find(pResource);
	if (i == mMappedResources.end())
		goto out_profile;
	map_info = &i->second;

	if (G->track_texture_updates == 1 && Subresource == 0 && map_info->mapped_writable)
		UpdateResourceHashFromCPU(pResource, map_info->map.pData, map_info->map.RowPitch, map_info->map.DepthPitch);

	if (map_info->analyse_cb) {
		DirectX::XMFLOAT4 *buf = (DirectX::XMFLOAT4*)map_info->map.pData;
		if (
				buf[122].x != buf[122].y && // Resolution is not square
				buf[122].x == buf[123].x && // Resolution X matches View X
				buf[122].y == buf[123].y && // Resolution Y matches View Y
				buf[58].z == -buf[59].z) {

			if (tagged_cbuffers.count(pResource) == 0) {
				LogInfo("UE4 cb analysis identified cbuffer %p on frame %u. Details: %fx%f, %fx%f, %f %f, res: %dx%d\n",
						pResource, G->frame_no, buf[122].x, buf[122].y, buf[123].x, buf[123].y, buf[58].z, buf[59].z,
						G->mResolutionInfo.width, G->mResolutionInfo.height);

				// FIXME: We should still allow this to be released.
				// Either use the private data, or release it if we
				// notice it's refcount has dropped to one:
				pResource->AddRef();
				tagged_cbuffers.insert(pResource);
			} else {
				LogInfo("UE4 cb analysis confirmed previous cbuffer %p still matches on frame %u. Details: %fx%f, %fx%f, %f %f, res: %dx%d\n",
						pResource, G->frame_no, buf[122].x, buf[122].y, buf[123].x, buf[123].y, buf[58].z, buf[59].z,
						G->mResolutionInfo.width, G->mResolutionInfo.height);
			}
			// Run both pre and post command lists now.
			// The reason for having a post command list is so that people can
			// write 'ps-t100 = ResourceFoo; post ps-t100 = null' and have it work.
			RunResourceCommandList(mHackerDevice, this, &G->xxx_command_list, pResource, false);
			RunResourceCommandList(mHackerDevice, this, &G->post_xxx_command_list, pResource, true);
		} else if (tagged_cbuffers.count(pResource) > 0) {
			LogInfo("UE4 cb analysis no longer matched previous cbuffer %p on frame %u. details: %fx%f, %fx%f, %f %f, res: %dx%d\n",
					pResource, G->frame_no, buf[122].x, buf[122].y, buf[123].x, buf[123].y, buf[58].z, buf[59].z,
						G->mResolutionInfo.width, G->mResolutionInfo.height);

			tagged_cbuffers.erase(pResource);
			pResource->Release();
		} else {
			LogInfo("UE4 cb analysis rejected cbuffer %p on frame %u. details: %fx%f, %fx%f, %f %f, res: %dx%d\n",
					pResource, G->frame_no, buf[122].x, buf[122].y, buf[123].x, buf[123].y, buf[58].z, buf[59].z,
						G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

	if (map_info->orig_pData) {
		// TODO: Measure performance vs. not diverting:
		if (map_info->mapped_writable)
			memcpy(map_info->orig_pData, map_info->map.pData, map_info->size);

		free(map_info->map.pData);
	}

	mMappedResources.erase(i);

out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::map_overhead);
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

	hr = mOrigContext1->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);

	TrackAndDivertMap(hr, pResource, Subresource, MapType, MapFlags, pMappedResource);

	return hr;
}

STDMETHODIMP_(void) HackerContext::Unmap(THIS_
	/* [annotation] */
	__in ID3D11Resource *pResource,
	/* [annotation] */
	__in  UINT Subresource)
{
	TrackAndDivertUnmap(pResource, Subresource);
	mOrigContext1->Unmap(pResource, Subresource);
}

STDMETHODIMP_(void) HackerContext::PSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers)
{
	 mOrigContext1->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::IASetInputLayout(THIS_
	/* [annotation] */
	__in_opt ID3D11InputLayout *pInputLayout)
{
	 mOrigContext1->IASetInputLayout(pInputLayout);
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
	 mOrigContext1->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

	 if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);
		for (UINT i = StartSlot; (i < StartSlot + NumBuffers) && (i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT); i++) {
			if (ppVertexBuffers && ppVertexBuffers[i]) {
				mCurrentVertexBuffers[i] = GetResourceHash(ppVertexBuffers[i]);
				G->mVisitedVertexBuffers.insert(mCurrentVertexBuffers[i]);
			} else
				mCurrentVertexBuffers[i] = 0;
		}
		LeaveCriticalSection(&G->mCriticalSection);
	 }
}

STDMETHODIMP_(void) HackerContext::GSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers)
{
	 mOrigContext1->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
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
}

STDMETHODIMP_(void) HackerContext::IASetPrimitiveTopology(THIS_
	/* [annotation] */
	__in D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	 mOrigContext1->IASetPrimitiveTopology(Topology);
}

STDMETHODIMP_(void) HackerContext::VSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers)
{
	 mOrigContext1->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::PSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers)
{
	mOrigContext1->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::Begin(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	mOrigContext1->Begin(pAsync);
}

STDMETHODIMP_(void) HackerContext::End(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	 mOrigContext1->End(pAsync);
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
	return mOrigContext1->GetData(pAsync, pData, DataSize, GetDataFlags);
}

STDMETHODIMP_(void) HackerContext::SetPredication(THIS_
	/* [annotation] */
	__in_opt ID3D11Predicate *pPredicate,
	/* [annotation] */
	__in  BOOL PredicateValue)
{
	return mOrigContext1->SetPredication(pPredicate, PredicateValue);
}

STDMETHODIMP_(void) HackerContext::GSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
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
	 mOrigContext1->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::OMSetBlendState(THIS_
	/* [annotation] */
	__in_opt  ID3D11BlendState *pBlendState,
	/* [annotation] */
	__in_opt  const FLOAT BlendFactor[4],
	/* [annotation] */
	__in  UINT SampleMask)
{
	 mOrigContext1->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

STDMETHODIMP_(void) HackerContext::OMSetDepthStencilState(THIS_
	/* [annotation] */
	__in_opt  ID3D11DepthStencilState *pDepthStencilState,
	/* [annotation] */
	__in  UINT StencilRef)
{
	 mOrigContext1->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}

STDMETHODIMP_(void) HackerContext::SOSetTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
	/* [annotation] */
	__in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	 mOrigContext1->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}

bool HackerContext::BeforeDispatch(DispatchContext *context)
{
	if (G->hunting == HUNTING_MODE_ENABLED) {
		if (G->DumpUsage)
			RecordComputeShaderStats();

		if (mCurrentComputeShader == G->mSelectedComputeShader) {
			if (G->marking_mode == MarkingMode::SKIP)
				return false;
		}
	}

	if (!G->fix_enabled)
		return true;

	DeferredShaderReplacementBeforeDispatch();

	// Override settings?
	if (!G->mShaderOverrideMap.empty()) {
		ShaderOverrideMap::iterator i;

		i = lookup_shaderoverride(mCurrentComputeShader);
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
		mOrigContext1->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	else
		Profiling::skipped_draw_calls++;

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
		mOrigContext1->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	else
		Profiling::skipped_draw_calls++;

	AfterDispatch(&context);
}

STDMETHODIMP_(void) HackerContext::RSSetState(THIS_
	/* [annotation] */
	__in_opt  ID3D11RasterizerState *pRasterizerState)
{
	 mOrigContext1->RSSetState(pRasterizerState);
}

STDMETHODIMP_(void) HackerContext::RSSetViewports(THIS_
	/* [annotation] */
	__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
	/* [annotation] */
	__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports)
{
	 mOrigContext1->RSSetViewports(NumViewports, pViewports);
}

STDMETHODIMP_(void) HackerContext::RSSetScissorRects(THIS_
	/* [annotation] */
	__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
	/* [annotation] */
	__in_ecount_opt(NumRects)  const D3D11_RECT *pRects)
{
	 mOrigContext1->RSSetScissorRects(NumRects, pRects);
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

	i = lookup_textureoverride(dstHash);
	if (i == G->mTextureOverrideMap.end())
		return false;

	if (!i->second.begin()->expand_region_copy)
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

	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pDstResource, DstSubresource, pSrcResource, SrcSubresource, 'S', DstX, DstY, DstZ, pSrcBox);
	}

	if (ExpandRegionCopy(pDstResource, DstX, DstY, pSrcResource, pSrcBox, &replaceDstX, &replaceSrcBox))
		pSrcBox = &replaceSrcBox;

	 mOrigContext1->CopySubresourceRegion(pDstResource, DstSubresource, replaceDstX, DstY, DstZ,
		pSrcResource, SrcSubresource, pSrcBox);

	// We only update the destination resource hash when the entire
	// subresource 0 is updated and pSrcBox is NULL. We could check if the
	// pSrcBox fills the entire resource, but if the game is using pSrcBox
	// it stands to reason that it won't always fill the entire resource
	// and the hashes might be less predictable. Possibly something to
	// enable as an option in the future if there is a proven need.
	if (G->track_texture_updates == 1 && DstSubresource == 0 && DstX == 0 && DstY == 0 && DstZ == 0 && pSrcBox == NULL)
		PropagateResourceHash(pDstResource, pSrcResource);
}

STDMETHODIMP_(void) HackerContext::CopyResource(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pDstResource,
	/* [annotation] */
	__in  ID3D11Resource *pSrcResource)
{
	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pDstResource, 0, pSrcResource, 0, 'C', 0, 0, 0, NULL);
	}

	 mOrigContext1->CopyResource(pDstResource, pSrcResource);

	if (G->track_texture_updates == 1)
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
	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pDstResource, DstSubresource, NULL, 0, 'U', 0, 0, 0, NULL);
	}

	 mOrigContext1->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
		SrcDepthPitch);

	// We only update the destination resource hash when the entire
	// subresource 0 is updated and pDstBox is NULL. We could check if the
	// pDstBox fills the entire resource, but if the game is using pDstBox
	// it stands to reason that it won't always fill the entire resource
	// and the hashes might be less predictable. Possibly something to
	// enable as an option in the future if there is a proven need.
	if (G->track_texture_updates == 1 && DstSubresource == 0 && pDstBox == NULL)
		UpdateResourceHashFromCPU(pDstResource, pSrcData, SrcRowPitch, SrcDepthPitch);
}

STDMETHODIMP_(void) HackerContext::CopyStructureCount(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pDstBuffer,
	/* [annotation] */
	__in  UINT DstAlignedByteOffset,
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pSrcView)
{
	 mOrigContext1->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}

STDMETHODIMP_(void) HackerContext::ClearUnorderedAccessViewUint(THIS_
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const UINT Values[4])
{
	RunViewCommandList(mHackerDevice, this, &G->clear_uav_uint_command_list, pUnorderedAccessView, false);
	mOrigContext1->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
	RunViewCommandList(mHackerDevice, this, &G->post_clear_uav_uint_command_list, pUnorderedAccessView, true);
}

STDMETHODIMP_(void) HackerContext::ClearUnorderedAccessViewFloat(THIS_
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const FLOAT Values[4])
{
	RunViewCommandList(mHackerDevice, this, &G->clear_uav_float_command_list, pUnorderedAccessView, false);
	mOrigContext1->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
	RunViewCommandList(mHackerDevice, this, &G->post_clear_uav_float_command_list, pUnorderedAccessView, true);
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
	RunViewCommandList(mHackerDevice, this, &G->clear_dsv_command_list, pDepthStencilView, false);
	mOrigContext1->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
	RunViewCommandList(mHackerDevice, this, &G->post_clear_dsv_command_list, pDepthStencilView, true);
}

STDMETHODIMP_(void) HackerContext::GenerateMips(THIS_
	/* [annotation] */
	__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	 mOrigContext1->GenerateMips(pShaderResourceView);
}

STDMETHODIMP_(void) HackerContext::SetResourceMinLOD(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	FLOAT MinLOD)
{
	 mOrigContext1->SetResourceMinLOD(pResource, MinLOD);
}

STDMETHODIMP_(FLOAT) HackerContext::GetResourceMinLOD(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource)
{
	FLOAT ret = mOrigContext1->GetResourceMinLOD(pResource);

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
	 mOrigContext1->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource,
		Format);
}

STDMETHODIMP_(void) HackerContext::ExecuteCommandList(THIS_
	/* [annotation] */
	__in  ID3D11CommandList *pCommandList,
	BOOL RestoreContextState)
{
	if (G->deferred_contexts_enabled)
		mOrigContext1->ExecuteCommandList(pCommandList, RestoreContextState);

	if (!RestoreContextState) {
		// This is equivalent to calling ClearState() afterwards, so we
		// need to rebind the 3DMigoto resources now. See also
		// FinishCommandList's RestoreDeferredContextState:
		Bind3DMigotoResources();
	}
}

STDMETHODIMP_(void) HackerContext::HSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
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
}

STDMETHODIMP_(void) HackerContext::HSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	 mOrigContext1->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::HSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	 mOrigContext1->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::DSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
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
}

STDMETHODIMP_(void) HackerContext::DSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	 mOrigContext1->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::DSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	 mOrigContext1->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::CSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
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
	if (ppUnorderedAccessViews) {
		// TODO: Record stats on unordered access view usage
		for (UINT i = 0; i < NumUAVs; ++i) {
			if (!ppUnorderedAccessViews[i])
				continue;
			// TODO: Record stats
		}
	}

	mOrigContext1->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
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
		// grumble grumble this optimisation caught me out *TWICE* grumble grumble -DSS
		if (!G->mShaderOverrideMap.empty()
				|| !shader_regex_groups.empty()
				|| !tagged_cbuffers.empty() // FIXME: Depending on the order of Map/Unmap/SetShader/Draw this might be too late. Always force on when extension DLL is in use.
				|| (G->hunting == HUNTING_MODE_ENABLED)) {
			ShaderMap::iterator i = lookup_shader_hash(pShader);
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
		ShaderReloadMap::iterator it = lookup_reloaded_shader(pShader);
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
			if (G->marking_mode == MarkingMode::ORIGINAL || !G->fix_enabled) {
				ShaderReplacementMap::iterator j = lookup_original_shader(pShader);
				if ((selectedShader == *currentShaderHash || !G->fix_enabled) && j != G->mOriginalShaders.end()) {
					repl_shader = (ID3D11Shader*)j->second;
				}
			}
			if (G->marking_mode == MarkingMode::ZERO) {
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
	(mOrigContext1->*OrigSetShader)(repl_shader, ppClassInstances, NumClassInstances);
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
}

STDMETHODIMP_(void) HackerContext::CSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
	 mOrigContext1->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::CSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers)
{
	 mOrigContext1->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::VSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext1->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::PSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext1->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::PSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11PixelShader **ppPixelShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext1->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
}

STDMETHODIMP_(void) HackerContext::PSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext1->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::VSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11VertexShader **ppVertexShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext1->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);

	// Todo: At GetShader, we need to return the original shader if it's been reloaded.
}

STDMETHODIMP_(void) HackerContext::PSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext1->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::IAGetInputLayout(THIS_
	/* [annotation] */
	__out  ID3D11InputLayout **ppInputLayout)
{
	 mOrigContext1->IAGetInputLayout(ppInputLayout);
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
	 mOrigContext1->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

STDMETHODIMP_(void) HackerContext::IAGetIndexBuffer(THIS_
	/* [annotation] */
	__out_opt  ID3D11Buffer **pIndexBuffer,
	/* [annotation] */
	__out_opt  DXGI_FORMAT *Format,
	/* [annotation] */
	__out_opt  UINT *Offset)
{
	 mOrigContext1->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}

STDMETHODIMP_(void) HackerContext::GSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext1->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::GSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11GeometryShader **ppGeometryShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext1->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
}

STDMETHODIMP_(void) HackerContext::IAGetPrimitiveTopology(THIS_
	/* [annotation] */
	__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	 mOrigContext1->IAGetPrimitiveTopology(pTopology);
}

STDMETHODIMP_(void) HackerContext::VSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext1->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::VSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext1->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::GetPredication(THIS_
	/* [annotation] */
	__out_opt  ID3D11Predicate **ppPredicate,
	/* [annotation] */
	__out_opt  BOOL *pPredicateValue)
{
	 mOrigContext1->GetPredication(ppPredicate, pPredicateValue);
}

STDMETHODIMP_(void) HackerContext::GSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext1->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::GSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext1->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::OMGetRenderTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
	/* [annotation] */
	__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
	/* [annotation] */
	__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	 mOrigContext1->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
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
	 mOrigContext1->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}

STDMETHODIMP_(void) HackerContext::OMGetBlendState(THIS_
	/* [annotation] */
	__out_opt  ID3D11BlendState **ppBlendState,
	/* [annotation] */
	__out_opt  FLOAT BlendFactor[4],
	/* [annotation] */
	__out_opt  UINT *pSampleMask)
{
	 mOrigContext1->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}

STDMETHODIMP_(void) HackerContext::OMGetDepthStencilState(THIS_
	/* [annotation] */
	__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
	/* [annotation] */
	__out_opt  UINT *pStencilRef)
{
	 mOrigContext1->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}

STDMETHODIMP_(void) HackerContext::SOGetTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets)
{
	 mOrigContext1->SOGetTargets(NumBuffers, ppSOTargets);
}

STDMETHODIMP_(void) HackerContext::RSGetState(THIS_
	/* [annotation] */
	__out  ID3D11RasterizerState **ppRasterizerState)
{
	 mOrigContext1->RSGetState(ppRasterizerState);
}

STDMETHODIMP_(void) HackerContext::RSGetViewports(THIS_
	/* [annotation] */
	__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
	/* [annotation] */
	__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports)
{
	 mOrigContext1->RSGetViewports(pNumViewports, pViewports);
}

STDMETHODIMP_(void) HackerContext::RSGetScissorRects(THIS_
	/* [annotation] */
	__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
	/* [annotation] */
	__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects)
{
	 mOrigContext1->RSGetScissorRects(pNumRects, pRects);
}

STDMETHODIMP_(void) HackerContext::HSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext1->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::HSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11HullShader **ppHullShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext1->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
}

STDMETHODIMP_(void) HackerContext::HSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext1->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::HSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext1->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::DSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext1->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::DSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11DomainShader **ppDomainShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext1->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
}

STDMETHODIMP_(void) HackerContext::DSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext1->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::DSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext1->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::CSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	 mOrigContext1->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::CSGetUnorderedAccessViews(THIS_
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
	/* [annotation] */
	__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	 mOrigContext1->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
}

STDMETHODIMP_(void) HackerContext::CSGetShader(THIS_
	/* [annotation] */
	__out  ID3D11ComputeShader **ppComputeShader,
	/* [annotation] */
	__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
	/* [annotation] */
	__inout_opt  UINT *pNumClassInstances)
{
	 mOrigContext1->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
}

STDMETHODIMP_(void) HackerContext::CSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	 mOrigContext1->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::CSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	 mOrigContext1->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::ClearState(THIS)
{
	 mOrigContext1->ClearState();

	 // ClearState() will unbind StereoParams and IniParams, so we need to
	 // rebind them now:
	 Bind3DMigotoResources();
}

STDMETHODIMP_(void) HackerContext::Flush(THIS)
{
	 mOrigContext1->Flush();
}

STDMETHODIMP_(D3D11_DEVICE_CONTEXT_TYPE) HackerContext::GetType(THIS)
{
	return mOrigContext1->GetType();
}

STDMETHODIMP_(UINT) HackerContext::GetContextFlags(THIS)
{
	return mOrigContext1->GetContextFlags();
}

STDMETHODIMP HackerContext::FinishCommandList(THIS_
	BOOL RestoreDeferredContextState,
	/* [annotation] */
	__out_opt  ID3D11CommandList **ppCommandList)
{
	BOOL ret = mOrigContext1->FinishCommandList(RestoreDeferredContextState, ppCommandList);

	if (!RestoreDeferredContextState) {
		// This is equivalent to calling ClearState() afterwards, so we
		// need to rebind the 3DMigoto resources now. See also
		// ExecuteCommandList's RestoreContextState:
		Bind3DMigotoResources();
	}

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

		(mOrigContext1->*OrigSetShaderResources)(G->StereoParamsReg, 1, &mHackerDevice->mStereoResourceView);
	}

	// Set constants from ini file if they exist
	if (mHackerDevice->mIniResourceView && G->IniParamsReg >= 0) {
		LogDebug("  adding ini constants as texture to shader resources in slot %i.\n", G->IniParamsReg);

		(mOrigContext1->*OrigSetShaderResources)(G->IniParamsReg, 1, &mHackerDevice->mIniResourceView);
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

void HackerContext::InitIniParams()
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr;

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
	if (mOrigContext1->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
		LogInfo("BUG: InitIniParams called on a deferred context\n");
		DoubleBeepExit();
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
	if (mHackerDevice->mIniTexture) {
		hr = mOrigContext1->Map(mHackerDevice->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr)) {
			memcpy(mappedResource.pData, G->iniParams.data(), sizeof(DirectX::XMFLOAT4) * G->iniParams.size());
			mOrigContext1->Unmap(mHackerDevice->mIniTexture, 0);
			Profiling::iniparams_updates++;
		} else {
			LogInfo("InitIniParams: Map failed\n");
		}
	}

	// The command list will take care of initialising any non-zero values:
	RunCommandList(mHackerDevice, this, &G->constants_command_list, NULL, false);
	// We don't consider persistent globals set in the [Constants] pre
	// command list as making the user config file dirty, because this
	// command list includes the user config file's [Constants] itself:
	G->user_config_dirty = false;
	RunCommandList(mHackerDevice, this, &G->post_constants_command_list, NULL, true);
}

// This function makes sure that the StereoParams and IniParams resources
// remain pinned whenever the game assigns shader resources:
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
		(mOrigContext1->*OrigSetShaderResources)(StartSlot, NumViews, override_srvs);
		delete [] override_srvs;
	} else {
		(mOrigContext1->*OrigSetShaderResources)(StartSlot, NumViews, ppShaderResourceViews);
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
}

STDMETHODIMP_(void) HackerContext::PSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
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

	if (pPixelShader) {
		// Set custom depth texture.
		if (mHackerDevice->mZBufferResourceView)
		{
			LogDebug("  adding Z buffer to shader resources in slot 126.\n");

			mOrigContext1->PSSetShaderResources(126, 1, &mHackerDevice->mZBufferResourceView);
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
	DrawContext c = DrawContext(DrawCall::DrawIndexed, 0, IndexCount, 0, BaseVertexLocation, StartIndexLocation, 0, NULL, 0);
	BeforeDraw(c);

	if (!c.call_info.skip)
		 mOrigContext1->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::Draw(THIS_
	/* [annotation] */
	__in  UINT VertexCount,
	/* [annotation] */
	__in  UINT StartVertexLocation)
{
	DrawContext c = DrawContext(DrawCall::Draw, VertexCount, 0, 0, StartVertexLocation, 0, 0, NULL, 0);
	BeforeDraw(c);

	if (!c.call_info.skip)
		 mOrigContext1->Draw(VertexCount, StartVertexLocation);
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
	mOrigContext1->IASetIndexBuffer(pIndexBuffer, Format, Offset);

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
	DrawContext c = DrawContext(DrawCall::DrawIndexedInstanced, 0, IndexCountPerInstance, InstanceCount, BaseVertexLocation, StartIndexLocation, StartInstanceLocation, NULL, 0);
	BeforeDraw(c);

	if (!c.call_info.skip)
		mOrigContext1->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
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
	DrawContext c = DrawContext(DrawCall::DrawInstanced, VertexCountPerInstance, 0, InstanceCount, StartVertexLocation, 0, StartInstanceLocation, NULL, 0);
	BeforeDraw(c);

	if (!c.call_info.skip)
		mOrigContext1->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
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
	Profiling::State profiling_state;

	if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);
			mCurrentRenderTargets.clear();
			mCurrentDepthTarget = NULL;
			mCurrentPSNumUAVs = 0;
		LeaveCriticalSection(&G->mCriticalSection);

		if (G->DumpUsage) {
			if (Profiling::mode == Profiling::Mode::SUMMARY)
				Profiling::start(&profiling_state);

			if (ppRenderTargetViews) {
				for (UINT i = 0; i < NumViews; ++i) {
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

	mOrigContext1->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
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
	Profiling::State profiling_state;

	if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);

		if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
			mCurrentRenderTargets.clear();
			mCurrentDepthTarget = NULL;
			if (G->DumpUsage) {
				if (Profiling::mode == Profiling::Mode::SUMMARY)
					Profiling::start(&profiling_state);

				if (ppRenderTargetViews) {
					for (UINT i = 0; i < NumRTVs; ++i) {
						if (ppRenderTargetViews[i])
							RecordRenderTargetInfo(ppRenderTargetViews[i], i);
					}
				}
				RecordDepthStencil(pDepthStencilView);

				if (Profiling::mode == Profiling::Mode::SUMMARY)
					Profiling::end(&profiling_state, &Profiling::stat_overhead);
			}
		}

		if (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS) {
			mCurrentPSUAVStartSlot = UAVStartSlot;
			mCurrentPSNumUAVs = NumUAVs;
			// TODO: Record UAV stats
		}

		LeaveCriticalSection(&G->mCriticalSection);
	}

	mOrigContext1->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

STDMETHODIMP_(void) HackerContext::DrawAuto(THIS)
{
	DrawContext c = DrawContext(DrawCall::DrawAuto, 0, 0, 0, 0, 0, 0, NULL, 0);
	BeforeDraw(c);

	if (!c.call_info.skip)
		mOrigContext1->DrawAuto();
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawIndexedInstancedIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	DrawContext c = DrawContext(DrawCall::DrawIndexedInstancedIndirect, 0, 0, 0, 0, 0, 0, pBufferForArgs, AlignedByteOffsetForArgs);
	BeforeDraw(c);

	if (!c.call_info.skip)
		mOrigContext1->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawInstancedIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	DrawContext c = DrawContext(DrawCall::DrawInstancedIndirect, 0, 0, 0, 0, 0, 0, pBufferForArgs, AlignedByteOffsetForArgs);
	BeforeDraw(c);

	if (!c.call_info.skip)
		mOrigContext1->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::ClearRenderTargetView(THIS_
	/* [annotation] */
	__in  ID3D11RenderTargetView *pRenderTargetView,
	/* [annotation] */
	__in  const FLOAT ColorRGBA[4])
{
	RunViewCommandList(mHackerDevice, this, &G->clear_rtv_command_list, pRenderTargetView, false);
	mOrigContext1->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
	RunViewCommandList(mHackerDevice, this, &G->post_clear_rtv_command_list, pRenderTargetView, true);
}


// -----------------------------------------------------------------------------
// Sort of HackerContext1
//	Requires Win7 Platform Update

// Hierarchy:
//  HackerContext <- ID3D11DeviceContext1 <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown


void STDMETHODCALLTYPE HackerContext::CopySubresourceRegion1(
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
	mOrigContext1->CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
}

void STDMETHODCALLTYPE HackerContext::UpdateSubresource1(
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
	mOrigContext1->UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);

	// TODO: Track resource hash updates
}


void STDMETHODCALLTYPE HackerContext::DiscardResource(
	/* [annotation] */
	_In_  ID3D11Resource *pResource)
{
	mOrigContext1->DiscardResource(pResource);
}

void STDMETHODCALLTYPE HackerContext::DiscardView(
	/* [annotation] */
	_In_  ID3D11View *pResourceView)
{
	mOrigContext1->DiscardView(pResourceView);
}


void STDMETHODCALLTYPE HackerContext::VSSetConstantBuffers1(
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
	mOrigContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}


void STDMETHODCALLTYPE HackerContext::HSSetConstantBuffers1(
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
	mOrigContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}


void STDMETHODCALLTYPE HackerContext::DSSetConstantBuffers1(
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
	mOrigContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext::GSSetConstantBuffers1(
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
	mOrigContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext::PSSetConstantBuffers1(
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
	mOrigContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext::CSSetConstantBuffers1(
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
	mOrigContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}



void STDMETHODCALLTYPE HackerContext::VSGetConstantBuffers1(
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
}



void STDMETHODCALLTYPE HackerContext::HSGetConstantBuffers1(
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
}



void STDMETHODCALLTYPE HackerContext::DSGetConstantBuffers1(
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
}


void STDMETHODCALLTYPE HackerContext::GSGetConstantBuffers1(
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
}


void STDMETHODCALLTYPE HackerContext::PSGetConstantBuffers1(
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
}


void STDMETHODCALLTYPE HackerContext::CSGetConstantBuffers1(
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
}


void STDMETHODCALLTYPE HackerContext::SwapDeviceContextState(
	/* [annotation] */
	_In_  ID3DDeviceContextState *pState,
	/* [annotation] */
	_Out_opt_  ID3DDeviceContextState **ppPreviousState)
{
	mOrigContext1->SwapDeviceContextState(pState, ppPreviousState);

	// If a game or overlay creates separate context state objects we won't
	// have had a chance to bind the 3DMigoto resources when it was
	// first created, so do so now:
	Bind3DMigotoResources();
}


void STDMETHODCALLTYPE HackerContext::ClearView(
	/* [annotation] */
	_In_  ID3D11View *pView,
	/* [annotation] */
	_In_  const FLOAT Color[4],
	/* [annotation] */
	_In_reads_opt_(NumRects)  const D3D11_RECT *pRect,
	UINT NumRects)
{
	// TODO: Add a command list here, but we probably actualy want to call
	// the existing RTV / DSV / UAV clear command lists instead for
	// compatibility with engines that might use this if the feature level
	// is high enough, and the others if it is not.
	mOrigContext1->ClearView(pView, Color, pRect, NumRects);
}


void STDMETHODCALLTYPE HackerContext::DiscardView1(
	/* [annotation] */
	_In_  ID3D11View *pResourceView,
	/* [annotation] */
	_In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
	UINT NumRects)
{
	mOrigContext1->DiscardView1(pResourceView, pRects, NumRects);
}
