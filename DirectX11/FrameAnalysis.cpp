// Include before util.h (or any header that includes util.h) to get pretty
// version of LockResourceCreationMode:
#include "lock.h"

#include "D3D11Wrapper.h"
#include "FrameAnalysis.h"
#include "Globals.h"
#include "input.h"

#include <ScreenGrab.h>
#include <wincodec.h>
#include <Strsafe.h>
#include <stdarg.h>
#include <Shlwapi.h>
#include <stdexcept>

// For windows shortcuts:
#include <shobjidl.h>
#include <shlguid.h>

// Flag introduced in Windows 10 Fall Creators Update
// Someone was clearly on crack when they decided this flag was necessary
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

static unordered_map<ID3D11CommandList*, FrameAnalysisDeferredBuffersPtr> frame_analysis_deferred_buffer_lists;
static unordered_map<ID3D11CommandList*, FrameAnalysisDeferredTex2DPtr> frame_analysis_deferred_tex2d_lists;

FrameAnalysisContext::FrameAnalysisContext(ID3D11Device1 *pDevice, ID3D11DeviceContext1 *pContext) :
	HackerContext(pDevice, pContext)
{
	analyse_options = FrameAnalysisOptions::INVALID;
	oneshot_analyse_options = FrameAnalysisOptions::INVALID;
	oneshot_valid = false;
	frame_analysis_log = NULL;
	draw_call = 0;
	non_draw_call_dump_counter = 0;
}

FrameAnalysisContext::~FrameAnalysisContext()
{
	if (frame_analysis_log)
		fclose(frame_analysis_log);
}

void FrameAnalysisContext::vFrameAnalysisLog(char *fmt, va_list ap)
{
	wchar_t filename[MAX_PATH];

	LogDebugNoNL("FrameAnalysisContext(%s@%p)::", type_name(this), this);
	vLogDebug(fmt, ap);

	if (!G->analyse_frame) {
		if (frame_analysis_log)
			fclose(frame_analysis_log);
		frame_analysis_log = NULL;
		return;
	}

	// DSS note: the below comment was originally referring to the
	// C->cur_analyse_options & FrameAnalysisOptions::LOG test we used to
	// have here, but even though we removed that test this is still a good
	// reminder for other settings as well.
	//
	// Using the global analyse options here as the local copy in the
	// context is only updated after draw calls. We could potentially
	// process the triggers here, but this function is intended to log
	// other calls as well where that wouldn't make sense. We could change
	// it so that this is called from FrameAnalysisAfterDraw, but we want
	// to log calls for deferred contexts here as well.

	if (!frame_analysis_log) {
		// Use the original context to check the type, otherwise we
		// will recursively call ourselves:
		if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
			swprintf_s(filename, MAX_PATH, L"%ls\\log.txt", G->ANALYSIS_PATH);
		else
			swprintf_s(filename, MAX_PATH, L"%ls\\log-0x%p.txt", G->ANALYSIS_PATH, this);

		frame_analysis_log = _wfsopen(filename, L"w", _SH_DENYNO);
		if (!frame_analysis_log) {
			LogInfoW(L"Error opening %s\n", filename);
			return;
		}
		draw_call = 1;

		fprintf(frame_analysis_log, "analyse_options: %08x\n", G->cur_analyse_options);
	}

	// We don't allow hold to be changed mid-frame due to potential
	// for filename conflicts, so use def_analyse_options:
	if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
		fprintf(frame_analysis_log, "%u.", G->analyse_frame_no);
	fprintf(frame_analysis_log, "%06u ", draw_call);

	vfprintf(frame_analysis_log, fmt, ap);
}

void FrameAnalysisContext::FrameAnalysisLog(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vFrameAnalysisLog(fmt, ap);
	va_end(ap);
}

#define FALogInfo(fmt, ...) { \
	FrameAnalysisLog("3DMigoto " fmt, __VA_ARGS__); \
} while (0)

#define FALogErr(fmt, ...) { \
	LogInfo("Frame Analysis: " fmt, __VA_ARGS__); \
	FrameAnalysisLog("3DMigoto " fmt, __VA_ARGS__); \
} while (0)


static void FrameAnalysisLogSlot(FILE *frame_analysis_log, int slot, char *slot_name)
{
	if (slot_name)
		fprintf(frame_analysis_log, "       %s:", slot_name);
	else if (slot != -1)
		fprintf(frame_analysis_log, "       %u:", slot);
}

template <class ID3D11Shader>
void FrameAnalysisContext::FrameAnalysisLogShaderHash(ID3D11Shader *shader)
{
	ShaderMap::iterator hash;

	// Always complete the line in the debug log:
	LogDebug("\n");

	if (!G->analyse_frame || !frame_analysis_log)
		return;

	if (!shader) {
		fprintf(frame_analysis_log, "\n");
		return;
	}

	EnterCriticalSectionPretty(&G->mCriticalSection);

	hash = lookup_shader_hash(shader);
	if (hash != end(G->mShaders))
		fprintf(frame_analysis_log, " hash=%016llx", hash->second);

	LeaveCriticalSection(&G->mCriticalSection);

	fprintf(frame_analysis_log, "\n");
}

void FrameAnalysisContext::FrameAnalysisLogResourceHash(ID3D11Resource *resource)
{
	uint32_t hash, orig_hash;
	struct ResourceHashInfo *info;

	// Always complete the line in the debug log:
	LogDebug("\n");

	if (!G->analyse_frame || !frame_analysis_log)
		return;

	if (!resource) {
		fprintf(frame_analysis_log, "\n");
		return;
	}

	EnterCriticalSectionPretty(&G->mCriticalSection);
	EnterCriticalSectionPretty(&G->mResourcesLock);

	try {
		hash = G->mResources.at(resource).hash;
		orig_hash = G->mResources.at(resource).orig_hash;
		if (hash)
			fprintf(frame_analysis_log, " hash=%08x", hash);
		if (orig_hash != hash)
			fprintf(frame_analysis_log, " orig_hash=%08x", orig_hash);

		info = &G->mResourceInfo.at(orig_hash);
		if (info->hash_contaminated) {
			fprintf(frame_analysis_log, " hash_contamination=");
			if (!info->map_contamination.empty())
				fprintf(frame_analysis_log, "Map,");
			if (!info->update_contamination.empty())
				fprintf(frame_analysis_log, "UpdateSubresource,");
			if (!info->copy_contamination.empty())
				fprintf(frame_analysis_log, "CopyResource,");
			if (!info->region_contamination.empty())
				fprintf(frame_analysis_log, "UpdateSubresourceRegion,");
		}
	} catch (std::out_of_range) {
	}

	LeaveCriticalSection(&G->mResourcesLock);
	LeaveCriticalSection(&G->mCriticalSection);

	fprintf(frame_analysis_log, "\n");
}

void FrameAnalysisContext::FrameAnalysisLogResource(int slot, char *slot_name, ID3D11Resource *resource)
{
	if (!resource || !G->analyse_frame || !frame_analysis_log)
		return;

	FrameAnalysisLogSlot(frame_analysis_log, slot, slot_name);
	fprintf(frame_analysis_log, " resource=0x%p", resource);

	FrameAnalysisLogResourceHash(resource);
}

void FrameAnalysisContext::FrameAnalysisLogView(int slot, char *slot_name, ID3D11View *view)
{
	ID3D11Resource *resource;

	if (!view || !G->analyse_frame || !frame_analysis_log)
		return;

	FrameAnalysisLogSlot(frame_analysis_log, slot, slot_name);
	fprintf(frame_analysis_log, " view=0x%p", view);

	view->GetResource(&resource);
	if (!resource)
		return;

	FrameAnalysisLogResource(-1, NULL, resource);

	resource->Release();
}

void FrameAnalysisContext::FrameAnalysisLogResourceArray(UINT start, UINT len, ID3D11Resource *const *ppResources)
{
	UINT i;

	if (!ppResources || !G->analyse_frame || !frame_analysis_log)
		return;

	for (i = 0; i < len; i++)
		FrameAnalysisLogResource(start + i, NULL, ppResources[i]);
}

void FrameAnalysisContext::FrameAnalysisLogViewArray(UINT start, UINT len, ID3D11View *const *ppViews)
{
	UINT i;

	if (!ppViews || !G->analyse_frame || !frame_analysis_log)
		return;

	for (i = 0; i < len; i++)
		FrameAnalysisLogView(start + i, NULL, ppViews[i]);
}

void FrameAnalysisContext::FrameAnalysisLogMiscArray(UINT start, UINT len, void *const *array)
{
	UINT i;
	void *item;

	if (!array || !G->analyse_frame || !frame_analysis_log)
		return;

	for (i = 0; i < len; i++) {
		item = array[i];
		if (item) {
			FrameAnalysisLogSlot(frame_analysis_log, start + i, NULL);
			fprintf(frame_analysis_log, " handle=0x%p\n", item);
		}
	}
}

static void FrameAnalysisLogQuery(ID3D11Query *query)
{
	D3D11_QUERY_DESC desc;

	query->GetDesc(&desc);
}

void FrameAnalysisContext::FrameAnalysisLogAsyncQuery(ID3D11Asynchronous *async)
{
	AsyncQueryType type;
	ID3D11Query *query;
	ID3D11Predicate *predicate;
	D3D11_QUERY_DESC desc;

	// Always complete the line in the debug log:
	LogDebug("\n");

	if (!G->analyse_frame || !frame_analysis_log)
		return;

	if (!async) {
		fprintf(frame_analysis_log, "\n");
		return;
	}

	try {
		type = G->mQueryTypes.at(async);
	} catch (std::out_of_range) {
		return;
	}

	switch (type) {
		case AsyncQueryType::QUERY:
			fprintf(frame_analysis_log, " type=query query=");
			query = (ID3D11Query*)async;
			query->GetDesc(&desc);
			break;
		case AsyncQueryType::PREDICATE:
			fprintf(frame_analysis_log, " type=predicate query=");
			predicate = (ID3D11Predicate*)async;
			predicate->GetDesc(&desc);
			break;
		case AsyncQueryType::COUNTER:
			fprintf(frame_analysis_log, " type=performance\n");
			// Don't care about this, and it's a different DESC
			return;
		default:
			// Should not happen
			return;
	}

	switch (desc.Query) {
		case D3D11_QUERY_EVENT:
			fprintf(frame_analysis_log, "event");
			break;
		case D3D11_QUERY_OCCLUSION:
			fprintf(frame_analysis_log, "occlusion");
			break;
		case D3D11_QUERY_TIMESTAMP:
			fprintf(frame_analysis_log, "timestamp");
			break;
		case D3D11_QUERY_TIMESTAMP_DISJOINT:
			fprintf(frame_analysis_log, "timestamp_disjoint");
			break;
		case D3D11_QUERY_PIPELINE_STATISTICS:
			fprintf(frame_analysis_log, "pipeline_statistics");
			break;
		case D3D11_QUERY_OCCLUSION_PREDICATE:
			fprintf(frame_analysis_log, "occlusion_predicate");
			break;
		case D3D11_QUERY_SO_STATISTICS:
			fprintf(frame_analysis_log, "so_statistics");
			break;
		case D3D11_QUERY_SO_OVERFLOW_PREDICATE:
			fprintf(frame_analysis_log, "so_overflow_predicate");
			break;
		case D3D11_QUERY_SO_STATISTICS_STREAM0:
			fprintf(frame_analysis_log, "so_statistics_stream0");
			break;
		case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0:
			fprintf(frame_analysis_log, "so_overflow_predicate_stream0");
			break;
		case D3D11_QUERY_SO_STATISTICS_STREAM1:
			fprintf(frame_analysis_log, "so_statistics_stream1");
			break;
		case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1:
			fprintf(frame_analysis_log, "so_overflow_predicate_stream1");
			break;
		case D3D11_QUERY_SO_STATISTICS_STREAM2:
			fprintf(frame_analysis_log, "so_statistics_stream2");
			break;
		case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2:
			fprintf(frame_analysis_log, "so_overflow_predicate_stream2");
			break;
		case D3D11_QUERY_SO_STATISTICS_STREAM3:
			fprintf(frame_analysis_log, "so_statistics_stream3");
			break;
		case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3:
			fprintf(frame_analysis_log, "so_overflow_predicate_stream3");
			break;
		default:
			fprintf(frame_analysis_log, "?");
			break;
	}
	fprintf(frame_analysis_log, " MiscFlags=0x%x\n", desc.MiscFlags);
}

void FrameAnalysisContext::FrameAnalysisLogData(void *buf, UINT size)
{
	unsigned char *ptr = (unsigned char*)buf;
	UINT i;

	if (!buf || !size || !G->analyse_frame || !frame_analysis_log)
		return;

	fprintf(frame_analysis_log, "    data: ");
	for (i = 0; i < size; i++, ptr++)
		fprintf(frame_analysis_log, "%02x", *ptr);
	fprintf(frame_analysis_log, "\n");
}

ID3D11DeviceContext* FrameAnalysisContext::GetDumpingContext()
{
	if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		return GetPassThroughOrigContext1();

	if (analyse_options & FrameAnalysisOptions::DEFRD_CTX_IMM) {
		// XXX Experimental deferred context support: Use the immediate
		// context to stage a resource back to the CPU and dump it to
		// disk, before the deferred command list has executed. This
		// may not be thread safe.
		return GetHackerDevice()->GetPassThroughOrigContext1();
	}

	// XXX Alternate experiemental deferred context support - in this mode
	// we are back to using the current context, but we will delay CPU
	// reads until the command list is executed in the immediate context.
	return GetPassThroughOrigContext1();
}

void FrameAnalysisContext::Dump2DResourceImmediateCtx(ID3D11Texture2D *staging,
		wstring filename, bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format)
{
	HRESULT hr = S_OK, dont_care;
	wchar_t dedupe_filename[MAX_PATH];
	wstring save_filename;
	wchar_t *wic_ext = (stereo ? L".jps" : L".jpg");
	size_t ext, save_ext;

	save_filename = dedupe_tex2d_filename(staging, orig_desc, dedupe_filename, MAX_PATH, filename.c_str(), format);

	ext = filename.find_last_of(L'.');
	save_ext = save_filename.find_last_of(L'.');
	if (ext == wstring::npos || save_ext == wstring::npos) {
		FALogErr("Dump2DResource: Filename missing extension\n");
		return;
	}

	// Needs to be called at some point before SaveXXXTextureToFile:
	dont_care = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if ((analyse_options & FrameAnalysisOptions::FMT_2D_JPS) ||
	    (analyse_options & FrameAnalysisOptions::FMT_2D_AUTO)) {
		// save a JPS file. This will be missing extra channels (e.g.
		// transparency, depth buffer, specular power, etc) or bit depth that
		// can be found in the DDS file, but is generally easier to work with.
		//
		// Not all formats can be saved as JPS with this function - if
		// only dump_rt was specified (as opposed to dump_rt_jps) we
		// will dump out DDS files for those instead.
		filename.replace(ext, wstring::npos, wic_ext);
		save_filename.replace(save_ext, wstring::npos, wic_ext);
		FALogInfo("Dumping Texture2D %S -> %S\n", filename.c_str(), save_filename.c_str());

		hr = S_OK;
		if (GetFileAttributes(save_filename.c_str()) == INVALID_FILE_ATTRIBUTES)
			hr = DirectX::SaveWICTextureToFile(GetDumpingContext(), staging, GUID_ContainerFormatJpeg, save_filename.c_str());
		link_deduplicated_files(filename.c_str(), save_filename.c_str());
	}


	if ((analyse_options & FrameAnalysisOptions::FMT_2D_DDS) ||
	   ((analyse_options & FrameAnalysisOptions::FMT_2D_AUTO) && FAILED(hr))) {
		filename.replace(ext, wstring::npos, L".dds");
		save_filename.replace(save_ext, wstring::npos, L".dds");
		FALogInfo("Dumping Texture2D %S -> %S\n", filename.c_str(), save_filename.c_str());

		hr = S_OK;
		if (GetFileAttributes(save_filename.c_str()) == INVALID_FILE_ATTRIBUTES)
			hr = DirectX::SaveDDSTextureToFile(GetDumpingContext(), staging, save_filename.c_str());
		link_deduplicated_files(filename.c_str(), save_filename.c_str());
	}

	if (FAILED(hr))
		FALogErr("Failed to dump Texture2D %S -> %S: 0x%x\n", filename.c_str(), save_filename.c_str(), hr);

	if (analyse_options & FrameAnalysisOptions::FMT_DESC) {
		filename.replace(ext, wstring::npos, L".dsc");
		save_filename.replace(save_ext, wstring::npos, L".dsc");
		FALogInfo("Dumping Texture2D %S -> %S\n", filename.c_str(), save_filename.c_str());

		if (GetFileAttributes(save_filename.c_str()) == INVALID_FILE_ATTRIBUTES)
			DumpDesc(orig_desc, save_filename.c_str());
		link_deduplicated_files(filename.c_str(), save_filename.c_str());
	}

	CoUninitialize();
}

void FrameAnalysisContext::Dump2DResource(ID3D11Texture2D *resource, wchar_t
		*filename, bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D *staging = resource;
	D3D11_TEXTURE2D_DESC staging_desc, *desc = orig_desc;

	// In order to de-dupe Texture2D resources, we need to compare their
	// contents before dumping them to a file, so we copy them into a
	// staging resource (if we did not already do so earlier for the
	// reverse stereo blit). DirectXTK will notice this has been done and
	// skip doing it again.
	resource->GetDesc(&staging_desc);
	if ((staging_desc.Usage != D3D11_USAGE_STAGING) || !(staging_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) || (staging_desc.Format != format)) {
		hr = StageResource(resource, &staging_desc, &staging, format);
		if (FAILED(hr))
			return;
	}

	if (!orig_desc)
		desc = &staging_desc;

	if (!DeferDump2DResource(staging, filename, stereo, desc, format))
		Dump2DResourceImmediateCtx(staging, filename, stereo, desc, format);

	if (staging != resource)
		staging->Release();
}

