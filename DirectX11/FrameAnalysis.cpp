#include "D3D11Wrapper.h"
#include "FrameAnalysis.h"
#include "Globals.h"

#include <ScreenGrab.h>
#include <wincodec.h>
#include <Strsafe.h>
#include <stdarg.h>
#include <Shlwapi.h>

// For windows shortcuts:
#include <shobjidl.h>
#include <shlguid.h>

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
	LogInfo("Frame Analysis: " fmt, __VA_ARGS__); \
	FrameAnalysisLog(fmt, __VA_ARGS__); \
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
	UINT64 hash;

	// Always complete the line in the debug log:
	LogDebug("\n");

	if (!G->analyse_frame || !frame_analysis_log)
		return;

	if (!shader) {
		fprintf(frame_analysis_log, "\n");
		return;
	}

	EnterCriticalSection(&G->mCriticalSection);

	try {
		hash = G->mShaders.at(shader);
		if (hash)
			fprintf(frame_analysis_log, " hash=%016llx", hash);
	} catch (std::out_of_range) {
	}

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

	EnterCriticalSection(&G->mCriticalSection);

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

ID3D11DeviceContext* FrameAnalysisContext::GetImmediateContext()
{
	if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		return GetPassThroughOrigContext1();

	// XXX Experimental deferred context support: We need to use
	// the immediate context to stage a resource back to the CPU.
	// This may not be thread safe - if it proves problematic we
	// may have to look into delaying this until the deferred
	// context command queue is submitted to the immediate context.
	return GetHackerDevice()->GetPassThroughOrigContext1();
}

void FrameAnalysisContext::Dump2DResource(ID3D11Texture2D *resource, wchar_t
		*filename, bool stereo, D3D11_TEXTURE2D_DESC *orig_desc)
{
	HRESULT hr = S_OK, dont_care;
	wchar_t dedupe_filename[MAX_PATH], *save_filename;
	wchar_t *wic_ext = (stereo ? L".jps" : L".jpg");
	wchar_t *ext, *save_ext;
	ID3D11Texture2D *staging = resource;
	D3D11_TEXTURE2D_DESC staging_desc, *desc = orig_desc;

	// In order to de-dupe Texture2D resources, we need to compare their
	// contents before dumping them to a file, so we copy them into a
	// staging resource (if we did not already do so earlier for the
	// reverse stereo blit). DirectXTK will notice this has been done and
	// skip doing it again.
	resource->GetDesc(&staging_desc);
	if ((staging_desc.Usage != D3D11_USAGE_STAGING) || !(staging_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)) {
		hr = StageResource(resource, &staging_desc, &staging);
		if (FAILED(hr))
			return;
	}

	if (!orig_desc)
		desc = &staging_desc;

	save_filename = dedupe_tex2d_filename(staging, desc, dedupe_filename, MAX_PATH, filename);

	ext = wcsrchr(filename, L'.');
	save_ext = wcsrchr(save_filename, L'.');
	if (!ext || !save_ext) {
		FALogInfo("Dump2DResource: Filename missing extension\n");
		goto out_release;
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
		wcscpy_s(ext, MAX_PATH + filename - ext, wic_ext);
		wcscpy_s(save_ext, MAX_PATH + save_filename - save_ext, wic_ext);

		hr = S_OK;
		if (GetFileAttributes(save_filename) == INVALID_FILE_ATTRIBUTES)
			hr = DirectX::SaveWICTextureToFile(GetImmediateContext(), staging, GUID_ContainerFormatJpeg, save_filename);
		link_deduplicated_files(filename, save_filename);
	}


	if ((analyse_options & FrameAnalysisOptions::FMT_2D_DDS) ||
	   ((analyse_options & FrameAnalysisOptions::FMT_2D_AUTO) && FAILED(hr))) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".dds");
		wcscpy_s(save_ext, MAX_PATH + save_filename - save_ext, L".dds");

		hr = S_OK;
		if (GetFileAttributes(save_filename) == INVALID_FILE_ATTRIBUTES)
			hr = DirectX::SaveDDSTextureToFile(GetImmediateContext(), staging, save_filename);
		link_deduplicated_files(filename, save_filename);
	}

	if (FAILED(hr))
		FALogInfo("Failed to dump Texture2D %S -> %S: 0x%x\n", filename, save_filename, hr);

	if (analyse_options & FrameAnalysisOptions::FMT_DESC) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".dsc");
		wcscpy_s(save_ext, MAX_PATH + save_filename - save_ext, L".dsc");

		if (GetFileAttributes(save_filename) == INVALID_FILE_ATTRIBUTES)
			DumpDesc(desc, save_filename);
		link_deduplicated_files(filename, save_filename);
	}

