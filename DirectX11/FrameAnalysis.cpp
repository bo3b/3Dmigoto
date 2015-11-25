#include "HackerContext.h"
#include "Globals.h"

#include <ScreenGrab.h>
#include <wincodec.h>
#include <Strsafe.h>

void HackerContext::Dump2DResource(ID3D11Texture2D *resource, wchar_t
		*filename, bool stereo, FrameAnalysisOptions type_mask)
{
	FrameAnalysisOptions options = (FrameAnalysisOptions)(analyse_options & type_mask);
	HRESULT hr = E_FAIL;
	wchar_t *ext;

	ext = wcsrchr(filename, L'.');
	if (!ext)
		return;

	// Needs to be called at some point before SaveXXXTextureToFile:
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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
		DirectX::SaveDDSTextureToFile(mOrigContext, resource, filename);
	}
}

HRESULT HackerContext::CreateStagingResource(ID3D11Texture2D **resource,
		D3D11_TEXTURE2D_DESC desc, bool stereo, bool msaa)
{
	NVAPI_STEREO_SURFACECREATEMODE orig_mode;
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
		LogInfo("DumpStereoResource failed to create stereo texture: 0x%x\n", hr);
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
			LogInfo("DumpStereoResource failed to create intermediate texture: 0x%x\n", hr);
			goto out;
		}

		if (srcDesc.SampleDesc.Count > 1) {
			// Resolve MSAA surfaces. Procedure copied from DirectXTK
			// These need to have D3D11_USAGE_DEFAULT to resolve,
			// so we need yet another intermediate texture:
			hr = CreateStagingResource(&tmpResource2, srcDesc, false, true);
			if (FAILED(hr)) {
				LogInfo("DumpStereoResource failed to create intermediate texture: 0x%x\n", hr);
				goto out1;
			}

			DXGI_FORMAT fmt = EnsureNotTypeless(srcDesc.Format);
			UINT support = 0;

			hr = mOrigDevice->CheckFormatSupport( fmt, &support );
			if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
				LogInfo("DumpStereoResource cannot resolve MSAA format %d\n", fmt);
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

	_wfopen_s(&fd, filename, L"w");
	if (!fd)
		return;

	if (offset)
		fprintf(fd, "offset: %u\n", offset);
	if (stride)
		fprintf(fd, "stride: %u\n", stride);

	// FIXME: For vertex buffers we should wrap the input layout object to
	// get the format (and other info like the semantic).

	for (i = offset / 16; i < size / 16; i++) {
		for (c = offset % 4; c < 4; c++)
			fprintf(fd, "%cb%i[%d].%c: %.9g\n", type, idx, i, components[c], buf[i*4+c]);
	}

	fclose(fd);
}

void HackerContext::DumpIBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
		UINT size, DXGI_FORMAT format, UINT offset)
{
	FILE *fd = NULL;
	short *buf16 = (short*)map->pData;
	int *buf32 = (int*)map->pData;
	UINT i;

	_wfopen_s(&fd, filename, L"w");
	if (!fd)
		return;

	fprintf(fd, "offset: %u\n", offset);

	// FIXME: Currently we are dumping the index buffer starting from the
	// offset from the IASetIndexBuffer() call to the end of the buffer. We
	// should also take into account any size and offset passed into the
	// DrawIndexed, DrawIndexedInstanced or DrawIndexedInstancedIndirect
	// calls (also, how do these two offsets interract - are they added
	// together?).

	switch(format) {
	case DXGI_FORMAT_R16_UINT:
		fprintf(fd, "format: DXGI_FORMAT_R16_UINT\n");
		for (i = offset / 2; i < size / 2; i++)
			fprintf(fd, "%u\n", buf16[i]);
		break;
	case DXGI_FORMAT_R32_UINT:
		fprintf(fd, "format: DXGI_FORMAT_R32_UINT\n");
		for (i = offset / 4; i < size / 4; i++)
			fprintf(fd, "%u\n", buf32[i]);
		break;
	default:
		// Illegal format for an index buffer
		break;
	}

	fclose(fd);
}