HRESULT FrameAnalysisContext::CreateStagingResource(ID3D11Texture2D **resource,
		D3D11_TEXTURE2D_DESC desc, bool stereo, bool msaa, DXGI_FORMAT format)
{
	NVAPI_STEREO_SURFACECREATEMODE orig_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	HRESULT hr;

	// NOTE: desc is passed by value - this is intentional so we don't
	// modify desc in the caller

	if (stereo)
		desc.Width *= 2;

	if (msaa) {
		// Resolving MSAA requires these flags:
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;
	} else {
		// Make this a staging resource to save DirectXTK having to create it's
		// own staging resource.
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	}

	// Clear out bind flags that may prevent the copy from working:
	desc.BindFlags = 0;

	// Mip maps requires bind flags, but we set them to 0:
	// XXX: Possibly want a whilelist instead? DirectXTK only allows
	// D3D11_RESOURCE_MISC_TEXTURECUBE
	desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

	// Must resolve MSAA surfaces before the reverse stereo blit:
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	// We want the staging resource to be fully typed if possible, to
	// maximise compatibility with other programs that won't necessarily
	// know what to do with a typeless resource (including texconv from
	// DirectXTex). Since views must be fully typed, we can usually use the
	// format from the view used to obtain this resource.
	if (format != DXGI_FORMAT_UNKNOWN)
		desc.Format = format;

	LockResourceCreationMode();

	if (analyse_options & FrameAnalysisOptions::STEREO) {
		// If we are dumping stereo at all force surface creation mode
		// to stereo (regardless of whether we are creating this double
		// width) to prevent driver heuristics interfering. If the
		// original surface was mono that's ok - thaks to the
		// intermediate stages we'll end up with both eyes the same
		// (without this one eye would be blank instead, which is
		// arguably better since it will be immediately obvious, but
		// risks missing the second perspective if the original
		// resource was actually stereo)
		Profiling::NvAPI_Stereo_GetSurfaceCreationMode(GetHackerDevice()->mStereoHandle, &orig_mode);
		Profiling::NvAPI_Stereo_SetSurfaceCreationMode(GetHackerDevice()->mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
	}

	hr = GetHackerDevice()->GetPassThroughOrigDevice1()->CreateTexture2D(&desc, NULL, resource);

	if (analyse_options & FrameAnalysisOptions::STEREO)
		Profiling::NvAPI_Stereo_SetSurfaceCreationMode(GetHackerDevice()->mStereoHandle, orig_mode);

	UnlockResourceCreationMode();

	return hr;
}

HRESULT FrameAnalysisContext::ResolveMSAA(ID3D11Texture2D *src,
		D3D11_TEXTURE2D_DESC *srcDesc, ID3D11Texture2D **dst, DXGI_FORMAT format)
{
	ID3D11Texture2D *resolved = NULL;
	UINT item, level, index;
	HRESULT hr;

	*dst = NULL;

	if (srcDesc->SampleDesc.Count <= 1)
		return S_OK;

	// Resolve MSAA surfaces. Procedure copied from DirectXTK
	// These need to have D3D11_USAGE_DEFAULT to resolve,
	// so we need yet another intermediate texture:
	hr = CreateStagingResource(&resolved, *srcDesc, false, true, format);
	if (FAILED(hr)) {
		FALogErr("ResolveMSAA failed to create intermediate texture: 0x%x\n", hr);
		return hr;
	}

	DXGI_FORMAT fmt = EnsureNotTypeless(srcDesc->Format);
	UINT support = 0;

	hr = GetHackerDevice()->GetPassThroughOrigDevice1()->CheckFormatSupport( fmt, &support );
	if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
		FALogErr("ResolveMSAA cannot resolve MSAA format %d\n", fmt);
		goto err_release;
	}

	for (item = 0; item < srcDesc->ArraySize; item++) {
		for (level = 0; level < srcDesc->MipLevels; level++) {
			index = D3D11CalcSubresource(level, item, max(srcDesc->MipLevels, 1));
			GetDumpingContext()->ResolveSubresource(resolved, index, src, index, fmt);
		}
	}

	*dst = resolved;
	return S_OK;

err_release:
	resolved->Release();
	return hr;
}

HRESULT FrameAnalysisContext::StageResource(ID3D11Texture2D *src,
		D3D11_TEXTURE2D_DESC *srcDesc, ID3D11Texture2D **dst, DXGI_FORMAT format)
{
	ID3D11Texture2D *staging = NULL;
	ID3D11Texture2D *resolved = NULL;
	HRESULT hr;

	*dst = NULL;

	hr = CreateStagingResource(&staging, *srcDesc, false, false, format);
	if (FAILED(hr)) {
		FALogErr("StageResource failed to create intermediate texture: 0x%x\n", hr);
		return hr;
	}

	hr = ResolveMSAA(src, srcDesc, &resolved, format);
	if (FAILED(hr))
		goto err_release;
	if (resolved)
		src = resolved;

	GetDumpingContext()->CopyResource(staging, src);

	if (resolved)
		resolved->Release();

	*dst = staging;
	return S_OK;

err_release:
	if (staging)
		staging->Release();
	return hr;
}

// TODO: Refactor this with StereoScreenShot().
// Expects the reverse stereo blit to be enabled by the caller
void FrameAnalysisContext::DumpStereoResource(ID3D11Texture2D *resource, wchar_t *filename, DXGI_FORMAT format)
{
	ID3D11Texture2D *stereoResource = NULL;
	ID3D11Texture2D *tmpResource = NULL;
	ID3D11Texture2D *src = resource;
	D3D11_TEXTURE2D_DESC srcDesc;
	D3D11_BOX srcBox;
	HRESULT hr;
	UINT item, level, index, width, height;

	resource->GetDesc(&srcDesc);

	hr = CreateStagingResource(&stereoResource, srcDesc, true, false, format);
	if (FAILED(hr)) {
		FALogErr("DumpStereoResource failed to create stereo texture: 0x%x\n", hr);
		return;
	}

	if ((srcDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) ||
	    (srcDesc.SampleDesc.Count > 1)) {
		// Reverse stereo blit won't work on these surfaces directly
		// since CopySubresourceRegion() will fail if the source and
		// destination dimensions don't match, so use yet another
		// intermediate staging resource first.
		hr = StageResource(src, &srcDesc, &tmpResource, format);
		if (FAILED(hr))
			goto out;
		src = tmpResource;
	}

	// Set the source box as per the nvapi documentation:
	srcBox.left = 0;
	srcBox.top = 0;
	srcBox.front = 0;
	srcBox.right = width = srcDesc.Width;
	srcBox.bottom = height = srcDesc.Height;
	srcBox.back = 1;

	// Perform the reverse stereo blit on all sub-resources and mip-maps:
	for (item = 0; item < srcDesc.ArraySize; item++) {
		for (level = 0; level < srcDesc.MipLevels; level++) {
			index = D3D11CalcSubresource(level, item, max(srcDesc.MipLevels, 1));
			srcBox.right = width >> level;
			srcBox.bottom = height >> level;
			GetPassThroughOrigContext1()->CopySubresourceRegion(stereoResource, index, 0, 0, 0,
					src, index, &srcBox);
		}
	}

	Dump2DResource(stereoResource, filename, true, &srcDesc, format);

	if (tmpResource)
		tmpResource->Release();

out:
	stereoResource->Release();
}

static void copy_until_extension(wchar_t *txt_filename, const wchar_t *bin_filename, size_t size, wchar_t **pos, size_t *rem)
{
	size_t ext_pos;
	const wchar_t *ext;

	ext = wcsrchr(bin_filename, L'.');
	if (ext)
		ext_pos = ext - bin_filename;
	else
		ext_pos = wcslen(bin_filename);

	StringCchPrintfExW(txt_filename, size, pos, rem, NULL, L"%.*s", ext_pos, bin_filename);
}

void FrameAnalysisContext::dedupe_buf_filename_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, char type, int idx,
		UINT stride, UINT offset)
{
	wchar_t *pos;
	size_t rem;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	if (idx != -1)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-%cb%i", type, idx);

	if (offset)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-offset=%u", offset);

	if (stride)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-stride=%u", stride);

	if (FAILED(StringCchPrintfW(pos, rem, L".txt")))
		FALogErr("Failed to create buffer filename\n");
}

/*
 * This just treats the buffer as an array of float4s. In the future we might
 * try to use the reflection information in the shaders to add names and
 * correct types.
 */
void FrameAnalysisContext::DumpBufferTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, char type, int idx, UINT stride, UINT offset)
{
	FILE *fd = NULL;
	char *components = "xyzw";
	float *buf = (float*)map->pData;
	UINT i, c;
	errno_t err;

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogErr("Unable to create %S: %u\n", filename, err);
		return;
	}

	if (offset)
		fprintf(fd, "offset: %u\n", offset);
	if (stride)
		fprintf(fd, "stride: %u\n", stride);

	for (i = offset / 16; i < size / 16; i++) {
		for (c = offset % 4; c < 4; c++) {
			if (idx == -1)
				fprintf(fd, "buf[%d].%c: %.9g\n", i, components[c], buf[i*4+c]);
			else
				fprintf(fd, "%cb%i[%d].%c: %.9g\n", type, idx, i, components[c], buf[i*4+c]);
		}
	}

	fclose(fd);
}

static const char* TopologyStr(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	switch(topology) {
		case D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED: return "undefined";
		case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST: return "pointlist";
		case D3D11_PRIMITIVE_TOPOLOGY_LINELIST: return "linelist";
		case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP: return "linestrip";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST: return "trianglelist";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return "trianglestrip";
		case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: return "linelist_adj";
		case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ: return "linestrip_adj";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ: return "trianglelist_adj";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ: return "trianglestrip_adj";
		case D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST: return "1_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST: return "2_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST: return "3_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST: return "4_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST: return "5_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST: return "6_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST: return "7_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST: return "8_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST: return "9_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST: return "10_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST: return "11_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST: return "12_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST: return "13_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST: return "14_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST: return "15_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST: return "16_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST: return "17_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST: return "18_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST: return "19_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST: return "20_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST: return "21_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST: return "22_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST: return "23_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST: return "24_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST: return "25_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST: return "26_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST: return "27_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST: return "28_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST: return "29_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST: return "30_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST: return "31_control_point_patchlist";
		case D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST: return "32_control_point_patchlist";
	}
	return "invalid";
}

void FrameAnalysisContext::dedupe_buf_filename_vb_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, int idx, UINT stride,
		UINT offset, UINT first, UINT count, ID3DBlob *layout,
		D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info)
{
	wchar_t *pos;
	size_t rem;
	uint32_t layout_hash;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-vb%i", idx);

	if (layout) {
		layout_hash = crc32c_hw(0, layout->GetBufferPointer(), layout->GetBufferSize());
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-layout=%08x", layout_hash);
	}

	if (topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-topology=%S", TopologyStr(topology));

	if (offset)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-offset=%u", offset);

	if (stride)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-stride=%u", stride);

	if (first)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-first=%u", first);

	if (count)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-count=%u", count);

	if (call_info && call_info->FirstInstance)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-first_inst=%u", call_info->FirstInstance);

	if (call_info && call_info->InstanceCount)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-inst_count=%u", call_info->InstanceCount);

	if (FAILED(StringCchPrintfW(pos, rem, L".txt")))
		FALogErr("Failed to create vertex buffer filename\n");
}

static void dump_ia_layout(FILE *fd, D3D11_INPUT_ELEMENT_DESC *layout_desc, size_t layout_elements, int slot, bool *per_vert, bool *per_inst)
{
	UINT i;

	for (i = 0; i < layout_elements; i++) {
		fprintf(fd, "element[%i]:\n", i);
		fprintf(fd, "  SemanticName: %s\n", layout_desc[i].SemanticName);
		fprintf(fd, "  SemanticIndex: %u\n", layout_desc[i].SemanticIndex);
		fprintf(fd, "  Format: %s\n", TexFormatStr(layout_desc[i].Format));
		fprintf(fd, "  InputSlot: %u\n", layout_desc[i].InputSlot);
		if (layout_desc[i].AlignedByteOffset == D3D11_APPEND_ALIGNED_ELEMENT)
			fprintf(fd, "  AlignedByteOffset: append\n");
		else
			fprintf(fd, "  AlignedByteOffset: %u\n", layout_desc[i].AlignedByteOffset);
		switch(layout_desc[i].InputSlotClass) {
			case D3D11_INPUT_PER_VERTEX_DATA:
				fprintf(fd, "  InputSlotClass: per-vertex\n");
				if (layout_desc[i].InputSlot == slot)
					*per_vert = true;
				break;
			case D3D11_INPUT_PER_INSTANCE_DATA:
				fprintf(fd, "  InputSlotClass: per-instance\n");
				if (layout_desc[i].InputSlot == slot)
					*per_inst = true;
				break;
			default:
				fprintf(fd, "  InputSlotClass: %u\n", layout_desc[i].InputSlotClass);
				break;
		}
		fprintf(fd, "  InstanceDataStepRate: %u\n", layout_desc[i].InstanceDataStepRate);
	}
}

static void dump_vb_unknown_layout(FILE *fd, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, int slot, UINT offset, UINT first, UINT count, UINT stride)
{
	float *buff = (float*)map->pData;
	uint32_t *buf32 = (uint32_t*)map->pData;
	uint8_t *buf8 = (uint8_t*)map->pData;
	UINT vertex, j, start, end, buf_idx;

	start = offset / stride + first;
	end = size / stride;
	if (count)
		end = min(end, start + count);

	for (vertex = start; vertex < end; vertex++) {
		fprintf(fd, "\n");

		for (j = 0; j < stride / 4; j++) {
			buf_idx = vertex * stride / 4 + j;
			fprintf(fd, "vb%i[%u]+%03u: 0x%08x %.9g\n", slot, vertex - start, j*4, buf32[buf_idx], buff[buf_idx]);
		}

		// In case we find one that is not a 32bit multiple finish off one byte at a time:
		for (j = j * 4; j < stride; j++) {
			buf_idx = vertex * stride + j;
			fprintf(fd, "vb%i[%u]+%03u: 0x%02x\n", slot, vertex - start, j, buf8[buf_idx]);
		}
	}
}

static UINT dxgi_format_alignment(DXGI_FORMAT format)
{
	// I'm not positive what the alignment constraints actually are - MSDN
	// mentions they exist, but I don't think they go so far as being
	// aligned to the size of the full format (I'm seeing vertex buffers
	// that clearly are not). For now I'm going with the assumption that
	// the alignment must match the individual components, and skipping
	// those with variable sized components or unusual formats.
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			return 4;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
			return 2;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return 1;
	}
	return 0;
}

static float float16(uint16_t f16)
{
	// Shift sign and mantissa to new positions:
	uint32_t f32 = ((f16 & 0x8000) << 16) | ((f16 & 0x3ff) << 13);
	// Need to check special cases of the biased exponent:
	int biased_exponent = (f16 & 0x7c00) >> 10;

	if (biased_exponent == 0) {
		// Zero / subnormal: New biased exponent remains zero
	} else if (biased_exponent == 0x1f) {
		// Infinity / NaN: New biased exponent is filled with 1s
		f32 |= 0x7f800000;
	} else {
		// Normal number: Adjust the exponent bias:
		biased_exponent = biased_exponent - 15 + 127;
		f32 |= biased_exponent << 23;
	}

	return *(float*)&f32;
}

static float unorm24(uint32_t val)
{
	return (float)val / (float)0xffffff;
}

static float unorm16(uint16_t val)
{
	return (float)val / (float)0xffff;
}

static float snorm16(int16_t val)
{
	return (float)val / (float)0x7fff;
}

static float unorm8(uint8_t val)
{
	return (float)val / (float)0xff;
}

static float snorm8(int8_t val)
{
	return (float)val / (float)0x7f;
}