out_release:
	if (staging != resource)
		staging->Release();
}

HRESULT FrameAnalysisContext::CreateStagingResource(ID3D11Texture2D **resource,
		D3D11_TEXTURE2D_DESC desc, bool stereo, bool msaa)
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
		NvAPI_Stereo_GetSurfaceCreationMode(GetHackerDevice()->mStereoHandle, &orig_mode);
		NvAPI_Stereo_SetSurfaceCreationMode(GetHackerDevice()->mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
	}

	hr = GetHackerDevice()->GetPassThroughOrigDevice1()->CreateTexture2D(&desc, NULL, resource);

	if (analyse_options & FrameAnalysisOptions::STEREO)
		NvAPI_Stereo_SetSurfaceCreationMode(GetHackerDevice()->mStereoHandle, orig_mode);

	return hr;
}

HRESULT FrameAnalysisContext::ResolveMSAA(ID3D11Texture2D *src,
		D3D11_TEXTURE2D_DESC *srcDesc, ID3D11Texture2D **dst)
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
	hr = CreateStagingResource(&resolved, *srcDesc, false, true);
	if (FAILED(hr)) {
		FALogInfo("ResolveMSAA failed to create intermediate texture: 0x%x\n", hr);
		return hr;
	}

	DXGI_FORMAT fmt = EnsureNotTypeless(srcDesc->Format);
	UINT support = 0;

	hr = GetHackerDevice()->GetPassThroughOrigDevice1()->CheckFormatSupport( fmt, &support );
	if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
		FALogInfo("ResolveMSAA cannot resolve MSAA format %d\n", fmt);
		goto err_release;
	}

	for (item = 0; item < srcDesc->ArraySize; item++) {
		for (level = 0; level < srcDesc->MipLevels; level++) {
			index = D3D11CalcSubresource(level, item, max(srcDesc->MipLevels, 1));
			GetImmediateContext()->ResolveSubresource(resolved, index, src, index, fmt);
		}
	}

	*dst = resolved;
	return S_OK;

err_release:
	resolved->Release();
	return hr;
}

