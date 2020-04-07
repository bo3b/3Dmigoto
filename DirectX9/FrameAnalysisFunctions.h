#pragma once
#include <Strsafe.h>
#include <stdarg.h>
#include <Shlwapi.h>

// For windows shortcuts:
#include <shobjidl.h>
#include <shlguid.h>
#include "FrameAnalysis.h"

#undef NOMINMAX
#ifndef NOMINMAX

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#endif

// Flag introduced in Windows 10 Fall Creators Update
// Someone was clearly on crack when they decided this flag was necessary
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisDevice(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex) :
	D3D9Wrapper::IDirect3DDevice9(pDevice, pD3D, ex)
{
	analyse_options = FrameAnalysisOptions::INVALID;
	oneshot_analyse_options = FrameAnalysisOptions::INVALID;
	oneshot_valid = false;
	frame_analysis_log = NULL;
	draw_call = 0;
	non_draw_call_dump_counter = 0;
}

inline D3D9Wrapper::FrameAnalysisDevice* D3D9Wrapper::FrameAnalysisDevice::GetDirect3DDevice(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 * pD3D, bool ex)
{
	D3D9Wrapper::FrameAnalysisDevice* p = (D3D9Wrapper::FrameAnalysisDevice*) m_List.GetDataPtr(pDevice);
	if (!p)
	{
		p = new D3D9Wrapper::FrameAnalysisDevice(pDevice, pD3D, ex);
		if (pDevice) m_List.AddMember(pDevice, p);
	}
	return p;
}

D3D9Wrapper::FrameAnalysisDevice::~FrameAnalysisDevice()
{
	if (frame_analysis_log)
		fclose(frame_analysis_log);
}

void D3D9Wrapper::FrameAnalysisDevice::vFrameAnalysisLog(char *fmt, va_list ap)
{
	wchar_t filename[MAX_PATH];
	LogDebugNoNL("HackerDevice(%s@%p)::", type_name_dx9((IUnknown*)this), this);
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
		//if (GetPassThroughOrigContext1()->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
			//swprintf_s(filename, MAX_PATH, L"%ls\\log.txt", G->ANALYSIS_PATH);
		//else
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

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLog(char *fmt, ...)
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

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogShaderHash(D3D9Wrapper::IDirect3DShader9 *shader)
{
	// Always complete the line in the debug log:
	LogDebug("\n");
	if (!G->analyse_frame || !frame_analysis_log)
		return;

	if (!shader) {
		fprintf(frame_analysis_log, "\n");
		return;
	}
	fprintf(frame_analysis_log, " hash=%016llx", shader->hash);
	fprintf(frame_analysis_log, "\n");
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogResourceHash(D3D9Wrapper::IDirect3DResource9 *resource)
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
	hash = resource->resourceHandleInfo.hash;
	orig_hash = resource->resourceHandleInfo.orig_hash;
	if (hash)
		fprintf(frame_analysis_log, " hash=%08x", hash);
	if (orig_hash != hash)
		fprintf(frame_analysis_log, " orig_hash=%08x", orig_hash);
	EnterCriticalSection(&G->mCriticalSection);
	try {
		info = &G->mResourceInfo.at(orig_hash);
		if (info->hash_contaminated) {
			fprintf(frame_analysis_log, " hash_contamination=");
			if (!info->lock_contamination.empty())
				fprintf(frame_analysis_log, "Lock,");
			if (!info->copy_contamination.empty())
				fprintf(frame_analysis_log, "CopyResource,");
			if (!info->region_contamination.empty())
				fprintf(frame_analysis_log, "CopyRegion,");
		}
	}
	catch (std::out_of_range) {

	}
	LeaveCriticalSection(&G->mCriticalSection);

	fprintf(frame_analysis_log, "\n");
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogResource(int slot, char *slot_name, D3D9Wrapper::IDirect3DResource9 *resource)
{
	if (!resource || !G->analyse_frame || !frame_analysis_log)
		return;

	FrameAnalysisLogSlot(frame_analysis_log, slot, slot_name);
	fprintf(frame_analysis_log, " resource=0x%p", resource);

	FrameAnalysisLogResourceHash(resource);
}
void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogResourceArray(UINT start, UINT len, D3D9Wrapper::IDirect3DResource9 *const *ppResources)
{
	UINT i;

	if (!ppResources || !G->analyse_frame || !frame_analysis_log)
		return;

	for (i = 0; i < len; i++)
		FrameAnalysisLogResource(start + i, NULL, ppResources[i]);
}
void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogMiscArray(UINT start, UINT len, void *const *array)
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
void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogQuery(::IDirect3DQuery9 *async)
{
	// Always complete the line in the debug log:
	LogDebug("\n");

	if (!G->analyse_frame || !frame_analysis_log)
		return;

	if (!async) {
		fprintf(frame_analysis_log, "\n");
		return;
	}
	fprintf(frame_analysis_log, " query=");

	::D3DQUERYTYPE type;

	type = async->GetType();

	switch (type) {
	case ::D3DQUERYTYPE_EVENT:
		fprintf(frame_analysis_log, "event");
		break;
	case ::D3DQUERYTYPE_OCCLUSION:
		fprintf(frame_analysis_log, "occlusion");
		break;
	case ::D3DQUERYTYPE_TIMESTAMP:
		fprintf(frame_analysis_log, "timestamp");
		break;
	case ::D3DQUERYTYPE_TIMESTAMPDISJOINT:
		fprintf(frame_analysis_log, "timestamp_disjoint");
		break;
	case ::D3DQUERYTYPE_PIPELINETIMINGS:
		fprintf(frame_analysis_log, "pipeline_statistics");
		break;
	case ::D3DQUERYTYPE_VCACHE:
		fprintf(frame_analysis_log, "vcache");
		break;
	case ::D3DQUERYTYPE_RESOURCEMANAGER:
		fprintf(frame_analysis_log, "resource_manager");
		break;
	case ::D3DQUERYTYPE_VERTEXSTATS:
		fprintf(frame_analysis_log, "vertex_stats");
		break;
	case ::D3DQUERYTYPE_TIMESTAMPFREQ:
		fprintf(frame_analysis_log, "time_stamp_frequency");
		break;
	case ::D3DQUERYTYPE_INTERFACETIMINGS:
		fprintf(frame_analysis_log, "interface_timings");
		break;
	case ::D3DQUERYTYPE_VERTEXTIMINGS:
		fprintf(frame_analysis_log, "vetex_timings");
		break;
	case ::D3DQUERYTYPE_PIXELTIMINGS:
		fprintf(frame_analysis_log, "pixel_timings");
		break;
	case ::D3DQUERYTYPE_BANDWIDTHTIMINGS:
		fprintf(frame_analysis_log, "bandwidth_timings");
		break;
	case ::D3DQUERYTYPE_CACHEUTILIZATION:
		fprintf(frame_analysis_log, "cache_utilization");
		break;
	case ::D3DQUERYTYPE_MEMORYPRESSURE:
		fprintf(frame_analysis_log, "memory_pressure");
		break;
	default:
		fprintf(frame_analysis_log, "?");
		break;
	}
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisLogData(void *buf, UINT size)
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

::IDirect3DDevice9* D3D9Wrapper::FrameAnalysisDevice::GetDumpingDevice()
{
	return GetPassThroughOrigDevice();
}
static HRESULT SaveSurfaceToFile(::IDirect3DTexture9 *texture, wstring filename, ::D3DXIMAGE_FILEFORMAT format)
{
	return D3DXSaveTextureToFile(filename.c_str(), format, texture, NULL);
}
static HRESULT SaveSurfaceToFile(::IDirect3DCubeTexture9 *texture, wstring filename, ::D3DXIMAGE_FILEFORMAT format)
{
	return D3DXSaveTextureToFile(filename.c_str(), format, texture, NULL);
}
static HRESULT SaveSurfaceToFile(::IDirect3DSurface9 *surface, wstring filename, ::D3DXIMAGE_FILEFORMAT format)
{
	return D3DXSaveSurfaceToFile(filename.c_str(), format, surface, NULL, NULL);
}
template<typename SurfaceType>
void D3D9Wrapper::FrameAnalysisDevice::DumpSurface(SurfaceType *resource, wstring filename, bool stereo, D3D2DTEXTURE_DESC *orig_desc, ::D3DFORMAT format)
{
	HRESULT hr = S_OK, dont_care;
	wchar_t dedupe_filename[MAX_PATH];
	wstring save_filename;
	wchar_t *wic_ext = (stereo ? L".jps" : L".jpg");
	size_t ext, save_ext;

	save_filename = dedupe_sur2d_filename(resource, orig_desc, dedupe_filename, MAX_PATH, filename.c_str(), format);

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
			hr = SaveSurfaceToFile(resource, filename.c_str(), ::D3DXIFF_JPG);
			link_deduplicated_files(filename.c_str(), save_filename.c_str());
	}


	if ((analyse_options & FrameAnalysisOptions::FMT_2D_DDS) ||
		((analyse_options & FrameAnalysisOptions::FMT_2D_AUTO) && FAILED(hr))) {
		filename.replace(ext, wstring::npos, L".dds");
		save_filename.replace(save_ext, wstring::npos, L".dds");
		FALogInfo("Dumping Texture2D %S -> %S\n", filename.c_str(), save_filename.c_str());

		hr = S_OK;
		if (GetFileAttributes(save_filename.c_str()) == INVALID_FILE_ATTRIBUTES)
			hr = SaveSurfaceToFile(resource, filename.c_str(), ::D3DXIFF_DDS);
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
template<typename SurfaceType>
void D3D9Wrapper::FrameAnalysisDevice::Dump2DSurface(SurfaceType *resource, wchar_t *filename,
	bool stereo, D3D2DTEXTURE_DESC *orig_desc, ::D3DFORMAT format)
{
	HRESULT hr = S_OK;
	SurfaceType *staging = resource;
	D3D2DTEXTURE_DESC staging_desc, *desc = orig_desc;
	// In order to de-dupe Texture2D resources, we need to compare their
	// contents before dumping them to a file, so we copy them into a
	// staging resource (if we did not already do so earlier for the
	// reverse stereo blit). DirectXTK will notice this has been done and
	// skip doing it again.
	staging_desc = D3D2DTEXTURE_DESC((SurfaceType*)resource);
	if (staging_desc.Pool == ::D3DPOOL_DEFAULT && (!(staging_desc.Usage & D3DUSAGE_DYNAMIC))){
		hr = StageResource(resource, &staging_desc, &staging, format);
		if (FAILED(hr))
			return;
	}
	if (!orig_desc)
		desc = &staging_desc;
	DumpSurface<SurfaceType>(staging, filename, stereo, desc, format);

	if (staging != resource)
		staging->Release();
}

static HRESULT _CreateStagingResource(::IDirect3DSurface9 **resource,
	D3D2DTEXTURE_DESC desc, ::IDirect3DDevice9 *device) {
	if (desc.Pool == ::D3DPOOL_DEFAULT)
		return device->CreateRenderTarget(desc.Width, desc.Height, desc.Format, ::D3DMULTISAMPLE_NONE, 0, false, resource, NULL);
	else
		return device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, ::D3DPOOL_SYSTEMMEM, resource, NULL);
}

static HRESULT _CreateStagingResource(::IDirect3DTexture9 **resource,
	D3D2DTEXTURE_DESC desc, ::IDirect3DDevice9 *device) {
	return device->CreateTexture(desc.Width, desc.Height, desc.Levels, desc.Usage, desc.Format, desc.Pool, resource, NULL);
}
static HRESULT _CreateStagingResource(::IDirect3DCubeTexture9 **resource,
	D3D2DTEXTURE_DESC desc, ::IDirect3DDevice9 *device) {
	return device->CreateCubeTexture(desc.Width, desc.Levels, desc.Usage, desc.Format, desc.Pool, resource, NULL);
}
template<typename Surface>
HRESULT D3D9Wrapper::FrameAnalysisDevice::CreateStagingResource(Surface **resource,
	D3D2DTEXTURE_DESC desc, bool stereo, bool msaa, ::D3DFORMAT format)
{
	NVAPI_STEREO_SURFACECREATEMODE orig_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	HRESULT hr;
	bool restoreOrig = false;

	// NOTE: desc is passed by value - this is intentional so we don't
	// modify desc in the caller

	if (stereo)
		desc.Width *= 2;

	if (!msaa) {
		// Make this a staging resource to save DirectXTK having to create it's
		// own staging resource.
		desc.Pool = ::D3DPOOL_SYSTEMMEM;
		desc.Usage &= ~D3DUSAGE_RENDERTARGET;
		desc.Usage &= ~D3DUSAGE_DEPTHSTENCIL;
		desc.Usage &= ~D3DUSAGE_DYNAMIC;
	}
	if (format != ::D3DFMT_UNKNOWN)
		desc.Format = format;

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
		Profiling::NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, &orig_mode);
		if (orig_mode != NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO) {
			Profiling::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
			restoreOrig = true;
		}

	}
	hr = _CreateStagingResource(resource, desc, GetPassThroughOrigDevice());
	if ((analyse_options & FrameAnalysisOptions::STEREO) && restoreOrig)
		Profiling::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, orig_mode);

	return hr;
}
static HRESULT _ResolveAA(::IDirect3DSurface9 *src, ::IDirect3DSurface9 *resolved, D3D2DTEXTURE_DESC *srcDesc, D3D9Wrapper::IDirect3DDevice9 *device) {
	if ((srcDesc->Usage & D3DUSAGE_RENDERTARGET)) {
		return device->GetD3D9Device()->StretchRect(src, NULL, resolved, NULL, ::D3DTEXTUREFILTERTYPE::D3DTEXF_POINT);
	}
	else if ((srcDesc->Usage & D3DUSAGE_DEPTHSTENCIL)) {
		HRESULT hr = D3DXLoadSurfaceFromSurface(resolved, NULL, NULL, src, NULL, NULL, D3DX_DEFAULT, 0);
		if (FAILED(hr))
			hr = device->NVAPIStretchRect(src, resolved, NULL, NULL);
		return hr;
	}
	else {
		return D3DXLoadSurfaceFromSurface(resolved, NULL, NULL, src, NULL, NULL, D3DX_DEFAULT, 0);
	}
}
static ::IDirect3DSurface9* GetSurface(::IDirect3DSurface9 *surface, UINT level)
{
	return surface;
}
static ::IDirect3DSurface9*  GetSurface(::IDirect3DTexture9 *texture, UINT level)
{
	::IDirect3DSurface9 *surface;
	texture->GetSurfaceLevel(level, &surface);
	return surface;
}
static ::IDirect3DSurface9*  GetSurface(::IDirect3DCubeTexture9 *texture, UINT level)
{
	::IDirect3DSurface9 *surface;
	texture->GetCubeMapSurface(::D3DCUBEMAP_FACE_POSITIVE_X, level, &surface);
	return surface;
}
template<typename SurfaceType>
HRESULT D3D9Wrapper::FrameAnalysisDevice::ResolveMSAA(SurfaceType *src, D3D2DTEXTURE_DESC *srcDesc,
	SurfaceType **dst, ::D3DFORMAT format)
{
	SurfaceType *resolved = NULL;
	UINT level;
	HRESULT hr;

	*dst = NULL;

	if (srcDesc->MultiSampleType == ::D3DMULTISAMPLE_NONE)
		return S_OK;

	// Resolve MSAA surfaces. Procedure copied from DirectXTK
	// These need to have D3D11_USAGE_DEFAULT to resolve,
	// so we need yet another intermediate texture:
	hr = CreateStagingResource(&resolved, *srcDesc, false, true, format);
	if (FAILED(hr)) {
		FALogErr("ResolveMSAA failed to create intermediate texture: 0x%x\n", hr);
		return hr;
	}
	for (level = 0; level < srcDesc->Levels; level++) {
		::IDirect3DSurface9 *srcSur = GetSurface(src, level);
		::IDirect3DSurface9 *dstSur = GetSurface(resolved, level);
		hr = _ResolveAA(srcSur, dstSur, srcDesc, this);
		srcSur->Release();
		dstSur->Release();
		if (FAILED(hr)) {
			resolved->Release();
			return hr;
		}
	}
	*dst = resolved;
	return S_OK;
}
template<typename Surface>HRESULT D3D9Wrapper::FrameAnalysisDevice::StageResource(Surface *src,
	D3D2DTEXTURE_DESC *srcDesc, Surface **dst, ::D3DFORMAT format)
{
	Surface *staging = NULL;
	Surface *resolved = NULL;
	HRESULT hr;

	*dst = NULL;

	hr = CreateStagingResource(&staging, *srcDesc, false, false, format);
	if (FAILED(hr)) {
		FALogErr("StageResource failed to create intermediate texture: 0x%x\n", hr);
		return hr;
	}

	hr = ResolveMSAA(src, srcDesc, &resolved, format);
	if (resolved)
		src = resolved;

	for (UINT level = 0; level < srcDesc->Levels; level++) {
		::IDirect3DSurface9 *srcSur = GetSurface(src, level);
		::IDirect3DSurface9 *dstSur = GetSurface(staging, level);
		hr = GetD3D9Device()->GetRenderTargetData(srcSur, dstSur);
		srcSur->Release();
		dstSur->Release();
		if (FAILED(hr)) {
			goto err_release;
		}
	}

	if (resolved)
		resolved->Release();

	*dst = staging;
	return S_OK;

err_release:
	if (resolved)
		resolved->Release();
	if (staging)
		staging->Release();
	return hr;
}