static int fprint_dxgi_format(FILE *fd, DXGI_FORMAT format, uint8_t *buf)
{
	float *f = (float*)buf;
	uint32_t *u32 = (uint32_t*)buf;
	int32_t *s32 = (int32_t*)buf;
	uint16_t *u16 = (uint16_t*)buf;
	int16_t *s16 = (int16_t*)buf;
	uint8_t *u8 = (uint8_t*)buf;
	int8_t *s8 = (int8_t*)buf;
	unsigned i;

	switch (format) {
		// --- 32-bit ---
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			return fprintf(fd, "%08x, %08x, %08x, %08x", u32[0], u32[1], u32[2], u32[3]);
		case DXGI_FORMAT_R32G32B32_TYPELESS:
			return fprintf(fd, "%08x, %08x, %08x", u32[0], u32[1], u32[2]);
		case DXGI_FORMAT_R32G32_TYPELESS:
			return fprintf(fd, "%08x, %08x", u32[0], u32[1]);
		case DXGI_FORMAT_R32_TYPELESS:
			return fprintf(fd, "%08x", u32[0]);

		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", f[0], f[1], f[2], f[3]);
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return fprintf(fd, "%.9g, %.9g, %.9g", f[0], f[1], f[2]);
		case DXGI_FORMAT_R32G32_FLOAT:
			return fprintf(fd, "%.9g, %.9g", f[0], f[1]);
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return fprintf(fd, "%.9g", f[0]);

		case DXGI_FORMAT_R32G32B32A32_UINT:
			return fprintf(fd, "%u, %u, %u, %u", u32[0], u32[1], u32[2], u32[3]);
		case DXGI_FORMAT_R32G32B32_UINT:
			return fprintf(fd, "%u, %u, %u", u32[0], u32[1], u32[2]);
		case DXGI_FORMAT_R32G32_UINT:
			return fprintf(fd, "%u, %u", u32[0], u32[1]);
		case DXGI_FORMAT_R32_UINT:
			return fprintf(fd, "%u", u32[0]);

		case DXGI_FORMAT_R32G32B32A32_SINT:
			return fprintf(fd, "%d, %d, %d, %d", s32[0], s32[1], s32[2], s32[3]);
		case DXGI_FORMAT_R32G32B32_SINT:
			return fprintf(fd, "%d, %d, %d", s32[0], s32[1], s32[2]);
		case DXGI_FORMAT_R32G32_SINT:
			return fprintf(fd, "%d, %d", s32[0], s32[1]);
		case DXGI_FORMAT_R32_SINT:
			return fprintf(fd, "%d", s32[0]);

		// --- 16-bit ---
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			return fprintf(fd, "%04x, %04x, %04x, %04x", u16[0], u16[1], u16[2], u16[3]);
		case DXGI_FORMAT_R16G16_TYPELESS:
			return fprintf(fd, "%04x, %04x", u16[0], u16[1]);
		case DXGI_FORMAT_R16_TYPELESS:
			return fprintf(fd, "%04x", u16[0]);

		// %.9g is probably excessive, but I haven't calculated or
		// verified the actual decimal precision needed to ensure
		// 16-bit floats can be reproduced exactly, and I know that
		// %.9g is enough for 32-bit floats so it is safer for now:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", float16(u16[0]), float16(u16[1]), float16(u16[2]), float16(u16[3]));
		case DXGI_FORMAT_R16G16_FLOAT:
			return fprintf(fd, "%.9g, %.9g", float16(u16[0]), float16(u16[1]));
		case DXGI_FORMAT_R16_FLOAT:
			return fprintf(fd, "%.9g", float16(u16[0]));

		// And of course, if we were to work out a better decimal
		// precision value, remember that a 16-bit UNORM has 16 bits of
		// precision, while a 16-bit FLOAT only has 11.
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", unorm16(u16[0]), unorm16(u16[1]), unorm16(u16[2]), unorm16(u16[3]));
		case DXGI_FORMAT_R16G16_UNORM:
			return fprintf(fd, "%.9g, %.9g", unorm16(u16[0]), unorm16(u16[1]));
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
			return fprintf(fd, "%.9g", unorm16(u16[0]));

		case DXGI_FORMAT_R16G16B16A16_SNORM:
			return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", snorm16(s16[0]), snorm16(s16[1]), snorm16(s16[2]), snorm16(s16[3]));
		case DXGI_FORMAT_R16G16_SNORM:
			return fprintf(fd, "%.9g, %.9g", snorm16(s16[0]), snorm16(s16[1]));
		case DXGI_FORMAT_R16_SNORM:
			return fprintf(fd, "%.9g", snorm16(s16[0]));

		case DXGI_FORMAT_R16G16B16A16_UINT:
			return fprintf(fd, "%u, %u, %u, %u", u16[0], u16[1], u16[2], u16[3]);
		case DXGI_FORMAT_R16G16_UINT:
			return fprintf(fd, "%u, %u", u16[0], u16[1]);
		case DXGI_FORMAT_R16_UINT:
			return fprintf(fd, "%u", u16[0]);

		case DXGI_FORMAT_R16G16B16A16_SINT:
			return fprintf(fd, "%d, %d, %d, %d", s16[0], s16[1], s16[2], s16[3]);
		case DXGI_FORMAT_R16G16_SINT:
			return fprintf(fd, "%d, %d", s16[0], s16[1]);
		case DXGI_FORMAT_R16_SINT:
			return fprintf(fd, "%d", s16[0]);

		// --- 8-bit ---
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			return fprintf(fd, "%02x, %02x, %02x, %02x", u8[0], u8[1], u8[2], u8[3]);
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			return fprintf(fd, "%02x, %02x, %02x", u8[0], u8[1], u8[2]);
		case DXGI_FORMAT_R8G8_TYPELESS:
			return fprintf(fd, "%02x, %02x", u8[0], u8[1]);
		case DXGI_FORMAT_R8_TYPELESS:
			return fprintf(fd, "%02x", u8[0]);

		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: // XXX: Should we apply the SRGB formula?
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: // XXX: Should we apply the SRGB formula?
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: // XXX: Should we apply the SRGB formula?
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
			return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", unorm8(u8[0]), unorm8(u8[1]), unorm8(u8[2]), unorm8(u8[3]));
		case DXGI_FORMAT_R8G8_UNORM:
			return fprintf(fd, "%.9g, %.9g", unorm8(u8[0]), unorm8(u8[1]));
		case DXGI_FORMAT_R8_UNORM:
			return fprintf(fd, "%.9g", unorm8(u8[0]));

		case DXGI_FORMAT_R8G8B8A8_SNORM:
			return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", snorm8(s8[0]), snorm8(s8[1]), snorm8(s8[2]), snorm8(s8[3]));
		case DXGI_FORMAT_R8G8_SNORM:
			return fprintf(fd, "%.9g, %.9g", snorm8(s8[0]), snorm8(s8[1]));
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_A8_UNORM:
			return fprintf(fd, "%.9g", snorm8(s8[0]));

		case DXGI_FORMAT_R8G8B8A8_UINT:
			return fprintf(fd, "%u, %u, %u, %u", u8[0], u8[1], u8[2], u8[3]);
		case DXGI_FORMAT_R8G8_UINT:
			return fprintf(fd, "%u, %u", u8[0], u8[1]);
		case DXGI_FORMAT_R8_UINT:
			return fprintf(fd, "%u", u8[0]);

		case DXGI_FORMAT_R8G8B8A8_SINT:
			return fprintf(fd, "%d, %d, %d, %d", s8[0], s8[1], s8[2], s8[3]);
		case DXGI_FORMAT_R8G8_SINT:
			return fprintf(fd, "%d, %d", s8[0], s8[1]);
		case DXGI_FORMAT_R8_SINT:
			return fprintf(fd, "%d", s8[0]);

		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return fprintf(fd, "%.9g, %d", f[0], u8[4]);

		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return fprintf(fd, "%.9g, %d", unorm24(u32[0] & 0xffffff), u8[3]);

		// TODO: Unusual field sizes:
		// case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		// case DXGI_FORMAT_R10G10B10A2_UNORM:
		// case DXGI_FORMAT_R10G10B10A2_UINT:
		// case DXGI_FORMAT_R11G11B10_FLOAT:
		// case DXGI_FORMAT_R1_UNORM:
		// case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		// case DXGI_FORMAT_B5G6R5_UNORM:
		// case DXGI_FORMAT_B5G5R5A1_UNORM:
		// case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	}

	for (i = 0; i < dxgi_format_size(format); i++)
		fprintf(fd, "%02x", buf[i]);
	return i * 2;
}


static void dump_vb_elem(FILE *fd, uint8_t *buf,
		D3D11_INPUT_ELEMENT_DESC *layout_desc, size_t layout_elements,
		int slot, UINT vb_idx, UINT elem, UINT stride)
{
	UINT offset = 0, alignment, size;

	if (layout_desc[elem].InputSlot != slot)
		return;

	if (layout_desc[elem].AlignedByteOffset != D3D11_APPEND_ALIGNED_ELEMENT) {
		offset = layout_desc[elem].AlignedByteOffset;
	} else {
		alignment = dxgi_format_alignment(layout_desc[elem].Format);
		if (!alignment) {
			fprintf(fd, "# WARNING: Unknown format alignment, vertex buffer may be decoded incorrectly\n");
		} else if (offset % alignment) {
			fprintf(fd, "# WARNING: Untested alignment code in use, please report incorrectly decoded vertex buffers\n");
			// XXX: Also, what if the entire vertex is misaligned in the buffer?
			offset += alignment - (offset % alignment);
		}
	}

	fprintf(fd, "vb%i[%u]+%03u %s", slot, vb_idx, offset, layout_desc[elem].SemanticName);
	if (layout_desc[elem].SemanticIndex)
		fprintf(fd, "%u", layout_desc[elem].SemanticIndex);
	fprintf(fd, ": ");

	fprint_dxgi_format(fd, layout_desc[elem].Format, buf + offset);
	fprintf(fd, "\n");

	size = dxgi_format_size(layout_desc[elem].Format);
	if (!size)
		fprintf(fd, "# WARNING: Unknown format size, vertex buffer may be decoded incorrectly\n");
	offset += size;
	if (offset > stride)
		fprintf(fd, "# WARNING: Offset exceeded stride, vertex buffer may be decoded incorrectly\n");
}

static void dump_vb_known_layout(FILE *fd, D3D11_MAPPED_SUBRESOURCE *map,
		D3D11_INPUT_ELEMENT_DESC *layout_desc, size_t layout_elements,
		UINT size, int slot, UINT offset, UINT first, UINT count, UINT stride)
{
	UINT vertex, elem, start, end;

	start = offset / stride + first;
	end = size / stride;
	if (count)
		end = min(end, start + count);

	for (vertex = start; vertex < end; vertex++) {
		fprintf(fd, "\n");
		for (elem = 0; elem < layout_elements; elem++) {
			if (layout_desc[elem].InputSlotClass != D3D11_INPUT_PER_VERTEX_DATA)
				continue;

			dump_vb_elem(fd, (uint8_t*)map->pData + stride*vertex,
					layout_desc, layout_elements, slot,
					vertex - start, elem, stride);
		}
	}
}

static void dump_vb_instance_data(FILE *fd, D3D11_MAPPED_SUBRESOURCE *map,
		D3D11_INPUT_ELEMENT_DESC *layout_desc, size_t layout_elements,
		UINT size, int slot, UINT offset, UINT first, UINT count, UINT stride)
{
	UINT instance, idx, elem, start, end;

	start = offset / stride + first;
	end = size / stride;
	if (count)
		end = min(end, start + count);

	for (instance = start; instance < end; instance++) {
		fprintf(fd, "\n");
		for (elem = 0; elem < layout_elements; elem++) {
			if (layout_desc[elem].InputSlotClass != D3D11_INPUT_PER_INSTANCE_DATA)
				continue;

			if (layout_desc[elem].InstanceDataStepRate)
				idx = (instance-start) / layout_desc[elem].InstanceDataStepRate + start;
			else
				idx = instance;

			dump_vb_elem(fd, (uint8_t*)map->pData + stride*idx,
					layout_desc, layout_elements, slot,
					idx - start, elem, stride);
		}
	}
}

/*
 * Dumps the vertex buffer in several formats.
 * FIXME: We should wrap the input layout object to get the correct format (and
 * other info like the semantic).
 */
void FrameAnalysisContext::DumpVBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, int slot, UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
		D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info)
{
	FILE *fd = NULL;
	errno_t err;
	D3D11_INPUT_ELEMENT_DESC *layout_desc = NULL;
	size_t layout_elements;
	bool per_vert = false, per_inst = false;

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogErr("Unable to create %S: %u\n", filename, err);
		return;
	}

	if (offset)
		fprintf(fd, "byte offset: %u\n", offset);
	fprintf(fd, "stride: %u\n", stride);
	if (first || count) {
		fprintf(fd, "first vertex: %u\n", first);
		fprintf(fd, "vertex count: %u\n", count);
	}
	if (call_info && call_info->FirstInstance || call_info->InstanceCount) {
		fprintf(fd, "first instance: %u\n", call_info->FirstInstance);
		fprintf(fd, "instance count: %u\n", call_info->InstanceCount);
	}
	if (topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		fprintf(fd, "topology: %s\n", TopologyStr(topology));
	if (layout) {
		layout_desc = (D3D11_INPUT_ELEMENT_DESC*)layout->GetBufferPointer();
		layout_elements = layout->GetBufferSize() / sizeof(D3D11_INPUT_ELEMENT_DESC);
		dump_ia_layout(fd, layout_desc, layout_elements, slot, &per_vert, &per_inst);
	}
	if (!stride) {
		FALogErr("Cannot dump vertex buffer with stride=0\n");
		goto out_close;
	}

	if (layout_desc) {
		if (per_vert) {
			fprintf(fd, "\nvertex-data:\n");
			dump_vb_known_layout(fd, map, layout_desc, layout_elements,
					size, slot, offset, first, count, stride);
		}

		if (per_inst && call_info) {
			fprintf(fd, "\ninstance-data:\n");
			dump_vb_instance_data(fd, map, layout_desc,
					layout_elements, size, slot, offset,
					call_info->FirstInstance,
					call_info->InstanceCount, stride);
		}
	} else {
		dump_vb_unknown_layout(fd, map, size, slot, offset, first, count, stride);
	}

out_close:
	fclose(fd);
}

void FrameAnalysisContext::dedupe_buf_filename_ib_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, DXGI_FORMAT ib_fmt,
		UINT offset, UINT first, UINT count, D3D11_PRIMITIVE_TOPOLOGY topology)
{
	wchar_t *pos;
	size_t rem;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ib");

	if (ib_fmt != DXGI_FORMAT_UNKNOWN)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-format=%S", TexFormatStr(ib_fmt));

	if (topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-topology=%S", TopologyStr(topology));

	if (offset)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-offset=%u", offset);

	if (first)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-first=%u", first);

	if (count)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-count=%u", count);

	if (FAILED(StringCchPrintfW(pos, rem, L".txt")))
		FALogErr("Failed to create index buffer filename\n");
}

void FrameAnalysisContext::DumpIBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, DXGI_FORMAT format, UINT offset, UINT first, UINT count,
		D3D11_PRIMITIVE_TOPOLOGY topology)
{
	FILE *fd = NULL;
	uint16_t *buf16 = (uint16_t*)map->pData;
	uint32_t *buf32 = (uint32_t*)map->pData;
	UINT start, end, i;
	errno_t err;
	int grouping = 1;

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogErr("Unable to create %S: %u\n", filename, err);
		return;
	}

	fprintf(fd, "byte offset: %u\n", offset);
	if (first || count) {
		fprintf(fd, "first index: %u\n", first);
		fprintf(fd, "index count: %u\n", count);
	}

	if (topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		fprintf(fd, "topology: %s\n", TopologyStr(topology));
	switch(topology) {
		case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
			grouping = 2;
			break;
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			grouping = 3;
			break;
		// TODO: Appropriate grouping for other input topologies
	}

	switch(format) {
	case DXGI_FORMAT_R16_UINT:
		fprintf(fd, "format: DXGI_FORMAT_R16_UINT\n");

		start = offset / 2 + first;
		end = size / 2;
		if (count)
			end = min(end, start + count);

		for (i = start; i < end; i++) {
			if ((i-start) % grouping == 0)
				fprintf(fd, "\n");
			else
				fprintf(fd, " ");
			fprintf(fd, "%u", buf16[i]);
		}
		fprintf(fd, "\n");
		break;
	case DXGI_FORMAT_R32_UINT:
		fprintf(fd, "format: DXGI_FORMAT_R32_UINT\n");

		start = offset / 4 + first;
		end = size / 4;
		if (count)
			end = min(end, start + count);

		for (i = start; i < end; i++) {
			if ((i-start) % grouping == 0)
				fprintf(fd, "\n");
			else
				fprintf(fd, " ");
			fprintf(fd, "%u", buf32[i]);
		}
		fprintf(fd, "\n");
		break;
	default:
		// Illegal format for an index buffer
		fprintf(fd, "format %u is illegal\n", format);
		break;
	}

	fclose(fd);
}

template <typename DescType>
void FrameAnalysisContext::DumpDesc(DescType *desc, const wchar_t *filename)
{
	FILE *fd = NULL;
	char buf[256];
	errno_t err;

	StrResourceDesc(buf, 256, desc);

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogErr("Unable to create %S: %u\n", filename, err);
		return;
	}
	fwrite(buf, 1, strlen(buf), fd);
	putc('\n', fd);
	fclose(fd);
}

bool FrameAnalysisContext::DeferDump2DResource(ID3D11Texture2D *staging,
		wchar_t *filename, bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format)
{
	if (!(analyse_options & FrameAnalysisOptions::DEFRD_CTX_DELAY))
		return false;

	if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		return false;

	if (!deferred_tex2d) {
		deferred_tex2d = make_unique<FrameAnalysisDeferredTex2D>();
		FALogInfo("Creating deferred staging Texture2D list %p on context %p\n", deferred_tex2d.get(), this);
	}

	FALogInfo("Deferring Texture2D dump: %S\n", filename);
	deferred_tex2d->emplace_back(analyse_options, staging, filename, stereo, orig_desc, format);

	return true;
}

bool FrameAnalysisContext::DeferDumpBuffer(ID3D11Buffer *staging,
		D3D11_BUFFER_DESC *orig_desc, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
		D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
		ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb)
{
	if (!(analyse_options & FrameAnalysisOptions::DEFRD_CTX_DELAY))
		return false;

	if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		return false;

	if (!deferred_buffers) {
		deferred_buffers = make_unique<FrameAnalysisDeferredBuffers>();
		FALogInfo("Creating deferred staging Buffer list %p on context %p\n", deferred_buffers.get(), this);
	}

	FALogInfo("Deferring Buffer dump: %S\n", filename);
	deferred_buffers->emplace_back(analyse_options, staging, orig_desc, filename,
			buf_type_mask, idx, ib_fmt, stride, offset, first, count, layout,
			topology, call_info, staged_ib_for_vb, ib_off_for_vb);
	return true;
}

