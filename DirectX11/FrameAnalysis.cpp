#include "HackerContext.h"
#include "Globals.h"

#include <ScreenGrab.h>
#include <wincodec.h>
#include <Strsafe.h>
#include <stdarg.h>

void HackerContext::FrameAnalysisLog(char *fmt, ...)
{
	va_list ap;
	wchar_t filename[MAX_PATH];

	LogDebug("HackerContext(%s@%p)::", type_name(this), this);
	va_start(ap, fmt);
	vLogDebug(fmt, ap);
	va_end(ap);

	if (!G->analyse_frame) {
		if (frame_analysis_log)
			fclose(frame_analysis_log);
		frame_analysis_log = NULL;
		return;
	}

	// Using the global analyse options here as the local copy in the
	// context is only updated after draw calls. We could potentially
	// process the triggers here, but this function is intended to log
	// other calls as well where that wouldn't make sense. We could change
	// it so that this is called from FrameAnalysisAfterDraw, but we want
	// to log calls for deferred contexts here as well.
	if (!(G->cur_analyse_options & FrameAnalysisOptions::LOG))
		return;

	if (!frame_analysis_log) {
		// Use the original context to check the type, otherwise we
		// will recursively call ourselves:
		if (mOrigContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
			swprintf_s(filename, MAX_PATH, L"%ls\\log.txt", G->ANALYSIS_PATH);
		else
			swprintf_s(filename, MAX_PATH, L"%ls\\log-0x%p.txt", G->ANALYSIS_PATH, this);

		frame_analysis_log = _wfsopen(filename, L"w", _SH_DENYNO);
		if (!frame_analysis_log) {
			LogInfoW(L"Error opening %s\n", filename);
			return;
		}

		fprintf(frame_analysis_log, "analyse_options: %08x\n", G->cur_analyse_options);
	}

	// We don't allow hold to be changed mid-frame due to potential
	// for filename conflicts, so use def_analyse_options:
	if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
		fprintf(frame_analysis_log, "%u.", G->analyse_frame_no);
	fprintf(frame_analysis_log, "%06u ", G->analyse_frame);

	va_start(ap, fmt);
	vfprintf(frame_analysis_log, fmt, ap);
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

void HackerContext::FrameAnalysisLogResourceHash(ID3D11Resource *resource)
{
	uint32_t hash, orig_hash;
	struct ResourceHashInfo *info;

	if (!G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
		return;

	if (!resource) {
		fprintf(frame_analysis_log, "\n");
		return;
	}

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

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

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	fprintf(frame_analysis_log, "\n");
}

void HackerContext::FrameAnalysisLogResource(int slot, char *slot_name, ID3D11Resource *resource)
{
	if (!resource || !G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
		return;

	FrameAnalysisLogSlot(frame_analysis_log, slot, slot_name);
	fprintf(frame_analysis_log, " resource=0x%p", resource);

	FrameAnalysisLogResourceHash(resource);
}

void HackerContext::FrameAnalysisLogView(int slot, char *slot_name, ID3D11View *view)
{
	ID3D11Resource *resource;

	if (!view || !G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
		return;

	FrameAnalysisLogSlot(frame_analysis_log, slot, slot_name);
	fprintf(frame_analysis_log, " view=0x%p", view);

	view->GetResource(&resource);
	if (!resource)
		return;

	FrameAnalysisLogResource(-1, NULL, resource);

	resource->Release();
}

void HackerContext::FrameAnalysisLogResourceArray(UINT start, UINT len, ID3D11Resource *const *ppResources)
{
	UINT i;

	if (!ppResources || !G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
		return;

	for (i = 0; i < len; i++)
		FrameAnalysisLogResource(start + i, NULL, ppResources[i]);
}

void HackerContext::FrameAnalysisLogViewArray(UINT start, UINT len, ID3D11View *const *ppViews)
{
	UINT i;

	if (!ppViews || !G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
		return;

	for (i = 0; i < len; i++)
		FrameAnalysisLogView(start + i, NULL, ppViews[i]);
}

void HackerContext::FrameAnalysisLogMiscArray(UINT start, UINT len, void *const *array)
{
	UINT i;
	void *item;

	if (!array || !G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
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

void HackerContext::FrameAnalysisLogAsyncQuery(ID3D11Asynchronous *async)
{
	AsyncQueryType type;
	ID3D11Query *query;
	ID3D11Predicate *predicate;
	D3D11_QUERY_DESC desc;

	if (!G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
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

void HackerContext::FrameAnalysisLogData(void *buf, UINT size)
{
	unsigned char *ptr = (unsigned char*)buf;
	UINT i;

	if (!G->analyse_frame || !(G->cur_analyse_options & FrameAnalysisOptions::LOG) || !frame_analysis_log)
		return;

	if (!buf || !size) {
		fprintf(frame_analysis_log, "\n");
		return;
	}

	fprintf(frame_analysis_log, "    data: ");
	for (i = 0; i < size; i++, ptr++)
		fprintf(frame_analysis_log, "%02x", *ptr);
	fprintf(frame_analysis_log, "\n");
}

void HackerContext::Dump2DResource(ID3D11Texture2D *resource, wchar_t
		*filename, bool stereo, FrameAnalysisOptions type_mask)
{
	FrameAnalysisOptions options = (FrameAnalysisOptions)(analyse_options & type_mask);
	HRESULT hr = S_OK, dont_care;
	wchar_t *ext;

	ext = wcsrchr(filename, L'.');
	if (!ext) {
		FALogInfo("Dump2DResource: Filename missing extension\n");
		return;
	}

	// Needs to be called at some point before SaveXXXTextureToFile:
	dont_care = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if ((options & FrameAnalysisOptions::DUMP_XXX_JPS) ||
	    (options & FrameAnalysisOptions::DUMP_XXX)) {
		// save a JPS file. This will be missing extra channels (e.g.
		// transparency, depth buffer, specular power, etc) or bit depth that
		// can be found in the DDS file, but is generally easier to work with.
		//
		// Not all formats can be saved as JPS with this function - if
		// only dump_rt was specified (as opposed to dump_rt_jps) we
		// will dump out DDS files for those instead.
		if (stereo)
			wcscpy_s(ext, MAX_PATH + filename - ext, L".jps");
		else
			wcscpy_s(ext, MAX_PATH + filename - ext, L".jpg");
		hr = DirectX::SaveWICTextureToFile(mOrigContext, resource, GUID_ContainerFormatJpeg, filename);
	}


	if ((options & FrameAnalysisOptions::DUMP_XXX_DDS) ||
	   ((options & FrameAnalysisOptions::DUMP_XXX) && FAILED(hr))) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".dds");
		hr = DirectX::SaveDDSTextureToFile(mOrigContext, resource, filename);
	}

	if (FAILED(hr))
		FALogInfo("Failed to dump Texture2D: 0x%x\n", hr);
}

HRESULT HackerContext::CreateStagingResource(ID3D11Texture2D **resource,
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

	// Force surface creation mode to stereo to prevent driver heuristics
	// interfering. If the original surface was mono that's ok - thaks to
	// the intermediate stages we'll end up with both eyes the same
	// (without this one eye would be blank instead, which is arguably
	// better since it will be immediately obvious, but risks missing the
	// second perspective if the original resource was actually stereo)
	NvAPI_Stereo_GetSurfaceCreationMode(mHackerDevice->mStereoHandle, &orig_mode);
	NvAPI_Stereo_SetSurfaceCreationMode(mHackerDevice->mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);

	hr = mOrigDevice->CreateTexture2D(&desc, NULL, resource);

	NvAPI_Stereo_SetSurfaceCreationMode(mHackerDevice->mStereoHandle, orig_mode);
	return hr;
}

// TODO: Refactor this with StereoScreenShot().
// Expects the reverse stereo blit to be enabled by the caller
void HackerContext::DumpStereoResource(ID3D11Texture2D *resource, wchar_t *filename,
		FrameAnalysisOptions type_mask)
{
	ID3D11Texture2D *stereoResource = NULL;
	ID3D11Texture2D *tmpResource = NULL;
	ID3D11Texture2D *tmpResource2 = NULL;
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
		hr = CreateStagingResource(&tmpResource, srcDesc, false, false);
		if (FAILED(hr)) {
			FALogInfo("DumpStereoResource failed to create intermediate texture: 0x%x\n", hr);
			goto out;
		}

		if (srcDesc.SampleDesc.Count > 1) {
			// Resolve MSAA surfaces. Procedure copied from DirectXTK
			// These need to have D3D11_USAGE_DEFAULT to resolve,
			// so we need yet another intermediate texture:
			hr = CreateStagingResource(&tmpResource2, srcDesc, false, true);
			if (FAILED(hr)) {
				FALogInfo("DumpStereoResource failed to create intermediate texture: 0x%x\n", hr);
				goto out1;
			}

			DXGI_FORMAT fmt = EnsureNotTypeless(srcDesc.Format);
			UINT support = 0;

			hr = mOrigDevice->CheckFormatSupport( fmt, &support );
			if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
				FALogInfo("DumpStereoResource cannot resolve MSAA format %d\n", fmt);
				goto out2;
			}

			for (item = 0; item < srcDesc.ArraySize; item++) {
				for (level = 0; level < srcDesc.MipLevels; level++) {
					index = D3D11CalcSubresource(level, item, max(srcDesc.MipLevels, 1));
					mOrigContext->ResolveSubresource(tmpResource2, index, src, index, fmt);
				}
			}
			src = tmpResource2;
		}

		mOrigContext->CopyResource(tmpResource, src);
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
			mOrigContext->CopySubresourceRegion(stereoResource, index, 0, 0, 0,
					src, index, &srcBox);
		}
	}

	Dump2DResource(stereoResource, filename, true, type_mask);

out2:
	if (tmpResource2)
		tmpResource2->Release();
out1:
	if (tmpResource)
		tmpResource->Release();

out:
	stereoResource->Release();
}

/*
 * This just treats the buffer as an array of float4s. In the future we might
 * try to use the reflection information in the shaders to add names and
 * correct types.
 */
void HackerContext::DumpBufferTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, char type, int idx, UINT stride, UINT offset)
{
	FILE *fd = NULL;
	char *components = "xyzw";
	float *buf = (float*)map->pData;
	UINT i, c;
	errno_t err;

	err = _wfopen_s(&fd, filename, L"w");
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

/*
 * Dumps the vertex buffer in several formats.
 * FIXME: We should wrap the input layout object to get the correct format (and
 * other info like the semantic).
 */
void HackerContext::DumpVBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, int idx, UINT stride, UINT offset, UINT first, UINT count)
{
	FILE *fd = NULL;
	float *buff = (float*)map->pData;
	int *buf32 = (int*)map->pData;
	char *buf8 = (char*)map->pData;
	UINT i, j, start, end, buf_idx;
	errno_t err;

	err = _wfopen_s(&fd, filename, L"w");
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

void HackerContext::DumpIBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, DXGI_FORMAT format, UINT offset, UINT first, UINT count)
{
	FILE *fd = NULL;
	short *buf16 = (short*)map->pData;
	int *buf32 = (int*)map->pData;
	UINT start, end, i;
	errno_t err;

	err = _wfopen_s(&fd, filename, L"w");
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

void HackerContext::DumpBuffer(ID3D11Buffer *buffer, wchar_t *filename,
		FrameAnalysisOptions type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset, UINT first, UINT count)
{
	FrameAnalysisOptions options = (FrameAnalysisOptions)(analyse_options & type_mask);
	D3D11_BUFFER_DESC desc;
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Buffer *staging = NULL;
	HRESULT hr;
	FILE *fd = NULL;
	wchar_t *ext;
	errno_t err;

	ext = wcsrchr(filename, L'.');
	if (!ext) {
		FALogInfo("DumpBuffer: Filename missing extension\n");
		return;
	}

	buffer->GetDesc(&desc);

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	hr = mOrigDevice->CreateBuffer(&desc, NULL, &staging);
	if (FAILED(hr)) {
		FALogInfo("DumpBuffer failed to create staging buffer: 0x%x\n", hr);
		return;
	}

	mOrigContext->CopyResource(staging, buffer);
	hr = mOrigContext->Map(staging, 0, D3D11_MAP_READ, 0, &map);
	if (FAILED(hr)) {
		FALogInfo("DumpBuffer failed to map staging resource: 0x%x\n", hr);
		return;
	}

	if (options & FrameAnalysisOptions::DUMP_XX_BIN) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".buf");

		err = _wfopen_s(&fd, filename, L"wb");
		if (!fd) {
			FALogInfo("Unable to create %S: %u\n", filename, err);
			goto out_unmap;
		}
		fwrite(map.pData, 1, desc.ByteWidth, fd);
		fclose(fd);
	}

	if (options & FrameAnalysisOptions::DUMP_XX_TXT) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".txt");
		if (options & FrameAnalysisOptions::DUMP_CB_TXT)
			DumpBufferTxt(filename, &map, desc.ByteWidth, 'c', idx, stride, offset);
		else if (options & FrameAnalysisOptions::DUMP_VB_TXT)
			DumpVBTxt(filename, &map, desc.ByteWidth, idx, stride, offset, first, count);
		else if (options & FrameAnalysisOptions::DUMP_IB_TXT)
			DumpIBTxt(filename, &map, desc.ByteWidth, ib_fmt, offset, first, count);
		else if (options & FrameAnalysisOptions::DUMP_ON_UNMAP) {
			// We don't know what kind of buffer this is, so just
			// use the generic dump routine:
			DumpBufferTxt(filename, &map, desc.ByteWidth, '?', idx, stride, offset);
		}
	}
	// TODO: Dump UAV, RT and SRV buffers as text taking their format,
	// offset, size, first entry and num entries into account.

out_unmap:
	mOrigContext->Unmap(staging, 0);
	staging->Release();
}

void HackerContext::DumpResource(ID3D11Resource *resource, wchar_t *filename,
		FrameAnalysisOptions type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset)
{
	D3D11_RESOURCE_DIMENSION dim;

	resource->GetType(&dim);

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			DumpBuffer((ID3D11Buffer*)resource, filename, type_mask, idx, ib_fmt, stride, offset, 0, 0);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			FALogInfo("Skipped dumping Texture1D resource\n");
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource((ID3D11Texture2D*)resource, filename, type_mask);
			if (analyse_options & FrameAnalysisOptions::MONO)
				Dump2DResource((ID3D11Texture2D*)resource, filename, false, type_mask);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			FALogInfo("Skipped dumping Texture3D resource\n");
			break;
		default:
			FALogInfo("Skipped dumping resource of unknown type %i\n", dim);
			break;
	}
}

HRESULT HackerContext::FrameAnalysisFilename(wchar_t *filename, size_t size, bool compute,
		wchar_t *reg, char shader_type, int idx, uint32_t hash, uint32_t orig_hash,
		ID3D11Resource *handle)
{
	struct ResourceHashInfo *info;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);

	if (!(analyse_options & FrameAnalysisOptions::FILENAME_REG)) {
		// We don't allow hold to be changed mid-frame due to potential
		// for filename conflicts, so use def_analyse_options:
		if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i.", G->analyse_frame_no);
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i-", G->analyse_frame);
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
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i", G->analyse_frame);
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

HRESULT HackerContext::FrameAnalysisFilenameResource(wchar_t *filename, size_t size, wchar_t *type,
		uint32_t hash, uint32_t orig_hash, ID3D11Resource *handle)
{
	struct ResourceHashInfo *info;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);

	// We don't allow hold to be changed mid-frame due to potential
	// for filename conflicts, so use def_analyse_options:
	if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i.", G->analyse_frame_no);
	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i-", G->analyse_frame);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%s-", type);

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

	// Always do this for resource dumps since hashes are likely to clash:
	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"@%p", handle);

	hr = StringCchPrintfW(pos, rem, L".XXX");
	if (FAILED(hr))
		FALogInfo("Failed to create filename: 0x%x\n", hr);

	return hr;
}