HRESULT FrameAnalysisContext::StageResource(ID3D11Texture2D *src,
		D3D11_TEXTURE2D_DESC *srcDesc, ID3D11Texture2D **dst)
{
	ID3D11Texture2D *staging = NULL;
	ID3D11Texture2D *resolved = NULL;
	HRESULT hr;

	*dst = NULL;

	hr = CreateStagingResource(&staging, *srcDesc, false, false);
	if (FAILED(hr)) {
		FALogInfo("StageResource failed to create intermediate texture: 0x%x\n", hr);
		return hr;
	}

	hr = ResolveMSAA(src, srcDesc, &resolved);
	if (FAILED(hr))
		goto err_release;
	if (resolved)
		src = resolved;

	GetImmediateContext()->CopyResource(staging, src);

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
void FrameAnalysisContext::DumpStereoResource(ID3D11Texture2D *resource, wchar_t *filename)
{
	ID3D11Texture2D *stereoResource = NULL;
	ID3D11Texture2D *tmpResource = NULL;
	ID3D11Texture2D *src = resource;
	D3D11_TEXTURE2D_DESC srcDesc;
	D3D11_BOX srcBox;
	HRESULT hr;
	UINT item, level, index, width, height;

	resource->GetDesc(&srcDesc);

	hr = CreateStagingResource(&stereoResource, srcDesc, true, false);
	if (FAILED(hr)) {
		FALogInfo("DumpStereoResource failed to create stereo texture: 0x%x\n", hr);
		return;
	}

	if ((srcDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) ||
	    (srcDesc.SampleDesc.Count > 1)) {
		// Reverse stereo blit won't work on these surfaces directly
		// since CopySubresourceRegion() will fail if the source and
		// destination dimensions don't match, so use yet another
		// intermediate staging resource first.
		hr = StageResource(src, &srcDesc, &tmpResource);
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

	Dump2DResource(stereoResource, filename, true, &srcDesc);

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
		FALogInfo("Failed to create buffer filename\n");
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
		FALogInfo("Unable to create %S: %u\n", filename, err);
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

void FrameAnalysisContext::dedupe_buf_filename_vb_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, int idx, UINT stride,
		UINT offset, UINT first, UINT count)
{
	wchar_t *pos;
	size_t rem;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-vb%i", idx);

	if (offset)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-offset=%u", offset);

	if (stride)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-stride=%u", stride);

	if (first)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-first=%u", first);

	if (count)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-count=%u", count);

	if (FAILED(StringCchPrintfW(pos, rem, L".txt")))
		FALogInfo("Failed to create vertex buffer filename\n");
}

/*
 * Dumps the vertex buffer in several formats.
 * FIXME: We should wrap the input layout object to get the correct format (and
 * other info like the semantic).
 */
void FrameAnalysisContext::DumpVBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, int idx, UINT stride, UINT offset, UINT first, UINT count)
{
	FILE *fd = NULL;
	float *buff = (float*)map->pData;
	int *buf32 = (int*)map->pData;
	char *buf8 = (char*)map->pData;
	UINT i, j, start, end, buf_idx;
	errno_t err;

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogInfo("Unable to create %S: %u\n", filename, err);
		return;
	}

	if (offset)
		fprintf(fd, "byte offset: %u\n", offset);
	fprintf(fd, "stride: %u\n", stride);
	if (first || count) {
		fprintf(fd, "first vertex: %u\n", first);
		fprintf(fd, "vertex count: %u\n", count);
	}
	if (!stride) {
		FALogInfo("Cannot dump vertex buffer with stride=0\n");
		return;
	}

	start = offset / stride + first;
	end = size / stride;
	if (count)
		end = min(end, start + count);

	// FIXME: For vertex buffers we should wrap the input layout object to
	// get the format (and other info like the semantic).

	for (i = start; i < end; i++) {
		fprintf(fd, "\n");
		for (j = 0; j < stride / 4; j++) {
			buf_idx = i * stride / 4 + j;
			fprintf(fd, "vb%i[%d]+%03d: 0x%08x %.9g\n", idx, i - start, j*4, buf32[buf_idx], buff[buf_idx]);
		}
		// In case we find one that is not a 32bit multiple finish off one byte at a time:
		for (j = j * 4; j < stride; j++) {
			buf_idx = i * stride + j;
			fprintf(fd, "vb%i[%d]+%03d: 0x%02x\n", idx, i - start, j, buf8[buf_idx]);
		}
	}

	fclose(fd);
}

void FrameAnalysisContext::dedupe_buf_filename_ib_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, DXGI_FORMAT ib_fmt,
		UINT offset, UINT first, UINT count)
{
	wchar_t *pos;
	size_t rem;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ib");

	if (ib_fmt != DXGI_FORMAT_UNKNOWN)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-format=%S", TexFormatStr(ib_fmt));

	if (offset)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-offset=%u", offset);

	if (first)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-first=%u", first);

	if (count)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-count=%u", count);

	if (FAILED(StringCchPrintfW(pos, rem, L".txt")))
		FALogInfo("Failed to create index buffer filename\n");
}