void HackerContext::DumpBuffer(ID3D11Buffer *buffer, wchar_t *filename,
		FrameAnalysisOptions type_mask, int idx, DXGI_FORMAT ib_fmt,
		UINT stride, UINT offset)
{
	FrameAnalysisOptions options = (FrameAnalysisOptions)(analyse_options & type_mask);
	D3D11_BUFFER_DESC desc;
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Buffer *staging = NULL;
	HRESULT hr;
	FILE *fd = NULL;
	wchar_t *ext;

	ext = wcsrchr(filename, L'.');
	if (!ext)
		return;

	buffer->GetDesc(&desc);

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	hr = mOrigDevice->CreateBuffer(&desc, NULL, &staging);
	if (FAILED(hr)) {
		LogInfo("DumpBuffer failed to create staging buffer: 0x%x\n", hr);
		return;
	}

	mOrigContext->CopyResource(staging, buffer);
	mOrigContext->Map(staging, 0, D3D11_MAP_READ, 0, &map);

	if (options & FrameAnalysisOptions::DUMP_XX_BIN) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".buf");

		_wfopen_s(&fd, filename, L"wb");
		if (!fd)
			goto out_unmap;
		fwrite(map.pData, 1, desc.ByteWidth, fd);
		fclose(fd);
	}

	if (options & FrameAnalysisOptions::DUMP_XX_TXT) {
		wcscpy_s(ext, MAX_PATH + filename - ext, L".txt");
		if (options & FrameAnalysisOptions::DUMP_CB_TXT)
			DumpBufferTxt(filename, &map, desc.ByteWidth, 'c', idx, 0, 0);
		else if (options & FrameAnalysisOptions::DUMP_VB_TXT)
			DumpBufferTxt(filename, &map, desc.ByteWidth, 'v', idx, stride, offset);
		else if (options & FrameAnalysisOptions::DUMP_IB_TXT)
			DumpIBTxt(filename, &map, desc.ByteWidth, ib_fmt, offset);
	}
	// TODO: Dump UAV, RT and SRV buffers as text taking their format,
	// offset & size into account.

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
			DumpBuffer((ID3D11Buffer*)resource, filename, type_mask, idx, ib_fmt, stride, offset);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource((ID3D11Texture2D*)resource, filename, type_mask);
			if (analyse_options & FrameAnalysisOptions::MONO)
				Dump2DResource((ID3D11Texture2D*)resource, filename, false, type_mask);
			break;
		default:
			LogInfo("frame analysis: skipped resource of type %i\n", dim);
			break;
	}
}

HRESULT HackerContext::FrameAnalysisFilename(wchar_t *filename, size_t size, bool compute,
		wchar_t *reg, char shader_type, int idx, uint32_t hash, ID3D11Resource *handle)
{
	struct ResourceInfo *info;
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\", G->ANALYSIS_PATH);

	if (!(analyse_options & FrameAnalysisOptions::FILENAME_REG))
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%06i-", G->analyse_frame);

	if (shader_type)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%cs-", shader_type);

	StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%ls", reg);
	if (idx != -1)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"%i", idx);

	if (analyse_options & FrameAnalysisOptions::FILENAME_REG)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-%06i", G->analyse_frame);

	if (hash) {
		try {
			info = &G->mResourceInfo.at(hash);
			if (info->hash_contaminated) {
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=!");
				if (info->update_contamination)
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"U");
				if (!info->copy_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"C");
				if (!info->region_contamination.empty())
					StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"S");
				StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"!");
			}
		} catch (std::out_of_range) {}

		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"=%08x", hash);
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
		LogInfo("frame analysis: failed to create filename: 0x%x\n", hr);
		// Could create a shorter filename without hashes if this
		// becomes a problem in practice
	}

	return hr;
}

void HackerContext::_DumpCBs(char shader_type,
	ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT])
{
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	for (i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
		if (!buffers[i])
			continue;

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"cb", shader_type, i, 0, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpResource(buffers[i], filename,
					FrameAnalysisOptions::DUMP_CB_MASK, i,
					DXGI_FORMAT_UNKNOWN, 0, 0);
		}

		buffers[i]->Release();
	}
}

void HackerContext::_DumpTextures(char shader_type,
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT])
{
	ID3D11Resource *resource;
	D3D11_RESOURCE_DIMENSION dim;
	wchar_t filename[MAX_PATH];
	uint32_t hash;
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
			hash = G->mResourceID.at((ID3D11Texture2D *)resource);
		} catch (std::out_of_range) {
			hash = 0;
		}

		// TODO: process description to get offset, strides & size for
		// buffer & bufferex type SRVs and pass down to dump routines,
		// although I have no idea how to determine which of the
		// entries in the two D3D11_BUFFER_SRV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"t", shader_type, i, hash, resource);
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
			CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('c', buffers);
		}
	} else {
		if (mCurrentVertexShader) {
			VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('v', buffers);
		}
		if (mCurrentHullShader) {
			HSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('h', buffers);
		}
		if (mCurrentDomainShader) {
			DSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('d', buffers);
		}
		if (mCurrentGeometryShader) {
			GSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('g', buffers);
		}
		if (mCurrentPixelShader) {
			PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, buffers);
			_DumpCBs('p', buffers);
		}
	}
}