void HackerContext::_DumpCBs(char shader_type, bool compute,
	ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT])
{
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	for (i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
		if (!buffers[i])
			continue;

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"cb", shader_type, i, 0, 0, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpResource(buffers[i], filename,
					FrameAnalysisOptions::DUMP_CB_MASK, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
		}

		buffers[i]->Release();
	}
}

void HackerContext::_DumpTextures(char shader_type, bool compute,
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT])
{
	ID3D11Resource *resource;
	D3D11_RESOURCE_DIMENSION dim;
	wchar_t filename[MAX_PATH];
	uint32_t hash, orig_hash;
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

		try {
			hash = G->mResources.at((ID3D11Texture2D *)resource).hash;
			orig_hash = G->mResources.at((ID3D11Texture2D *)resource).orig_hash;
		} catch (std::out_of_range) {
			hash = orig_hash = 0;
		}

		// TODO: process description to get offset, strides & size for
		// buffer & bufferex type SRVs and pass down to dump routines,
		// although I have no idea how to determine which of the
		// entries in the two D3D11_BUFFER_SRV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"t", shader_type, i, hash, orig_hash, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_TEX_MASK, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
		}

		resource->Release();
		views[i]->Release();
	}
}

void HackerContext::DumpCBs(bool compute)
{
	ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	if (compute) {
		if (mCurrentComputeShader) {
			mOrigContext->CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('c', compute, buffers);
		}
	} else {
		if (mCurrentVertexShader) {
			mOrigContext->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('v', compute, buffers);
		}
		if (mCurrentHullShader) {
			mOrigContext->HSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('h', compute, buffers);
		}
		if (mCurrentDomainShader) {
			mOrigContext->DSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('d', compute, buffers);
		}
		if (mCurrentGeometryShader) {
			mOrigContext->GSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('g', compute, buffers);
		}
		if (mCurrentPixelShader) {
			mOrigContext->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('p', compute, buffers);
		}
	}
}