void FrameAnalysisContext::dump_deferred_resources(ID3D11CommandList *command_list)
{
	FrameAnalysisDeferredBuffersPtr deferred_buffers = nullptr;
	FrameAnalysisDeferredTex2DPtr deferred_tex2d = nullptr;

	if (!command_list)
		return;

	if (frame_analysis_deferred_buffer_lists.empty() && frame_analysis_deferred_tex2d_lists.empty())
		return;

	EnterCriticalSectionPretty(&G->mCriticalSection);

	try {
		deferred_buffers = std::move(frame_analysis_deferred_buffer_lists.at(command_list));
		frame_analysis_deferred_buffer_lists.erase(command_list);
	} catch (std::out_of_range) {}
	if (deferred_buffers) {
		for (FrameAnalysisDeferredDumpBufferArgs &i : *deferred_buffers) {
			// Process key inputs to allow user to abort long running frame analysis sessions:
			DispatchInputEvents(GetHackerDevice());
			if (!G->analyse_frame)
				break;

			this->analyse_options = i.analyse_options;
			DumpBufferImmediateCtx(i.staging.Get(), &i.orig_desc,
					i.filename, i.buf_type_mask, i.idx,
					i.ib_fmt, i.stride, i.offset, i.first,
					i.count, i.layout.Get(), i.topology, &i.call_info,
					i.staged_ib_for_vb.Get(), i.ib_off_for_vb);
		}
	}

	try {
		deferred_tex2d = std::move(frame_analysis_deferred_tex2d_lists.at(command_list));
		frame_analysis_deferred_tex2d_lists.erase(command_list);
	} catch (std::out_of_range) {}
	if (deferred_tex2d) {
		for (FrameAnalysisDeferredDumpTex2DArgs &i : *deferred_tex2d) {
			// Process key inputs to allow user to abort long running frame analysis sessions:
			DispatchInputEvents(GetHackerDevice());
			if (!G->analyse_frame)
				break;

			this->analyse_options = i.analyse_options;
			Dump2DResourceImmediateCtx(i.staging.Get(), i.filename,
					i.stereo, &i.orig_desc, i.format);
		}
	}

	LeaveCriticalSection(&G->mCriticalSection);
}

void FrameAnalysisContext::finish_deferred_resources(ID3D11CommandList *command_list)
{
	if (!command_list || (!deferred_buffers && !deferred_tex2d))
		return;

	EnterCriticalSectionPretty(&G->mCriticalSection);

	if (deferred_buffers) {
		FALogInfo("Finishing deferred staging Buffer list %p on context %p\n", deferred_buffers.get(), this);
		frame_analysis_deferred_buffer_lists.erase(command_list);
		frame_analysis_deferred_buffer_lists.emplace(command_list, std::move(deferred_buffers));
	}

	if (deferred_tex2d) {
		FALogInfo("Finishing deferred staging Texture2D list %p on context %p\n", deferred_tex2d.get(), this);
		frame_analysis_deferred_tex2d_lists.erase(command_list);
		frame_analysis_deferred_tex2d_lists.emplace(command_list, std::move(deferred_tex2d));
	}

	LeaveCriticalSection(&G->mCriticalSection);
}

void FrameAnalysisContext::determine_vb_count(UINT *count, ID3D11Buffer *staged_ib_for_vb,
		DrawCallInfo *call_info, UINT ib_off_for_vb, DXGI_FORMAT ib_fmt)
{
	D3D11_MAPPED_SUBRESOURCE ib_map;
	UINT i, ib_start, ib_end, max_vertex = 0;
	D3D11_BUFFER_DESC ib_desc;
	uint16_t *buf16;
	uint32_t *buf32;
	HRESULT hr;

	// If an indexed draw call is in use, we don't explicitly know how many
	// vertices we will need to dump from the vertex buffer, so we used to
	// dump every vertex from the offset/first to the end of the buffer.
	// This worked, but resulted in many quite large text files with lots
	// of overlap for games that used larger vertex buffers to cover many
	// objects (e.g. Witcher 3). This function scans over the staged index
	// buffer from the draw call to find the largest vertex it refers to
	// determine how many vertices we will need to dump.

	if (!staged_ib_for_vb || !call_info || call_info->IndexCount == 0)
		return;

	hr = GetDumpingContext()->Map(staged_ib_for_vb, 0, D3D11_MAP_READ, 0, &ib_map);
	if (FAILED(hr)) {
		FALogErr("determine_vb_count failed to map index buffer staging resource: 0x%x\n", hr);
		return;
	}

	buf16 = (uint16_t*)ib_map.pData;
	buf32 = (uint32_t*)ib_map.pData;

	staged_ib_for_vb->GetDesc(&ib_desc);

	switch(ib_fmt) {
	case DXGI_FORMAT_R16_UINT:
		ib_start = ib_off_for_vb / 2 + call_info->FirstIndex;
		ib_end = ib_desc.ByteWidth / 2;
		if (call_info->IndexCount)
			ib_end = min(ib_end, ib_start + call_info->IndexCount);

		for (i = ib_start; i < ib_end; i++)
			max_vertex = max(max_vertex, buf16[i]);
		*count = max_vertex + 1;
		break;
	case DXGI_FORMAT_R32_UINT:
		ib_start = ib_off_for_vb / 4 + call_info->FirstIndex;
		ib_end = ib_desc.ByteWidth / 4;
		if (call_info->IndexCount)
			ib_end = min(ib_end, ib_start + call_info->IndexCount);

		for (i = ib_start; i < ib_end; i++)
			max_vertex = max(max_vertex, buf32[i]);
		*count = max_vertex + 1;
		break;
	}

	GetDumpingContext()->Unmap(staged_ib_for_vb, 0);
}

void FrameAnalysisContext::DumpBufferImmediateCtx(ID3D11Buffer *staging, D3D11_BUFFER_DESC *orig_desc,
		wstring filename, FrameAnalysisOptions buf_type_mask, int idx,
		DXGI_FORMAT ib_fmt, UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
		D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
		ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb)
{
	wchar_t bin_filename[MAX_PATH], txt_filename[MAX_PATH];
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr;
	FILE *fd = NULL;
	wchar_t *bin_ext;
	size_t ext;
	errno_t err;

	hr = GetDumpingContext()->Map(staging, 0, D3D11_MAP_READ, 0, &map);
	if (FAILED(hr)) {
		FALogErr("DumpBuffer failed to map staging resource: 0x%x\n", hr);
		return;
	}

	dedupe_buf_filename(staging, orig_desc, &map, bin_filename, MAX_PATH);

	ext = filename.find_last_of(L'.');
	bin_ext = wcsrchr(bin_filename, L'.');
	if (ext == wstring::npos || !bin_ext) {
		FALogErr("DumpBuffer: Filename missing extension\n");
		goto out_unmap;
	}

	if (analyse_options & FrameAnalysisOptions::FMT_BUF_BIN) {
		filename.replace(ext, wstring::npos, L".buf");
		wcscpy_s(bin_ext, MAX_PATH + bin_filename - bin_ext, L".buf");
		FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), bin_filename);

		if (GetFileAttributes(bin_filename) == INVALID_FILE_ATTRIBUTES) {
			err = wfopen_ensuring_access(&fd, bin_filename, L"wb");
			if (!fd) {
				FALogErr("Unable to create %S: %u\n", bin_filename, err);
				goto out_unmap;
			}
			fwrite(map.pData, 1, orig_desc->ByteWidth, fd);
			fclose(fd);
		}
		link_deduplicated_files(filename.c_str(), bin_filename);
	}

	if (analyse_options & FrameAnalysisOptions::FMT_BUF_TXT) {
		filename.replace(ext, wstring::npos, L".txt");

		if (buf_type_mask & FrameAnalysisOptions::DUMP_CB) {
			dedupe_buf_filename_txt(bin_filename, txt_filename, MAX_PATH, 'c', idx, stride, offset);
			FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), txt_filename);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpBufferTxt(txt_filename, &map, orig_desc->ByteWidth, 'c', idx, stride, offset);
			}
		} else if (buf_type_mask & FrameAnalysisOptions::DUMP_VB) {
			determine_vb_count(&count, staged_ib_for_vb, call_info, ib_off_for_vb, ib_fmt);
			dedupe_buf_filename_vb_txt(bin_filename, txt_filename, MAX_PATH, idx, stride, offset, first, count, layout, topology, call_info);
			FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), txt_filename);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpVBTxt(txt_filename, &map, orig_desc->ByteWidth, idx, stride, offset, first, count, layout, topology, call_info);
			}
		} else if (buf_type_mask & FrameAnalysisOptions::DUMP_IB) {
			dedupe_buf_filename_ib_txt(bin_filename, txt_filename, MAX_PATH, ib_fmt, offset, first, count, topology);
			FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), txt_filename);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpIBTxt(txt_filename, &map, orig_desc->ByteWidth, ib_fmt, offset, first, count, topology);
			}
		} else {
			// We don't know what kind of buffer this is, so just
			// use the generic dump routine:

			dedupe_buf_filename_txt(bin_filename, txt_filename, MAX_PATH, '?', idx, stride, offset);
			FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), txt_filename);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpBufferTxt(txt_filename, &map, orig_desc->ByteWidth, '?', idx, stride, offset);
			}
		}
		link_deduplicated_files(filename.c_str(), txt_filename);
	}
	// TODO: Dump UAV, RT and SRV buffers as text taking their format,
	// offset, size, first entry and num entries into account.

	if (analyse_options & FrameAnalysisOptions::FMT_DESC) {
		filename.replace(ext, wstring::npos, L".dsc");
		wcscpy_s(bin_ext, MAX_PATH + bin_filename - bin_ext, L".dsc");
		FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), bin_filename);

		if (GetFileAttributes(bin_filename) == INVALID_FILE_ATTRIBUTES)
			DumpDesc(orig_desc, bin_filename);
		link_deduplicated_files(filename.c_str(), bin_filename);
	}

out_unmap:
	GetDumpingContext()->Unmap(staging, 0);
}

void FrameAnalysisContext::DumpBuffer(ID3D11Buffer *buffer, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
		D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
		ID3D11Buffer **staged_ib_ret, ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb)
{
	D3D11_BUFFER_DESC desc, orig_desc;
	ID3D11Buffer *staging = NULL;
	HRESULT hr;

	// Process key inputs to allow user to abort long running frame
	// analysis sessions (this case is specifically for dump_vb and dump_ib
	// which bypasses DumpResource()):
	DispatchInputEvents(GetHackerDevice());
	if (!G->analyse_frame)
		return;

	buffer->GetDesc(&desc);
	memcpy(&orig_desc, &desc, sizeof(D3D11_BUFFER_DESC));

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	LockResourceCreationMode();
	hr = GetHackerDevice()->GetPassThroughOrigDevice1()->CreateBuffer(&desc, NULL, &staging);
	UnlockResourceCreationMode();
	if (FAILED(hr)) {
		FALogErr("DumpBuffer failed to create staging buffer: 0x%x\n", hr);
		return;
	}

	GetDumpingContext()->CopyResource(staging, buffer);

	if (!DeferDumpBuffer(staging, &orig_desc, filename, buf_type_mask, idx, ib_fmt, stride, offset, first, count, layout, topology, call_info, staged_ib_for_vb, ib_off_for_vb))
		DumpBufferImmediateCtx(staging, &orig_desc, filename, buf_type_mask, idx, ib_fmt, stride, offset, first, count, layout, topology, call_info, staged_ib_for_vb, ib_off_for_vb);

	// We can return the staged index buffer for later use when dumping the
	// vertex buffers as text, to determine the maximum vertex count:
	if (staged_ib_ret) {
		*staged_ib_ret = staging;
		staging->AddRef();
	}

	staging->Release();
}

void FrameAnalysisContext::DumpResource(ID3D11Resource *resource, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT format,
		UINT stride, UINT offset)
{
	D3D11_RESOURCE_DIMENSION dim;

	// Process key inputs to allow user to abort long running frame analysis sessions:
	DispatchInputEvents(GetHackerDevice());
	if (!G->analyse_frame)
		return;

	resource->GetType(&dim);

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			if (analyse_options & FrameAnalysisOptions::FMT_BUF_MASK)
				DumpBuffer((ID3D11Buffer*)resource, filename, buf_type_mask, idx, format, stride, offset,
						0, 0, NULL, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED, NULL, NULL, NULL, 0);
			else
				FALogInfo("Skipped dumping Buffer (No buffer formats enabled): %S\n", filename);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			FALogInfo("Skipped dumping Texture1D: %S\n", filename);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if (analyse_options & FrameAnalysisOptions::FMT_2D_MASK) {
				if (analyse_options & FrameAnalysisOptions::STEREO)
					DumpStereoResource((ID3D11Texture2D*)resource, filename, format);
				if (analyse_options & FrameAnalysisOptions::MONO)
					Dump2DResource((ID3D11Texture2D*)resource, filename, false, NULL, format);
			} else
				FALogInfo("Skipped dumping Texture2D (No Texture2D formats enabled): %S\n", filename);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			FALogInfo("Skipped dumping Texture3D: %S\n", filename);
			break;
		default:
			FALogInfo("Skipped dumping resource of unknown type %i: %S\n", dim, filename);
			break;
	}
}

static BOOL CreateDeferredFADirectory(LPCWSTR path)
{
	DWORD err;

	// Deferred contexts don't have an opportunity to create their
	// dumps directory earlier, so do so now:

	if (!CreateDirectoryEnsuringAccess(path)) {
		err = GetLastError();
		if (err != ERROR_ALREADY_EXISTS) {
			LogInfoW(L"Error creating deferred frame analysis directory: %i\n", err);
			return FALSE;
		}
	}

	return TRUE;
}

void FrameAnalysisContext::get_deduped_dir(wchar_t *path, size_t size)
{
	if (analyse_options & FrameAnalysisOptions::SHARE_DEDUPED) {
		if (!GetModuleFileName(migoto_handle, path, (DWORD)size))
			return;
		wcsrchr(path, L'\\')[1] = 0;
		wcscat_s(path, size, L"FrameAnalysisDeduped");
	} else {
		_snwprintf_s(path, size, size, L"%ls\\deduped", G->ANALYSIS_PATH);
	}

	CreateDirectoryEnsuringAccess(path);
}

HRESULT FrameAnalysisContext::FrameAnalysisFilename(wchar_t *filename, size_t size, bool compute,
		wchar_t *reg, char shader_type, int idx, ID3D11Resource *handle)
{
	struct ResourceHashInfo *info;
	uint32_t hash, orig_hash;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);
	if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"ctx-0x%p\\", this);
		if (!CreateDeferredFADirectory(filename))
			return E_FAIL;
	}

	if (!(analyse_options & FrameAnalysisOptions::FILENAME_REG)) {
		// We don't allow hold to be changed mid-frame due to potential
		// for filename conflicts, so use def_analyse_options:
		if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i.", G->analyse_frame_no);
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i-", draw_call);
	}

	if (shader_type)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%cs-", shader_type);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%ls", reg);
	if (idx != -1)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i", idx);

	if (analyse_options & FrameAnalysisOptions::FILENAME_REG) {
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-");
		// We don't allow hold to be changed mid-frame due to potential
		// for filename conflicts, so use def_analyse_options:
		if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i.", G->analyse_frame_no);
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i", draw_call);
	}

	EnterCriticalSectionPretty(&G->mResourcesLock);
	try {
		hash = G->mResources.at(handle).hash;
		orig_hash = G->mResources.at(handle).orig_hash;
	} catch (std::out_of_range) {
		hash = orig_hash = 0;
	}
	LeaveCriticalSection(&G->mResourcesLock);

	if (hash) {
		try {
			info = &G->mResourceInfo.at(orig_hash);
			if (info->hash_contaminated) {
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=!");
				if (!info->map_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"M");
				if (!info->update_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"U");
				if (!info->copy_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"C");
				if (!info->region_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"S");
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"!");
			}
		} catch (std::out_of_range) {}

		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=%08x", hash);

		if (hash != orig_hash)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"(%08x)", orig_hash);
	}
	if (analyse_options & FrameAnalysisOptions::FILENAME_HANDLE)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"@%p", handle);

	if (compute) {
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-cs=%016I64x", mCurrentComputeShader);
	} else {
		if (mCurrentVertexShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-vs=%016I64x", mCurrentVertexShader);
		if (mCurrentHullShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-hs=%016I64x", mCurrentHullShader);
		if (mCurrentDomainShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ds=%016I64x", mCurrentDomainShader);
		if (mCurrentGeometryShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-gs=%016I64x", mCurrentGeometryShader);
		if (mCurrentPixelShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ps=%016I64x", mCurrentPixelShader);
	}

	hr = StringCchPrintfW(pos, rem, L".XXX");
	if (FAILED(hr)) {
		FALogErr("Failed to create filename: 0x%x\n", hr);
		// Could create a shorter filename without hashes if this
		// becomes a problem in practice
	}

	return hr;
}

HRESULT FrameAnalysisContext::FrameAnalysisFilenameResource(wchar_t *filename, size_t size, const wchar_t *type, ID3D11Resource *handle, bool force_filename_handle)
{
	struct ResourceHashInfo *info;
	uint32_t hash, orig_hash;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);
	if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"ctx-0x%p\\", this);
		if (!CreateDeferredFADirectory(filename))
			return E_FAIL;
	}

	// We don't allow hold to be changed mid-frame due to potential
	// for filename conflicts, so use def_analyse_options:
	if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i.", G->analyse_frame_no);
	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i.%i-", draw_call, non_draw_call_dump_counter);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%s", type);

	EnterCriticalSectionPretty(&G->mResourcesLock);
	try {
		hash = G->mResources.at(handle).hash;
		orig_hash = G->mResources.at(handle).orig_hash;
	} catch (std::out_of_range) {
		hash = orig_hash = 0;
	}
	LeaveCriticalSection(&G->mResourcesLock);

	if (hash) {
		try {
			info = &G->mResourceInfo.at(orig_hash);
			if (info->hash_contaminated) {
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=!");
				if (!info->map_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"M");
				if (!info->update_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"U");
				if (!info->copy_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"C");
				if (!info->region_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"S");
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"!");
			}
		} catch (std::out_of_range) {}

		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=%08x", hash);

		if (hash != orig_hash)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"(%08x)", orig_hash);
	}

	// Always do this for update/unmap resource dumps since hashes are likely to clash:
	if (force_filename_handle || (analyse_options & FrameAnalysisOptions::FILENAME_HANDLE))
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"@%p", handle);

	hr = StringCchPrintfW(pos, rem, L".XXX");
	if (FAILED(hr))
		FALogErr("Failed to create filename: 0x%x\n", hr);

	return hr;
}