void FrameAnalysisContext::DumpIBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, DXGI_FORMAT format, UINT offset, UINT first, UINT count)
{
	FILE *fd = NULL;
	short *buf16 = (short*)map->pData;
	int *buf32 = (int*)map->pData;
	UINT start, end, i;
	errno_t err;

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogInfo("Unable to create %S: %u\n", filename, err);
		return;
	}

	fprintf(fd, "byte offset: %u\n", offset);
	if (first || count) {
		fprintf(fd, "first index: %u\n", first);
		fprintf(fd, "index count: %u\n", count);
	}

	switch(format) {
	case DXGI_FORMAT_R16_UINT:
		fprintf(fd, "format: DXGI_FORMAT_R16_UINT\n");

		start = offset / 2 + first;
		end = size / 2;
		if (count)
			end = min(end, start + count);

		for (i = start; i < end; i++)
			fprintf(fd, "%u\n", buf16[i]);
		break;
	case DXGI_FORMAT_R32_UINT:
		fprintf(fd, "format: DXGI_FORMAT_R32_UINT\n");

		start = offset / 4 + first;
		end = size / 4;
		if (count)
			end = min(end, start + count);

		for (i = start; i < end; i++)
			fprintf(fd, "%u\n", buf32[i]);
		break;
	default:
		// Illegal format for an index buffer
		fprintf(fd, "format %u is illegal\n", format);
		break;
	}

	fclose(fd);
}

template <typename DescType>
void FrameAnalysisContext::DumpDesc(DescType *desc, wchar_t *filename)
{
	FILE *fd = NULL;
	char buf[256];
	errno_t err;

	StrResourceDesc(buf, 256, desc);

	err = wfopen_ensuring_access(&fd, filename, L"w");
	if (!fd) {
		FALogInfo("Unable to create %S: %u\n", filename, err);
		return;
	}
	fwrite(buf, 1, strlen(buf), fd);
	putc('\n', fd);
	fclose(fd);
}

void FrameAnalysisContext::DumpBuffer(ID3D11Buffer *buffer, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset, UINT first, UINT count)
{
	wchar_t bin_filename[MAX_PATH], txt_filename[MAX_PATH];
	D3D11_BUFFER_DESC desc, orig_desc;
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Buffer *staging = NULL;
	HRESULT hr;
	FILE *fd = NULL;
	wchar_t *ext, *bin_ext;
	errno_t err;

	buffer->GetDesc(&desc);
	memcpy(&orig_desc, &desc, sizeof(D3D11_BUFFER_DESC));

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	hr = GetHackerDevice()->GetPassThroughOrigDevice1()->CreateBuffer(&desc, NULL, &staging);
	if (FAILED(hr)) {
		FALogInfo("DumpBuffer failed to create staging buffer: 0x%x\n", hr);
		return;
	}

	GetImmediateContext()->CopyResource(staging, buffer);
	hr = GetImmediateContext()->Map(staging, 0, D3D11_MAP_READ, 0, &map);
	if (FAILED(hr)) {
		FALogInfo("DumpBuffer failed to map staging resource: 0x%x\n", hr);
		goto out_release;
	}

	dedupe_buf_filename(staging, &orig_desc, &map, bin_filename, MAX_PATH);

	ext = wcsrchr(filename, L'.');
	bin_ext = wcsrchr(bin_filename, L'.');
	if (!ext || !bin_ext) {
		FALogInfo("DumpBuffer: Filename missing extension\n");
		goto out_unmap;
	}

	if (analyse_options & FrameAnalysisOptions::FMT_BUF_BIN) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".buf");
		wcscpy_s(bin_ext, MAX_PATH + bin_filename - bin_ext, L".buf");

		if (GetFileAttributes(bin_filename) == INVALID_FILE_ATTRIBUTES) {
			err = wfopen_ensuring_access(&fd, bin_filename, L"wb");
			if (!fd) {
				FALogInfo("Unable to create %S: %u\n", bin_filename, err);
				goto out_unmap;
			}
			fwrite(map.pData, 1, desc.ByteWidth, fd);
			fclose(fd);
		}
		link_deduplicated_files(filename, bin_filename);
	}

	if (analyse_options & FrameAnalysisOptions::FMT_BUF_TXT) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".txt");

		if (buf_type_mask & FrameAnalysisOptions::DUMP_CB) {
			dedupe_buf_filename_txt(bin_filename, txt_filename, MAX_PATH, 'c', idx, stride, offset);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpBufferTxt(txt_filename, &map, desc.ByteWidth, 'c', idx, stride, offset);
			}
		} else if (buf_type_mask & FrameAnalysisOptions::DUMP_VB) {
			dedupe_buf_filename_vb_txt(bin_filename, txt_filename, MAX_PATH, idx, stride, offset, first, count);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpVBTxt(txt_filename, &map, desc.ByteWidth, idx, stride, offset, first, count);
			}
		} else if (buf_type_mask & FrameAnalysisOptions::DUMP_IB) {
			dedupe_buf_filename_ib_txt(bin_filename, txt_filename, MAX_PATH, ib_fmt, offset, first, count);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpIBTxt(txt_filename, &map, desc.ByteWidth, ib_fmt, offset, first, count);
			}
		} else {
			// We don't know what kind of buffer this is, so just
			// use the generic dump routine:

			dedupe_buf_filename_txt(bin_filename, txt_filename, MAX_PATH, '?', idx, stride, offset);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpBufferTxt(txt_filename, &map, desc.ByteWidth, '?', idx, stride, offset);
			}
		}
		link_deduplicated_files(filename, txt_filename);
	}
	// TODO: Dump UAV, RT and SRV buffers as text taking their format,
	// offset, size, first entry and num entries into account.

	if (analyse_options & FrameAnalysisOptions::FMT_DESC) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".dsc");
		wcscpy_s(bin_ext, MAX_PATH + bin_filename - bin_ext, L".dsc");

		if (GetFileAttributes(bin_filename) == INVALID_FILE_ATTRIBUTES)
			DumpDesc(&orig_desc, bin_filename);
		link_deduplicated_files(filename, bin_filename);
	}