void HackerContext::DumpVBs(DrawCallInfo *call_info)
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

	mOrigContext->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, buffers, strides, offsets);

	for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (!buffers[i])
			continue;

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"vb", NULL, i, 0, 0, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpBuffer(buffers[i], filename,
				FrameAnalysisOptions::DUMP_VB_MASK, i,
				DXGI_FORMAT_UNKNOWN, strides[i], offsets[i],
				first, count);
		}

		buffers[i]->Release();
	}
}

void HackerContext::DumpIB(DrawCallInfo *call_info)
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

	mOrigContext->IAGetIndexBuffer(&buffer, &format, &offset);
	if (!buffer)
		return;

	hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"ib", NULL, -1, 0, 0, buffer);
	if (SUCCEEDED(hr)) {
		DumpBuffer(buffer, filename,
				FrameAnalysisOptions::DUMP_IB_MASK, -1,
				format, 0, offset, first, count);
	}

	buffer->Release();
}

void HackerContext::DumpTextures(bool compute)
{
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

	if (compute) {
		if (mCurrentComputeShader) {
			mOrigContext->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('c', compute, views);
		}
	} else {
		if (mCurrentVertexShader) {
			mOrigContext->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('v', compute, views);
		}
		if (mCurrentHullShader) {
			mOrigContext->HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('h', compute, views);
		}
		if (mCurrentDomainShader) {
			mOrigContext->DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('d', compute, views);
		}
		if (mCurrentGeometryShader) {
			mOrigContext->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('g', compute, views);
		}
		if (mCurrentPixelShader) {
			mOrigContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('p', compute, views);
		}
	}
}