void HackerContext::DumpVBs()
{
	ID3D11Buffer *buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	UINT i;

	// TODO: The format of each vertex buffer cannot be obtained from this
	// call. Rather, it is available in the input layout assigned to the
	// pipeline. There is no API to get the layout description, so if we
	// want to obtain it we will need to wrap the input layout objects
	// (there may be other good reasons to consider wrapping the input
	// layout if we ever do anything advanced with the vertex buffers).

	IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, buffers, strides, offsets);

	for (i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
		if (!buffers[i])
			continue;

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"vb", NULL, i, 0, buffers[i]);
		if (SUCCEEDED(hr)) {
			DumpResource(buffers[i], filename,
				FrameAnalysisOptions::DUMP_VB_MASK, i,
				DXGI_FORMAT_UNKNOWN, strides[i], offsets[i]);
		}

		buffers[i]->Release();
	}
}

void HackerContext::DumpIB()
{
	ID3D11Buffer *buffer = NULL;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	DXGI_FORMAT format;
	UINT offset;

	IAGetIndexBuffer(&buffer, &format, &offset);
	if (!buffer)
		return;

	hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"ib", NULL, -1, 0, buffer);
	if (SUCCEEDED(hr)) {
		DumpResource(buffer, filename,
				FrameAnalysisOptions::DUMP_IB_MASK, -1,
				format, 0, offset);
	}

	buffer->Release();
}

void HackerContext::DumpTextures(bool compute)
{
	ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

	if (compute) {
		if (mCurrentComputeShader) {
			CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('c', views);
		}
	} else {
		if (mCurrentVertexShader) {
			VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('v', views);
		}
		if (mCurrentHullShader) {
			HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('h', views);
		}
		if (mCurrentDomainShader) {
			DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('d', views);
		}
		if (mCurrentGeometryShader) {
			GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('g', views);
		}
		if (mCurrentPixelShader) {
			PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, views);
			_DumpTextures('p', views);
		}
	}
}

void HackerContext::DumpRenderTargets()
{
	UINT i;
	wchar_t filename[MAX_PATH];
	HRESULT hr;
	uint32_t hash;

	for (i = 0; i < mCurrentRenderTargets.size(); ++i) {
		// TODO: Decouple from HackerContext and remove dependency on
		// stat collection by querying the DeviceContext directly like
		// we do for all other resources
		try {
			hash = G->mResourceID.at(mCurrentRenderTargets[i]);
		} catch (std::out_of_range) {
			hash = 0;
		}

		// TODO: process description to get offset, strides & size for
		// buffer type RTVs and pass down to dump routines, although I
		// have no idea how to determine which of the entries in the
		// two D3D11_BUFFER_RTV unions will be valid.

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"o", NULL, i, hash, (ID3D11Resource*)mCurrentRenderTargets[i]);
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
	uint32_t hash;

	if (mCurrentDepthTarget) {
		// TODO: Decouple from HackerContext and remove dependency on
		// stat collection by querying the DeviceContext directly like
		// we do for all other resources
		try {
			hash = G->mResourceID.at(mCurrentDepthTarget);
		} catch (std::out_of_range) {
			hash = 0;
		}

		hr = FrameAnalysisFilename(filename, MAX_PATH, false, L"oD", NULL, -1, hash, (ID3D11Resource*)mCurrentDepthTarget);
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
	uint32_t hash;

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
			hash = G->mResourceID.at(resource);
		} catch (std::out_of_range) {
			hash = 0;
		}

		// TODO: process description to get offset & size for buffer
		// type UAVs and pass down to dump routines.

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, L"u", NULL, i, hash, resource);
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
				hash = G->mResourceID.at(mCurrentRenderTargets[i]);
				textureOverride = &G->mTextureOverrideMap.at(hash);
				new_options |= textureOverride->analyse_options;
			} catch (std::out_of_range) {}
		}

		if (mCurrentDepthTarget) {
			try {
				hash = G->mResourceID[mCurrentDepthTarget];
				textureOverride = &G->mTextureOverrideMap.at(hash);
				new_options |= textureOverride->analyse_options;
			} catch (std::out_of_range) {}
		}
	}

	if (!new_options)
		return;

	analyse_options = new_options;

	if (new_options & FrameAnalysisOptions::PERSIST)
		G->cur_analyse_options = new_options;
}

void HackerContext::FrameAnalysisAfterDraw(bool compute)
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
	if (GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
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
			LogInfo("DumpStereoResource failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}


	if (analyse_options & FrameAnalysisOptions::DUMP_CB_MASK)
		DumpCBs(compute);

	if (!compute) {
		if (analyse_options & FrameAnalysisOptions::DUMP_VB_MASK)
			DumpVBs();

		if (analyse_options & FrameAnalysisOptions::DUMP_IB_MASK)
			DumpIB();
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

	if ((analyse_options & FrameAnalysisOptions::DUMP_XXX_MASK) &&
	    (analyse_options & FrameAnalysisOptions::STEREO)) {
		NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, false);
	}

	G->analyse_frame++;
}