const wchar_t* FrameAnalysisContext::dedupe_tex2d_filename(ID3D11Texture2D *resource,
		D3D11_TEXTURE2D_DESC *orig_desc, wchar_t *dedupe_filename,
		size_t size, const wchar_t *traditional_filename, DXGI_FORMAT format)
{
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr;
	uint32_t hash;
	wchar_t dedupe_dir[MAX_PATH];

	// Many of the files dumped with frame analysis are identical, and this
	// can take a very long time and waste a lot of disk space to dump them
	// all. We employ a new strategy to minimise this by saving only unique
	// files and hardlinking them into their traditional FA location.
	//
	// This function is responsible for finding a filename that will be
	// unique for the given resource, with its current contents (not the
	// contents it was created with). To do this, we will re-hash the
	// resource now. If there happen to be any hash collisions in a frame
	// analysis dump it will become misleading, as the later resource will
	// point to the earlier resource with the same hash, but I'm not
	// expecting this to be a major problem in practice.
	//
	// We use the original description from the resource prior to being
	// staged to try to get the best chance of matching the 3DMigoto hash
	// for textures that have not been changed on the GPU. There may still
	// be reasons it won't if the description retrieved from DirectX
	// doesn't match the description used to create it (e.g. mip-maps being
	// generated after creation I guess).

	hr = GetDumpingContext()->Map(resource, 0, D3D11_MAP_READ, 0, &map);
	if (FAILED(hr)) {
		FALogErr("Frame Analysis filename deduplication failed to map resource: 0x%x\n", hr);
		goto err;
	};

	// CalcTexture2DDataHash takes a D3D11_SUBRESOURCE_DATA*, which happens
	// to be binary identical to a D3D11_MAPPED_SUBRESOURCE (though it is
	// not an array), so we can safely cast it.
	//
	// Now using CalcTexture2DDataHashAccurate to take the full texture
	// into consideration when generating the hash - necessary as our
	// legacy texture hash is very broken (passable for texture filtering,
	// but not for this) and doesn't hash anywhere near the full image, so
	// changes in the mid to lower half of the image won't affect the hash.
	hash = CalcTexture2DDataHashAccurate(orig_desc, (D3D11_SUBRESOURCE_DATA*)&map);
	hash = CalcTexture2DDescHash(hash, orig_desc);

	GetDumpingContext()->Unmap(resource, 0);

	if (format == DXGI_FORMAT_UNKNOWN)
		format = orig_desc->Format;

	get_deduped_dir(dedupe_dir, MAX_PATH);
	_snwprintf_s(dedupe_filename, size, size, L"%ls\\%08x-%S.XXX", dedupe_dir, hash, TexFormatStr(format));

	return dedupe_filename;
err:
	return traditional_filename;
}

void FrameAnalysisContext::dedupe_buf_filename(ID3D11Buffer *resource,
		D3D11_BUFFER_DESC *orig_desc, D3D11_MAPPED_SUBRESOURCE *map,
		wchar_t *dedupe_filename, size_t size)
{
	wchar_t dedupe_dir[MAX_PATH];
	uint32_t hash;

	// Many of the files dumped with frame analysis are identical, and this
	// can take a very long time and waste a lot of disk space to dump them
	// all. We employ a new strategy to minimise this by saving only unique
	// files and hardlinking them into their traditional FA location.
	//
	// This function is responsible for finding a filename that will be
	// unique for the given resource, with its current contents (not the
	// contents it was created with). To do this, we will re-hash the
	// resource now. If there happen to be any hash collisions in a frame
	// analysis dump it will become misleading, as the later resource will
	// point to the earlier resource with the same hash, but I'm not
	// expecting this to be a major problem in practice.
	//
	// We use the original description from the resource prior to being
	// staged to try to get the best chance of matching the 3DMigoto hash
	// for textures that have not been changed on the GPU. There may still
	// be reasons it won't if the description retrieved from DirectX
	// doesn't match the description used to create it (e.g. unused fields
	// for a given buffer type being zeroed out).

	hash = crc32c_hw(0, map->pData, orig_desc->ByteWidth);
	hash = crc32c_hw(hash, orig_desc, sizeof(D3D11_BUFFER_DESC));

	get_deduped_dir(dedupe_dir, MAX_PATH);
	_snwprintf_s(dedupe_filename, size, size, L"%ls\\%08x.XXX", dedupe_dir, hash);
}

void FrameAnalysisContext::rotate_deduped_file(const wchar_t *dedupe_filename)
{
	wchar_t rotated_filename[MAX_PATH];
	unsigned rotate;
	size_t ext_pos;
	const wchar_t *ext;

	ext = wcsrchr(dedupe_filename, L'.');
	if (ext) {
		ext_pos = ext - dedupe_filename;
	} else {
		ext_pos = wcslen(dedupe_filename);
		ext = L"";
	}

	for (rotate = 1; rotate; rotate++) {
		swprintf_s(rotated_filename, MAX_PATH,
				L"%.*s.%d%s", (int)ext_pos, dedupe_filename, rotate, ext);

		if (GetFileAttributes(rotated_filename) == INVALID_FILE_ATTRIBUTES) {
			// Move the base file to have the rotated filename,
			// then copy it back to the original filename. That
			// way code creating the link doesn't have to know
			// about this rotation, and we only deal with it when
			// we hit the too many hard links error:
			// xxxxxxx.xxx - n hard links
			// xxxxxxx.1.xxx - max 1023 hard links
			// xxxxxxx.2.xxx - max 1023 hard links
			// etc.
			FALogInfo("Max hard links exceeded, rotating deduped file: %S\n", rotated_filename);
			MoveFile(dedupe_filename, rotated_filename);
			CopyFile(rotated_filename, dedupe_filename, TRUE);
			return;
		}
	}
}

void FrameAnalysisContext::rotate_when_nearing_hard_link_limit(const wchar_t *dedupe_filename)
{
	HANDLE f;
	BY_HANDLE_FILE_INFORMATION info;

	// Waiting until we hit the hard link limit before rotating a file can
	// result in certin other applications having difficulty if they
	// perform some operation by using a temporary hard link, such as
	// happens when moving files from one directory to another with cygwin,
	// so this function aims to leave one free link for each file. I'm not
	// sure how to query the actual max hard links available at a given
	// path, so for now this assumes the limit is 1024 (NTFS). We will also
	// keep the error path for rotating when actually hitting the limit to
	// handle any cases where the limit may be lower for some reason.

	f = CreateFile(dedupe_filename, GENERIC_READ, FILE_SHARE_READ |
			FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return;

	if (GetFileInformationByHandle(f, &info)) {
		if (info.nNumberOfLinks >= 1023)
			rotate_deduped_file(dedupe_filename);
	}

	CloseHandle(f);
}

static bool create_shortcut(const wchar_t *filename, const wchar_t *dedupe_filename)
{
	IShellLink *psl;
	IPersistFile *ppf;
	HRESULT hr, dont_care;
	wchar_t lnk_path[MAX_PATH];

	dont_care = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// https://msdn.microsoft.com/en-us/library/aa969393.aspx#Shellink_Creating_Shortcut
	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
	if (SUCCEEDED(hr)) {
		psl->SetPath(dedupe_filename);
		hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
		if (SUCCEEDED(hr)) {
			swprintf_s(lnk_path, MAX_PATH, L"%s.lnk", filename);
			hr = ppf->Save(lnk_path, TRUE);
			ppf->Release();
		}
		psl->Release();
	}

	CoUninitialize();

	return SUCCEEDED(hr);
}

void FrameAnalysisContext::link_deduplicated_files(const wchar_t *filename, const wchar_t *dedupe_filename)
{
	wchar_t relative_path[MAX_PATH] = {0};

	// Bail if source didn't get created:
	if (GetFileAttributes(dedupe_filename) == INVALID_FILE_ATTRIBUTES)
		return;

	// Bail if destination already exists:
	if (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES)
		return;

	if (analyse_options & FrameAnalysisOptions::SYMLINK) {
		if (PathRelativePathTo(relative_path, filename, 0, dedupe_filename, 0)) {
			if (CreateSymbolicLink(filename, relative_path, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
				return;
		}

		// May fail if developer mode is not enabled on Windows 10:
		FALogErr("Symlinking %S -> %S failed (0x%u), trying hard link\n",
				filename, relative_path, GetLastError());
	}

	rotate_when_nearing_hard_link_limit(dedupe_filename);
	if (CreateHardLink(filename, dedupe_filename, NULL))
		return;
	if (GetLastError() == ERROR_TOO_MANY_LINKS) {
		rotate_deduped_file(dedupe_filename);
		if (CreateHardLink(filename, dedupe_filename, NULL))
			return;
	}

	// Hard links may fail e.g. if running the game off a FAT32 partition:
	LogDebug("Hard linking %S -> %S failed (0x%u), trying windows shortcut\n",
			filename, dedupe_filename, GetLastError());

	if (create_shortcut(filename, dedupe_filename))
		return;

	LogDebug("Creating shortcut %S -> %S failed. Cannot deduplicate file, moving back.\n",
			filename, dedupe_filename);

	if (MoveFile(dedupe_filename, filename))
		return;

	FALogErr("All attempts to link deduplicated file failed, giving up: %S -> %S\n",
			filename, dedupe_filename);
}

void FrameAnalysisContext::_DumpCBs(char shader_type, bool compute,
	ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT])
{
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	for (i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT && G->analyse_frame; i++) {
		if (!buffers[i])
			continue;

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"cb", shader_type, i, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpResource(buffers[i], filename,
					FrameAnalysisOptions::DUMP_CB, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
		}

		buffers[i]->Release();
	}
}

void FrameAnalysisContext::_DumpTextures(char shader_type, bool compute,
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT])
{
	ID3D11Resource *resource;
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT && G->analyse_frame; i++) {
		if (!views[i])
			continue;

		if (i == G->StereoParamsReg || i == G->IniParamsReg) {
			FALogInfo("Skipped 3DMigoto resource in slot %cs-t%i\n", shader_type, i);
			continue;
		}

		views[i]->GetResource(&resource);
		if (!resource) {
			views[i]->Release();
			continue;
		}

		views[i]->GetDesc(&view_desc);

		// TODO: process description to get offset, strides & size for
		// buffer & bufferex type SRVs and pass down to dump routines,
		// although I have no idea how to determine which of the
		// entries in the two D3D11_BUFFER_SRV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"t", shader_type, i, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_SRV, i,
					view_desc.Format, 0, 0);
		}

		resource->Release();
		views[i]->Release();
	}
}

void FrameAnalysisContext::DumpCBs(bool compute)
{
	ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	if (compute) {
		if (mCurrentComputeShader) {
			GetPassThroughOrigContext1()->CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('c', compute, buffers);
		}
	} else {
		if (mCurrentVertexShader) {
			GetPassThroughOrigContext1()->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('v', compute, buffers);
		}
		if (mCurrentHullShader) {
			GetPassThroughOrigContext1()->HSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('h', compute, buffers);
		}
		if (mCurrentDomainShader) {
			GetPassThroughOrigContext1()->DSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('d', compute, buffers);
		}
		if (mCurrentGeometryShader) {
			GetPassThroughOrigContext1()->GSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('g', compute, buffers);
		}
		if (mCurrentPixelShader) {
			GetPassThroughOrigContext1()->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('p', compute, buffers);
		}
	}
}

void FrameAnalysisContext::DumpMesh(DrawCallInfo *call_info)
{
	bool dump_ibs = !!(analyse_options & FrameAnalysisOptions::DUMP_IB);
	bool dump_vbs = !!(analyse_options & FrameAnalysisOptions::DUMP_VB);
	ID3D11Buffer *staged_ib = NULL;
	DXGI_FORMAT ib_fmt = DXGI_FORMAT_UNKNOWN;
	UINT ib_off = 0;

	// If we are dumping vertex buffers as text and an indexed draw call
	// was in use, we also need to dump (or at the very least stage) the
	// index buffer so that we can determine the maximum vertex count to
	// dump to keep the text files small. This is not applicable when only
	// dumping vertex buffers as binary, since we always dump the entire
	// buffer in that case.
	if (dump_vbs && (analyse_options & FrameAnalysisOptions::FMT_BUF_TXT) && call_info->IndexCount)
		dump_ibs = true;

	if (dump_ibs)
		DumpIB(call_info, &staged_ib, &ib_fmt, &ib_off);

	if (dump_vbs)
		DumpVBs(call_info, staged_ib, ib_fmt, ib_off);

	if (staged_ib)
		staged_ib->Release();
}

static bool vb_slot_in_layout(int slot, ID3DBlob *layout)
{
	D3D11_INPUT_ELEMENT_DESC *layout_desc = NULL;
	size_t layout_elements;
	UINT i;

	if (!layout)
		return true;

	layout_desc = (D3D11_INPUT_ELEMENT_DESC*)layout->GetBufferPointer();
	layout_elements = layout->GetBufferSize() / sizeof(D3D11_INPUT_ELEMENT_DESC);

	for (i = 0; i < layout_elements; i++)
		if (layout_desc[i].InputSlot == slot)
			return true;

	return false;
}

void FrameAnalysisContext::DumpVBs(DrawCallInfo *call_info, ID3D11Buffer *staged_ib, DXGI_FORMAT ib_fmt, UINT ib_off)
{
	ID3D11Buffer *buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i, first = 0, count = 0;
	ID3D11InputLayout *layout = NULL;
	ID3DBlob *layout_desc = NULL;

	if (call_info) {
		first = call_info->FirstVertex;
		count = call_info->VertexCount;
	}

	// The format of each vertex buffer cannot be obtained from this call.
	// Rather, it is available in the input layout assigned to the
	// pipeline, and there is no API to get the layout description, so we
	// store it in a blob attached to the layout when it was created that
	// we retrieve here.

	GetPassThroughOrigContext1()->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, buffers, strides, offsets);
	GetPassThroughOrigContext1()->IAGetInputLayout(&layout);
	GetPassThroughOrigContext1()->IAGetPrimitiveTopology(&topology);
	if (layout) {
		UINT size = sizeof(ID3DBlob*);
		layout->GetPrivateData(InputLayoutDescGuid, &size, &layout_desc);
		layout->Release();
	}

	for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (!buffers[i])
			continue;

		// Skip this vertex buffer if it is not used in the IA layout:
		if (!vb_slot_in_layout(i, layout_desc))
			goto continue_release;

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"vb", NULL, i, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpBuffer(buffers[i], filename,
				FrameAnalysisOptions::DUMP_VB, i,
				ib_fmt, strides[i], offsets[i],
				first, count, layout_desc, topology,
				call_info, NULL, staged_ib, ib_off);
		}

continue_release:
		buffers[i]->Release();
	}

	// Although the documentation fails to mention it, GetPrivateData()
	// does bump the refcount if SetPrivateDataInterface() was used, so we
	// need to balance it here:
	if (layout_desc)
		layout_desc->Release();
}

void FrameAnalysisContext::DumpIB(DrawCallInfo *call_info, ID3D11Buffer **staged_ib, DXGI_FORMAT *format, UINT *offset)
{
	ID3D11Buffer *buffer = NULL;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT first = 0, count = 0;
	D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	if (call_info) {
		first = call_info->FirstIndex;
		count = call_info->IndexCount;
	}

	GetPassThroughOrigContext1()->IAGetIndexBuffer(&buffer, format, offset);
	if (!buffer)
		return;
	GetPassThroughOrigContext1()->IAGetPrimitiveTopology(&topology);

	hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"ib", NULL, -1, buffer);
	if (SUCCEEDED(hr)) {
		DumpBuffer(buffer, filename,
				FrameAnalysisOptions::DUMP_IB, -1,
				*format, 0, *offset, first, count, NULL,
				topology, call_info, staged_ib, NULL, 0);
	}

	buffer->Release();
}

void FrameAnalysisContext::DumpTextures(bool compute)
{
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

	if (compute) {
		if (mCurrentComputeShader) {
			GetPassThroughOrigContext1()->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('c', compute, views);
		}
	} else {
		if (mCurrentVertexShader) {
			GetPassThroughOrigContext1()->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('v', compute, views);
		}
		if (mCurrentHullShader) {
			GetPassThroughOrigContext1()->HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('h', compute, views);
		}
		if (mCurrentDomainShader) {
			GetPassThroughOrigContext1()->DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('d', compute, views);
		}
		if (mCurrentGeometryShader) {
			GetPassThroughOrigContext1()->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('g', compute, views);
		}
		if (mCurrentPixelShader) {
			GetPassThroughOrigContext1()->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('p', compute, views);
		}
	}
}