void HackerContext::DumpRenderTargets()
{
	UINT i;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	uint32_t hash, orig_hash;

	for (i = 0; i < mCurrentRenderTargets.size(); ++i) {
		// TODO: Decouple from HackerContext and remove dependency on
		// stat collection by querying the DeviceContext directly like
		// we do for all other resources
		try {
			hash = G->mResources.at(mCurrentRenderTargets[i]).hash;
			orig_hash = G->mResources.at(mCurrentRenderTargets[i]).orig_hash;
		} catch (std::out_of_range) {
			hash = orig_hash = 0;
		}

		// TODO: process description to get offset, strides & size for
		// buffer type RTVs and pass down to dump routines, although I
		// have no idea how to determine which of the entries in the
		// two D3D11_BUFFER_RTV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"o", NULL, i,
				hash, orig_hash, (ID3D11Resource*)mCurrentRenderTargets[i]);
		if (FAILED(hr))
			return;
		DumpResource((ID3D11Resource*)mCurrentRenderTargets[i], filename,
				FrameAnalysisOptions::DUMP_RT_MASK, i,
				DXGI_FORMAT_UNKNOWN, 0, 0);
	}
}

void HackerContext::DumpDepthStencilTargets()
{
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	uint32_t hash, orig_hash;

	if (mCurrentDepthTarget) {
		// TODO: Decouple from HackerContext and remove dependency on
		// stat collection by querying the DeviceContext directly like
		// we do for all other resources
		try {
			hash = G->mResources.at(mCurrentDepthTarget).hash;
			orig_hash = G->mResources.at(mCurrentDepthTarget).orig_hash;
		} catch (std::out_of_range) {
			hash = orig_hash = 0;
		}

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"oD", NULL, -1,
				hash, orig_hash, (ID3D11Resource*)mCurrentDepthTarget);
		if (FAILED(hr))
			return;
		DumpResource((ID3D11Resource*)mCurrentDepthTarget, filename,
				FrameAnalysisOptions::DUMP_DEPTH_MASK, -1,
				DXGI_FORMAT_UNKNOWN, 0, 0);
	}
}

