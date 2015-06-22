#include "HackerContext.h"
#include "Globals.h"

#include <ScreenGrab.h>
#include <wincodec.h>
#include <Strsafe.h>

void HackerContext::Dump2DResource(ID3D11Texture2D *resource, wchar_t *filename, bool stereo)
{
	HRESULT hr;
	wchar_t *ext;

	ext = wcsrchr(filename, L'.');
	if (!ext)
		return;

	// Needs to be called at some point before SaveXXXTextureToFile:
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if ((analyse_options & FrameAnalysisOptions::DUMP_RT_JPS) ||
	    (analyse_options & FrameAnalysisOptions::DUMP_RT)) {
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


	if ((analyse_options & FrameAnalysisOptions::DUMP_RT_DDS) ||
	   ((analyse_options & FrameAnalysisOptions::DUMP_RT) && FAILED(hr))) {
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
void HackerContext::DumpStereoResource(ID3D11Texture2D *resource, wchar_t *filename)
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
		LogInfo("DumpStereoResource failed to create stereo texture: 0x%x \n", hr);
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
			LogInfo("DumpStereoResource failed to create intermediate texture: 0x%x \n", hr);
			goto out;
		}

		if (srcDesc.SampleDesc.Count > 1) {
			// Resolve MSAA surfaces. Procedure copied from DirectXTK
			// These need to have D3D11_USAGE_DEFAULT to resolve,
			// so we need yet another intermediate texture:
			hr = CreateStagingResource(&tmpResource2, srcDesc, false, true);
			if (FAILED(hr)) {
				LogInfo("DumpStereoResource failed to create intermediate texture: 0x%x \n", hr);
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
					index = D3D11CalcSubresource(level, item, srcDesc.MipLevels);
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
			index = D3D11CalcSubresource(level, item, srcDesc.MipLevels);
			srcBox.right = width >> level;
			srcBox.bottom = height >> level;
			mOrigContext->CopySubresourceRegion(stereoResource, index, 0, 0, 0,
					src, index, &srcBox);
		}
	}

	Dump2DResource(stereoResource, filename, true);

out2:
	if (tmpResource2)
		tmpResource2->Release();
out1:
	if (tmpResource)
		tmpResource->Release();

out:
	stereoResource->Release();
}

void HackerContext::DumpResource(ID3D11Resource *resource, wchar_t *filename)
{
	D3D11_RESOURCE_DIMENSION dim;

	resource->GetType(&dim);

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if (analyse_options & FrameAnalysisOptions::STEREO)
				DumpStereoResource((ID3D11Texture2D*)resource, filename);
			if (analyse_options & FrameAnalysisOptions::MONO)
				Dump2DResource((ID3D11Texture2D*)resource, filename, false);
			break;
		default:
			LogInfo("frame analysis: skipped resource of type %i\n", dim);
			break;
	}
}

HRESULT HackerContext::FrameAnalysisFilename(wchar_t *filename, size_t size,
		bool compute, bool uav, bool depth, int idx)
{
	wchar_t *pos;
	size_t rem;
	HRESULT hr;

	StringCchPrintfExW(filename, size, &pos, &rem, NULL, L"%ls\\%06i", G->ANALYSIS_PATH, G->analyse_frame);

	if (uav)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-u%i", idx);
	else if (depth)
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-oD");
	else
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-o%i", idx);

	if (compute) {
		StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-cs-%016I64x", mCurrentComputeShader);
	} else {
		if (mCurrentVertexShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-vs-%016I64x", mCurrentVertexShader);
		if (mCurrentHullShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-hs-%016I64x", mCurrentHullShader);
		if (mCurrentDomainShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ds-%016I64x", mCurrentDomainShader);
		if (mCurrentGeometryShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-gs-%016I64x", mCurrentGeometryShader);
		if (mCurrentPixelShader)
			StringCchPrintfExW(pos, rem, &pos, &rem, NULL, L"-ps-%016I64x", mCurrentPixelShader);
	}

	hr = StringCchPrintfW(pos, rem, L".XXX");
	if (FAILED(hr)) {
		LogInfo("frame analysis: failed to create filename: 0x%x\n", hr);
		// Could create a shorter filename without hashes if this
		// becomes a problem in practice
	}

	return hr;
}

void HackerContext::DumpRenderTargets()
{
	UINT i;
	NvAPI_Status nvret;
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	if (analyse_options & FrameAnalysisOptions::STEREO) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			LogInfo("DumpStereoResource failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	for (i = 0; i < mCurrentRenderTargets.size(); ++i) {
		hr = FrameAnalysisFilename(filename, MAX_PATH, false, false, false, i);
		if (FAILED(hr))
			goto out;
		DumpResource((ID3D11Resource*)mCurrentRenderTargets[i], filename);
	}
	if (mCurrentDepthTarget) {
		hr = FrameAnalysisFilename(filename, MAX_PATH, false, false, true, 0);
		if (FAILED(hr))
			goto out;
		DumpResource((ID3D11Resource*)mCurrentDepthTarget, filename);
	}

out:
	if (analyse_options & FrameAnalysisOptions::STEREO)
		NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, false);
}

void HackerContext::DumpUAVs(bool compute)
{
	UINT i;
	NvAPI_Status nvret;
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	ID3D11Resource *resource;
	wchar_t filename[MAX_PATH];
	HRESULT hr;

	if (analyse_options & FrameAnalysisOptions::STEREO) {
		// Enable reverse stereo blit for all resources we are about to dump:
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			LogInfo("DumpStereoResource failed to enable reverse stereo blit\n");
			// Continue anyway, we should still be able to dump in 2D...
		}
	}

	mOrigContext->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);

	for (i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; ++i) {
		if (!uavs[i])
			continue;

		uavs[i]->GetResource(&resource);
		if (!resource) {
			uavs[i]->Release();
			continue;
		}

		hr = FrameAnalysisFilename(filename, MAX_PATH, compute, true, false, i);
		if (SUCCEEDED(hr))
			DumpResource(resource, filename);

		resource->Release();
		uavs[i]->Release();
	}

	if (analyse_options & FrameAnalysisOptions::STEREO)
		NvAPI_Stereo_ReverseStereoBlitControl(mHackerDevice->mStereoHandle, false);
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
	UINT64 hash;
	UINT i;

	// TODO: Trigger on texture inputs

	if (compute) {
		// TODO: Trigger on current UAVs
	} else {
		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentVertexShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		try {
			shaderOverride = &G->mShaderOverrideMap.at(mCurrentPixelShader);
			new_options |= shaderOverride->analyse_options;
		} catch (std::out_of_range) {}

		for (i = 0; i < mCurrentRenderTargets.size(); ++i) {
			try {
				hash = G->mRenderTargets.at(mCurrentRenderTargets[i]);
				textureOverride = &G->mTextureOverrideMap.at(hash);
				new_options |= textureOverride->analyse_options;
			} catch (std::out_of_range) {}
		}

		if (mCurrentDepthTarget) {
			try {
				hash = G->mRenderTargets[mCurrentDepthTarget];
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

	if (analyse_options & FrameAnalysisOptions::DUMP_RT_MASK) {
		if (!compute)
			DumpRenderTargets();

		// UAVs can be used by both pixel shaders and compute shaders:
		DumpUAVs(compute);
	}

	G->analyse_frame++;
}