void FrameAnalysisContext::DumpRenderTargets()
{
	UINT i;
	ID3D11RenderTargetView *rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11Resource *resource;
	D3D11_RENDER_TARGET_VIEW_DESC view_desc;
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	GetPassThroughOrigContext1()->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, NULL);

	for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && G->analyse_frame; ++i) {
		if (!rtvs[i])
			continue;

		rtvs[i]->GetResource(&resource);
		if (!resource) {
			rtvs[i]->Release();
			continue;
		}

		rtvs[i]->GetDesc(&view_desc);

		// TODO: process description to get offset, strides & size for
		// buffer type RTVs and pass down to dump routines, although I
		// have no idea how to determine which of the entries in the
		// two D3D11_BUFFER_RTV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"o", NULL, i, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_RT, i,
					view_desc.Format, 0, 0);
		}

		resource->Release();
		rtvs[i]->Release();
	}
}

void FrameAnalysisContext::DumpDepthStencilTargets()
{
	ID3D11DepthStencilView *dsv = NULL;
	ID3D11Resource *resource = NULL;
	D3D11_DEPTH_STENCIL_VIEW_DESC view_desc;
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	GetPassThroughOrigContext1()->OMGetRenderTargets(0, NULL, &dsv);
	if (!dsv)
		return;

	dsv->GetResource(&resource);
	if (!resource) {
		dsv->Release();
		return;
	}

	dsv->GetDesc(&view_desc);

	hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"oD", NULL, -1, resource);
	if (SUCCEEDED(hr)) {
		DumpResource(resource, filename, FrameAnalysisOptions::DUMP_DEPTH,
				-1, view_desc.Format, 0, 0);
	}

	resource->Release();
	dsv->Release();
}

void FrameAnalysisContext::DumpUAVs(bool compute)
{
	UINT i;
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	ID3D11Resource *resource;
	D3D11_UNORDERED_ACCESS_VIEW_DESC view_desc;
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	if (compute)
		GetPassThroughOrigContext1()->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);
	else
		GetPassThroughOrigContext1()->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);

	for (i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT && G->analyse_frame; ++i) {
		if (!uavs[i])
			continue;

		uavs[i]->GetResource(&resource);
		if (!resource) {
			uavs[i]->Release();
			continue;
		}

		uavs[i]->GetDesc(&view_desc);

		// TODO: process description to get offset & size for buffer
		// type UAVs and pass down to dump routines.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"u", NULL, i, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_RT, i,
					view_desc.Format, 0, 0);
		}

		resource->Release();
		uavs[i]->Release();
	}
}

void FrameAnalysisContext::FrameAnalysisClearRT(ID3D11RenderTargetView *target)
{
	FLOAT colour[4] = {0,0,0,0};
	ID3D11Resource *resource = NULL;

	// FIXME: Do this before each draw call instead of when render targets
	// are assigned to fix assigned render targets not being cleared, and
	// work better with frame analysis triggers

	if (!(G->cur_analyse_options & FrameAnalysisOptions::CLEAR_RT))
		return;

	// Use the address of the resource rather than the view to determine if
	// we have seen it before so we don't clear a render target that is
	// simply used as several different types of views:
	target->GetResource(&resource);
	if (!resource)
		return;
	resource->Release(); // Don't need the object, only the address

	if (G->frame_analysis_seen_rts.count(resource))
		return;
	G->frame_analysis_seen_rts.insert(resource);

	GetPassThroughOrigContext1()->ClearRenderTargetView(target, colour);
}

void FrameAnalysisContext::FrameAnalysisClearUAV(ID3D11UnorderedAccessView *uav)
{
	UINT values[4] = {0,0,0,0};
	ID3D11Resource *resource = NULL;

	// FIXME: Do this before each draw/dispatch call instead of when UAVs
	// are assigned to fix assigned render targets not being cleared, and
	// work better with frame analysis triggers

	if (!(G->cur_analyse_options & FrameAnalysisOptions::CLEAR_RT))
		return;

	// Use the address of the resource rather than the view to determine if
	// we have seen it before so we don't clear a render target that is
	// simply used as several different types of views:
	uav->GetResource(&resource);
	if (!resource)
		return;
	resource->Release(); // Don't need the object, only the address

	if (G->frame_analysis_seen_rts.count(resource))
		return;
	G->frame_analysis_seen_rts.insert(resource);

	GetPassThroughOrigContext1()->ClearUnorderedAccessViewUint(uav, values);
}

void FrameAnalysisContext::FrameAnalysisTrigger(FrameAnalysisOptions new_options)
{
	if (new_options & FrameAnalysisOptions::PERSIST) {
		G->cur_analyse_options = new_options;
	} else {
		if (oneshot_valid)
			oneshot_analyse_options |= new_options;
		else
			oneshot_analyse_options = new_options;
		oneshot_valid = true;
	}
}

void FrameAnalysisContext::update_per_draw_analyse_options()
{
	analyse_options = G->cur_analyse_options;

	// Log whenever new persistent options take effect, but only once:
	if (G->cur_analyse_options & FrameAnalysisOptions::PERSIST) {
		FALogInfo("analyse_options (persistent): %08x\n", G->cur_analyse_options);
		G->cur_analyse_options &= (FrameAnalysisOptions)~FrameAnalysisOptions::PERSIST;
	}

	if (!oneshot_valid)
		return;

	FALogInfo("analyse_options (one-shot): %08x\n", oneshot_analyse_options);

	analyse_options = oneshot_analyse_options;
	oneshot_analyse_options = FrameAnalysisOptions::INVALID;
	oneshot_valid = false;
}

void FrameAnalysisContext::update_stereo_dumping_mode()
{
	NvU8 stereo = false;

	NvAPIOverride();
	Profiling::NvAPI_Stereo_IsEnabled(&stereo);
	if (stereo)
		Profiling::NvAPI_Stereo_IsActivated(GetHackerDevice()->mStereoHandle, &stereo);

	if (!stereo) {
		// 3D Vision is disabled, force mono dumping mode:
		analyse_options &= (FrameAnalysisOptions)~FrameAnalysisOptions::STEREO_MASK;
		analyse_options |= FrameAnalysisOptions::MONO;
		return;
	}

	// If neither stereo or mono specified, default to stereo:
	if (!(analyse_options & FrameAnalysisOptions::STEREO_MASK))
		analyse_options |= FrameAnalysisOptions::STEREO;
}

void FrameAnalysisContext::set_default_dump_formats(bool draw)
{
	// Textures: default to .jps when possible, .bin otherwise:
	if (!(analyse_options & FrameAnalysisOptions::FMT_2D_MASK))
		analyse_options |= FrameAnalysisOptions::FMT_2D_AUTO;

	if (!(analyse_options & FrameAnalysisOptions::FMT_BUF_MASK)) {
		if (draw) {
			// If we are dumping specific buffer binds slots default to
			// txt, otherwise buffers aren't dumped by default:
			if (analyse_options & FrameAnalysisOptions::DUMP_XB_MASK)
				analyse_options |= FrameAnalysisOptions::FMT_BUF_TXT;
		} else {
			// Command list dump, or dump_on_update/unmap - we always want
			// to dump both textures and buffers. For buffers we default to
			// both binary and text for now:
			analyse_options |= FrameAnalysisOptions::FMT_BUF_TXT;
			analyse_options |= FrameAnalysisOptions::FMT_BUF_BIN;
		}
	}
}