void HackerContext::DumpUAVs(bool compute)
{
	UINT i;
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	ID3D11Resource *resource;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	uint32_t hash, orig_hash;

	if (compute)
		mOrigContext->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);
	else
		mOrigContext->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);

	for (i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; ++i) {
		if (!uavs[i])
			continue;

		uavs[i]->GetResource(&resource);
		if (!resource) {
			uavs[i]->Release();
			continue;
		}

		try {
			hash = G->mResources.at(resource).hash;
			orig_hash = G->mResources.at(resource).orig_hash;
		} catch (std::out_of_range) {
			hash = orig_hash = 0;
		}

		// TODO: process description to get offset & size for buffer
		// type UAVs and pass down to dump routines.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"u", NULL, i, hash, orig_hash, resource);
		if (SUCCEEDED(hr)) {
			DumpResource(resource, filename,
					FrameAnalysisOptions::DUMP_RT_MASK, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
		}

		resource->Release();
		uavs[i]->Release();
	}
}

void HackerContext::FrameAnalysisClearRT(ID3D11RenderTargetView *target)
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

	mOrigContext->ClearRenderTargetView(target, colour);
}

void HackerContext::FrameAnalysisClearUAV(ID3D11UnorderedAccessView *uav)
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

	mOrigContext->ClearUnorderedAccessViewUint(uav, values);
}