out_unmap:
	GetImmediateContext()->Unmap(staging, 0);
out_release:
	staging->Release();
}

void FrameAnalysisContext::DumpResource(ID3D11Resource *resource, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset)
{
	D3D11_RESOURCE_DIMENSION dim;

	resource->GetType(&dim);

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			DumpBuffer((ID3D11Buffer*)resource, filename, buf_type_mask, idx, ib_fmt, stride, offset, 0, 0);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			FALogInfo("Skipped dumping Texture1D resource\n");
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource((ID3D11Texture2D*)resource, filename);
			if (analyse_options & FrameAnalysisOptions::MONO)
				Dump2DResource((ID3D11Texture2D*)resource, filename, false, NULL);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			FALogInfo("Skipped dumping Texture3D resource\n");
			break;
		default:
			FALogInfo("Skipped dumping resource of unknown type %i\n", dim);
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
		if (!GetModuleFileName(0, path, (DWORD)size))
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

	try {
		hash = G->mResources.at(handle).hash;
		orig_hash = G->mResources.at(handle).orig_hash;
	} catch (std::out_of_range) {
		hash = orig_hash = 0;
	}

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
		FALogInfo("Failed to create filename: 0x%x\n", hr);
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

	try {
		hash = G->mResources.at(handle).hash;
		orig_hash = G->mResources.at(handle).orig_hash;
	} catch (std::out_of_range) {
		hash = orig_hash = 0;
	}

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
		FALogInfo("Failed to create filename: 0x%x\n", hr);

	return hr;
}