void FrameAnalysisContext::FrameAnalysisAfterDraw(bool compute, DrawCallInfo *call_info)
{
	NvAPI_Status nvret;

	update_per_draw_analyse_options();

	// Update: We now have an option to allow analysis on deferred
	// contexts, because it can still be useful to dump some types of
	// resources in these cases. Render and depth targets will be pretty
	// useless, and their use in texture slots will be similarly useless,
	// but textures that come from the CPU, constant buffers, vertex
	// buffers, etc that aren't changed on the GPU can still be useful.
	//
	// Later we might want to think about ways we could analyse render
	// targets & UAVs in deferred contexts - a simple approach would be to
	// dump out the back buffer after executing a command list in the
	// immediate context, however this would only show the combined result
	// of all the draw calls from the deferred context, and not the results
	// of the individual draw operations.
	//
	// Another more in-depth approach would be to create the stereo
	// resources now and issue the reverse blits, then dump them all after
	// executing the command list. Note that the NVAPI call is not
	// per-context and therefore may have threading issues, and it's not
	// clear if it would have to be enabled while submitting the copy
	// commands in the deferred context, or while playing the command queue
	// in the immediate context, or both.
	if (!(analyse_options & FrameAnalysisOptions::DEFRD_CTX_MASK) &&
	   (GetPassThroughOrigContext1()->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		draw_call++;
		return;
	}

	update_stereo_dumping_mode();
	set_default_dump_formats(true);

	if ((analyse_options & FrameAnalysisOptions::FMT_2D_MASK) &&
	    (analyse_options & FrameAnalysisOptions::STEREO) &&
	    (GetDumpingContext()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			FALogErr("DumpStereoResource failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	// Grab the critical section now as we may need it several times during
	// dumping for mResources
	EnterCriticalSectionPretty(&G->mCriticalSection);

	if (analyse_options & FrameAnalysisOptions::DUMP_CB)
		DumpCBs(compute);

	if (!compute)
		DumpMesh(call_info);

	if (analyse_options & FrameAnalysisOptions::DUMP_SRV)
		DumpTextures(compute);

	if (analyse_options & FrameAnalysisOptions::DUMP_RT) {
		if (!compute)
			DumpRenderTargets();

		// UAVs can be used by both pixel shaders and compute shaders:
		DumpUAVs(compute);
	}

	if (analyse_options & FrameAnalysisOptions::DUMP_DEPTH && !compute)
		DumpDepthStencilTargets();

	LeaveCriticalSection(&G->mCriticalSection);

	if ((analyse_options & FrameAnalysisOptions::FMT_2D_MASK) &&
	    (analyse_options & FrameAnalysisOptions::STEREO) &&
	    (GetDumpingContext()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		Profiling::NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, false);
	}

	draw_call++;
}

void FrameAnalysisContext::_FrameAnalysisAfterUpdate(ID3D11Resource *resource,
		FrameAnalysisOptions type_mask, wchar_t *type)
{
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	analyse_options = G->cur_analyse_options;

	if (!(analyse_options & type_mask))
		return;

	if (!(analyse_options & FrameAnalysisOptions::DEFRD_CTX_MASK) &&
	   (GetPassThroughOrigContext1()->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		FALogInfo("WARNING: dump_on_%S used on deferred context, but no deferred_ctx options enabled\n", type);
		non_draw_call_dump_counter++;
		return;
	}

	// Don't bother trying to dump as stereo - Map/Unmap/Update are inherently mono
	analyse_options &= (FrameAnalysisOptions)~FrameAnalysisOptions::STEREO_MASK;
	analyse_options |= FrameAnalysisOptions::MONO;

	set_default_dump_formats(false);

	EnterCriticalSectionPretty(&G->mCriticalSection);

	// We don't have a view at this point to get a fully typed format, so
	// we leave format as DXGI_FORMAT_UNKNOWN, which will use the format
	// from the resource description.

	hr = FrameAnalysisFilenameResource(filename, MAX_PATH, type, resource, true);
	if (SUCCEEDED(hr)) {
		DumpResource(resource, filename, analyse_options, -1, DXGI_FORMAT_UNKNOWN, 0, 0);
	}

	LeaveCriticalSection(&G->mCriticalSection);

	non_draw_call_dump_counter++;
}

void FrameAnalysisContext::FrameAnalysisAfterUnmap(ID3D11Resource *resource)
{
	_FrameAnalysisAfterUpdate(resource, FrameAnalysisOptions::DUMP_ON_UNMAP, L"unmap");
}

void FrameAnalysisContext::FrameAnalysisAfterUpdate(ID3D11Resource *resource)
{
	_FrameAnalysisAfterUpdate(resource, FrameAnalysisOptions::DUMP_ON_UPDATE, L"update");
}

void FrameAnalysisContext::FrameAnalysisDump(ID3D11Resource *resource, FrameAnalysisOptions options,
		const wchar_t *target, DXGI_FORMAT format, UINT stride, UINT offset)
{
	wchar_t filename[MAX_PATH];
	NvAPI_Status nvret;
	HRESULT hr;

	analyse_options = options;

	if (!(analyse_options & FrameAnalysisOptions::DEFRD_CTX_MASK) &&
	   (GetPassThroughOrigContext1()->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		// If the dump command is used, the user probably expects it to
		// just work, so turn on dumping from deferred contexts. This
		// generally should be fine since the dump command is only used
		// for specific resources and so we are likely to be able to
		// fit them all in memory. We can't afford the same to
		// analyse_options though.
		analyse_options |= FrameAnalysisOptions::DEFRD_CTX_DELAY;
	}

	update_stereo_dumping_mode();
	set_default_dump_formats(false);

	if ((analyse_options & FrameAnalysisOptions::STEREO) &&
	    (GetDumpingContext()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			FALogErr("FrameAnalyisDump failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	EnterCriticalSectionPretty(&G->mCriticalSection);

	hr = FrameAnalysisFilenameResource(filename, MAX_PATH, target, resource, false);
	if (FAILED(hr)) {
		// If the ini section and resource name makes the filename too
		// long, try again without them:
		hr = FrameAnalysisFilenameResource(filename, MAX_PATH, L"...", resource, false);
	}
	if (SUCCEEDED(hr))
		DumpResource(resource, filename, analyse_options, -1, format, stride, offset);

	LeaveCriticalSection(&G->mCriticalSection);

	if ((analyse_options & FrameAnalysisOptions::STEREO) &&
	    (GetDumpingContext()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		Profiling::NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, false);
	}

	non_draw_call_dump_counter++;
}

// -----------------------------------------------------------------------------------------------

ULONG STDMETHODCALLTYPE FrameAnalysisContext::AddRef(void)
{
	return HackerContext::AddRef();
}

STDMETHODIMP_(ULONG) FrameAnalysisContext::Release(THIS)
{
	return HackerContext::Release();
}

HRESULT STDMETHODCALLTYPE FrameAnalysisContext::QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if (ppvObject && IsEqualIID(riid, IID_FrameAnalysisContext)) {
		// This is a special case - only 3DMigoto itself should know
		// this IID, so this is us checking if it has a FrameAnalysisContext.
		// There's no need to call through to DX for this one.
		AddRef();
		*ppvObject = this;
		return S_OK;
	}

	return HackerContext::QueryInterface(riid, ppvObject);
}

STDMETHODIMP_(void) FrameAnalysisContext::GetDevice(THIS_
		/* [annotation] */
		__out  ID3D11Device **ppDevice)
{
	return HackerContext::GetDevice(ppDevice);
}

STDMETHODIMP FrameAnalysisContext::GetPrivateData(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt(*pDataSize)  void *pData)
{
	return HackerContext::GetPrivateData(guid, pDataSize, pData);
}

STDMETHODIMP FrameAnalysisContext::SetPrivateData(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt(DataSize)  const void *pData)
{
	return HackerContext::SetPrivateData(guid, DataSize, pData);
}

STDMETHODIMP FrameAnalysisContext::SetPrivateDataInterface(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData)
{
	return HackerContext::SetPrivateDataInterface(guid, pData);
}

STDMETHODIMP_(void) FrameAnalysisContext::VSSetConstantBuffers(THIS_
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

	HackerContext::VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP FrameAnalysisContext::Map(THIS_
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
	FrameAnalysisLogNoNL("Map(pResource:0x%p, Subresource:%u, MapType:%u, MapFlags:%u, pMappedResource:0x%p)",
			pResource, Subresource, MapType, MapFlags, pMappedResource);
	FrameAnalysisLogResourceHash(pResource);

	return HackerContext::Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
}

STDMETHODIMP_(void) FrameAnalysisContext::Unmap(THIS_
		/* [annotation] */
		__in ID3D11Resource *pResource,
		/* [annotation] */
		__in  UINT Subresource)
{
	FrameAnalysisLogNoNL("Unmap(pResource:0x%p, Subresource:%u)",
			pResource, Subresource);
	FrameAnalysisLogResourceHash(pResource);

	HackerContext::Unmap(pResource, Subresource);

	if (G->analyse_frame)
		FrameAnalysisAfterUnmap(pResource);
}

STDMETHODIMP_(void) FrameAnalysisContext::PSSetConstantBuffers(THIS_
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

	HackerContext::PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::IASetInputLayout(THIS_
		/* [annotation] */
		__in_opt ID3D11InputLayout *pInputLayout)
{
	FrameAnalysisLog("IASetInputLayout(pInputLayout:0x%p)\n",
			pInputLayout);

	HackerContext::IASetInputLayout(pInputLayout);
}

STDMETHODIMP_(void) FrameAnalysisContext::IASetVertexBuffers(THIS_
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

	HackerContext::IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSSetConstantBuffers(THIS_
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

	HackerContext::GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSSetShader(THIS_
		/* [annotation] */
		__in_opt ID3D11GeometryShader *pShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	HackerContext::GSSetShader(pShader, ppClassInstances, NumClassInstances);

	FrameAnalysisLogNoNL("GSSetShader(pShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u)",
			pShader, ppClassInstances, NumClassInstances);
	FrameAnalysisLogShaderHash<ID3D11GeometryShader>(pShader);
}

STDMETHODIMP_(void) FrameAnalysisContext::IASetPrimitiveTopology(THIS_
		/* [annotation] */
		__in D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	FrameAnalysisLog("IASetPrimitiveTopology(Topology:%u)\n",
			Topology);

	HackerContext::IASetPrimitiveTopology(Topology);
}

STDMETHODIMP_(void) FrameAnalysisContext::VSSetSamplers(THIS_
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

	HackerContext::VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::PSSetSamplers(THIS_
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

	HackerContext::PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::Begin(THIS_
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync)
{
	FrameAnalysisLogNoNL("Begin(pAsync:0x%p)", pAsync);
	FrameAnalysisLogAsyncQuery(pAsync);

	HackerContext::Begin(pAsync);
}

STDMETHODIMP_(void) FrameAnalysisContext::End(THIS_
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync)
{
	FrameAnalysisLogNoNL("End(pAsync:0x%p)", pAsync);
	FrameAnalysisLogAsyncQuery(pAsync);

	HackerContext::End(pAsync);
}

STDMETHODIMP FrameAnalysisContext::GetData(THIS_
		/* [annotation] */
		__in  ID3D11Asynchronous *pAsync,
		/* [annotation] */
		__out_bcount_opt(DataSize)  void *pData,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in  UINT GetDataFlags)
{
	HRESULT ret = HackerContext::GetData(pAsync, pData, DataSize, GetDataFlags);

	FrameAnalysisLogNoNL("GetData(pAsync:0x%p, pData:0x%p, DataSize:%u, GetDataFlags:%u) = %u",
			pAsync, pData, DataSize, GetDataFlags, ret);
	FrameAnalysisLogAsyncQuery(pAsync);
	if (SUCCEEDED(ret))
		FrameAnalysisLogData(pData, DataSize);

	return ret;
}

STDMETHODIMP_(void) FrameAnalysisContext::SetPredication(THIS_
		/* [annotation] */
		__in_opt ID3D11Predicate *pPredicate,
		/* [annotation] */
		__in  BOOL PredicateValue)
{
	FrameAnalysisLogNoNL("SetPredication(pPredicate:0x%p, PredicateValue:%s)",
			pPredicate, PredicateValue ? "true" : "false");
	FrameAnalysisLogAsyncQuery(pPredicate);

	return HackerContext::SetPredication(pPredicate, PredicateValue);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSSetShaderResources(THIS_
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

	HackerContext::GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSSetSamplers(THIS_
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

	HackerContext::GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMSetBlendState(THIS_
		/* [annotation] */
		__in_opt  ID3D11BlendState *pBlendState,
		/* [annotation] */
		__in_opt  const FLOAT BlendFactor[4],
		/* [annotation] */
		__in  UINT SampleMask)
{
	FrameAnalysisLog("OMSetBlendState(pBlendState:0x%p, BlendFactor:0x%p, SampleMask:%u)\n",
			pBlendState, BlendFactor, SampleMask); // Beware dereferencing optional BlendFactor

	HackerContext::OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMSetDepthStencilState(THIS_
		/* [annotation] */
		__in_opt  ID3D11DepthStencilState *pDepthStencilState,
		/* [annotation] */
		__in  UINT StencilRef)
{
	FrameAnalysisLog("OMSetDepthStencilState(pDepthStencilState:0x%p, StencilRef:%u)\n",
			pDepthStencilState, StencilRef);

	HackerContext::OMSetDepthStencilState(pDepthStencilState, StencilRef);
}

STDMETHODIMP_(void) FrameAnalysisContext::SOSetTargets(THIS_
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

	HackerContext::SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}

STDMETHODIMP_(void) FrameAnalysisContext::Dispatch(THIS_
		/* [annotation] */
		__in  UINT ThreadGroupCountX,
		/* [annotation] */
		__in  UINT ThreadGroupCountY,
		/* [annotation] */
		__in  UINT ThreadGroupCountZ)
{
	FrameAnalysisLog("Dispatch(ThreadGroupCountX:%u, ThreadGroupCountY:%u, ThreadGroupCountZ:%u)\n",
			ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	HackerContext::Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if (G->analyse_frame)
		FrameAnalysisAfterDraw(true, NULL);
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::DispatchIndirect(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	FrameAnalysisLogNoNL("DispatchIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)",
			pBufferForArgs, AlignedByteOffsetForArgs);
	FrameAnalysisLogResourceHash(pBufferForArgs);

	HackerContext::DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);

	if (G->analyse_frame)
		FrameAnalysisAfterDraw(true, NULL);
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::RSSetState(THIS_
		/* [annotation] */
		__in_opt  ID3D11RasterizerState *pRasterizerState)
{
	FrameAnalysisLog("RSSetState(pRasterizerState:0x%p)\n",
			pRasterizerState);

	HackerContext::RSSetState(pRasterizerState);
}

STDMETHODIMP_(void) FrameAnalysisContext::RSSetViewports(THIS_
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		/* [annotation] */
		__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports)
{
	FrameAnalysisLog("RSSetViewports(NumViewports:%u, pViewports:0x%p)\n",
			NumViewports, pViewports);

	HackerContext::RSSetViewports(NumViewports, pViewports);
}

STDMETHODIMP_(void) FrameAnalysisContext::RSSetScissorRects(THIS_
		/* [annotation] */
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		/* [annotation] */
		__in_ecount_opt(NumRects)  const D3D11_RECT *pRects)
{
	FrameAnalysisLog("RSSetScissorRects(NumRects:%u, pRects:0x%p)\n",
			NumRects, pRects);

	HackerContext::RSSetScissorRects(NumRects, pRects);
}

STDMETHODIMP_(void) FrameAnalysisContext::CopySubresourceRegion(THIS_
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
	FrameAnalysisLog("CopySubresourceRegion(pDstResource:0x%p, DstSubresource:%u, DstX:%u, DstY:%u, DstZ:%u, pSrcResource:0x%p, SrcSubresource:%u, pSrcBox:0x%p)\n",
			pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
	FrameAnalysisLogResource(-1, "Src", pSrcResource);
	FrameAnalysisLogResource(-1, "Dst", pDstResource);

	HackerContext::CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
			pSrcResource, SrcSubresource, pSrcBox);
}

STDMETHODIMP_(void) FrameAnalysisContext::CopyResource(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */
		__in  ID3D11Resource *pSrcResource)
{
	FrameAnalysisLog("CopyResource(pDstResource:0x%p, pSrcResource:0x%p)\n",
			pDstResource, pSrcResource);
	FrameAnalysisLogResource(-1, "Src", pSrcResource);
	FrameAnalysisLogResource(-1, "Dst", pDstResource);

	HackerContext::CopyResource(pDstResource, pSrcResource);
}

STDMETHODIMP_(void) FrameAnalysisContext::UpdateSubresource(THIS_
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

	HackerContext::UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
			SrcDepthPitch);

	if (G->analyse_frame)
		FrameAnalysisAfterUpdate(pDstResource);
}

STDMETHODIMP_(void) FrameAnalysisContext::CopyStructureCount(THIS_
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

	HackerContext::CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}

STDMETHODIMP_(void) FrameAnalysisContext::ClearUnorderedAccessViewUint(THIS_
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const UINT Values[4])
{
	FrameAnalysisLog("ClearUnorderedAccessViewUint(pUnorderedAccessView:0x%p, Values:0x%p)\n",
			pUnorderedAccessView, Values);
	FrameAnalysisLogView(-1, "", pUnorderedAccessView);

	HackerContext::ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
}

STDMETHODIMP_(void) FrameAnalysisContext::ClearUnorderedAccessViewFloat(THIS_
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const FLOAT Values[4])
{
	FrameAnalysisLog("ClearUnorderedAccessViewFloat(pUnorderedAccessView:0x%p, Values:0x%p)\n",
			pUnorderedAccessView, Values);
	FrameAnalysisLogView(-1, "", pUnorderedAccessView);

	HackerContext::ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
}

STDMETHODIMP_(void) FrameAnalysisContext::ClearDepthStencilView(THIS_
		/* [annotation] */
		__in  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */
		__in  UINT ClearFlags,
		/* [annotation] */
		__in  FLOAT Depth,
		/* [annotation] */
		__in  UINT8 Stencil)
{
	FrameAnalysisLog("ClearDepthStencilView(pDepthStencilView:0x%p, ClearFlags:%u, Depth:%f, Stencil:%u)\n",
			pDepthStencilView, ClearFlags, Depth, Stencil);
	FrameAnalysisLogView(-1, "", pDepthStencilView);

	HackerContext::ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}

STDMETHODIMP_(void) FrameAnalysisContext::GenerateMips(THIS_
		/* [annotation] */
		__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	FrameAnalysisLog("GenerateMips(pShaderResourceView:0x%p)\n",
			pShaderResourceView);
	FrameAnalysisLogView(-1, "", pShaderResourceView);

	HackerContext::GenerateMips(pShaderResourceView);
}

STDMETHODIMP_(void) FrameAnalysisContext::SetResourceMinLOD(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		FLOAT MinLOD)
{
	FrameAnalysisLogNoNL("SetResourceMinLOD(pResource:0x%p)",
			pResource);
	FrameAnalysisLogResourceHash(pResource);

	HackerContext::SetResourceMinLOD(pResource, MinLOD);
}

STDMETHODIMP_(FLOAT) FrameAnalysisContext::GetResourceMinLOD(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource)
{
	FLOAT ret = HackerContext::GetResourceMinLOD(pResource);

	FrameAnalysisLogNoNL("GetResourceMinLOD(pResource:0x%p) = %f",
			pResource, ret);
	FrameAnalysisLogResourceHash(pResource);
	return ret;
}

STDMETHODIMP_(void) FrameAnalysisContext::ResolveSubresource(THIS_
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

	HackerContext::ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

STDMETHODIMP_(void) FrameAnalysisContext::ExecuteCommandList(THIS_
		/* [annotation] */
		__in  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState)
{
	NvAPI_Status nvret;

	FrameAnalysisLog("ExecuteCommandList(pCommandList:0x%p, RestoreContextState:%s)\n",
			pCommandList, RestoreContextState ? "true" : "false");

	if (G->analyse_frame) {
		// Reverse stereo blit only applies to the immediate context - to work
		// on a deferred context it must be enabled on the immediate context
		// when the command list is executed. We don't know what options may
		// have been used during the dump, so enable it unconditionally.
		nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			FALogErr("FrameAnalyisDump failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	HackerContext::ExecuteCommandList(pCommandList, RestoreContextState);

	if (G->analyse_frame)
		Profiling::NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, false);

	dump_deferred_resources(pCommandList);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSSetShaderResources(THIS_
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

	HackerContext::HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSSetShader(THIS_
		/* [annotation] */
		__in_opt  ID3D11HullShader *pHullShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	FrameAnalysisLogNoNL("HSSetShader(pHullShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u)",
			pHullShader, ppClassInstances, NumClassInstances);
	FrameAnalysisLogShaderHash<ID3D11HullShader>(pHullShader);

	HackerContext::HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSSetSamplers(THIS_
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

	HackerContext::HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSSetConstantBuffers(THIS_
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

	HackerContext::HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSSetShaderResources(THIS_
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

	HackerContext::DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSSetShader(THIS_
		/* [annotation] */
		__in_opt  ID3D11DomainShader *pDomainShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	FrameAnalysisLogNoNL("DSSetShader(pDomainShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u)",
			pDomainShader, ppClassInstances, NumClassInstances);
	FrameAnalysisLogShaderHash<ID3D11DomainShader>(pDomainShader);

	HackerContext::DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSSetSamplers(THIS_
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

	HackerContext::DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSSetConstantBuffers(THIS_
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

	HackerContext::DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSSetShaderResources(THIS_
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

	HackerContext::CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSSetUnorderedAccessViews(THIS_
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

	HackerContext::CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);

	if (G->analyse_frame && ppUnorderedAccessViews) {
		for (UINT i = 0; i < NumUAVs; ++i) {
			if (ppUnorderedAccessViews[i])
				FrameAnalysisClearUAV(ppUnorderedAccessViews[i]);
		}
	}
}

STDMETHODIMP_(void) FrameAnalysisContext::CSSetShader(THIS_
		/* [annotation] */
		__in_opt  ID3D11ComputeShader *pComputeShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	FrameAnalysisLogNoNL("CSSetShader(pComputeShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u)",
			pComputeShader, ppClassInstances, NumClassInstances);
	FrameAnalysisLogShaderHash<ID3D11ComputeShader>(pComputeShader);

	HackerContext::CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSSetSamplers(THIS_
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

	HackerContext::CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSSetConstantBuffers(THIS_
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

	HackerContext::CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::VSGetConstantBuffers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HackerContext::VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("VSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::PSGetShaderResources(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HackerContext::PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("PSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::PSGetShader(THIS_
		/* [annotation] */
		__out  ID3D11PixelShader **ppPixelShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HackerContext::PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLogNoNL("PSGetShader(ppPixelShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p)",
			ppPixelShader, ppClassInstances, pNumClassInstances);
	if (ppPixelShader)
		FrameAnalysisLogShaderHash<ID3D11PixelShader>(*ppPixelShader);
	else
		FrameAnalysisLog("\n");
}

STDMETHODIMP_(void) FrameAnalysisContext::PSGetSamplers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HackerContext::PSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("PSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::VSGetShader(THIS_
		/* [annotation] */
		__out  ID3D11VertexShader **ppVertexShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HackerContext::VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLogNoNL("VSGetShader(ppVertexShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p)",
			ppVertexShader, ppClassInstances, pNumClassInstances);
	if (ppVertexShader)
		FrameAnalysisLogShaderHash<ID3D11VertexShader>(*ppVertexShader);
	else
		FrameAnalysisLog("\n");
}

STDMETHODIMP_(void) FrameAnalysisContext::PSGetConstantBuffers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HackerContext::PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("PSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::IAGetInputLayout(THIS_
		/* [annotation] */
		__out  ID3D11InputLayout **ppInputLayout)
{
	HackerContext::IAGetInputLayout(ppInputLayout);

	FrameAnalysisLog("IAGetInputLayout(ppInputLayout:0x%p)\n",
			ppInputLayout);
}

STDMETHODIMP_(void) FrameAnalysisContext::IAGetVertexBuffers(THIS_
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
	HackerContext::IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

	FrameAnalysisLog("IAGetVertexBuffers(StartSlot:%u, NumBuffers:%u, ppVertexBuffers:0x%p, pStrides:0x%p, pOffsets:0x%p)\n",
			StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppVertexBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::IAGetIndexBuffer(THIS_
		/* [annotation] */
		__out_opt  ID3D11Buffer **pIndexBuffer,
		/* [annotation] */
		__out_opt  DXGI_FORMAT *Format,
		/* [annotation] */
		__out_opt  UINT *Offset)
{
	HackerContext::IAGetIndexBuffer(pIndexBuffer, Format, Offset);

	FrameAnalysisLog("IAGetIndexBuffer(pIndexBuffer:0x%p, Format:0x%p, Offset:0x%p)\n",
			pIndexBuffer, Format, Offset);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSGetConstantBuffers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HackerContext::GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("GSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSGetShader(THIS_
		/* [annotation] */
		__out  ID3D11GeometryShader **ppGeometryShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HackerContext::GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLogNoNL("GSGetShader(ppGeometryShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p)",
			ppGeometryShader, ppClassInstances, pNumClassInstances);
	if (ppGeometryShader)
		FrameAnalysisLogShaderHash<ID3D11GeometryShader>(*ppGeometryShader);
	else
		FrameAnalysisLog("\n");
}

STDMETHODIMP_(void) FrameAnalysisContext::IAGetPrimitiveTopology(THIS_
		/* [annotation] */
		__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	HackerContext::IAGetPrimitiveTopology(pTopology);

	FrameAnalysisLog("IAGetPrimitiveTopology(pTopology:0x%p)\n",
			pTopology);
}

STDMETHODIMP_(void) FrameAnalysisContext::VSGetShaderResources(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HackerContext::VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("VSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::VSGetSamplers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HackerContext::VSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("VSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::GetPredication(THIS_
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate,
		/* [annotation] */
		__out_opt  BOOL *pPredicateValue)
{
	HackerContext::GetPredication(ppPredicate, pPredicateValue);

	FrameAnalysisLogNoNL("GetPredication(ppPredicate:0x%p, pPredicateValue:0x%p)",
			ppPredicate, pPredicateValue);
	FrameAnalysisLogAsyncQuery(ppPredicate ? *ppPredicate : NULL);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSGetShaderResources(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HackerContext::GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("GSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::GSGetSamplers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HackerContext::GSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("GSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMGetRenderTargets(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		/* [annotation] */
		__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	HackerContext::OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);

	FrameAnalysisLog("OMGetRenderTargets(NumViews:%u, ppRenderTargetViews:0x%p, ppDepthStencilView:0x%p)\n",
			NumViews, ppRenderTargetViews, ppDepthStencilView);
	FrameAnalysisLogViewArray(0, NumViews, (ID3D11View *const *)ppRenderTargetViews);
	if (ppDepthStencilView)
		FrameAnalysisLogView(-1, "D", *ppDepthStencilView);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMGetRenderTargetsAndUnorderedAccessViews(THIS_
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
	HackerContext::OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView,
			UAVStartSlot, NumUAVs, ppUnorderedAccessViews);

	FrameAnalysisLog("OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs:%i, ppRenderTargetViews:0x%p, ppDepthStencilView:0x%p, UAVStartSlot:%i, NumUAVs:%u, ppUnorderedAccessViews:0x%p)\n",
			NumRTVs, ppRenderTargetViews, ppDepthStencilView,
			UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
	FrameAnalysisLogViewArray(0, NumRTVs, (ID3D11View *const *)ppRenderTargetViews);
	if (ppDepthStencilView)
		FrameAnalysisLogView(-1, "D", *ppDepthStencilView);
	FrameAnalysisLogViewArray(UAVStartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMGetBlendState(THIS_
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState,
		/* [annotation] */
		__out_opt  FLOAT BlendFactor[4],
		/* [annotation] */
		__out_opt  UINT *pSampleMask)
{
	HackerContext::OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);

	FrameAnalysisLog("OMGetBlendState(ppBlendState:0x%p, BlendFactor:0x%p, pSampleMask:0x%p)\n",
			ppBlendState, BlendFactor, pSampleMask);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMGetDepthStencilState(THIS_
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
		/* [annotation] */
		__out_opt  UINT *pStencilRef)
{
	HackerContext::OMGetDepthStencilState(ppDepthStencilState, pStencilRef);

	FrameAnalysisLog("OMGetDepthStencilState(ppDepthStencilState:0x%p, pStencilRef:0x%p)\n",
			ppDepthStencilState, pStencilRef);
}

STDMETHODIMP_(void) FrameAnalysisContext::SOGetTargets(THIS_
		/* [annotation] */
		__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets)
{
	HackerContext::SOGetTargets(NumBuffers, ppSOTargets);

	FrameAnalysisLog("SOGetTargets(NumBuffers:%u, ppSOTargets:0x%p)\n",
			NumBuffers, ppSOTargets);
	FrameAnalysisLogResourceArray(0, NumBuffers, (ID3D11Resource *const *)ppSOTargets);
}

STDMETHODIMP_(void) FrameAnalysisContext::RSGetState(THIS_
		/* [annotation] */
		__out  ID3D11RasterizerState **ppRasterizerState)
{
	HackerContext::RSGetState(ppRasterizerState);

	FrameAnalysisLog("RSGetState(ppRasterizerState:0x%p)\n",
			ppRasterizerState);
}

STDMETHODIMP_(void) FrameAnalysisContext::RSGetViewports(THIS_
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		/* [annotation] */
		__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports)
{
	HackerContext::RSGetViewports(pNumViewports, pViewports);

	FrameAnalysisLog("RSGetViewports(pNumViewports:0x%p, pViewports:0x%p)\n",
			pNumViewports, pViewports);
}

STDMETHODIMP_(void) FrameAnalysisContext::RSGetScissorRects(THIS_
		/* [annotation] */
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		/* [annotation] */
		__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects)
{
	HackerContext::RSGetScissorRects(pNumRects, pRects);

	FrameAnalysisLog("RSGetScissorRects(pNumRects:0x%p, pRects:0x%p)\n",
			pNumRects, pRects);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSGetShaderResources(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HackerContext::HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("HSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSGetShader(THIS_
		/* [annotation] */
		__out  ID3D11HullShader **ppHullShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HackerContext::HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLogNoNL("HSGetShader(ppHullShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p)",
			ppHullShader, ppClassInstances, pNumClassInstances);
	if (ppHullShader)
		FrameAnalysisLogShaderHash<ID3D11HullShader>(*ppHullShader);
	else
		FrameAnalysisLog("\n");
}

STDMETHODIMP_(void) FrameAnalysisContext::HSGetSamplers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HackerContext::HSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("HSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::HSGetConstantBuffers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HackerContext::HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("HSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSGetShaderResources(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HackerContext::DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("DSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSGetShader(THIS_
		/* [annotation] */
		__out  ID3D11DomainShader **ppDomainShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HackerContext::DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLogNoNL("DSGetShader(ppDomainShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p)",
			ppDomainShader, ppClassInstances, pNumClassInstances);
	if (ppDomainShader)
		FrameAnalysisLogShaderHash<ID3D11DomainShader>(*ppDomainShader);
	else
		FrameAnalysisLog("\n");
}

STDMETHODIMP_(void) FrameAnalysisContext::DSGetSamplers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HackerContext::DSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("DSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::DSGetConstantBuffers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HackerContext::DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("DSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSGetShaderResources(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		/* [annotation] */
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews)
{
	HackerContext::CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	FrameAnalysisLog("CSGetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSGetUnorderedAccessViews(THIS_
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
		/* [annotation] */
		__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	HackerContext::CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);

	FrameAnalysisLog("CSGetUnorderedAccessViews(StartSlot:%u, NumUAVs:%u, ppUnorderedAccessViews:0x%p)\n",
			StartSlot, NumUAVs, ppUnorderedAccessViews);
	FrameAnalysisLogViewArray(StartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSGetShader(THIS_
		/* [annotation] */
		__out  ID3D11ComputeShader **ppComputeShader,
		/* [annotation] */
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */
		__inout_opt  UINT *pNumClassInstances)
{
	HackerContext::CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);

	FrameAnalysisLogNoNL("CSGetShader(ppComputeShader:0x%p, ppClassInstances:0x%p, pNumClassInstances:0x%p)",
			ppComputeShader, ppClassInstances, pNumClassInstances);
	if (ppComputeShader)
		FrameAnalysisLogShaderHash<ID3D11ComputeShader>(*ppComputeShader);
	else
		FrameAnalysisLog("\n");
}

STDMETHODIMP_(void) FrameAnalysisContext::CSGetSamplers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		/* [annotation] */
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers)
{
	HackerContext::CSGetSamplers(StartSlot, NumSamplers, ppSamplers);

	FrameAnalysisLog("CSGetSamplers(StartSlot:%u, NumSamplers:%u, ppSamplers:0x%p)\n",
			StartSlot, NumSamplers, ppSamplers);
	FrameAnalysisLogMiscArray(StartSlot, NumSamplers, (void *const *)ppSamplers);
}

STDMETHODIMP_(void) FrameAnalysisContext::CSGetConstantBuffers(THIS_
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		/* [annotation] */
		__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		/* [annotation] */
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers)
{
	HackerContext::CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

	FrameAnalysisLog("CSGetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

STDMETHODIMP_(void) FrameAnalysisContext::ClearState(THIS)
{
	FrameAnalysisLog("ClearState()\n");

	HackerContext::ClearState();
}

STDMETHODIMP_(void) FrameAnalysisContext::Flush(THIS)
{
	FrameAnalysisLog("Flush()\n");

	HackerContext::Flush();
}

STDMETHODIMP_(D3D11_DEVICE_CONTEXT_TYPE) FrameAnalysisContext::GetType(THIS)
{
	D3D11_DEVICE_CONTEXT_TYPE ret = HackerContext::GetType();

	FrameAnalysisLog("GetType() = %u\n", ret);
	return ret;
}

STDMETHODIMP_(UINT) FrameAnalysisContext::GetContextFlags(THIS)
{
	UINT ret = HackerContext::GetContextFlags();

	FrameAnalysisLog("GetContextFlags() = %u\n", ret);
	return ret;
}

STDMETHODIMP FrameAnalysisContext::FinishCommandList(THIS_
		BOOL RestoreDeferredContextState,
		/* [annotation] */
		__out_opt  ID3D11CommandList **ppCommandList)
{
	HRESULT ret = HackerContext::FinishCommandList(RestoreDeferredContextState, ppCommandList);

	FrameAnalysisLog("FinishCommandList(ppCommandList:0x%p -> 0x%p) = %u\n", ppCommandList, ppCommandList ? *ppCommandList : NULL, ret);

	if (ppCommandList)
		finish_deferred_resources(*ppCommandList);

	return ret;
}

STDMETHODIMP_(void) FrameAnalysisContext::VSSetShader(THIS_
		/* [annotation] */
		__in_opt ID3D11VertexShader *pVertexShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	FrameAnalysisLogNoNL("VSSetShader(pVertexShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u)",
			pVertexShader, ppClassInstances, NumClassInstances);
	FrameAnalysisLogShaderHash<ID3D11VertexShader>(pVertexShader);

	HackerContext::VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) FrameAnalysisContext::PSSetShaderResources(THIS_
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

	HackerContext::PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::PSSetShader(THIS_
		/* [annotation] */
		__in_opt ID3D11PixelShader *pPixelShader,
		/* [annotation] */
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances)
{
	FrameAnalysisLogNoNL("PSSetShader(pPixelShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u)",
			pPixelShader, ppClassInstances, NumClassInstances);
	FrameAnalysisLogShaderHash<ID3D11PixelShader>(pPixelShader);

	HackerContext::PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) FrameAnalysisContext::DrawIndexed(THIS_
		/* [annotation] */
		__in  UINT IndexCount,
		/* [annotation] */
		__in  UINT StartIndexLocation,
		/* [annotation] */
		__in  INT BaseVertexLocation)
{
	FrameAnalysisLog("DrawIndexed(IndexCount:%u, StartIndexLocation:%u, BaseVertexLocation:%u)\n",
			IndexCount, StartIndexLocation, BaseVertexLocation);

	HackerContext::DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawIndexed, 0, IndexCount, 0, BaseVertexLocation, StartIndexLocation, 0, NULL, 0);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::Draw(THIS_
		/* [annotation] */
		__in  UINT VertexCount,
		/* [annotation] */
		__in  UINT StartVertexLocation)
{
	FrameAnalysisLog("Draw(VertexCount:%u, StartVertexLocation:%u)\n",
			VertexCount, StartVertexLocation);

	HackerContext::Draw(VertexCount, StartVertexLocation);

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::Draw, VertexCount, 0, 0, StartVertexLocation, 0, 0, NULL, 0);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::IASetIndexBuffer(THIS_
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

	HackerContext::IASetIndexBuffer(pIndexBuffer, Format, Offset);
}

STDMETHODIMP_(void) FrameAnalysisContext::DrawIndexedInstanced(THIS_
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
	FrameAnalysisLog("DrawIndexedInstanced(IndexCountPerInstance:%u, InstanceCount:%u, StartIndexLocation:%u, BaseVertexLocation:%i, StartInstanceLocation:%u)\n",
			IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

	HackerContext::DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
			BaseVertexLocation, StartInstanceLocation);

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawIndexedInstanced, 0, IndexCountPerInstance, InstanceCount, BaseVertexLocation, StartIndexLocation, StartInstanceLocation, NULL, 0);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::DrawInstanced(THIS_
		/* [annotation] */
		__in  UINT VertexCountPerInstance,
		/* [annotation] */
		__in  UINT InstanceCount,
		/* [annotation] */
		__in  UINT StartVertexLocation,
		/* [annotation] */
		__in  UINT StartInstanceLocation)
{
	FrameAnalysisLog("DrawInstanced(VertexCountPerInstance:%u, InstanceCount:%u, StartVertexLocation:%u, StartInstanceLocation:%u)\n",
			VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

	HackerContext::DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawInstanced, VertexCountPerInstance, 0, InstanceCount, StartVertexLocation, 0, StartInstanceLocation, NULL, 0);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::VSSetShaderResources(THIS_
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

	HackerContext::VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) FrameAnalysisContext::OMSetRenderTargets(THIS_
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

	HackerContext::OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);

	if (G->analyse_frame && ppRenderTargetViews) {
		for (UINT i = 0; i < NumViews; ++i) {
			if (ppRenderTargetViews[i])
				FrameAnalysisClearRT(ppRenderTargetViews[i]);
		}
	}
}

STDMETHODIMP_(void) FrameAnalysisContext::OMSetRenderTargetsAndUnorderedAccessViews(THIS_
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

	HackerContext::OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView,
			UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);

	if (G->analyse_frame) {
		if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
			if (ppRenderTargetViews) {
				for (UINT i = 0; i < NumRTVs; ++i) {
					if (ppRenderTargetViews[i])
						FrameAnalysisClearRT(ppRenderTargetViews[i]);
				}
			}
		}

		if (ppUnorderedAccessViews && (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)) {
			for (UINT i = 0; i < NumUAVs; ++i) {
				if (ppUnorderedAccessViews[i])
					FrameAnalysisClearUAV(ppUnorderedAccessViews[i]);
			}
		}
	}
}

STDMETHODIMP_(void) FrameAnalysisContext::DrawAuto(THIS)
{
	FrameAnalysisLog("DrawAuto()\n");

	HackerContext::DrawAuto();

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawAuto, 0, 0, 0, 0, 0, 0, NULL, 0);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::DrawIndexedInstancedIndirect(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	FrameAnalysisLogNoNL("DrawIndexedInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)",
			pBufferForArgs, AlignedByteOffsetForArgs);
	FrameAnalysisLogResourceHash(pBufferForArgs);

	HackerContext::DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawIndexedInstancedIndirect, 0, 0, 0, 0, 0, 0, &pBufferForArgs, AlignedByteOffsetForArgs);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::DrawInstancedIndirect(THIS_
		/* [annotation] */
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */
		__in  UINT AlignedByteOffsetForArgs)
{
	FrameAnalysisLogNoNL("DrawInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)",
			pBufferForArgs, AlignedByteOffsetForArgs);
	FrameAnalysisLogResourceHash(pBufferForArgs);

	HackerContext::DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);

	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawInstancedIndirect, 0, 0, 0, 0, 0, 0, &pBufferForArgs, AlignedByteOffsetForArgs);
		FrameAnalysisAfterDraw(false, &call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
}

STDMETHODIMP_(void) FrameAnalysisContext::ClearRenderTargetView(THIS_
		/* [annotation] */
		__in  ID3D11RenderTargetView *pRenderTargetView,
		/* [annotation] */
		__in  const FLOAT ColorRGBA[4])
{
	FrameAnalysisLog("ClearRenderTargetView(pRenderTargetView:0x%p, ColorRGBA:0x%p)\n",
			pRenderTargetView, ColorRGBA);
	FrameAnalysisLogView(-1, "", pRenderTargetView);

	HackerContext::ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}

void STDMETHODCALLTYPE FrameAnalysisContext::CopySubresourceRegion1(
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

	HackerContext::CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
}

void STDMETHODCALLTYPE FrameAnalysisContext::UpdateSubresource1(
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

	HackerContext::UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
}

void STDMETHODCALLTYPE FrameAnalysisContext::DiscardResource(
		/* [annotation] */
		_In_  ID3D11Resource *pResource)
{
	FrameAnalysisLogNoNL("DiscardResource(pResource:0x%p)",
			pResource);
	FrameAnalysisLogResourceHash(pResource);

	HackerContext::DiscardResource(pResource);
}

void STDMETHODCALLTYPE FrameAnalysisContext::DiscardView(
		/* [annotation] */
		_In_  ID3D11View *pResourceView)
{
	FrameAnalysisLog("DiscardView(pResourceView:0x%p)\n",
			pResourceView);
	FrameAnalysisLogView(-1, "", pResourceView);

	HackerContext::DiscardView(pResourceView);
}

void STDMETHODCALLTYPE FrameAnalysisContext::VSSetConstantBuffers1(
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

	HackerContext::VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE FrameAnalysisContext::HSSetConstantBuffers1(
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

	HackerContext::HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE FrameAnalysisContext::DSSetConstantBuffers1(
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

	HackerContext::DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE FrameAnalysisContext::GSSetConstantBuffers1(
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

	HackerContext::GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE FrameAnalysisContext::PSSetConstantBuffers1(
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

	HackerContext::PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE FrameAnalysisContext::CSSetConstantBuffers1(
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

	HackerContext::CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

void STDMETHODCALLTYPE FrameAnalysisContext::VSGetConstantBuffers1(
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
	HackerContext::VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("VSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

void STDMETHODCALLTYPE FrameAnalysisContext::HSGetConstantBuffers1(
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
	HackerContext::HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("HSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

void STDMETHODCALLTYPE FrameAnalysisContext::DSGetConstantBuffers1(
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
	HackerContext::DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("DSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

void STDMETHODCALLTYPE FrameAnalysisContext::GSGetConstantBuffers1(
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
	HackerContext::GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("GSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

void STDMETHODCALLTYPE FrameAnalysisContext::PSGetConstantBuffers1(
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
	HackerContext::PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("PSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

void STDMETHODCALLTYPE FrameAnalysisContext::CSGetConstantBuffers1(
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
	HackerContext::CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);

	FrameAnalysisLog("CSGetConstantBuffers1(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p, pFirstConstant:0x%p, pNumConstants:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);
}

void STDMETHODCALLTYPE FrameAnalysisContext::SwapDeviceContextState(
		/* [annotation] */
		_In_  ID3DDeviceContextState *pState,
		/* [annotation] */
		_Out_opt_  ID3DDeviceContextState **ppPreviousState)
{
	FrameAnalysisLog("SwapDeviceContextState(pState:0x%p, ppPreviousState:0x%p)\n",
			pState, ppPreviousState);

	HackerContext::SwapDeviceContextState(pState, ppPreviousState);
}

void STDMETHODCALLTYPE FrameAnalysisContext::ClearView(
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
	FrameAnalysisLogView(-1, "", pView);

	HackerContext::ClearView(pView, Color, pRect, NumRects);
}

void STDMETHODCALLTYPE FrameAnalysisContext::DiscardView1(
		/* [annotation] */
		_In_  ID3D11View *pResourceView,
		/* [annotation] */
		_In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
		UINT NumRects)
{
	FrameAnalysisLog("DiscardView1(pResourceView:0x%p, pRects:0x%p)\n",
			pResourceView, pRects);
	FrameAnalysisLogView(-1, "", pResourceView);

	HackerContext::DiscardView1(pResourceView, pRects, NumRects);
}