void HackerContext::FrameAnalysisProcessTriggers(bool compute)
{
	FrameAnalysisOptions new_options = FrameAnalysisOptions::INVALID;
	struct ShaderOverride *shaderOverride;
	struct TextureOverride *textureOverride;
	uint32_t hash;
	UINT i;

	// TODO: Trigger on texture inputs

	if (compute) {
		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentComputeShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		// TODO: Trigger on current UAVs
	} else {
		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentVertexShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentHullShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentDomainShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentGeometryShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentPixelShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		for (i = 0; i < mCurrentRenderTargets.size(); ++i) {
			try {
				hash = G->mResources.at(mCurrentRenderTargets[i]).hash;
				textureOverride = &G->mTextureOverrideMap.at(hash);
				new_options |= textureOverride->analyse_options;
			} catch (std::out_of_range) {}
		}

		if (mCurrentDepthTarget) {
			try {
				hash = G->mResources.at(mCurrentDepthTarget).hash;
				textureOverride = &G->mTextureOverrideMap.at(hash);
				new_options |= textureOverride->analyse_options;
			} catch (std::out_of_range) {}
		}
	}

	if (!new_options)
		return;

	analyse_options = new_options;

	if (new_options & FrameAnalysisOptions::PERSIST) {
		G->cur_analyse_options = new_options;
		FALogInfo("analyse_options (persistent): %08x\n", new_options);
	} else
		FALogInfo("analyse_options (one-shot): %08x\n", new_options);
}