wchar_t* FrameAnalysisContext::dedupe_tex2d_filename(ID3D11Texture2D *resource,
		D3D11_TEXTURE2D_DESC *orig_desc, wchar_t *dedupe_filename,
		size_t size, wchar_t *traditional_filename)
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

	hr = GetImmediateContext()->Map(resource, 0, D3D11_MAP_READ, 0, &map);
	if (FAILED(hr)) {
		FALogInfo("Frame Analysis filename deduplication failed to map resource: 0x%x\n", hr);
		goto err;
	};

	// CalcTexture2DDataHash takes a D3D11_SUBRESOURCE_DATA*, which happens
	// to be binary identical to a D3D11_MAPPED_SUBRESOURCE (though it is
	// not an array), so we can safely cast it:
	hash = CalcTexture2DDataHash(orig_desc, (D3D11_SUBRESOURCE_DATA*)&map, true);
	hash = CalcTexture2DDescHash(hash, orig_desc);

	GetImmediateContext()->Unmap(resource, 0);

	get_deduped_dir(dedupe_dir, MAX_PATH);
	_snwprintf_s(dedupe_filename, size, size, L"%ls\\%08x.XXX", dedupe_dir, hash);

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

void FrameAnalysisContext::rotate_deduped_file(wchar_t *dedupe_filename)
{
	wchar_t rotated_filename[MAX_PATH];
	unsigned rotate;
	size_t ext_pos;
	wchar_t *ext;

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

static bool create_shortcut(wchar_t *filename, wchar_t *dedupe_filename)
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

void FrameAnalysisContext::link_deduplicated_files(wchar_t *filename, wchar_t *dedupe_filename)
{
	wchar_t relative_path[MAX_PATH] = {0};

	// Bail if source didn't get created:
	if (GetFileAttributes(dedupe_filename) == INVALID_FILE_ATTRIBUTES)
		return;

	// Bail if destination already exists:
	if (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES)
		return;

	if (PathRelativePathTo(relative_path, filename, 0, dedupe_filename, 0)) {
		if (CreateSymbolicLink(filename, relative_path, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
			return;
	}

	// Too noisy if symlinks aren't available, and pretty likely case (e.g.
	// Windows 10 only allows symlinks if developer mode is enabled), so
	// reserve this message for debug logging:
	LogDebug("Symlinking %S -> %S failed (0x%u), trying hard link\n",
			filename, relative_path, GetLastError());

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

	FALogInfo("All attempts to link deduplicated file failed, giving up: %S -> %S\n",
			filename, dedupe_filename);
}

void FrameAnalysisContext::_DumpCBs(char shader_type, bool compute,
	ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT])
{
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	for (i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
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
	D3D11_RESOURCE_DIMENSION dim;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (!views[i])
			continue;

		views[i]->GetResource(&resource);
		if (!resource) {
			views[i]->Release();
			continue;
		}

		resource->GetType(&dim);

		// TODO: process description to get offset, strides & size for
		// buffer & bufferex type SRVs and pass down to dump routines,
		// although I have no idea how to determine which of the
		// entries in the two D3D11_BUFFER_SRV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"t", shader_type, i, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_SRV, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
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

void FrameAnalysisContext::DumpVBs(DrawCallInfo *call_info)
{
	ID3D11Buffer *buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i, first = 0, count = 0;

	if (call_info) {
		first = call_info->FirstVertex;
		count = call_info->VertexCount;
	}

	// TODO: The format of each vertex buffer cannot be obtained from this
	// call. Rather, it is available in the input layout assigned to the
	// pipeline. There is no API to get the layout description, so if we
	// want to obtain it we will need to wrap the input layout objects
	// (there may be other good reasons to consider wrapping the input
	// layout if we ever do anything advanced with the vertex buffers).

	GetPassThroughOrigContext1()->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, buffers, strides, offsets);

	for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (!buffers[i])
			continue;

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"vb", NULL, i, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpBuffer(buffers[i], filename,
				FrameAnalysisOptions::DUMP_VB, i,
				DXGI_FORMAT_UNKNOWN, strides[i], offsets[i],
				first, count);
		}

		buffers[i]->Release();
	}
}

void FrameAnalysisContext::DumpIB(DrawCallInfo *call_info)
{
	ID3D11Buffer *buffer = NULL;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	DXGI_FORMAT format;
	UINT offset, first = 0, count = 0;

	if (call_info) {
		first = call_info->FirstIndex;
		count = call_info->IndexCount;
	}

	GetPassThroughOrigContext1()->IAGetIndexBuffer(&buffer, &format, &offset);
	if (!buffer)
		return;

	hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"ib", NULL, -1, buffer);
	if (SUCCEEDED(hr)) {
		DumpBuffer(buffer, filename,
				FrameAnalysisOptions::DUMP_IB, -1,
				format, 0, offset, first, count);
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
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	GetPassThroughOrigContext1()->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, NULL);

	for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
		if (!rtvs[i])
			continue;

		rtvs[i]->GetResource(&resource);
		if (!resource) {
			rtvs[i]->Release();
			continue;
		}

		// TODO: process description to get offset, strides & size for
		// buffer type RTVs and pass down to dump routines, although I
		// have no idea how to determine which of the entries in the
		// two D3D11_BUFFER_RTV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"o", NULL, i, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_RT, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
		}

		resource->Release();
		rtvs[i]->Release();
	}
}