// TODO: Refactor this with StereoScreenShot().
// Expects the reverse stereo blit to be enabled by the caller
template <typename Surface> void D3D9Wrapper::FrameAnalysisDevice::DumpStereoResource(Surface *resource, wchar_t *filename,
	::D3DFORMAT format)
{
	Surface *stereoResource = NULL;
	Surface *tmpResource = NULL;
	Surface *src = resource;
	D3D2DTEXTURE_DESC srcDesc;
	RECT dstRect;
	HRESULT hr;
	srcDesc = D3D2DTEXTURE_DESC((Surface*)resource);

	hr = CreateStagingResource(&stereoResource, srcDesc, true, false, format);
	if (FAILED(hr)) {
		FALogErr("DumpStereoResource failed to create stereo texture: 0x%x\n", hr);
		return;
	}
	hr = ResolveMSAA<Surface>(resource, &srcDesc, &tmpResource, format);
	if (tmpResource)
		src = tmpResource;
	// Perform the reverse stereo blit on all sub-resources and mip-maps:
	LONG upperX = srcDesc.Width;
	dstRect.left = upperX;
	dstRect.top = 0;
	dstRect.right = upperX + srcDesc.Width;
	dstRect.bottom = srcDesc.Height;
	for (UINT level = 0; level < srcDesc.Levels; level++) {
		::IDirect3DSurface9 *srcSur = GetSurface(src, level);
		::IDirect3DSurface9 *dstSur = GetSurface(stereoResource, level);
		hr = GetD3D9Device()->StretchRect(srcSur, NULL, dstSur, &dstRect, ::D3DTEXF_POINT);
		srcSur->Release();
		dstSur->Release();
		if (FAILED(hr))
			goto out;
	}
	Dump2DSurface(stereoResource, filename, true, &srcDesc, format);
out:
	if (tmpResource)
		tmpResource->Release();
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

void D3D9Wrapper::FrameAnalysisDevice::dedupe_buf_filename_txt(const wchar_t *bin_filename,
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
void D3D9Wrapper::FrameAnalysisDevice::DumpBufferTxt(wchar_t *filename, void *map,
	UINT size, char type, int idx, UINT stride, UINT offset)
{
	FILE *fd = NULL;
	char *components = "xyzw";
	float *buf = (float*)map;
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
				fprintf(fd, "buf[%d].%c: %.9g\n", i, components[c], buf[i * 4 + c]);
			else
				fprintf(fd, "%cb%i[%d].%c: %.9g\n", type, idx, i, components[c], buf[i * 4 + c]);
		}
	}

	fclose(fd);
}

static const char* TopologyStr(::D3DPRIMITIVETYPE topology)
{
	switch (topology) {
	case ::D3DPT_POINTLIST: return "pointlist";
	case ::D3DPT_LINELIST: return "linelist";
	case ::D3DPT_LINESTRIP: return "linestrip";
	case ::D3DPT_TRIANGLELIST: return "trianglelist";
	case ::D3DPT_TRIANGLESTRIP: return "trianglestrip";
	case ::D3DPT_TRIANGLEFAN: return "trianglesfan";
	}
	return "invalid";
}

void D3D9Wrapper::FrameAnalysisDevice::dedupe_buf_filename_vb_txt(const wchar_t *bin_filename,
	wchar_t *txt_filename, size_t size, int idx, UINT stride,
	UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
	::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info)
{
	wchar_t *pos;
	size_t rem;
	uint32_t layout_hash;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-vb%i", idx);

	::D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH];
	UINT numElements;
	layout->GetDeclaration(decl, &numElements);
	if (layout) {
		layout_hash = crc32c_hw(0, decl, (sizeof(::D3DVERTEXELEMENT9)*numElements));
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

	if (FAILED(StringCchPrintfW(pos, rem, L".txt")))
		FALogErr("Failed to create vertex buffer filename\n");
}

static void dump_ia_layout(FILE *fd, ::D3DVERTEXELEMENT9 *vertex_elemet, UINT num_elements)//, int slot)//, bool *per_vert, bool *per_inst)
{
	UINT i;

	for (i = 0; i < num_elements; i++) {
		fprintf(fd, "element[%i]:\n", i);
		fprintf(fd, "  SemanticName: %s\n", ::D3DDECLUSAGE(vertex_elemet[i].Usage));
		fprintf(fd, "  SemanticIndex: %u\n", vertex_elemet[i].UsageIndex);
		fprintf(fd, "  Format: %s\n", ::D3DDECLTYPE(vertex_elemet[i].Type));
		fprintf(fd, "  InputSlot: %u\n", vertex_elemet[i].Stream);
		fprintf(fd, "  Offset: %u\n", vertex_elemet[i].Offset);
		fprintf(fd, "  Method: %u\n", ::D3DDECLMETHOD (vertex_elemet[i].Method));
	}
}