void HackerContext::FrameAnalysisAfterDraw(bool compute, DrawCallInfo *call_info)
{
	NvAPI_Status nvret;

	// Bail if we are a deferred context, as there will not be anything to
	// dump out yet and we don't want to alter the global draw count. Later
	// we might want to think about ways we could analyse deferred contexts
	// - a simple approach would be to dump out the back buffer after
	// executing a command list in the immediate context, however this
	// would only show the combined result of all the draw calls from the
	// deferred context, and not the results of the individual draw
	// operations.
	//
	// Another more in-depth approach would be to create the stereo
	// resources now and issue the reverse blits, then dump them all after
	// executing the command list. Note that the NVAPI call is not
	// per-context and therefore may have threading issues, and it's not
	// clear if it would have to be enabled while submitting the copy
	// commands in the deferred context, or while playing the command queue
	// in the immediate context, or both.
	if (mOrigContext->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
		return;

	analyse_options = G->cur_analyse_options;

	FrameAnalysisProcessTriggers(compute);

	// If neither stereo or mono specified, default to stereo:
	if (!(analyse_options & FrameAnalysisOptions::STEREO_MASK))
		analyse_options |= FrameAnalysisOptions::STEREO;

	if ((analyse_options & FrameAnalysisOptions::DUMP_XXX_MASK) &&
	    (analyse_options & FrameAnalysisOptions::STEREO)) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			FALogInfo("DumpStereoResource failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	// Grab the critical section now as we may need it several times during
	// dumping for mResources
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	if (analyse_options & FrameAnalysisOptions::DUMP_CB_MASK)
		DumpCBs(compute);

	if (!compute) {
		if (analyse_options & FrameAnalysisOptions::DUMP_VB_MASK)
			DumpVBs(call_info);

		if (analyse_options & FrameAnalysisOptions::DUMP_IB_MASK)
			DumpIB(call_info);
	}

	if (analyse_options & FrameAnalysisOptions::DUMP_TEX_MASK)
		DumpTextures(compute);

	if (analyse_options & FrameAnalysisOptions::DUMP_RT_MASK) {
		if (!compute)
			DumpRenderTargets();

		// UAVs can be used by both pixel shaders and compute shaders:
		DumpUAVs(compute);
	}

	if (analyse_options & FrameAnalysisOptions::DUMP_DEPTH_MASK && !compute)
		DumpDepthStencilTargets();

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	if ((analyse_options & FrameAnalysisOptions::DUMP_XXX_MASK) &&
	    (analyse_options & FrameAnalysisOptions::STEREO)) {
		NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, false);
	}

	G->analyse_frame++;
}

void HackerContext::FrameAnalysisAfterUnmap(ID3D11Resource *resource)
{
	wchar_t filename[MAX_PATH];
	uint32_t hash = 0, orig_hash = 0;
	HRESULT hr;

	analyse_options = G->cur_analyse_options;

	if (!(analyse_options & FrameAnalysisOptions::DUMP_ON_UNMAP))
		return;

	// Don't bother trying to dump as stereo - map/unmap is inherently mono
	analyse_options &= (FrameAnalysisOptions)~FrameAnalysisOptions::STEREO_MASK;
	analyse_options |= FrameAnalysisOptions::MONO;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	try {
		hash = G->mResources.at(resource).hash;
		orig_hash = G->mResources.at(resource).orig_hash;
	} catch (std::out_of_range) {
	}

	hr = FrameAnalysisFilenameResource(filename, MAX_PATH, L"unmap", hash, orig_hash, resource);
	if (SUCCEEDED(hr)) {
		DumpResource(resource, filename, FrameAnalysisOptions::DUMP_ON_UNMAP, -1,
						DXGI_FORMAT_UNKNOWN, 0, 0);
	}

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	// XXX: Might be better to use a second counter for these
	G->analyse_frame++;
}