void FrameAnalysisContext::DumpDepthStencilTargets()
{
	ID3D11DepthStencilView *dsv = NULL;
	ID3D11Resource *resource = NULL;
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

	hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"oD", NULL, -1, resource);
	if (FAILED(hr))
		return;

	DumpResource(resource, filename, FrameAnalysisOptions::DUMP_DEPTH,
			-1, DXGI_FORMAT_UNKNOWN, 0, 0);
}

void FrameAnalysisContext::DumpUAVs(bool compute)
{
	UINT i;
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	ID3D11Resource *resource;
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	if (compute)
		GetPassThroughOrigContext1()->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);
	else
		GetPassThroughOrigContext1()->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);

	for (i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; ++i) {
		if (!uavs[i])
			continue;

		uavs[i]->GetResource(&resource);
		if (!resource) {
			uavs[i]->Release();
			continue;
		}

		// TODO: process description to get offset & size for buffer
		// type UAVs and pass down to dump routines.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"u", NULL, i, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_RT, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
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
	NvAPI_Stereo_IsEnabled(&stereo);
	if (stereo)
		NvAPI_Stereo_IsActivated(GetHackerDevice()->mStereoHandle, &stereo);

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
	if (!(analyse_options & FrameAnalysisOptions::DEFERRED_CONTEXT) &&
	   (GetPassThroughOrigContext1()->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)) {
		draw_call++;
		return;
	}

	update_stereo_dumping_mode();
	set_default_dump_formats(true);

	if ((analyse_options & FrameAnalysisOptions::FMT_2D_MASK) &&
	    (analyse_options & FrameAnalysisOptions::STEREO)) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			FALogInfo("DumpStereoResource failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	// Grab the critical section now as we may need it several times during
	// dumping for mResources
	EnterCriticalSection(&G->mCriticalSection);

	if (analyse_options & FrameAnalysisOptions::DUMP_CB)
		DumpCBs(compute);

	if (!compute) {
		if (analyse_options & FrameAnalysisOptions::DUMP_VB)
			DumpVBs(call_info);

		if (analyse_options & FrameAnalysisOptions::DUMP_IB)
			DumpIB(call_info);
	}

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
	    (analyse_options & FrameAnalysisOptions::STEREO)) {
		NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, false);
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

	// Don't bother trying to dump as stereo - Map/Unmap/Update are inherently mono
	analyse_options &= (FrameAnalysisOptions)~FrameAnalysisOptions::STEREO_MASK;
	analyse_options |= FrameAnalysisOptions::MONO;

	set_default_dump_formats(false);

	EnterCriticalSection(&G->mCriticalSection);

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
		const wchar_t *target, DXGI_FORMAT ib_fmt, UINT stride, UINT offset)
{
	wchar_t filename[MAX_PATH];
	NvAPI_Status nvret;
	HRESULT hr;

	analyse_options = options;

	update_stereo_dumping_mode();
	set_default_dump_formats(false);

	if (analyse_options & FrameAnalysisOptions::STEREO) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			FALogInfo("FrameAnalyisDump failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	EnterCriticalSection(&G->mCriticalSection);

	hr = FrameAnalysisFilenameResource(filename, MAX_PATH, target, resource, false);
	if (FAILED(hr)) {
		// If the ini section and resource name makes the filename too
		// long, try again without them:
		hr = FrameAnalysisFilenameResource(filename, MAX_PATH, L"...", resource, false);
	}
	if (SUCCEEDED(hr))
		DumpResource(resource, filename, analyse_options, -1, ib_fmt, stride, offset);

	LeaveCriticalSection(&G->mCriticalSection);

	if (analyse_options & FrameAnalysisOptions::STEREO)
		NvAPI_Stereo_ReverseStereoBlitControl(GetHackerDevice()->mStereoHandle, false);

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
	FrameAnalysisLog("DispatchIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

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
	FrameAnalysisLog("ClearUnorderedAccessViewUint(pUnorderedAccessView:0x%p, Values:0x%p\n)",
			pUnorderedAccessView, Values);
	FrameAnalysisLogView(-1, NULL, pUnorderedAccessView);

	HackerContext::ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
}

STDMETHODIMP_(void) FrameAnalysisContext::ClearUnorderedAccessViewFloat(THIS_
		/* [annotation] */
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */
		__in  const FLOAT Values[4])
{
	FrameAnalysisLog("ClearUnorderedAccessViewFloat(pUnorderedAccessView:0x%p, Values:0x%p\n)",
			pUnorderedAccessView, Values);
	FrameAnalysisLogView(-1, NULL, pUnorderedAccessView);

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
	FrameAnalysisLog("ClearDepthStencilView(pDepthStencilView:0x%p, ClearFlags:%u, Depth:%f, Stencil:%u\n)",
			pDepthStencilView, ClearFlags, Depth, Stencil);
	FrameAnalysisLogView(-1, NULL, pDepthStencilView);

	HackerContext::ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}

STDMETHODIMP_(void) FrameAnalysisContext::GenerateMips(THIS_
		/* [annotation] */
		__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	FrameAnalysisLog("GenerateMips(pShaderResourceView:0x%p\n)",
			pShaderResourceView);
	FrameAnalysisLogView(-1, NULL, pShaderResourceView);

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
	FrameAnalysisLog("ExecuteCommandList(pCommandList:0x%p, RestoreContextState:%s)\n",
			pCommandList, RestoreContextState ? "true" : "false");

	HackerContext::ExecuteCommandList(pCommandList, RestoreContextState);
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
		DrawCallInfo call_info(0, IndexCount, 0, BaseVertexLocation, StartIndexLocation, 0, NULL, 0, false);
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
		DrawCallInfo call_info(VertexCount, 0, 0, StartVertexLocation, 0, 0, NULL, 0, false);
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
		DrawCallInfo call_info(0, IndexCountPerInstance, InstanceCount, BaseVertexLocation, StartIndexLocation, StartInstanceLocation, NULL, 0, false);
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
		DrawCallInfo call_info(VertexCountPerInstance, 0, InstanceCount, StartVertexLocation, 0, StartInstanceLocation, NULL, 0, false);
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
		DrawCallInfo call_info(0, 0, 0, 0, 0, 0, NULL, 0, false);
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
	FrameAnalysisLog("DrawIndexedInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

	HackerContext::DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);

	if (G->analyse_frame) {
		DrawCallInfo call_info(0, 0, 0, 0, 0, 0, pBufferForArgs, AlignedByteOffsetForArgs, false);
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
	FrameAnalysisLog("DrawInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

	HackerContext::DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);

	if (G->analyse_frame) {
		DrawCallInfo call_info(0, 0, 0, 0, 0, 0, pBufferForArgs, AlignedByteOffsetForArgs, true);
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
	FrameAnalysisLogView(-1, NULL, pRenderTargetView);

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
	FrameAnalysisLogView(-1, NULL, pResourceView);

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
	FrameAnalysisLogView(-1, NULL, pView);

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
	FrameAnalysisLogView(-1, NULL, pResourceView);

	HackerContext::DiscardView1(pResourceView, pRects, NumRects);
}