static void dump_vb_unknown_layout(FILE *fd, void *map,
	UINT size, int slot, UINT offset, UINT first, UINT count, UINT stride)
{
	float *buff = (float*)map;
	uint32_t *buf32 = (uint32_t*)map;
	uint8_t *buf8 = (uint8_t*)map;
	UINT vertex, j, start, end, buf_idx;

	start = offset / stride + first;
	end = size / stride;
	if (count)
		end = min(end, start + count);

	for (vertex = start; vertex < end; vertex++) {
		fprintf(fd, "\n");

		for (j = 0; j < stride / 4; j++) {
			buf_idx = vertex * stride / 4 + j;
			fprintf(fd, "vb%i[%u]+%03u: 0x%08x %.9g\n", slot, vertex - start, j * 4, buf32[buf_idx], buff[buf_idx]);
		}

		// In case we find one that is not a 32bit multiple finish off one byte at a time:
		for (j = j * 4; j < stride; j++) {
			buf_idx = vertex * stride + j;
			fprintf(fd, "vb%i[%u]+%03u: 0x%02x\n", slot, vertex - start, j, buf8[buf_idx]);
		}
	}
}
static float float16(uint16_t f16)
{
	// Shift sign and mantissa to new positions:
	uint32_t f32 = ((f16 & 0x8000) << 16) | ((f16 & 0x3ff) << 13);
	// Need to check special cases of the biased exponent:
	int biased_exponent = (f16 & 0x7c00) >> 10;

	if (biased_exponent == 0) {
		// Zero / subnormal: New biased exponent remains zero
	}
	else if (biased_exponent == 0x1f) {
		// Infinity / NaN: New biased exponent is filled with 1s
		f32 |= 0x7f800000;
	}
	else {
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

static int fprint_decltype(FILE *fd, ::D3DDECLTYPE type, uint8_t *buf)
{
	float *f = (float*)buf;
	uint32_t *u32 = (uint32_t*)buf;
	int32_t *s32 = (int32_t*)buf;
	uint16_t *u16 = (uint16_t*)buf;
	int16_t *s16 = (int16_t*)buf;
	uint8_t *u8 = (uint8_t*)buf;
	int8_t *s8 = (int8_t*)buf;
	unsigned i;

	switch (type) {
	case ::D3DDECLTYPE_FLOAT1:
			return fprintf(fd, "%08x", f[0]);
	case ::D3DDECLTYPE_FLOAT2:
		return fprintf(fd, "%08x, %08x", f[0], f[1]);
	case ::D3DDECLTYPE_FLOAT3:
		return fprintf(fd, "%08x, %08x, %08x", f[0], f[1], f[2]);
	case ::D3DDECLTYPE_FLOAT4:
		return fprintf(fd, "%08x, %08x, %08x, %08x", f[0], f[1], f[2], f[3]);
	case ::D3DDECLTYPE_D3DCOLOR:
		return fprintf(fd, "%08x, %08x, %08x, %08x", u8[0], u8[1], u8[2], u8[3]);
	case ::D3DDECLTYPE_UBYTE4:
		return fprintf(fd, "%08x, %08x, %08x, %08x", u8[0], u8[1], u8[2], u8[3]);
	case ::D3DDECLTYPE_UBYTE4N:
		return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", unorm8(u8[0]), unorm8(u8[1]), unorm8(u8[2]), unorm8(u8[3]));
	case ::D3DDECLTYPE_SHORT2:
		return fprintf(fd, "%08x, %08x", s16[0], s16[1]);
	case ::D3DDECLTYPE_SHORT2N:
		return fprintf(fd, "%.9g, %.9g", snorm16(s16[0]), snorm16(s16[1]));
	case ::D3DDECLTYPE_USHORT2N:
		return fprintf(fd, "%.9g, %.9g", unorm16(s16[0]), unorm16(s16[1]));
	case ::D3DDECLTYPE_FLOAT16_2:
		return fprintf(fd, "%.9g, %.9g", float16(u16[0]), float16(u16[1]));
	case ::D3DDECLTYPE_SHORT4:
		return fprintf(fd, "%08x, %08x, %08x, %08x", s8[0], s8[1], s8[2], s8[3]);
	case ::D3DDECLTYPE_SHORT4N:
		return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", snorm16(s8[0]), snorm16(s8[1]), snorm16(s8[2]), snorm16(s8[3]));
	case ::D3DDECLTYPE_USHORT4N:
		return fprintf(fd, "%.9g, %.9g, %.9g, %.9g", unorm16(u16[0]), unorm16(u16[1]), unorm16(u16[2]), unorm16(u16[3]));
	case ::D3DDECLTYPE_FLOAT16_4:
		return fprintf(fd, "%08x, %08x", float16(u16[0]), float16(u16[1]), float16(u16[2]), float16(u16[3]));
	case ::D3DDECLTYPE_UDEC3:
	case ::D3DDECLTYPE_DEC3N:
		// TODO: Unusual field sizes:
		return 0;
	case ::D3DDECLTYPE_UNUSED:
		return 0;
	}

	for (i = 0; i < byteSizeFromD3DType(type); i++)
		fprintf(fd, "%02x", buf[i]);
	return i * 2;
}


static void dump_vb_elem(FILE *fd, uint8_t *buf,
	::D3DVERTEXELEMENT9 *vertex_element, UINT num_elements,
	int slot, UINT vb_idx, UINT elem, UINT stride)
{
	UINT offset = 0, size;

	if (vertex_element[elem].Stream != slot)
		return;

	offset = vertex_element[elem].Offset;

	fprintf(fd, "vb%i[%u]+%03u %s", slot, vb_idx, offset, ::D3DDECLUSAGE(vertex_element[elem].Usage));
	if (vertex_element[elem].UsageIndex)
		fprintf(fd, "%u", vertex_element[elem].UsageIndex);
	fprintf(fd, ": ");

	fprint_decltype(fd, ::D3DDECLTYPE(vertex_element[elem].Type), buf + offset);
	fprintf(fd, "\n");

	size = byteSizeFromD3DType(::D3DDECLTYPE(vertex_element[elem].Type));// dxgi_format_size(layout_desc[elem].Format);
	if (!size)
		fprintf(fd, "# WARNING: Unknown format size, vertex buffer may be decoded incorrectly\n");
	offset += size;
	if (offset > stride)
		fprintf(fd, "# WARNING: Offset exceeded stride, vertex buffer may be decoded incorrectly\n");
}

static void dump_vb_known_layout(FILE *fd, void *map,
	::D3DVERTEXELEMENT9 *vertex_element, UINT num_elements,
	UINT size, int slot, UINT offset, UINT first, UINT count, UINT stride)
{
	UINT vertex, elem, start, end;

	start = offset / stride + first;
	end = size / stride;
	if (count)
		end = min(end, start + count);

	for (vertex = start; vertex < end; vertex++) {
		fprintf(fd, "\n");
		for (elem = 0; elem < num_elements; elem++) {
			dump_vb_elem(fd, (uint8_t*)map + stride * vertex,
				vertex_element, num_elements, slot,
				vertex - start, elem, stride);
		}
	}
}

/*
* Dumps the vertex buffer in several formats.
* FIXME: We should wrap the input layout object to get the correct format (and
* other info like the semantic).
*/
void D3D9Wrapper::FrameAnalysisDevice::DumpVBTxt(wchar_t *filename, void *map,
	UINT size, int slot, UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
	::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info)
{
	FILE *fd = NULL;
	errno_t err;
	::D3DVERTEXELEMENT9 vertex_element[MAXD3DDECLLENGTH];
	UINT num_elements = 0;
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
	if (topology != ::D3DPRIMITIVETYPE(-1))
		fprintf(fd, "topology: %s\n", TopologyStr(topology));
	if (layout) {
		layout->GetDeclaration(vertex_element, &num_elements);
		dump_ia_layout(fd, vertex_element, num_elements);
	}
	if (!stride) {
		FALogErr("Cannot dump vertex buffer with stride=0\n");
		goto out_close;
	}

	if (num_elements > 0) {
		fprintf(fd, "\nvertex-data:\n");
		dump_vb_known_layout(fd, map, vertex_element, num_elements,
				size, slot, offset, first, count, stride);
	}
	else {
		dump_vb_unknown_layout(fd, map, size, slot, offset, first, count, stride);
	}

out_close:
	fclose(fd);
}

void D3D9Wrapper::FrameAnalysisDevice::dedupe_buf_filename_ib_txt(const wchar_t *bin_filename,
	wchar_t *txt_filename, size_t size, ::D3DFORMAT ib_fmt,
	UINT offset, UINT first, UINT count, ::D3DPRIMITIVETYPE topology)
{
	wchar_t *pos;
	size_t rem;

	copy_until_extension(txt_filename, bin_filename, MAX_PATH, &pos, &rem);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ib");

	if (ib_fmt != ::D3DFORMAT::D3DFMT_UNKNOWN)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-format=%S", TexFormatStrDX9(ib_fmt));

	if (topology != ::D3DPRIMITIVETYPE(-1))
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

void D3D9Wrapper::FrameAnalysisDevice::DumpIBTxt(wchar_t *filename, void *map,
	UINT size, ::D3DFORMAT format, UINT offset, UINT first, UINT count,
	::D3DPRIMITIVETYPE topology)
{
	FILE *fd = NULL;
	uint16_t *buf16 = (uint16_t*)map;
	uint32_t *buf32 = (uint32_t*)map;
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

	if (topology != ::D3DPRIMITIVETYPE(-1))
		fprintf(fd, "topology: %s\n", TopologyStr(topology));
	switch (topology) {
	case ::D3DPT_LINELIST:
		grouping = 2;
		break;
	case ::D3DPT_TRIANGLELIST:
		grouping = 3;
		break;
		// TODO: Appropriate grouping for other input topologies
	}

	switch (format) {
	case ::D3DFMT_INDEX16:
		fprintf(fd, "format: D3DFMT_INDEX16\n");

		start = offset / 2 + first;
		end = size / 2;
		if (count)
			end = min(end, start + count);

		for (i = start; i < end; i++) {
			if ((i - start) % grouping == 0)
				fprintf(fd, "\n");
			else
				fprintf(fd, " ");
			fprintf(fd, "%u", buf16[i]);
		}
		fprintf(fd, "\n");
		break;
	case ::D3DFMT_INDEX32:
		fprintf(fd, "format: D3DFMT_INDEX32\n");

		start = offset / 4 + first;
		end = size / 4;
		if (count)
			end = min(end, start + count);

		for (i = start; i < end; i++) {
			if ((i - start) % grouping == 0)
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
void D3D9Wrapper::FrameAnalysisDevice::DumpDesc(DescType *desc, const wchar_t *filename)
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
void D3D9Wrapper::FrameAnalysisDevice::determine_vb_count(UINT *count, ::IDirect3DIndexBuffer9 *staged_ib_for_vb,
	DrawCallInfo *call_info, UINT ib_off_for_vb, ::D3DFORMAT ib_fmt)
{
	void *ib_map;
	UINT i, ib_start, ib_end, max_vertex = 0;
	::D3DINDEXBUFFER_DESC ib_desc;
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

	if (!staged_ib_for_vb || !call_info || call_info->NumVertices == 0)
		return;

	staged_ib_for_vb->GetDesc(&ib_desc);
	hr = staged_ib_for_vb->Lock(0, ib_desc.Size, &ib_map, D3DLOCK_READONLY);
	if (FAILED(hr)) {
		FALogErr("determine_vb_count failed to map index buffer staging resource: 0x%x\n", hr);
		return;
	}

	buf16 = (uint16_t*)ib_map;
	buf32 = (uint32_t*)ib_map;
	switch (ib_fmt) {
	case ::D3DFMT_INDEX16:
		ib_start = ib_off_for_vb / 2 + call_info->StartIndex;
		ib_end = ib_desc.Size / 2;
		if (call_info->NumVertices)
			ib_end = min(ib_end, ib_start + call_info->NumVertices);
		for (i = ib_start; i < ib_end; i++)
			max_vertex = max(max_vertex, buf16[i]);
		*count = max_vertex + 1;
		break;
	case ::D3DFMT_INDEX32:
		ib_start = ib_off_for_vb / 4 + call_info->StartIndex;
		ib_end = ib_desc.Size / 4;
		if (call_info->NumVertices)
			ib_end = min(ib_end, ib_start + call_info->NumVertices);

		for (i = ib_start; i < ib_end; i++)
			max_vertex = max(max_vertex, buf32[i]);
		*count = max_vertex + 1;
		break;
	}
	staged_ib_for_vb->Unlock();
}
template<typename ID3D9Buffer, typename ID3D9BufferDesc>
void D3D9Wrapper::FrameAnalysisDevice::DumpBufferImmediateCtx(ID3D9Buffer *staging, ID3D9BufferDesc *orig_desc,
	wstring filename, FrameAnalysisOptions buf_type_mask, int idx,
	::D3DFORMAT ib_fmt, UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
	::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info,
	::IDirect3DIndexBuffer9 *staged_ib_for_vb, UINT ib_off_for_vb)
{
	wchar_t bin_filename[MAX_PATH], txt_filename[MAX_PATH];
	void *map;
	HRESULT hr;
	FILE *fd = NULL;
	wchar_t *bin_ext;
	size_t ext;
	errno_t err;

	hr = staging->Lock(0, orig_desc->Size, &map, D3DLOCK_READONLY);
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
			fwrite(map, 1, orig_desc->Size, fd);
			fclose(fd);
		}
		link_deduplicated_files(filename.c_str(), bin_filename);
	}

	if (analyse_options & FrameAnalysisOptions::FMT_BUF_TXT) {
		filename.replace(ext, wstring::npos, L".txt");
		FALogInfo("Dumping Buffer %S -> %S\n", filename.c_str(), txt_filename);
		if (buf_type_mask & FrameAnalysisOptions::DUMP_VB) {
			determine_vb_count(&count, staged_ib_for_vb, call_info, ib_off_for_vb, ib_fmt);
			dedupe_buf_filename_vb_txt(bin_filename, txt_filename, MAX_PATH, idx, stride, offset, first, count, layout, topology, call_info);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpVBTxt(txt_filename, &map, orig_desc->Size, idx, stride, offset, first, count, layout, topology, call_info);
			}
		}
		else if (buf_type_mask & FrameAnalysisOptions::DUMP_IB) {
			dedupe_buf_filename_ib_txt(bin_filename, txt_filename, MAX_PATH, ib_fmt, offset, first, count, topology);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpIBTxt(txt_filename, &map, orig_desc->Size, ib_fmt, offset, first, count, topology);
			}
		}
		else {
			// We don't know what kind of buffer this is, so just
			// use the generic dump routine:

			dedupe_buf_filename_txt(bin_filename, txt_filename, MAX_PATH, '?', idx, stride, offset);
			if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
				DumpBufferTxt(txt_filename, &map, orig_desc->Size, '?', idx, stride, offset);
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
	staging->Unlock();
}
static HRESULT _CreateBuffer(::IDirect3DVertexBuffer9 *buffer, ::D3DVERTEXBUFFER_DESC *desc, ::IDirect3DDevice9 *device) {
	return device->CreateVertexBuffer(desc->Size, desc->Usage, desc->FVF, desc->Pool, &buffer, NULL);
}
static HRESULT _CreateBuffer(::IDirect3DIndexBuffer9 *buffer, ::D3DINDEXBUFFER_DESC *desc, ::IDirect3DDevice9 *device) {
	return device->CreateIndexBuffer(desc->Size, desc->Usage, desc->Format, desc->Pool, &buffer, NULL);
}
template<typename ID3D9Buffer, typename ID3D9BufferDesc>
void D3D9Wrapper::FrameAnalysisDevice::DumpBuffer(ID3D9Buffer *buffer, wchar_t *filename,
	FrameAnalysisOptions buf_type_mask, int idx, ::D3DFORMAT ib_fmt,
	UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
	::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info,
	ID3D9Buffer **staged_b_ret, ::IDirect3DIndexBuffer9 *staged_ib_for_vb, UINT ib_off_for_vb)
{
	ID3D9BufferDesc desc, orig_desc;
	ID3D9Buffer *staging = NULL;
	HRESULT hr;

	// Process key inputs to allow user to abort long running frame
	// analysis sessions (this case is specifically for dump_vb and dump_ib
	// which bypasses DumpResource()):
	DispatchInputEvents(this);
	if (!G->analyse_frame)
		return;

	buffer->GetDesc(&desc);
	memcpy(&orig_desc, &desc, sizeof(ID3D9BufferDesc));

	desc.Pool = ::D3DPOOL_SYSTEMMEM;
	desc.Usage &= ~D3DUSAGE_DYNAMIC;
	hr = _CreateBuffer(staging, &desc, GetPassThroughOrigDevice());
	if (FAILED(hr)) {
		FALogErr("DumpBuffer failed to create staging buffer: 0x%x\n", hr);
		return;
	}
	void *pDataSrc;
	void *pDataDst;
	buffer->Lock(0, desc.Size, &pDataSrc, D3DLOCK_READONLY);
	staging->Lock(0, desc.Size, &pDataDst, 0);
	memcpy(pDataDst, pDataSrc, desc.Size);
	buffer->Unlock();
	staging->Unlock();
	DumpBufferImmediateCtx(staging, &orig_desc, filename, buf_type_mask, idx, ib_fmt, stride, offset, first, count, layout, topology, call_info, staged_ib_for_vb, ib_off_for_vb);

	// We can return the staged index buffer for later use when dumping the
	// vertex buffers as text, to determine the maximum vertex count:
	if (staged_b_ret) {
		*staged_b_ret = staging;
		staging->AddRef();
	}
	staging->Release();
}
void D3D9Wrapper::FrameAnalysisDevice::DumpResource(::IDirect3DResource9 *resource, wchar_t *filename,
	FrameAnalysisOptions buf_type_mask, int idx, ::D3DFORMAT format,
	UINT stride, UINT offset)
{
	::D3DRESOURCETYPE type;
	// Process key inputs to allow user to abort long running frame analysis sessions:
	DispatchInputEvents(this);
	if (!G->analyse_frame)
		return;

	type = resource->GetType();

	switch (type) {
	case ::D3DRTYPE_VERTEXBUFFER:
		if (analyse_options & FrameAnalysisOptions::FMT_BUF_MASK)
			DumpBuffer<::IDirect3DVertexBuffer9, ::D3DVERTEXBUFFER_DESC>((::IDirect3DVertexBuffer9*)resource, filename, buf_type_mask, idx, format, stride, offset,
				0, 0, NULL, ::D3DPRIMITIVETYPE(-1), NULL, NULL, NULL, 0);
		else
			FALogInfo("Skipped dumping Buffer (No buffer formats enabled): %S\n", filename);
		break;
	case ::D3DRTYPE_INDEXBUFFER:
		if (analyse_options & FrameAnalysisOptions::FMT_BUF_MASK)
			DumpBuffer<::IDirect3DIndexBuffer9, ::D3DINDEXBUFFER_DESC>((::IDirect3DIndexBuffer9*)resource, filename, buf_type_mask, idx, format, stride, offset,
				0, 0, NULL, ::D3DPRIMITIVETYPE(-1), NULL, NULL, NULL, 0);
		else
			FALogInfo("Skipped dumping Buffer (No buffer formats enabled): %S\n", filename);
		break;
	case ::D3DRTYPE_SURFACE:
		if (analyse_options & FrameAnalysisOptions::FMT_2D_MASK) {
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource<::IDirect3DSurface9>((::IDirect3DSurface9*)resource, filename, format);
			if (analyse_options & FrameAnalysisOptions::MONO)
				DumpSurface<::IDirect3DSurface9>((::IDirect3DSurface9*)resource, filename, false, NULL, format);
		}
		else
			FALogInfo("Skipped dumping surface (No surface formats enabled): %S\n", filename);
		break;
	case ::D3DRTYPE_TEXTURE:
		if (analyse_options & FrameAnalysisOptions::FMT_2D_MASK) {
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource<::IDirect3DTexture9>((::IDirect3DTexture9*)resource, filename, format);
			if (analyse_options & FrameAnalysisOptions::MONO)
				DumpSurface<::IDirect3DTexture9>((::IDirect3DTexture9*)resource, filename, false, NULL, format);
		}
		else
			FALogInfo("Skipped dumping Texture2D (No Texture2D formats enabled): %S\n", filename);
		break;
	case ::D3DRTYPE_CUBETEXTURE:
		if (analyse_options & FrameAnalysisOptions::FMT_2D_MASK) {
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource<::IDirect3DCubeTexture9>((::IDirect3DCubeTexture9*)resource, filename, format);
			if (analyse_options & FrameAnalysisOptions::MONO)
				DumpSurface<::IDirect3DCubeTexture9>((::IDirect3DCubeTexture9*)resource, filename, false, NULL, format);
		}
		else
			FALogInfo("Skipped dumping Texture2D (No Texture2D formats enabled): %S\n", filename);
		break;
	case ::D3DRTYPE_VOLUMETEXTURE:
		FALogInfo("Skipped dumping Texture3D: %S\n", filename);
		break;
	default:
		FALogInfo("Skipped dumping resource of unknown type %i: %S\n", type, filename);
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

void D3D9Wrapper::FrameAnalysisDevice::get_deduped_dir(wchar_t *path, size_t size)
{
	if (analyse_options & FrameAnalysisOptions::SHARE_DEDUPED) {
		if (!GetModuleFileName(0, path, (DWORD)size))
			return;
		wcsrchr(path, L'\\')[1] = 0;
		wcscat_s(path, size, L"FrameAnalysisDeduped");
	}
	else {
		_snwprintf_s(path, size, size, L"%ls\\deduped", G->ANALYSIS_PATH);
	}

	CreateDirectoryEnsuringAccess(path);
}

HRESULT D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisFilename(wchar_t *filename, size_t size,
	wchar_t *reg, char shader_type, int idx, D3D9Wrapper::IDirect3DResource9 *handle)
{
	struct ResourceHashInfo *info;
	uint32_t hash = 0;
	uint32_t orig_hash = 0;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);
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

	if (handle) {
		hash = handle->resourceHandleInfo.hash;
		orig_hash = handle->resourceHandleInfo.orig_hash;
	}

	if (hash) {
		try {
			info = &G->mResourceInfo.at(orig_hash);
			if (info->hash_contaminated) {
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=!");
				if (!info->lock_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"L");
				if (!info->copy_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"C");
				if (!info->region_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"R");
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"!");
			}
		}
		catch (std::out_of_range) {}

		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=%08x", hash);

		if (hash != orig_hash)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"(%08x)", orig_hash);
	}
	if (analyse_options & FrameAnalysisOptions::FILENAME_HANDLE)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"@%p", handle);

		if (mCurrentVertexShaderHandle && mCurrentVertexShaderHandle->hash)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-vs=%016I64x", mCurrentVertexShaderHandle->hash);
		if (mCurrentPixelShaderHandle && mCurrentPixelShaderHandle->hash)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ps=%016I64x", mCurrentPixelShaderHandle->hash);


	hr = StringCchPrintfW(pos, rem, L".XXX");
	if (FAILED(hr)) {
		FALogErr("Failed to create filename: 0x%x\n", hr);
		// Could create a shorter filename without hashes if this
		// becomes a problem in practice
	}

	return hr;
}
HRESULT D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisFilenameResource(wchar_t *filename, size_t size, const wchar_t *type, ::IDirect3DResource9 *handle, bool force_filename_handle, ResourceHandleInfo *handle_info)
{
	struct ResourceHashInfo *info;
	uint32_t hash = 0;
	uint32_t orig_hash = 0;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);

	// We don't allow hold to be changed mid-frame due to potential
	// for filename conflicts, so use def_analyse_options:
	if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i.", G->analyse_frame_no);
	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i.%i-", draw_call, non_draw_call_dump_counter);
	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%s", type);
	if (handle_info) {
		hash = handle_info->hash;
		orig_hash = handle_info->orig_hash;
	}
	if (hash) {
		try {
			info = &G->mResourceInfo.at(orig_hash);
			if (info->hash_contaminated) {
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=!");
				if (!info->lock_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"L");
				if (!info->copy_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"C");
				if (!info->region_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"R");
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"!");
			}
		}
		catch (std::out_of_range) {}

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

static HRESULT mapSurface(::IDirect3DSurface9 *surface, ::D3DLOCKED_RECT *map) {
	return surface->LockRect(map, NULL, D3DLOCK_READONLY);
}
static HRESULT mapSurface(::IDirect3DTexture9 *texture, ::D3DLOCKED_RECT *map) {
	return texture->LockRect(0, map, NULL, D3DLOCK_READONLY);
}
static HRESULT mapSurface(::IDirect3DCubeTexture9 *texture, ::D3DLOCKED_RECT *map) {
	return texture->LockRect(::D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, map, NULL, D3DLOCK_READONLY);
}
static HRESULT unlockSurface(::IDirect3DSurface9 *surface) {
	return surface->UnlockRect();
}
static HRESULT unlockSurface(::IDirect3DTexture9 *texture) {
	return texture->UnlockRect(0);
}
static HRESULT unlockSurface(::IDirect3DCubeTexture9 *texture) {
	return texture->UnlockRect(::D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0);
}
template<typename Surface>
const wchar_t* D3D9Wrapper::FrameAnalysisDevice::dedupe_sur2d_filename(Surface *resource,
	D3D2DTEXTURE_DESC *orig_desc, wchar_t *dedupe_filename,
	size_t size, const wchar_t *traditional_filename, ::D3DFORMAT format)
{
	::D3DLOCKED_BOX map;
	::D3DLOCKED_RECT rect;
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
	hr = mapSurface(resource, &rect);
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
	map.pBits = rect.pBits;
	map.RowPitch = rect.Pitch;
	map.SlicePitch = 0;
	hash = Calc2DDataHashAccurate(orig_desc, &map);
	hash = CalcDescHash(hash, orig_desc);
	unlockSurface(resource);

	if (format == ::D3DFMT_UNKNOWN)
		format = orig_desc->Format;

	get_deduped_dir(dedupe_dir, MAX_PATH);
	_snwprintf_s(dedupe_filename, size, size, L"%ls\\%08x-%S.XXX", dedupe_dir, hash, TexFormatStrDX9(format));

	return dedupe_filename;
err:
	return traditional_filename;
}

template<typename ID3D9Buffer, typename BufferDesc>
void D3D9Wrapper::FrameAnalysisDevice::dedupe_buf_filename(ID3D9Buffer *resource,
	BufferDesc *orig_desc,
	void *map,
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

	hash = crc32c_hw(0, map, orig_desc->Size);
	hash = crc32c_hw(hash, orig_desc, sizeof(BufferDesc));

	get_deduped_dir(dedupe_dir, MAX_PATH);
	_snwprintf_s(dedupe_filename, size, size, L"%ls\\%08x.XXX", dedupe_dir, hash);
}

void D3D9Wrapper::FrameAnalysisDevice::rotate_deduped_file(const wchar_t *dedupe_filename)
{
	wchar_t rotated_filename[MAX_PATH];
	unsigned rotate;
	size_t ext_pos;
	const wchar_t *ext;

	ext = wcsrchr(dedupe_filename, L'.');
	if (ext) {
		ext_pos = ext - dedupe_filename;
	}
	else {
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

void D3D9Wrapper::FrameAnalysisDevice::rotate_when_nearing_hard_link_limit(const wchar_t *dedupe_filename)
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

void D3D9Wrapper::FrameAnalysisDevice::link_deduplicated_files(const wchar_t *filename, const wchar_t *dedupe_filename)
{
	wchar_t relative_path[MAX_PATH] = { 0 };

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
void D3D9Wrapper::FrameAnalysisDevice::_DumpTextures(char shader_type)
{
	D3D9Wrapper::IDirect3DBaseTexture9 *texture;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;
	UINT maxSampler = 0;
	::IDirect3DTexture9 *tex2d = NULL;
	::IDirect3DCubeTexture9 *texCube = NULL;
	::IDirect3DVolumeTexture9 *tex3d = NULL;
	::IDirect3DSurface9 *sur = NULL;
	::IDirect3DVolume9 *vol = NULL;
	::D3DSURFACE_DESC sur_desc;
	::D3DVOLUME_DESC vol_desc;
	::D3DFORMAT format;

	if (shader_type == 'v') {
		maxSampler = D3D9_VERTEX_INPUT_TEXTURE_SLOT_COUNT;
	}
	else if (shader_type == 'p') {
		maxSampler = D3D9_PIXEL_INPUT_TEXTURE_SLOT_COUNT;
	}
	for (i = 0; i < maxSampler && G->analyse_frame; i++) {
		if ((i == G->StereoParamsPixelReg || (D3DDMAPSAMPLER + 1 + i == G->StereoParamsVertexReg)) || (G->helix_fix && (i == G->helix_StereoParamsPixelReg || (D3DDMAPSAMPLER + 1 + i == G->helix_StereoParamsVertexReg)))) {
			FALogInfo("Skipped 3DMigoto resource in slot %cs-t%i\n", shader_type, i);
			continue;
		}
		if (shader_type == 'v') {
			texture = m_activeTextureStages[D3DDMAPSAMPLER + (i + 1)];
		}
		else {
			texture = m_activeTextureStages[i];
		}
		::D3DRESOURCETYPE type = texture->GetD3DResource9()->GetType();
		switch (type)
		{
			case D3DRTYPE_TEXTURE:
				tex2d = ((D3D9Wrapper::IDirect3DTexture9*)texture)->GetD3DTexture9();
				tex2d->GetSurfaceLevel(0, &sur);
				sur->GetDesc(&sur_desc);
				format = sur_desc.Format;
				break;
			case D3DRTYPE_CUBETEXTURE:
				texCube = ((D3D9Wrapper::IDirect3DCubeTexture9*)texture)->GetD3DCubeTexture9();
				texCube->GetCubeMapSurface(::D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &sur);
				sur->GetDesc(&sur_desc);
				format = sur_desc.Format;
				break;
			case D3DRTYPE_VOLUMETEXTURE:
				tex3d = ((D3D9Wrapper::IDirect3DVolumeTexture9*)texture)->GetD3DVolumeTexture9();
				tex3d->GetVolumeLevel(0, &vol);
				vol->GetDesc(&vol_desc);
				format = vol_desc.Format;
				break;
		}

		// TODO: process description to get offset, strides & size for
		// buffer & bufferex type SRVs and pass down to dump routines,
		// although I have no idea how to determine which of the
		// entries in the two D3D11_BUFFER_SRV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, L"t", shader_type, i, texture);
		if (SUCCEEDED(hr)) {
			DumpResource(texture->GetD3DResource9(), filename,
				FrameAnalysisOptions::DUMP_SRV, i,
				format, 0, 0);
		}

		texture->Release();
		texture = NULL;
	}
}
void D3D9Wrapper::FrameAnalysisDevice::DumpMesh(DrawCallInfo *call_info)
{
	bool dump_ibs = !!(analyse_options & FrameAnalysisOptions::DUMP_IB);
	bool dump_vbs = !!(analyse_options & FrameAnalysisOptions::DUMP_VB);
	::IDirect3DIndexBuffer9 *staged_ib = NULL;
	::D3DFORMAT ib_fmt = ::D3DFMT_UNKNOWN;
	UINT ib_off = 0;

	// If we are dumping vertex buffers as text and an indexed draw call
	// was in use, we also need to dump (or at the very least stage) the
	// index buffer so that we can determine the maximum vertex count to
	// dump to keep the text files small. This is not applicable when only
	// dumping vertex buffers as binary, since we always dump the entire
	// buffer in that case.
	if (dump_vbs && (analyse_options & FrameAnalysisOptions::FMT_BUF_TXT) && DrawPrimitiveCountToVerticesCount(call_info->PrimitiveCount, call_info->primitive_type))
		dump_ibs = true;

	if (dump_ibs)
		DumpIB(call_info, &staged_ib, &ib_fmt, &ib_off);

	if (dump_vbs)
		DumpVBs(call_info, staged_ib, ib_fmt, ib_off);

	if (staged_ib)
		staged_ib->Release();
}

static bool vb_slot_in_layout(int slot, ::IDirect3DVertexDeclaration9 *layout)
{
	::D3DVERTEXELEMENT9 vertex_elem[MAXD3DDECLLENGTH];
	UINT layout_elements;
	UINT i;

	if (!layout)
		return true;

	layout->GetDeclaration(vertex_elem, &layout_elements);
	layout_elements--;
	for (i = 0; i < layout_elements; i++)
		if (vertex_elem[i].Stream == slot)
			return true;

	return false;
}

void D3D9Wrapper::FrameAnalysisDevice::_DumpCBs(char shader_type, wchar_t constant_type, void *buffer, UINT size, UINT start_slot, UINT num_slots)
{
	wchar_t filename[MAX_PATH], txt_filename[MAX_PATH], dedupe_dir[MAX_PATH];
	wstring _filename;
	uint32_t hash;
	HRESULT hr;
	size_t ext;
	wchar_t *txt_ext;
	if (!buffer)
		return;

	wstring reg(constant_type + L"b");
	if (start_slot != -1) {
		reg += L"_" + to_wstring(start_slot);
		if (num_slots != -1)
			reg += L"_" + to_wstring(num_slots);
	}else if(num_slots != -1)
		reg += L"_?_" + to_wstring(num_slots);

	hr = FrameAnalysisFilename(filename, MAX_PATH, const_cast< wchar_t* >(reg.c_str()), shader_type, -1, NULL);
	if (SUCCEEDED(hr)) {
		DispatchInputEvents(this);
		if (!G->analyse_frame)
			return;
		_filename = filename;

		hash = crc32c_hw(0, buffer,size);
		get_deduped_dir(dedupe_dir, MAX_PATH);
		_snwprintf_s(txt_filename, MAX_PATH, MAX_PATH, L"%ls\\%08x.XXX", dedupe_dir, hash);

		ext = _filename.find_last_of(L'.');
		txt_ext = wcsrchr(txt_filename, L'.');
		if (ext == wstring::npos || !txt_ext) {
			FALogErr("DumpBuffer: Filename missing extension\n");
			return;
		}

		_filename.replace(ext, wstring::npos, L".txt");
		wcscpy_s(txt_ext, MAX_PATH + txt_filename - txt_ext, L".txt");
		FALogInfo("Dumping Buffer %S -> %S\n", _filename.c_str(), txt_filename);

		if (GetFileAttributes(txt_filename) == INVALID_FILE_ATTRIBUTES) {
			DumpBufferTxt(txt_filename, buffer, size, '?', -1, 0, 0);
		}
		link_deduplicated_files(_filename.c_str(), txt_filename);
	}
}

void D3D9Wrapper::FrameAnalysisDevice::DumpCBs()
{
	if (mCurrentVertexShaderHandle && mCurrentVertexShaderHandle->hash) {
		float floats[D3D9_MAX_VERTEX_FLOAT_CONSTANT_SLOT_COUNT * 4];
		GetPassThroughOrigDevice()->GetVertexShaderConstantF(0, floats, D3D9_MAX_VERTEX_FLOAT_CONSTANT_SLOT_COUNT);
		_DumpCBs('v', 'f', floats, D3D9_MAX_VERTEX_FLOAT_CONSTANT_SLOT_COUNT * 16, -1, -1);
		int ints[D3D9_MAX_INT_CONSTANT_SLOT_COUNT * 4];
		GetPassThroughOrigDevice()->GetVertexShaderConstantI(0, ints, D3D9_MAX_INT_CONSTANT_SLOT_COUNT);
		_DumpCBs('v', 'i', ints, D3D9_MAX_INT_CONSTANT_SLOT_COUNT * 16, -1, -1);
		BOOL bools[D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT * 4];
		GetPassThroughOrigDevice()->GetVertexShaderConstantB(0, bools, D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT);
		_DumpCBs('v', 'b', bools, D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT * 16, -1, -1);
	}
	if (mCurrentPixelShaderHandle && mCurrentPixelShaderHandle->hash) {
		float floats[D3D9_MAX_PIXEL_FLOAT_CONSTANT_SLOT_COUNT * 4];
		GetPassThroughOrigDevice()->GetPixelShaderConstantF(0, floats, D3D9_MAX_PIXEL_FLOAT_CONSTANT_SLOT_COUNT);
		_DumpCBs('p', 'f', floats, D3D9_MAX_PIXEL_FLOAT_CONSTANT_SLOT_COUNT * 16, -1, -1);
		int ints[D3D9_MAX_INT_CONSTANT_SLOT_COUNT * 4];
		GetPassThroughOrigDevice()->GetPixelShaderConstantI(0, ints, D3D9_MAX_INT_CONSTANT_SLOT_COUNT);
		_DumpCBs('p', 'i', ints, D3D9_MAX_INT_CONSTANT_SLOT_COUNT * 16, -1, -1);
		BOOL bools[D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT * 4];
		GetPassThroughOrigDevice()->GetPixelShaderConstantB(0, bools, D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT);
		_DumpCBs('p', 'b', bools, D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT * 16, -1, -1);
	}
}

void D3D9Wrapper::FrameAnalysisDevice::DumpCB(char shader_type, wchar_t constant_type, UINT start_slot, UINT num_slots)
{
	vector<float> floats;
	vector<int> ints;
	vector<BOOL> bools;
	if (shader_type == 'v' && mCurrentVertexShaderHandle && mCurrentVertexShaderHandle->hash) {
		switch (constant_type) {
		case 'f':
			if (start_slot >= 0 && (start_slot + num_slots) < D3D9_MAX_VERTEX_FLOAT_CONSTANT_SLOT_COUNT) {
				floats.assign((num_slots * 4), 0.0);
				GetPassThroughOrigDevice()->GetVertexShaderConstantF(start_slot, floats.data(), num_slots);
				_DumpCBs(shader_type, constant_type, floats.data(), (num_slots * 16), start_slot, num_slots);
			}
			break;
		case 'i':
			if (start_slot >= 0 && (start_slot + num_slots) < D3D9_MAX_INT_CONSTANT_SLOT_COUNT) {
				ints.assign((num_slots * 4), 0);
				GetPassThroughOrigDevice()->GetVertexShaderConstantI(start_slot, ints.data(), num_slots);
				_DumpCBs(shader_type, constant_type, ints.data(), (num_slots * 16), start_slot, num_slots);
			}
			break;
		case 'b':
			if (start_slot >= 0 && (start_slot + num_slots) < D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT) {
				bools.assign((num_slots * 4), 0);
				GetPassThroughOrigDevice()->GetVertexShaderConstantB(start_slot, bools.data(), num_slots);
				_DumpCBs(shader_type, constant_type, bools.data(), (num_slots * 16), start_slot, num_slots);
			}
			break;
		}
		return;
	}

	if (shader_type == 'p' && mCurrentPixelShaderHandle && mCurrentPixelShaderHandle->hash) {
		switch (constant_type) {
		case 'f':
			if (start_slot >= 0 && (start_slot + num_slots) < D3D9_MAX_PIXEL_FLOAT_CONSTANT_SLOT_COUNT) {
				floats.assign((num_slots * 4), 0.0);
				GetPassThroughOrigDevice()->GetPixelShaderConstantF(start_slot, floats.data(), num_slots);
				_DumpCBs(shader_type, constant_type, floats.data(), (num_slots * 16), start_slot, num_slots);
			}
			break;
		case 'i':
			if (start_slot >= 0 && (start_slot + num_slots) < D3D9_MAX_INT_CONSTANT_SLOT_COUNT) {
				ints.assign((num_slots * 4), 0);
				GetPassThroughOrigDevice()->GetPixelShaderConstantI(start_slot, ints.data(), num_slots);
				_DumpCBs(shader_type, constant_type, ints.data(), (num_slots * 16), start_slot, num_slots);
			}
			break;
		case 'b':
			if (start_slot >= 0 && (start_slot + num_slots) < D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT) {
				bools.assign((num_slots * 4), 0);
				GetPassThroughOrigDevice()->GetPixelShaderConstantB(start_slot, bools.data(), num_slots);
				_DumpCBs(shader_type, constant_type, bools.data(), (num_slots * 16), start_slot, num_slots);
			}
			break;
		}
		return;
	}
}

void D3D9Wrapper::FrameAnalysisDevice::DumpVBs(DrawCallInfo *call_info, ::IDirect3DIndexBuffer9 *staged_ib, ::D3DFORMAT ib_fmt, UINT ib_off)
{
	::IDirect3DVertexBuffer9 *buffer;
	UINT stride;
	UINT offset;
	::D3DPRIMITIVETYPE topology = call_info->primitive_type;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i, first = 0, count = 0;
	::IDirect3DVertexDeclaration9 *vertex_decl = NULL;
	ID3DBlob *layout_desc = NULL;

	if (call_info) {
		first = call_info->StartVertex;
		count = DrawPrimitiveCountToVerticesCount(call_info->PrimitiveCount, call_info->primitive_type);
	}

	// The format of each vertex buffer cannot be obtained from this call.
	// Rather, it is available in the input layout assigned to the
	// pipeline, and there is no API to get the layout description, so we
	// store it in a blob attached to the layout when it was created that
	// we retrieve here.
	GetPassThroughOrigDevice()->GetVertexDeclaration(&vertex_decl);
	for (i = 0; i < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (m_activeVertexBuffers.find(i) == m_activeVertexBuffers.end())
			continue;
		D3D9Wrapper::activeVertexBuffer vertexBuffer = m_activeVertexBuffers[i];
		if (!vertexBuffer.m_vertexBuffer)
			continue;
		buffer = vertexBuffer.m_vertexBuffer->GetD3DVertexBuffer9();
		offset = vertexBuffer.m_offsetInBytes;
		stride = vertexBuffer.m_pStride;
		// Skip this vertex buffer if it is not used in the IA layout:
		if (!vb_slot_in_layout(i, vertex_decl))
			goto continue_release;

		hr = FrameAnalysisFilename(filename, MAX_PATH, L"vb", NULL, i, vertexBuffer.m_vertexBuffer);
		if (SUCCEEDED(hr)) {
			DumpBuffer<::IDirect3DVertexBuffer9, ::D3DVERTEXBUFFER_DESC>(buffer, filename,
				FrameAnalysisOptions::DUMP_VB, i,
				ib_fmt, stride, offset,
				first, count, vertex_decl, topology,
				call_info, NULL, staged_ib, ib_off);
		}

	continue_release:
		buffer->Release();
		buffer = NULL;
	}
	// Although the documentation fails to mention it, GetPrivateData()
	// does bump the refcount if SetPrivateDataInterface() was used, so we
	// need to balance it here:
	if (vertex_decl)
		vertex_decl->Release();
}

void D3D9Wrapper::FrameAnalysisDevice::DumpIB(DrawCallInfo *call_info, ::IDirect3DIndexBuffer9 **staged_ib, ::D3DFORMAT *format, UINT *offset)
{
	::IDirect3DIndexBuffer9 *buffer = NULL;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT first = 0, count = 0;
	::D3DPRIMITIVETYPE topology = call_info->primitive_type;

	if (call_info) {
		first = call_info->StartIndex;
		count = DrawPrimitiveCountToVerticesCount(call_info->PrimitiveCount, call_info->primitive_type);
	}
	if (!m_activeIndexBuffer)
		return;

	buffer = m_activeIndexBuffer->GetD3DIndexBuffer9();
	hr = FrameAnalysisFilename(filename, MAX_PATH, L"ib", NULL, -1, m_activeIndexBuffer);
	if (SUCCEEDED(hr)) {
		DumpBuffer<::IDirect3DIndexBuffer9, ::D3DINDEXBUFFER_DESC>(buffer, filename,
			FrameAnalysisOptions::DUMP_IB, -1,
			*format, 0, *offset, first, count, NULL,
			topology, call_info, staged_ib, NULL, 0);
	}

	buffer->Release();
}

void D3D9Wrapper::FrameAnalysisDevice::DumpTextures()
{
	if (mCurrentVertexShaderHandle) {
		_DumpTextures('v');
	}
	if (mCurrentPixelShaderHandle) {
		_DumpTextures('p');
	}
}

void D3D9Wrapper::FrameAnalysisDevice::DumpRenderTargets()
{
	UINT i;
	D3D9Wrapper::IDirect3DSurface9 *rtv = NULL;
	::D3DSURFACE_DESC sur_desc;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	for (i = 0; i < D3D9_SIMULTANEOUS_RENDER_TARGET_COUNT && G->analyse_frame; ++i) {
		if ((i >= m_activeRenderTargets.size()) || (i < 0))
			continue;
		rtv = m_activeRenderTargets[i];
		if (!rtv)
			continue;
		rtv->GetD3DSurface9()->GetDesc(&sur_desc);

		// TODO: process description to get offset, strides & size for
		// buffer type RTVs and pass down to dump routines, although I
		// have no idea how to determine which of the entries in the
		// two D3D11_BUFFER_RTV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, L"o", NULL, i, rtv);
		if (SUCCEEDED(hr)) {
			DumpResource(rtv->GetD3DResource9(), filename,
				FrameAnalysisOptions::DUMP_RT, i,
				sur_desc.Format, 0, 0);
		}

		rtv->GetD3DSurface9()->Release();
		rtv = NULL;
	}
}

void D3D9Wrapper::FrameAnalysisDevice::DumpDepthStencilTargets()
{
	D3D9Wrapper::IDirect3DSurface9 *dsv = NULL;
	::D3DSURFACE_DESC sur_desc;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	if (!m_pActiveDepthStencil)
		return;

	dsv = m_pActiveDepthStencil;
	dsv->GetD3DSurface9()->GetDesc(&sur_desc);

	hr = FrameAnalysisFilename(filename, MAX_PATH, L"oD", NULL, -1, dsv);
	if (SUCCEEDED(hr)) {
		DumpResource(dsv->GetD3DResource9(), filename, FrameAnalysisOptions::DUMP_DEPTH,
			-1, sur_desc.Format, 0, 0);
	}
	dsv->GetD3DSurface9()->Release();
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisClearRT(::IDirect3DSurface9 *target)
{
	// FIXME: Do this before each draw call instead of when render targets
	// are assigned to fix assigned render targets not being cleared, and
	// work better with frame analysis triggers

	if (!(G->cur_analyse_options & FrameAnalysisOptions::CLEAR_RT))
		return;
	if (G->frame_analysis_seen_rts.count(target))
		return;
	G->frame_analysis_seen_rts.insert(target);

	GetPassThroughOrigDevice()->Clear(0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
}
void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisTrigger(FrameAnalysisOptions new_options)
{
	if (new_options & FrameAnalysisOptions::PERSIST) {
		G->cur_analyse_options = new_options;
	}
	else {
		if (oneshot_valid)
			oneshot_analyse_options |= new_options;
		else
			oneshot_analyse_options = new_options;
		oneshot_valid = true;
	}
}

void D3D9Wrapper::FrameAnalysisDevice::update_per_draw_analyse_options()
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

void D3D9Wrapper::FrameAnalysisDevice::update_stereo_dumping_mode()
{
	NvU8 stereo = false;
	NvAPIOverride();
	Profiling::NvAPI_Stereo_IsEnabled(&stereo);
	if (stereo)
		Profiling::NvAPI_Stereo_IsActivated(mStereoHandle, &stereo);

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

void D3D9Wrapper::FrameAnalysisDevice::set_default_dump_formats(bool draw)
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
		}
		else {
			// Command list dump, or dump_on_update/unmap - we always want
			// to dump both textures and buffers. For buffers we default to
			// both binary and text for now:
			analyse_options |= FrameAnalysisOptions::FMT_BUF_TXT;
			analyse_options |= FrameAnalysisOptions::FMT_BUF_BIN;
		}
	}
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisAfterDraw(DrawCallInfo *call_info)
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
	update_stereo_dumping_mode();
	set_default_dump_formats(true);

	if ((analyse_options & FrameAnalysisOptions::FMT_2D_MASK) &&
		(analyse_options & FrameAnalysisOptions::STEREO)){// &&
		// Enable reverse stereo blit for all resources we are about to dump:
		if (!G->stereoblit_control_set_once) {
			nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, true);
			if (nvret != NVAPI_OK) {
				FALogErr("DumpStereoResource failed to enable reverse stereo blit\n");
				// Continue anyway, we should still be able to dump in 2D...
			}
		}
	}
	if (analyse_options & FrameAnalysisOptions::DUMP_CB)
		DumpCBs();
	DumpMesh(call_info);

	if (analyse_options & FrameAnalysisOptions::DUMP_SRV)
		DumpTextures();

	if (analyse_options & FrameAnalysisOptions::DUMP_RT) {
		DumpRenderTargets();
	}

	if (analyse_options & FrameAnalysisOptions::DUMP_DEPTH)
		DumpDepthStencilTargets();
	if ((analyse_options & FrameAnalysisOptions::FMT_2D_MASK) &&
		(analyse_options & FrameAnalysisOptions::STEREO)){// &&
		if (!G->stereoblit_control_set_once)
			Profiling::NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, false);
	}
	draw_call++;
}

void D3D9Wrapper::FrameAnalysisDevice::_FrameAnalysisAfterUpdate(D3D9Wrapper::IDirect3DResource9 *resource,
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

	// We don't have a view at this point to get a fully typed format, so
	// we leave format as DXGI_FORMAT_UNKNOWN, which will use the format
	// from the resource description.

	hr = FrameAnalysisFilenameResource(filename, MAX_PATH, type, resource->GetD3DResource9(), true, &resource->resourceHandleInfo);
	if (SUCCEEDED(hr)) {
		DumpResource(resource->GetD3DResource9(), filename, analyse_options, -1, ::D3DFMT_UNKNOWN, 0, 0);
	}

	LeaveCriticalSection(&G->mCriticalSection);

	non_draw_call_dump_counter++;
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisAfterUnmap(D3D9Wrapper::IDirect3DResource9 *resource)
{
	_FrameAnalysisAfterUpdate(resource, FrameAnalysisOptions::DUMP_ON_UNMAP, L"unmap");
}

void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisAfterUpdate(D3D9Wrapper::IDirect3DResource9 *resource)
{
	_FrameAnalysisAfterUpdate(resource, FrameAnalysisOptions::DUMP_ON_UPDATE, L"update");
}
void D3D9Wrapper::FrameAnalysisDevice::FrameAnalysisDump(::IDirect3DResource9 *resource, FrameAnalysisOptions options,
	const wchar_t *target, ::D3DFORMAT format, UINT stride, UINT offset, ResourceHandleInfo *info)
{
	wchar_t filename[MAX_PATH];
	NvAPI_Status nvret;
	HRESULT hr;

	analyse_options = options;
	update_stereo_dumping_mode();
	set_default_dump_formats(false);

	if ((analyse_options & FrameAnalysisOptions::STEREO)){// &&
		// Enable reverse stereo blit for all resources we are about to dump:
		if (!G->stereoblit_control_set_once) {
			nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, true);
			if (nvret != NVAPI_OK) {
				FALogErr("FrameAnalyisDump failed to enable reverse stereo blit\n");
				// Continue anyway, we should still be able to dump in 2D...
			}
		}
	}

	EnterCriticalSection(&G->mCriticalSection);

	hr = FrameAnalysisFilenameResource(filename, MAX_PATH, target, resource, false, info);
	if (FAILED(hr)) {
		// If the ini section and resource name makes the filename too
		// long, try again without them:
		hr = FrameAnalysisFilenameResource(filename, MAX_PATH, L"...", resource, false, info);
	}
	if (SUCCEEDED(hr))
		DumpResource(resource, filename, analyse_options, -1, format, stride, offset);

	LeaveCriticalSection(&G->mCriticalSection);

	if ((analyse_options & FrameAnalysisOptions::STEREO)){
		if (!G->stereoblit_control_set_once)
			Profiling::NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, false);
	}

	non_draw_call_dump_counter++;
}
inline STDMETHODIMP_(ULONG) D3D9Wrapper::FrameAnalysisDevice::AddRef(void)
{
	return D3D9Wrapper::IDirect3DDevice9::AddRef();
}

inline STDMETHODIMP_(ULONG) D3D9Wrapper::FrameAnalysisDevice::Release(void)
{
	return D3D9Wrapper::IDirect3DDevice9::Release();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::QueryInterface(REFIID riid, void ** ppvObj)
{
	if (ppvObj && IsEqualIID(riid, IID_FrameAnalysisDevice9)) {
		// This is a special case - only 3DMigoto itself should know
		// this IID, so this is us checking if it has a FrameAnalysisContext.
		// There's no need to call through to DX for this one.
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return D3D9Wrapper::IDirect3DDevice9::QueryInterface(riid, ppvObj);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::TestCooperativeLevel(void)
{
	return D3D9Wrapper::IDirect3DDevice9::TestCooperativeLevel();
}

inline STDMETHODIMP_(UINT) D3D9Wrapper::FrameAnalysisDevice::GetAvailableTextureMem(void)
{
	return D3D9Wrapper::IDirect3DDevice9::GetAvailableTextureMem();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::EvictManagedResources(void)
{
	return D3D9Wrapper::IDirect3DDevice9::EvictManagedResources();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetDirect3D(D3D9Wrapper::IDirect3D9 ** ppD3D9)
{
	return D3D9Wrapper::IDirect3DDevice9::GetDirect3D(ppD3D9);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetDeviceCaps(::D3DCAPS9 * pCaps)
{
	return D3D9Wrapper::IDirect3DDevice9::GetDeviceCaps(pCaps);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetDisplayMode(UINT iSwapChain, ::D3DDISPLAYMODE * pMode)
{
	return D3D9Wrapper::IDirect3DDevice9::GetDisplayMode(iSwapChain, pMode);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetCreationParameters(::D3DDEVICE_CREATION_PARAMETERS * pParameters)
{
	return D3D9Wrapper::IDirect3DDevice9::GetCreationParameters(pParameters);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, D3D9Wrapper::IDirect3DSurface9 * pCursorBitmap)
{
	return D3D9Wrapper::IDirect3DDevice9::SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

inline STDMETHODIMP_(void) D3D9Wrapper::FrameAnalysisDevice::SetCursorPosition(int X, int Y, DWORD Flags)
{
	return D3D9Wrapper::IDirect3DDevice9::SetCursorPosition(X, Y, Flags);
}

inline STDMETHODIMP_(BOOL) D3D9Wrapper::FrameAnalysisDevice::ShowCursor(BOOL bShow)
{
	return D3D9Wrapper::IDirect3DDevice9::ShowCursor(bShow);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateAdditionalSwapChain(::D3DPRESENT_PARAMETERS * pPresentationParameters, D3D9Wrapper::IDirect3DSwapChain9 ** pSwapChain)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetSwapChain(UINT iSwapChain, D3D9Wrapper::IDirect3DSwapChain9 ** pSwapChain)
{
	return D3D9Wrapper::IDirect3DDevice9::GetSwapChain(iSwapChain, pSwapChain);
}

inline STDMETHODIMP_(UINT) D3D9Wrapper::FrameAnalysisDevice::GetNumberOfSwapChains(void)
{
	return D3D9Wrapper::IDirect3DDevice9::GetNumberOfSwapChains();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::Reset(::D3DPRESENT_PARAMETERS * pPresentationParameters)
{
	return D3D9Wrapper::IDirect3DDevice9::Reset(pPresentationParameters);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::Present(CONST RECT * pSourceRect, CONST RECT * pDestRect, HWND hDestWindowOverride, CONST RGNDATA * pDirtyRegion)
{
	return D3D9Wrapper::IDirect3DDevice9::Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, ::D3DBACKBUFFER_TYPE Type, D3D9Wrapper::IDirect3DSurface9 ** ppBackBuffer)
{
	return D3D9Wrapper::IDirect3DDevice9::GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetRasterStatus(UINT iSwapChain, ::D3DRASTER_STATUS * pRasterStatus)
{
	return D3D9Wrapper::IDirect3DDevice9::GetRasterStatus(iSwapChain, pRasterStatus);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetDialogBoxMode(BOOL bEnableDialogs)
{
	return D3D9Wrapper::IDirect3DDevice9::SetDialogBoxMode(bEnableDialogs);
}

inline STDMETHODIMP_(void) D3D9Wrapper::FrameAnalysisDevice::SetGammaRamp(UINT iSwapChain, DWORD Flags, const ::D3DGAMMARAMP * pRamp)
{
	return D3D9Wrapper::IDirect3DDevice9::SetGammaRamp(iSwapChain, Flags, pRamp);
}

inline STDMETHODIMP_(void) D3D9Wrapper::FrameAnalysisDevice::GetGammaRamp(UINT iSwapChain, ::D3DGAMMARAMP * pRamp)
{
	return D3D9Wrapper::IDirect3DDevice9::GetGammaRamp(iSwapChain, pRamp);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DTexture9 ** ppTexture, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DVolumeTexture9 ** ppVolumeTexture, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DCubeTexture9 ** ppCubeTexture, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DVertexBuffer9 ** ppVertexBuffer, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateIndexBuffer(UINT Length, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DIndexBuffer9 ** ppIndexBuffer, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateRenderTarget(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateDepthStencilSurface(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::UpdateSurface(D3D9Wrapper::IDirect3DSurface9 * pSourceSurface, CONST RECT * pSourceRect, D3D9Wrapper::IDirect3DSurface9 * pDestinationSurface, CONST POINT * pDestPoint)
{
	::IDirect3DSurface9 *baseSource = baseSurface9(pSourceSurface);
	::IDirect3DSurface9 *baseDest = baseSurface9(pDestinationSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedSource = wrappedSurface9(pSourceSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedDest = wrappedSurface9(pDestinationSurface);
	FrameAnalysisLog("UpdateSurface(pSourceSurface:0x%p, pSourceRect:0x%p, pDestinationSurface:0x%p, pDestPoint:0x%p)\n",
		baseSource, pSourceRect, baseDest, pDestPoint);
	FrameAnalysisLogResource(-1, "Src", wrappedSource);
	FrameAnalysisLogResource(-1, "Dst", wrappedDest);
	return D3D9Wrapper::IDirect3DDevice9::UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
	if (G->analyse_frame)
		FrameAnalysisAfterUpdate(wrappedDest);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::UpdateTexture(D3D9Wrapper::IDirect3DBaseTexture9 * pSourceTexture, D3D9Wrapper::IDirect3DBaseTexture9 * pDestinationTexture)
{
	::IDirect3DBaseTexture9 *baseSource = baseTexture9(pSourceTexture);
	::IDirect3DBaseTexture9 *baseDest = baseTexture9(pDestinationTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedSource = wrappedTexture9(pSourceTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedDest = wrappedTexture9(pDestinationTexture);
	FrameAnalysisLog("UpdateTexture(pSourceTexture:0x%p, pDestinationTexture:0x%p)\n",
		baseSource, baseDest);
	FrameAnalysisLogResource(-1, "Src", wrappedSource);
	FrameAnalysisLogResource(-1, "Dst", wrappedDest);
	FrameAnalysisLogResourceHash(wrappedDest);
	return D3D9Wrapper::IDirect3DDevice9::UpdateTexture(pSourceTexture, pDestinationTexture);
	if (G->analyse_frame)
		FrameAnalysisAfterUpdate(wrappedDest);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetRenderTargetData(D3D9Wrapper::IDirect3DSurface9 * pRenderTarget, D3D9Wrapper::IDirect3DSurface9 * pDestSurface)
{
	::IDirect3DSurface9 *baseSource = baseSurface9(pRenderTarget);
	::IDirect3DSurface9 *baseDest = baseSurface9(pDestSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedSource = wrappedSurface9(pRenderTarget);
	D3D9Wrapper::IDirect3DSurface9 *wrappedDest = wrappedSurface9(pDestSurface);
	FrameAnalysisLog("GetRenderTargetData(pRenderTarget:0x%p, pDestSurface:0x%p)\n",
		baseSource, baseDest);
	FrameAnalysisLogResource(-1, "Src", wrappedSource);
	FrameAnalysisLogResource(-1, "Dst", wrappedDest);
	return D3D9Wrapper::IDirect3DDevice9::GetRenderTargetData(pRenderTarget, pDestSurface);
	if (G->analyse_frame)
		FrameAnalysisAfterUpdate(wrappedDest);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetFrontBufferData(UINT iSwapChain, D3D9Wrapper::IDirect3DSurface9 * pDestSurface)
{
	return D3D9Wrapper::IDirect3DDevice9::GetFrontBufferData(iSwapChain, pDestSurface);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::StretchRect(D3D9Wrapper::IDirect3DSurface9 * pSourceSurface, CONST RECT * pSourceRect, D3D9Wrapper::IDirect3DSurface9 * pDestSurface, CONST RECT * pDestRect, ::D3DTEXTUREFILTERTYPE Filter)
{
	::IDirect3DSurface9 *baseSource = baseSurface9(pSourceSurface);
	::IDirect3DSurface9 *baseDest = baseSurface9(pDestSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedSource = wrappedSurface9(pSourceSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedDest = wrappedSurface9(pDestSurface);
	FrameAnalysisLog("StretchRect(pSourceSurface:0x%p, pSourceRect:0x%p, pDestSurface:0x%p, pDestRect:0x%p, Filter:0x%p)\n",
		baseSource, pSourceRect, baseDest, pDestRect, Filter);
	FrameAnalysisLogResource(-1, "Src", wrappedSource);
	FrameAnalysisLogResource(-1, "Dst", wrappedDest);
	return D3D9Wrapper::IDirect3DDevice9::StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
	if (G->analyse_frame)
		FrameAnalysisAfterUpdate(wrappedDest);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::ColorFill(D3D9Wrapper::IDirect3DSurface9 * pSurface, CONST RECT * pRect, ::D3DCOLOR color)
{
	return D3D9Wrapper::IDirect3DDevice9::ColorFill(pSurface, pRect, color);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateOffscreenPlainSurface(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetRenderTarget(DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 * pRenderTarget)
{
	::LPDIRECT3DSURFACE9 baseRenderTarget = baseSurface9(pRenderTarget);
	D3D9Wrapper::IDirect3DSurface9 *wrappedRenderTarget = wrappedSurface9(pRenderTarget);
	FrameAnalysisLog("SetRenderTarget(RenderTargetIndex:%u, pRenderTarget:0x%p)\n",
		RenderTargetIndex, baseRenderTarget);
	FrameAnalysisLogResource(RenderTargetIndex, "RT", wrappedRenderTarget);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::SetRenderTarget(RenderTargetIndex, pRenderTarget);
	if (G->analyse_frame && baseRenderTarget) {
		FrameAnalysisClearRT(baseRenderTarget);
	}
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetRenderTarget(DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 ** ppRenderTarget)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetRenderTarget(RenderTargetIndex, ppRenderTarget);
	::IDirect3DSurface9 *baseRenderTarget = baseSurface9(*ppRenderTarget);
	D3D9Wrapper::IDirect3DSurface9 *wrappedRenderTarget = wrappedSurface9(*ppRenderTarget);
	FrameAnalysisLog("GetRenderTarget(RenderTargetIndex:%x, ppRenderTarget:0x%p)\n",
		baseRenderTarget, baseRenderTarget);
	FrameAnalysisLogResource(RenderTargetIndex, "RT", wrappedRenderTarget);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetDepthStencilSurface(D3D9Wrapper::IDirect3DSurface9 * pNewZStencil)
{
	::LPDIRECT3DSURFACE9 baseStencil = baseSurface9(pNewZStencil);
	D3D9Wrapper::IDirect3DSurface9 *wrappedStencil = wrappedSurface9(pNewZStencil);
	FrameAnalysisLog("SetDepthStencilSurface(pNewZStencil:0x%p)\n",
		baseStencil);
	FrameAnalysisLogResource(-1, "D", wrappedStencil);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::SetDepthStencilSurface(pNewZStencil);
	if (G->analyse_frame && baseStencil) {
		FrameAnalysisClearRT(baseStencil);
	}
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetDepthStencilSurface(D3D9Wrapper::IDirect3DSurface9 ** ppZStencilSurface)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetDepthStencilSurface(ppZStencilSurface);
	::IDirect3DSurface9 *baseDepthSurface = baseSurface9(*ppZStencilSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedDepthSurface = wrappedSurface9(*ppZStencilSurface);
	FrameAnalysisLog("GetDepthStencilSurface(RenderTargetIndex:%x, ppRenderTarget:0x%p)\n",
		baseDepthSurface);
	FrameAnalysisLogResource(-1, "D", wrappedDepthSurface);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::BeginScene(void)
{
	FrameAnalysisLog("BeginScene()\n");
	return D3D9Wrapper::IDirect3DDevice9::BeginScene();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::EndScene(void)
{
	FrameAnalysisLog("EndScene()\n");
	return D3D9Wrapper::IDirect3DDevice9::EndScene();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::Clear(DWORD Count, const ::D3DRECT * pRects, DWORD Flags, ::D3DCOLOR Color, float Z, DWORD Stencil)
{
	FrameAnalysisLog("Clear(Count:%x, pRects:0x%p, Flags:0x%p, Color:0x%p, float:%f Stencil:%x\n)",
		Count, pRects, Flags, Color, Z, Stencil);
	return D3D9Wrapper::IDirect3DDevice9::Clear(Count, pRects, Flags, Color, Z, Stencil);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetTransform(::D3DTRANSFORMSTATETYPE State, const ::D3DMATRIX * pMatrix)
{
	return D3D9Wrapper::IDirect3DDevice9::SetTransform(State, pMatrix);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetTransform(::D3DTRANSFORMSTATETYPE State, ::D3DMATRIX * pMatrix)
{
	return D3D9Wrapper::IDirect3DDevice9::GetTransform(State, pMatrix);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::MultiplyTransform(::D3DTRANSFORMSTATETYPE a, const ::D3DMATRIX * b)
{
	return D3D9Wrapper::IDirect3DDevice9::MultiplyTransform(a, b);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetViewport(const ::D3DVIEWPORT9 * pViewport)
{
	FrameAnalysisLog("SetViewport(pViewport:0x%p)\n",
		pViewport);
	return D3D9Wrapper::IDirect3DDevice9::SetViewport(pViewport);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetViewport(::D3DVIEWPORT9 * pViewport)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetViewport(pViewport);
	FrameAnalysisLog("GetViewport(pViewport:0x%p)\n",
		pViewport);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetMaterial(const ::D3DMATERIAL9 * pMaterial)
{
	return D3D9Wrapper::IDirect3DDevice9::SetMaterial(pMaterial);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetMaterial(::D3DMATERIAL9 * pMaterial)
{
	return D3D9Wrapper::IDirect3DDevice9::GetMaterial(pMaterial);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetLight(DWORD Index, const ::D3DLIGHT9 * Light)
{
	return D3D9Wrapper::IDirect3DDevice9::SetLight(Index, Light);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetLight(DWORD Index, ::D3DLIGHT9 *Light)
{
	return D3D9Wrapper::IDirect3DDevice9::GetLight(Index, Light);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::LightEnable(DWORD Index, BOOL Enable)
{
	return D3D9Wrapper::IDirect3DDevice9::LightEnable(Index, Enable);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetLightEnable(DWORD Index, BOOL * pEnable)
{
	return D3D9Wrapper::IDirect3DDevice9::GetLightEnable(Index, pEnable);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetClipPlane(DWORD Index, CONST float * pPlane)
{
	return D3D9Wrapper::IDirect3DDevice9::SetClipPlane(Index, pPlane);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetClipPlane(DWORD Index, float * pPlane)
{
	return D3D9Wrapper::IDirect3DDevice9::GetClipPlane(Index, pPlane);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetRenderState(::D3DRENDERSTATETYPE State, DWORD Value)
{
	FrameAnalysisLog("SetRenderState(State:%d, Value:%x)\n",
		State, Value);
	return D3D9Wrapper::IDirect3DDevice9::SetRenderState(State, Value);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetRenderState(::D3DRENDERSTATETYPE State, DWORD * pValue)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetRenderState(State, pValue);
	FrameAnalysisLog("GetRenderState(State:%d, Value:%x)\n",
		State, pValue);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateStateBlock(::D3DSTATEBLOCKTYPE Type, D3D9Wrapper::IDirect3DStateBlock9 ** ppSB)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateStateBlock(Type, ppSB);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::BeginStateBlock(void)
{
	return D3D9Wrapper::IDirect3DDevice9::BeginStateBlock();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::EndStateBlock(D3D9Wrapper::IDirect3DStateBlock9 ** ppSB)
{
	return D3D9Wrapper::IDirect3DDevice9::EndStateBlock(ppSB);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetClipStatus(const ::D3DCLIPSTATUS9 * pClipStatus)
{
	return D3D9Wrapper::IDirect3DDevice9::SetClipStatus(pClipStatus);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetClipStatus(::D3DCLIPSTATUS9 * pClipStatus)
{
	return D3D9Wrapper::IDirect3DDevice9::GetClipStatus(pClipStatus);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetTexture(DWORD Stage, D3D9Wrapper::IDirect3DBaseTexture9 ** ppTexture)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetTexture(Stage, ppTexture);
	::IDirect3DBaseTexture9 *baseTexture = baseTexture9(*ppTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedTexture = wrappedTexture9(*ppTexture);
	FrameAnalysisLog("GetTexture(Stage:%u, ppTexture:0x%p)\n",
		Stage, baseTexture);
	FrameAnalysisLogResource(Stage, "Stage", wrappedTexture);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetTexture(DWORD Stage, D3D9Wrapper::IDirect3DBaseTexture9 * pTexture)
{
	::IDirect3DBaseTexture9 *baseTexture = baseTexture9(pTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedTexture = wrappedTexture9(pTexture);
	FrameAnalysisLog("SetTexture(Stage:%u, pTexture:0x%p)\n",
		Stage, baseTexture);
	FrameAnalysisLogResource(Stage, "Stage", wrappedTexture);

	return D3D9Wrapper::IDirect3DDevice9::SetTexture(Stage, pTexture);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetTextureStageState(DWORD Stage, ::D3DTEXTURESTAGESTATETYPE Type, DWORD * pValue)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetTextureStageState(Stage, Type, pValue);
	FrameAnalysisLog("GetTextureStageState(Stage:%x, State:%d, Value:%x)\n",
		Stage, Type, pValue);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetTextureStageState(DWORD Stage, ::D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	FrameAnalysisLog("SetTextureStageState(Stage:%x, State:%d, Value:%x)\n",
		Stage, Type, Value);
	return D3D9Wrapper::IDirect3DDevice9::SetTextureStageState(Stage, Type, Value);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetSamplerState(DWORD Sampler, ::D3DSAMPLERSTATETYPE Type, DWORD * pValue)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetSamplerState(Sampler, Type, pValue);
	FrameAnalysisLog("GetSamplerState(Stage:%x, State:%d, Value:%x)\n",
		Sampler, Type, pValue);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetSamplerState(DWORD Sampler, ::D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	FrameAnalysisLog("SetSamplerState(Sampler:%u, State:%d, Value:%x)\n",
		Sampler, Type, Value);
	return D3D9Wrapper::IDirect3DDevice9::SetSamplerState(Sampler, Type, Value);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::ValidateDevice(DWORD * pNumPasses)
{
	return D3D9Wrapper::IDirect3DDevice9::ValidateDevice(pNumPasses);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY * pEntries)
{
	return D3D9Wrapper::IDirect3DDevice9::SetPaletteEntries(PaletteNumber, pEntries);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY * pEntries)
{
	return D3D9Wrapper::IDirect3DDevice9::GetPaletteEntries(PaletteNumber, pEntries);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return D3D9Wrapper::IDirect3DDevice9::SetCurrentTexturePalette(PaletteNumber);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetCurrentTexturePalette(UINT * PaletteNumber)
{
	return D3D9Wrapper::IDirect3DDevice9::GetCurrentTexturePalette(PaletteNumber);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetScissorRect(CONST RECT * pRect)
{
	FrameAnalysisLog("SetScissorRect(pRect:0x%p)\n",
		pRect);
	return D3D9Wrapper::IDirect3DDevice9::SetScissorRect(pRect);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetScissorRect(RECT * pRect)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetScissorRect(pRect);
	FrameAnalysisLog("GetScissorRect(pRect:0x%p)\n",
		pRect);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	return D3D9Wrapper::IDirect3DDevice9::SetSoftwareVertexProcessing(bSoftware);
}

inline STDMETHODIMP_(BOOL) D3D9Wrapper::FrameAnalysisDevice::GetSoftwareVertexProcessing(void)
{
	return D3D9Wrapper::IDirect3DDevice9::GetSoftwareVertexProcessing();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetNPatchMode(float nSegments)
{
	return D3D9Wrapper::IDirect3DDevice9::SetNPatchMode(nSegments);
}

inline STDMETHODIMP_(float) D3D9Wrapper::FrameAnalysisDevice::GetNPatchMode(void)
{
	return D3D9Wrapper::IDirect3DDevice9::GetNPatchMode();
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DrawPrimitive(::D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	FrameAnalysisLog("DrawPrimitive(PrimitiveType:%u, StartVertex:%u, PrimitiveCount:%u)\n",
		PrimitiveType, StartVertex, PrimitiveCount);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::Draw, PrimitiveType, PrimitiveCount, StartVertex, 0, 0, 0, 0, NULL, 0, NULL, ::D3DFMT_UNKNOWN, 0, NULL, NULL, NULL);
		FrameAnalysisAfterDraw(&call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DrawIndexedPrimitive(::D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
	FrameAnalysisLog("DrawIndexedPrimitive(PrimitiveType:%u, BaseVertexIndex:%u, MinVertexIndex:%u, NumVertices:%u, startIndex:%u, primCount:%u)\n",
		PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawIndexed, PrimitiveType, primCount, 0, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, NULL, 0, NULL, ::D3DFMT_UNKNOWN, 0, NULL, NULL, NULL);
		FrameAnalysisAfterDraw(&call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DrawPrimitiveUP(::D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void * pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	FrameAnalysisLog("DrawPrimitiveUP(PrimitiveType:%u, PrimitiveCount:%u, pVertexStreamZeroData:0x%p, VertexStreamZeroStride:%u)\n",
		PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawUP, PrimitiveType, PrimitiveCount, 0, 0, 0, 0, 0, pVertexStreamZeroData, VertexStreamZeroStride, NULL, ::D3DFMT_UNKNOWN, 0, NULL, NULL, NULL);
		FrameAnalysisAfterDraw(&call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DrawIndexedPrimitiveUP(::D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void * pIndexData, ::D3DFORMAT IndexDataFormat, CONST void * pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	FrameAnalysisLog("DrawIndexedPrimitiveUP(PrimitiveType:%u, MinVertexIndex:%u, NumVertices:%u, PrimitiveCount:%u, pIndexData:%u, IndexDataFormat:%u, pVertexStreamZeroData:0x%p, VertexStreamZeroStride:%u)\n",
		PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawIndexedUP, PrimitiveType, PrimitiveCount, 0, 0, MinVertexIndex, NumVertices, 0, pVertexStreamZeroData, VertexStreamZeroStride, pIndexData, IndexDataFormat, 0, NULL, NULL, NULL);
		FrameAnalysisAfterDraw(&call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
	return hr;
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, D3D9Wrapper::IDirect3DVertexBuffer9 * pDestBuffer, D3D9Wrapper::IDirect3DVertexDeclaration9 * pVertexDecl, DWORD Flags)
{
	return D3D9Wrapper::IDirect3DDevice9::ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateVertexDeclaration(const ::D3DVERTEXELEMENT9 * pVertexElements, D3D9Wrapper::IDirect3DVertexDeclaration9 ** ppDecl)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateVertexDeclaration(pVertexElements, ppDecl);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetVertexDeclaration(D3D9Wrapper::IDirect3DVertexDeclaration9 * pDecl)
{
	::IDirect3DVertexDeclaration9 *baseVertexDeclaration = baseVertexDeclaration9(pDecl);
	FrameAnalysisLog("SetVertexDeclaration(pDecl:0x%p)\n",
		baseVertexDeclaration);
	return D3D9Wrapper::IDirect3DDevice9::SetVertexDeclaration(pDecl);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetVertexDeclaration(D3D9Wrapper::IDirect3DVertexDeclaration9 ** ppDecl)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetVertexDeclaration(ppDecl);
	::IDirect3DVertexDeclaration9 *baseVertexDeclaration = baseVertexDeclaration9(*ppDecl);
	FrameAnalysisLog("GetVertexDeclaration(ppDecl:0x%p)\n",
		baseVertexDeclaration);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetFVF(DWORD FVF)
{
	return D3D9Wrapper::IDirect3DDevice9::SetFVF(FVF);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetFVF(DWORD * pFVF)
{
	return D3D9Wrapper::IDirect3DDevice9::GetFVF(pFVF);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateVertexShader(CONST DWORD * pFunction, D3D9Wrapper::IDirect3DVertexShader9 ** ppShader)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateVertexShader(pFunction, ppShader);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetVertexShader(D3D9Wrapper::IDirect3DVertexShader9 * pShader)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::SetVertexShader(pShader);
	::IDirect3DVertexShader9 *baseVertexShader = baseVertexShader9(pShader);
	D3D9Wrapper::IDirect3DVertexShader9 *wrappedShader = wrappedVertexShader9(pShader);
	FrameAnalysisLogNoNL("SetVertexShader(pShader:0x%p)",
		baseVertexShader);
	FrameAnalysisLogShaderHash(wrappedShader);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetVertexShader(D3D9Wrapper::IDirect3DVertexShader9 ** ppShader)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetVertexShader(ppShader);
	::IDirect3DVertexShader9 *baseVertexShader = baseVertexShader9(*ppShader);
	D3D9Wrapper::IDirect3DVertexShader9 *wrappedVertexShader = wrappedVertexShader9(*ppShader);
	FrameAnalysisLogNoNL("GetVertexShader(ppShader:0x%p)",
		baseVertexShader);
	if (wrappedVertexShader)
		FrameAnalysisLogShaderHash(wrappedVertexShader);
	else
		FrameAnalysisLog("\n");
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetVertexShaderConstantF(UINT StartRegister, CONST float * pConstantData, UINT Vector4fCount)
{
	FrameAnalysisLog("SetVertexShaderConstantF(StartRegister:%u, Vector4fCount:%u, pConstantData:0x%p)\n",
		StartRegister, Vector4fCount, pConstantData);
	FrameAnalysisLogMiscArray(StartRegister, Vector4fCount, (void *const *)pConstantData);
	return D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetVertexShaderConstantF(UINT StartRegister, float * pConstantData, UINT Vector4fCount)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	FrameAnalysisLog("GetVertexShaderConstantF(StartRegister:%u, pConstantData:0x%p, Vector4fCount:%u)\n",
		StartRegister, pConstantData, Vector4fCount);
	FrameAnalysisLogMiscArray(StartRegister, Vector4fCount, (void *const *)pConstantData);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetVertexShaderConstantI(UINT StartRegister, CONST int * pConstantData, UINT Vector4iCount)
{
	FrameAnalysisLog("SetVertexShaderConstantI(StartRegister:%u, Vector4iCount:%u, pConstantData:0x%p)\n",
		StartRegister, Vector4iCount, pConstantData);
	FrameAnalysisLogMiscArray(StartRegister, Vector4iCount, (void *const *)pConstantData);
	return D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetVertexShaderConstantI(UINT StartRegister, int * pConstantData, UINT Vector4iCount)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	FrameAnalysisLog("GetVertexShaderConstantF(StartRegister:%u, pConstantData:0x%p, Vector4iCount:%u)\n",
		StartRegister, pConstantData, Vector4iCount);
	FrameAnalysisLogMiscArray(StartRegister, Vector4iCount, (void *const *)pConstantData);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetVertexShaderConstantB(UINT StartRegister, CONST BOOL * pConstantData, UINT BoolCount)
{
	FrameAnalysisLog("SetVertexShaderConstantB(StartRegister:%u, BoolCount:%u, pConstantData:0x%p)\n",
		StartRegister, BoolCount, pConstantData);
	FrameAnalysisLogMiscArray(StartRegister, BoolCount, (void *const *)pConstantData);
	return D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetVertexShaderConstantB(UINT StartRegister, BOOL * pConstantData, UINT BoolCount)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	FrameAnalysisLog("GetVertexShaderConstantF(StartRegister:%u, pConstantData:0x%p, BoolCount:%u)\n",
		StartRegister, pConstantData, BoolCount);
	FrameAnalysisLogMiscArray(StartRegister, BoolCount, (void *const *)pConstantData);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetStreamSource(UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 * pStreamData, UINT OffsetInBytes, UINT Stride)
{
	::IDirect3DVertexBuffer9 *baseVertexBuffer = baseVertexBuffer9(pStreamData);
	D3D9Wrapper::IDirect3DVertexBuffer9 *wrappedVertexBuffer = wrappedVertexBuffer9(pStreamData);
	FrameAnalysisLog("SetStreamSource(StreamNumber:%u, pStreamData:0x%p, OffsetInBytes:%u, Stride:%u)\n",
		StreamNumber, baseVertexBuffer, OffsetInBytes, Stride);
	FrameAnalysisLogResource(StreamNumber, "Stream", wrappedVertexBuffer);
	return D3D9Wrapper::IDirect3DDevice9::SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetStreamSource(UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 ** ppStreamData, UINT * pOffsetInBytes, UINT * pStride)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
	::IDirect3DVertexBuffer9 *baseVertexBuffer = baseVertexBuffer9(*ppStreamData);
	D3D9Wrapper::IDirect3DVertexBuffer9 *wrappedVertexBuffer = wrappedVertexBuffer9(*ppStreamData);
	FrameAnalysisLog("GetStreamSource(StreamNumber:%u, pStreamData:0x%p, OffsetInBytes:%u, Stride:%u)\n",
		StreamNumber, baseVertexBuffer, pOffsetInBytes, pStride);
	FrameAnalysisLogResource(StreamNumber, "Stream", wrappedVertexBuffer);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetStreamSourceFreq(UINT StreamNumber, UINT Setting)
{
	return D3D9Wrapper::IDirect3DDevice9::SetStreamSourceFreq(StreamNumber, Setting);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetStreamSourceFreq(UINT StreamNumber, UINT * pSetting)
{
	return D3D9Wrapper::IDirect3DDevice9::GetStreamSourceFreq(StreamNumber, pSetting);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetIndices(D3D9Wrapper::IDirect3DIndexBuffer9 * pIndexData)
{
	::IDirect3DIndexBuffer9 *baseIndexBuffer = baseIndexBuffer9(pIndexData);
	D3D9Wrapper::IDirect3DIndexBuffer9 *wrappedIndexBuffer = wrappedIndexBuffer9(pIndexData);
	FrameAnalysisLogNoNL("SetIndices(pIndexData:0x%p)",
		baseIndexBuffer);
	FrameAnalysisLogResourceHash(wrappedIndexBuffer);
	return D3D9Wrapper::IDirect3DDevice9::SetIndices(pIndexData);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetIndices(D3D9Wrapper::IDirect3DIndexBuffer9 ** ppIndexData)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetIndices(ppIndexData);
	::IDirect3DIndexBuffer9 *baseIndexBuffer = baseIndexBuffer9(*ppIndexData);
	FrameAnalysisLog("GetIndices(ppIndexData:0x%p)\n",
		baseIndexBuffer);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreatePixelShader(CONST DWORD * pFunction, D3D9Wrapper::IDirect3DPixelShader9 ** ppShader)
{
	return D3D9Wrapper::IDirect3DDevice9::CreatePixelShader(pFunction, ppShader);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetPixelShader(D3D9Wrapper::IDirect3DPixelShader9 * pShader)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::SetPixelShader(pShader);
	::IDirect3DPixelShader9 *baseShader = basePixelShader9(pShader);
	D3D9Wrapper::IDirect3DPixelShader9 *wrappedShader = wrappedPixelShader9(pShader);
	FrameAnalysisLogNoNL("SetPixelShader(pShader:0x%p)",
		baseShader);
	FrameAnalysisLogShaderHash(wrappedShader);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetPixelShader(D3D9Wrapper::IDirect3DPixelShader9 ** ppShader)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetPixelShader(ppShader);
	::IDirect3DPixelShader9 *basePixelShader = basePixelShader9(*ppShader);
	D3D9Wrapper::IDirect3DPixelShader9 *wrappedPixelShader = wrappedPixelShader9(*ppShader);
	FrameAnalysisLogNoNL("GetVertexShader(ppShader:0x%p)",
		basePixelShader);
	if (wrappedPixelShader)
		FrameAnalysisLogShaderHash(wrappedPixelShader);
	else
		FrameAnalysisLog("\n");
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetPixelShaderConstantF(UINT StartRegister, CONST float * pConstantData, UINT Vector4fCount)
{
	FrameAnalysisLog("SetPixelShaderConstantF(StartRegister:%u, Vector4fCount:%u, pConstantData:0x%p)\n",
		StartRegister, Vector4fCount, pConstantData);
	FrameAnalysisLogMiscArray(StartRegister, Vector4fCount, (void *const *)pConstantData);
	return D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetPixelShaderConstantF(UINT StartRegister, float * pConstantData, UINT Vector4fCount)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	FrameAnalysisLog("GetVertexShaderConstantF(StartRegister:%u, pConstantData:0x%p, Vector4fCount:%u)\n",
		StartRegister, pConstantData, Vector4fCount);
	FrameAnalysisLogMiscArray(StartRegister, Vector4fCount, (void *const *)pConstantData);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetPixelShaderConstantI(UINT StartRegister, CONST int * pConstantData, UINT Vector4iCount)
{
	FrameAnalysisLog("SetPixelShaderConstantI(StartRegister:%u, Vector4iCount:%u, pConstantData:0x%p)\n",
		StartRegister, Vector4iCount, pConstantData);
	FrameAnalysisLogMiscArray(StartRegister, Vector4iCount, (void *const *)pConstantData);
	return D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetPixelShaderConstantI(UINT StartRegister, int * pConstantData, UINT Vector4iCount)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	FrameAnalysisLog("GetVertexShaderConstantF(StartRegister:%u, pConstantData:0x%p, Vector4iCount:%u)\n",
		StartRegister, pConstantData, Vector4iCount);
	FrameAnalysisLogMiscArray(StartRegister, Vector4iCount, (void *const *)pConstantData);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetPixelShaderConstantB(UINT StartRegister, CONST BOOL * pConstantData, UINT BoolCount)
{
	FrameAnalysisLog("SetPixelShaderConstantI(StartRegister:%u, BoolCount:%u, pConstantData:0x%p)\n",
		StartRegister, BoolCount, pConstantData);
	FrameAnalysisLogMiscArray(StartRegister, BoolCount, (void *const *)pConstantData);
	return D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetPixelShaderConstantB(UINT StartRegister, BOOL * pConstantData, UINT BoolCount)
{
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
	FrameAnalysisLog("GetVertexShaderConstantF(StartRegister:%u, pConstantData:0x%p, BoolCount:%u)\n",
		StartRegister, pConstantData, BoolCount);
	FrameAnalysisLogMiscArray(StartRegister, BoolCount, (void *const *)pConstantData);
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DrawRectPatch(UINT Handle, CONST float * pNumSegs, CONST ::D3DRECTPATCH_INFO * pRectPatchInfo)
{
	FrameAnalysisLog("DrawRectPatch(Handle:%u, pNumSegs:%u, pRectPatchInfo:%u)\n",
		Handle, pNumSegs, pRectPatchInfo);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawTriPatch, ::D3DPRIMITIVETYPE(-1), 0, 0, 0, 0, 0, 0, NULL, 0, NULL, ::D3DFMT_UNKNOWN, Handle, pNumSegs, pRectPatchInfo, NULL);
		FrameAnalysisAfterDraw(&call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DrawTriPatch(UINT Handle, CONST float * pNumSegs, CONST ::D3DTRIPATCH_INFO * pTriPatchInfo)
{
	FrameAnalysisLog("DrawTriPatch(Handle:%u, pNumSegs:%u, pTriPatchInfo:%u)\n",
		Handle, pNumSegs, pTriPatchInfo);
	HRESULT hr = D3D9Wrapper::IDirect3DDevice9::DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
	if (G->analyse_frame) {
		DrawCallInfo call_info(DrawCall::DrawTriPatch, ::D3DPRIMITIVETYPE(-1), 0, 0, 0, 0, 0, 0, NULL, 0, NULL, ::D3DFMT_UNKNOWN, Handle, pNumSegs, NULL, pTriPatchInfo);
		FrameAnalysisAfterDraw(&call_info);
	}
	oneshot_valid = false;
	non_draw_call_dump_counter = 0;
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::DeletePatch(UINT Handle)
{
	return D3D9Wrapper::IDirect3DDevice9::DeletePatch(Handle);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateQuery(::D3DQUERYTYPE Type, ::IDirect3DQuery9 ** ppQuery)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateQuery(Type, ppQuery);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetDisplayModeEx(UINT iSwapChain, ::D3DDISPLAYMODEEX * pMode, ::D3DDISPLAYROTATION * pRotation)
{
	return D3D9Wrapper::IDirect3DDevice9::GetDisplayModeEx(iSwapChain, pMode, pRotation);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::ResetEx(::D3DPRESENT_PARAMETERS * pPresentationParameters, ::D3DDISPLAYMODEEX * pFullscreenDisplayMode)
{
	return D3D9Wrapper::IDirect3DDevice9::ResetEx(pPresentationParameters, pFullscreenDisplayMode);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CheckDeviceState(HWND hWindow)
{
	return D3D9Wrapper::IDirect3DDevice9::CheckDeviceState(hWindow);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CheckResourceResidency(::IDirect3DResource9 ** pResourceArray, UINT32 NumResources)
{
	return D3D9Wrapper::IDirect3DDevice9::CheckResourceResidency(pResourceArray, NumResources);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::ComposeRects(D3D9Wrapper::IDirect3DSurface9 * pSource, D3D9Wrapper::IDirect3DSurface9 * pDestination, D3D9Wrapper::IDirect3DVertexBuffer9 * pSrcRectDescriptors, UINT NumRects, D3D9Wrapper::IDirect3DVertexBuffer9 * pDstRectDescriptors, ::D3DCOMPOSERECTSOP Operation, INT XOffset, INT YOffset)
{
	return D3D9Wrapper::IDirect3DDevice9::ComposeRects(pSource, pDestination, pSrcRectDescriptors, NumRects, pDstRectDescriptors, Operation, XOffset, YOffset);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle, DWORD Usage)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle, DWORD Usage)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::CreateRenderTargetEx(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle, DWORD Usage)
{
	return D3D9Wrapper::IDirect3DDevice9::CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetGPUThreadPriority(INT * pPriority)
{
	return D3D9Wrapper::IDirect3DDevice9::GetGPUThreadPriority(pPriority);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::GetMaximumFrameLatency(UINT * pMaxLatency)
{
	return D3D9Wrapper::IDirect3DDevice9::GetMaximumFrameLatency(pMaxLatency);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::PresentEx(RECT * pSourceRect, const RECT * pDestRect, HWND hDestWindowOverride, const RGNDATA * pDirtyRegion, DWORD dwFlags)
{
	return D3D9Wrapper::IDirect3DDevice9::PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetConvolutionMonoKernel(UINT Width, UINT Height, float * RowWeights, float * ColumnWeights)
{
	return D3D9Wrapper::IDirect3DDevice9::SetConvolutionMonoKernel(Width, Height, RowWeights, ColumnWeights);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetGPUThreadPriority(INT pPriority)
{
	return D3D9Wrapper::IDirect3DDevice9::SetGPUThreadPriority(pPriority);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::SetMaximumFrameLatency(UINT pMaxLatency)
{
	return D3D9Wrapper::IDirect3DDevice9::SetMaximumFrameLatency(pMaxLatency);
}

inline STDMETHODIMP D3D9Wrapper::FrameAnalysisDevice::WaitForVBlank(UINT SwapChainIndex)
{
	return D3D9Wrapper::IDirect3DDevice9::WaitForVBlank(SwapChainIndex);
}
