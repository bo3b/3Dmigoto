// Wrapper for the ID3D11DeviceContext.
// This gives us access to every D3D11 call for a Context, and override the pieces needed.
// Superclass access is directly through ID3D11DeviceContext interfaces.
// Hierarchy:
//  HackerContext <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

#include "HackerContext.h"

#include "log.h"
#include "HackerDevice.h"
#include "D3D11Wrapper.h"
#include "Globals.h"
#include "Hunting.h"
#include "Override.h"
#include "IniHandler.h"

// -----------------------------------------------------------------------------------------------

HackerContext::HackerContext(ID3D11Device *pDevice, ID3D11DeviceContext *pContext)
	: ID3D11DeviceContext()
{
	mOrigDevice = pDevice;
	mOrigContext = pContext;
}


// Save the corresponding HackerDevice, as we need to use it periodically to get
// access to the StereoParams.

void HackerContext::SetHackerDevice(HackerDevice *pDevice)
{
	mHackerDevice = pDevice;
}

// -----------------------------------------------------------------------------------------------

UINT64 HackerContext::GetTexture2DHash(ID3D11Texture2D *texture,
	bool log_new, struct ResourceInfo *resource_info)
{

	D3D11_TEXTURE2D_DESC desc;
	std::unordered_map<ID3D11Texture2D *, UINT64>::iterator j;

	texture->GetDesc(&desc);

	if (resource_info)
		*resource_info = desc;

	j = G->mTexture2D_ID.find(texture);
	if (j != G->mTexture2D_ID.end())
		return j->second;

	if (log_new) {
		// TODO: Refactor with LogRenderTarget()
		LogDebug("    Unknown render target:\n");
		LogDebug("    Width = %d, Height = %d, MipLevels = %d, ArraySize = %d\n",
			desc.Width, desc.Height, desc.MipLevels, desc.ArraySize);
		LogDebug("    Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
			desc.Format, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags);
	}

	return CalcTexture2DDescHash(&desc, 0, 0, 0);
}

UINT64 HackerContext::GetTexture3DHash(ID3D11Texture3D *texture,
	bool log_new, struct ResourceInfo *resource_info)
{

	D3D11_TEXTURE3D_DESC desc;
	std::unordered_map<ID3D11Texture3D *, UINT64>::iterator j;

	texture->GetDesc(&desc);

	if (resource_info)
		*resource_info = desc;

	j = G->mTexture3D_ID.find(texture);
	if (j != G->mTexture3D_ID.end())
		return j->second;

	if (log_new) {
		// TODO: Refactor with LogRenderTarget()
		LogDebug("    Unknown 3D render target:\n");
		LogDebug("    Width = %d, Height = %d, MipLevels = %d\n",
			desc.Width, desc.Height, desc.MipLevels);
		LogDebug("    Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
			desc.Format, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags);
	}

	return CalcTexture3DDescHash(&desc, 0, 0, 0);
}

// Records the hash of this shader resource view for later lookup. Returns the
// handle to the resource, but be aware that it no longer has a reference and
// should only be used for map lookups.
void* HackerContext::RecordResourceViewStats(ID3D11ShaderResourceView *view)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ID3D11Resource *resource = NULL;
	UINT64 hash = 0;

	if (!view)
		return NULL;

	view->GetResource(&resource);
	if (!resource)
		return NULL;

	view->GetDesc(&desc);

	switch (desc.ViewDimension) {
		case D3D11_SRV_DIMENSION_TEXTURE2D:
		case D3D11_SRV_DIMENSION_TEXTURE2DMS:
		case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
			hash = GetTexture2DHash((ID3D11Texture2D *)resource, false, NULL);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE3D:
			hash = GetTexture3DHash((ID3D11Texture3D *)resource, false, NULL);
			break;
	}

	resource->Release();

	if (hash)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mRenderTargets[resource] = hash;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	return resource;
}

void HackerContext::RecordShaderResourceUsage()
{
	ID3D11ShaderResourceView *ps_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	ID3D11ShaderResourceView *vs_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	void *resource;
	int i;

	mOrigContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, ps_views);
	mOrigContext->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vs_views);

	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
		resource = RecordResourceViewStats(ps_views[i]);
		if (resource) {
			// FIXME: Don't clobber these - it would be useful to
			// collect a set of all seen resources, e.g. for
			// matching all textures used by a shader.
			G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[i] = resource;
		}

		resource = RecordResourceViewStats(vs_views[i]);
		if (resource) {
			// FIXME: Don't clobber these - it would be useful to
			// collect a set of all seen resources, e.g. for
			// matching all textures used by a shader.
			G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[i] = resource;
		}
	}
}

void HackerContext::RecordRenderTargetInfo(ID3D11RenderTargetView *target, UINT view_num)
{
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	ID3D11Resource *resource = NULL;
	struct ResourceInfo resource_info;
	UINT64 hash = 0;

	if (!target)
		return;

	target->GetDesc(&desc);

	LogDebug("  View #%d, Format = %d, Is2D = %d\n",
		view_num, desc.Format, D3D11_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension);

	switch (desc.ViewDimension) {
		case D3D11_RTV_DIMENSION_TEXTURE2D:
		case D3D11_RTV_DIMENSION_TEXTURE2DMS:
			target->GetResource(&resource);
			if (!resource)
				return;
			hash = GetTexture2DHash((ID3D11Texture2D *)resource,
				gLogDebug, &resource_info);
			resource->Release();
			break;
		case D3D11_RTV_DIMENSION_TEXTURE3D:
			target->GetResource(&resource);
			if (!resource)
				return;
			hash = GetTexture3DHash((ID3D11Texture3D *)resource,
				gLogDebug, &resource_info);
			resource->Release();
			break;
	}

	if (!resource)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mRenderTargets[resource] = hash;
		G->mCurrentRenderTargets.push_back(resource);
		G->mVisitedRenderTargets.insert(resource);
		G->mRenderTargetInfo[hash] = resource_info;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

void HackerContext::RecordDepthStencil(ID3D11DepthStencilView *target)
{
	D3D11_DEPTH_STENCIL_VIEW_DESC desc;
	D3D11_TEXTURE2D_DESC tex_desc;
	ID3D11Resource *resource = NULL;
	ID3D11Texture2D *texture;
	struct ResourceInfo resource_info;
	UINT64 hash = 0;

	if (!target)
		return;

	target->GetResource(&resource);
	if (!resource)
		return;

	target->GetDesc(&desc);

	switch (desc.ViewDimension) {
		// TODO: Is it worth recording the type of 2D texture view?
		// TODO: Maybe for array variants, record all resources in array?
		case D3D11_DSV_DIMENSION_TEXTURE2D:
		case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
		case D3D11_DSV_DIMENSION_TEXTURE2DMS:
		case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
			texture = (ID3D11Texture2D *)resource;
			hash = GetTexture2DHash(texture, false, NULL);
			texture->GetDesc(&tex_desc);
			resource_info = tex_desc;
			break;
	}

	resource->Release();

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mRenderTargets[resource] = hash;
		G->mCurrentDepthTarget = resource;
		G->mDepthTargetInfo[hash] = resource_info;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
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

void HackerContext::ProcessShaderOverride(ShaderOverride *shaderOverride, bool isPixelShader,
	DrawContext *data, float *separationValue, float *convergenceValue)
{
	bool use_orig = false;

	LogDebug("  override found for shader\n");

	*separationValue = shaderOverride->separation;
	if (*separationValue != FLT_MAX)
		data->override = true;
	*convergenceValue = shaderOverride->convergence;
	if (*convergenceValue != FLT_MAX)
		data->override = true;
	data->skip = shaderOverride->skip;

#if 0 /* Iterations are broken since we no longer use present() */
	// Check iteration.
	if (!shaderOverride->iterations.empty()) {
		std::vector<int>::iterator k = shaderOverride->iterations.begin();
		int currentiterations = *k = *k + 1;
		LogDebug("  current iterations = %d\n", currentiterations);

		data->override = false;
		while (++k != shaderOverride->iterations.end())
		{
			if (currentiterations == *k)
			{
				data->override = true;
				break;
			}
		}
		if (!data->override)
		{
			LogDebug("  override skipped\n");
		}
	}
#endif
	// Check index buffer filter.
	if (!shaderOverride->indexBufferFilter.empty()) {
		bool found = false;
		for (vector<UINT64>::iterator l = shaderOverride->indexBufferFilter.begin(); l != shaderOverride->indexBufferFilter.end(); ++l)
			if (G->mCurrentIndexBuffer == *l)
			{
				found = true;
				break;
			}
		if (!found)
		{
			data->override = false;
			data->skip = false;
		}

		// TODO: This filter currently seems pretty limited as it only
		// applies to handling=skip and per-draw separation/convergence.
	}

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

	if (shaderOverride->partner_hash) {
		if (isPixelShader) {
			if (G->mCurrentVertexShader != shaderOverride->partner_hash)
				use_orig = true;
		}
		else {
			if (G->mCurrentPixelShader != shaderOverride->partner_hash)
				use_orig = true;
		}
	}

	// TODO: Add render target filters, texture filters, etc.

	if (use_orig) {
		if (isPixelShader) {
			PixelShaderReplacementMap::iterator i = G->mOriginalPixelShaders.find(G->mCurrentPixelShaderHandle);
			if (i != G->mOriginalPixelShaders.end())
				data->oldPixelShader = SwitchPSShader(i->second);
		}
		else {
			VertexShaderReplacementMap::iterator i = G->mOriginalVertexShaders.find(G->mCurrentVertexShaderHandle);
			if (i != G->mOriginalVertexShaders.end())
				data->oldVertexShader = SwitchVSShader(i->second);
		}
	}

}

// Rather than do all that, we now insert a RunFrameActions in the Draw method of the Context object,
// where it is absolutely certain that the game is fully loaded and ready to go, because it's actively
// drawing.  This gives us too many calls, maybe 5 per frame, but should not be a problem. The code
// is expecting to be called in a loop, and locks out auto-repeat using that looping.

// Draw is a very late binding for the game, and should solve all these problems, and allow us to retire
// the dxgi wrapper as unneeded.  The draw is caught at AfterDraw in the Context, which is called for
// every type of Draw, including DrawIndexed.

void HackerContext::RunFrameActions()
{
	static ULONGLONG last_ticks = 0;
	ULONGLONG ticks = GetTickCount64();

	// Prevent excessive input processing. XInput added an extreme
	// performance hit when processing four controllers on every draw call,
	// so only process input if at least 8ms has passed (approx 125Hz - may
	// be less depending on timer resolution)
	if (ticks - last_ticks < 8)
		return;
	last_ticks = ticks;

	LogDebug("Running frame actions.  Device: %p\n", mHackerDevice);

	// Regardless of log settings, since this runs every frame, let's flush the log
	// so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
	if (LogFile) fflush(LogFile);

	bool newEvent = DispatchInputEvents(mHackerDevice);

	CurrentTransition.UpdateTransitions(mHackerDevice);

	// The config file is not safe to reload from within the input handler
	// since it needs to change the key bindings, so it sets this flag
	// instead and we handle it now.
	if (G->gReloadConfigPending)
		ReloadConfig(mHackerDevice);

	// When not hunting most keybindings won't have been registered, but
	// still skip the below logic that only applies while hunting.
	if (!G->hunting)
		return;

	// Update the huntTime whenever we get fresh user input.
	if (newEvent)
		G->huntTime = time(NULL);

	// Clear buffers after some user idle time.  This allows the buffers to be
	// stable during a hunt, and cleared after one minute of idle time.  The idea
	// is to make the arrays of shaders stable so that hunting up and down the arrays
	// is consistent, while the user is engaged.  After 1 minute, they are likely onto
	// some other spot, and we should start with a fresh set, to keep the arrays and
	// active shader list small for easier hunting.  Until the first keypress, the arrays
	// are cleared at each thread wake, just like before. 
	// The arrays will be continually filled by the SetShader sections, but should 
	// rapidly converge upon all active shaders.

	if (difftime(time(NULL), G->huntTime) > 60) {
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		TimeoutHuntingBuffers();
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
}

DrawContext HackerContext::BeforeDraw()
{
	DrawContext data;
	float separationValue = FLT_MAX, convergenceValue = FLT_MAX;

	// Skip?
	data.skip = G->mBlockingMode; // mBlockingMode doesn't appear that it can ever be set - hardcoded hack?

	// If we are not hunting shaders, we should skip all of this shader management for a performance bump.
	if (G->hunting)
	{
		UINT selectedRenderTargetPos;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		{
			// Stats
			if (G->mCurrentVertexShader && G->mCurrentPixelShader)
			{
				G->mVertexShaderInfo[G->mCurrentVertexShader].PartnerShader.insert(G->mCurrentPixelShader);
				G->mPixelShaderInfo[G->mCurrentPixelShader].PartnerShader.insert(G->mCurrentVertexShader);
			}
			if (G->mCurrentPixelShader) {
				for (selectedRenderTargetPos = 0; selectedRenderTargetPos < G->mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
					std::vector<std::set<void *>> &targets = G->mPixelShaderInfo[G->mCurrentPixelShader].RenderTargets;

					if (selectedRenderTargetPos >= targets.size())
						targets.push_back(std::set<void *>());

					targets[selectedRenderTargetPos].insert(G->mCurrentRenderTargets[selectedRenderTargetPos]);
				}
				if (G->mCurrentDepthTarget)
					G->mPixelShaderInfo[G->mCurrentPixelShader].DepthTargets.insert(G->mCurrentDepthTarget);
			}

			// Maybe make this optional if it turns out to have a
			// significant performance impact:
			RecordShaderResourceUsage();

			// Selection
			for (selectedRenderTargetPos = 0; selectedRenderTargetPos < G->mCurrentRenderTargets.size(); ++selectedRenderTargetPos)
				if (G->mCurrentRenderTargets[selectedRenderTargetPos] == G->mSelectedRenderTarget) break;
			if (G->mCurrentIndexBuffer == G->mSelectedIndexBuffer ||
				G->mCurrentVertexShader == G->mSelectedVertexShader ||
				G->mCurrentPixelShader == G->mSelectedPixelShader ||
				selectedRenderTargetPos < G->mCurrentRenderTargets.size())
			{
				LogDebug("  Skipping selected operation. CurrentIndexBuffer = %08lx%08lx, CurrentVertexShader = %08lx%08lx, CurrentPixelShader = %08lx%08lx\n",
					(UINT32)(G->mCurrentIndexBuffer >> 32), (UINT32)G->mCurrentIndexBuffer,
					(UINT32)(G->mCurrentVertexShader >> 32), (UINT32)G->mCurrentVertexShader,
					(UINT32)(G->mCurrentPixelShader >> 32), (UINT32)G->mCurrentPixelShader);

				// Snapshot render target list.
				if (G->mSelectedRenderTargetSnapshot != G->mSelectedRenderTarget)
				{
					G->mSelectedRenderTargetSnapshotList.clear();
					G->mSelectedRenderTargetSnapshot = G->mSelectedRenderTarget;
				}
				G->mSelectedRenderTargetSnapshotList.insert(G->mCurrentRenderTargets.begin(), G->mCurrentRenderTargets.end());
				// Snapshot info.
				if (G->mCurrentIndexBuffer == G->mSelectedIndexBuffer)
				{
					G->mSelectedIndexBuffer_VertexShader.insert(G->mCurrentVertexShader);
					G->mSelectedIndexBuffer_PixelShader.insert(G->mCurrentPixelShader);
				}
				if (G->mCurrentVertexShader == G->mSelectedVertexShader)
					G->mSelectedVertexShader_IndexBuffer.insert(G->mCurrentIndexBuffer);
				if (G->mCurrentPixelShader == G->mSelectedPixelShader)
					G->mSelectedPixelShader_IndexBuffer.insert(G->mCurrentIndexBuffer);
				if (G->marking_mode == MARKING_MODE_MONO)
				{
					data.override = true;
					separationValue = 0;
				}
				else if (G->marking_mode == MARKING_MODE_SKIP)
				{
					data.skip = true;
				}
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (!G->fix_enabled)
		return data;

	// Override settings?
	ShaderOverrideMap::iterator iVertex = G->mShaderOverrideMap.find(G->mCurrentVertexShader);
	ShaderOverrideMap::iterator iPixel = G->mShaderOverrideMap.find(G->mCurrentPixelShader);

	if (iVertex != G->mShaderOverrideMap.end())
		ProcessShaderOverride(&iVertex->second, false, &data, &separationValue, &convergenceValue);
	if (iPixel != G->mShaderOverrideMap.end())
		ProcessShaderOverride(&iPixel->second, true, &data, &separationValue, &convergenceValue);

	if (data.override) {
		HackerDevice *device = mHackerDevice;
		if (device->mStereoHandle) {
			if (separationValue != FLT_MAX) {
				LogDebug("  setting custom separation value\n");

				if (NVAPI_OK != NvAPI_Stereo_GetSeparation(device->mStereoHandle, &data.oldSeparation))
				{
					LogDebug("    Stereo_GetSeparation failed.\n");
				}
				NvAPIOverride();
				if (NVAPI_OK != NvAPI_Stereo_SetSeparation(device->mStereoHandle, separationValue * data.oldSeparation))
				{
					LogDebug("    Stereo_SetSeparation failed.\n");
				}
			}

			if (convergenceValue != FLT_MAX) {
				LogDebug("  setting custom convergence value\n");

				if (NVAPI_OK != NvAPI_Stereo_GetConvergence(device->mStereoHandle, &data.oldConvergence)) {
					LogDebug("    Stereo_GetConvergence failed.\n");
				}
				NvAPIOverride();
				if (NVAPI_OK != NvAPI_Stereo_SetConvergence(device->mStereoHandle, convergenceValue * data.oldConvergence)) {
					LogDebug("    Stereo_SetConvergence failed.\n");
				}
			}
		}
	}
	return data;
}

void HackerContext::AfterDraw(DrawContext &data)
{
	if (data.skip)
		return;

	if (data.override) {
		if (mHackerDevice->mStereoHandle) {
			if (data.oldSeparation != FLT_MAX) {
				NvAPIOverride();
				if (NVAPI_OK != NvAPI_Stereo_SetSeparation(mHackerDevice->mStereoHandle, data.oldSeparation)) {
					LogDebug("    Stereo_SetSeparation failed.\n");
				}
			}

			if (data.oldConvergence != FLT_MAX) {
				NvAPIOverride();
				if (NVAPI_OK != NvAPI_Stereo_SetConvergence(mHackerDevice->mStereoHandle, data.oldConvergence)) {
					LogDebug("    Stereo_SetConvergence failed.\n");
				}
			}
		}
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

	// When in hunting mode, we need to get time to run the UI for stepping through shaders.
	// This gets called for every Draw, and is a definitely overkill, but is a convenient spot
	// where we are absolutely certain that everyone is set up correctly.  And where we can
	// get the original ID3D11Device.  This used to be done through the DXGI Present interface,
	// but that had a number of problems.
	RunFrameActions();
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


STDMETHODIMP_(ULONG) HackerContext::Release(THIS)
{
	ULONG ulRef = mOrigContext->Release();
	LogDebug("HackerContext::Release counter=%d, this=%p\n", ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("  deleting self\n");

		delete this;
		return 0L;
	}
	return ulRef;
}

HRESULT STDMETHODCALLTYPE HackerContext::QueryInterface(
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	return mOrigContext->QueryInterface(riid, ppvObject);
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

// Todo: Be sure this works

STDMETHODIMP_(void) HackerContext::GetDevice(THIS_
	/* [annotation] */
	__out  ID3D11Device **ppDevice)
{
	*ppDevice = mHackerDevice;

	LogInfo("*** Double check context is correct ****\n\n");
	LogInfo("HackerContext::GetDevice return: %s", typeid(*ppDevice).name());
	LogInfo("\n*** Double check context is correct ****\n");

	// Old version for reference.
/*	// Map device to wrapper.
	HackerDevice *wrapper = (HackerDevice*)HackerDevice::GetDirect3DDevice(origDevice);

	if (!wrapper)
	{
		LogInfo("ID3D11DeviceContext::GetDevice called");
		LogInfo("  can't find wrapper for parent device. Returning original device handle = %p\n", origDevice);

		// Get original device pointer
		ID3D11Device *origDevice;
		ID3D11DeviceContext::GetDevice(&origDevice);

		*ppDevice = (ID3D11Device *)origDevice;
		return;
	}

	*ppDevice = wrapper;
*/
}

STDMETHODIMP HackerContext::GetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__inout  UINT *pDataSize,
	/* [annotation] */
	__out_bcount_opt(*pDataSize)  void *pData)
{
	LogInfo("HackerContext::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

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
	LogInfo("HackerContext::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
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
	LogInfo("mOrigContext->SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

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
	mOrigContext->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
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
	return mOrigContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
}

STDMETHODIMP_(void) HackerContext::Unmap(THIS_
	/* [annotation] */
	__in ID3D11Resource *pResource,
	/* [annotation] */
	__in  UINT Subresource)
{
	 mOrigContext->Unmap(pResource, Subresource);
}

STDMETHODIMP_(void) HackerContext::PSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers)
{
	 mOrigContext->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::IASetInputLayout(THIS_
	/* [annotation] */
	__in_opt ID3D11InputLayout *pInputLayout)
{
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
	 mOrigContext->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) HackerContext::GSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11GeometryShader *pShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	 mOrigContext->GSSetShader(pShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) HackerContext::IASetPrimitiveTopology(THIS_
	/* [annotation] */
	__in D3D11_PRIMITIVE_TOPOLOGY Topology)
{
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
	mOrigContext->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) HackerContext::Begin(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	 mOrigContext->Begin(pAsync);
}

STDMETHODIMP_(void) HackerContext::End(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	LogDebug("HackerContext::End called\n");

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
	return mOrigContext->GetData(pAsync, pData, DataSize, GetDataFlags);
}

STDMETHODIMP_(void) HackerContext::SetPredication(THIS_
	/* [annotation] */
	__in_opt ID3D11Predicate *pPredicate,
	/* [annotation] */
	__in  BOOL PredicateValue)
{
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
	 mOrigContext->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::GSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers)
{
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
	 mOrigContext->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

STDMETHODIMP_(void) HackerContext::OMSetDepthStencilState(THIS_
	/* [annotation] */
	__in_opt  ID3D11DepthStencilState *pDepthStencilState,
	/* [annotation] */
	__in  UINT StencilRef)
{
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
	LogDebug("HackerContext::SOSetTargets called with NumBuffers = %d\n", NumBuffers);

	 mOrigContext->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}

STDMETHODIMP_(void) HackerContext::Dispatch(THIS_
	/* [annotation] */
	__in  UINT ThreadGroupCountX,
	/* [annotation] */
	__in  UINT ThreadGroupCountY,
	/* [annotation] */
	__in  UINT ThreadGroupCountZ)
{
	 mOrigContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

STDMETHODIMP_(void) HackerContext::DispatchIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	 mOrigContext->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
}

STDMETHODIMP_(void) HackerContext::RSSetState(THIS_
	/* [annotation] */
	__in_opt  ID3D11RasterizerState *pRasterizerState)
{
	 mOrigContext->RSSetState(pRasterizerState);
}

STDMETHODIMP_(void) HackerContext::RSSetViewports(THIS_
	/* [annotation] */
	__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
	/* [annotation] */
	__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports)
{
	 mOrigContext->RSSetViewports(NumViewports, pViewports);
}

STDMETHODIMP_(void) HackerContext::RSSetScissorRects(THIS_
	/* [annotation] */
	__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
	/* [annotation] */
	__in_ecount_opt(NumRects)  const D3D11_RECT *pRects)
{
	 mOrigContext->RSSetScissorRects(NumRects, pRects);
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
	 mOrigContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
		pSrcResource, SrcSubresource, pSrcBox);
}

STDMETHODIMP_(void) HackerContext::CopyResource(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pDstResource,
	/* [annotation] */
	__in  ID3D11Resource *pSrcResource)
{
	 mOrigContext->CopyResource(pDstResource, pSrcResource);
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
	 mOrigContext->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
		SrcDepthPitch);
}

STDMETHODIMP_(void) HackerContext::CopyStructureCount(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pDstBuffer,
	/* [annotation] */
	__in  UINT DstAlignedByteOffset,
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pSrcView)
{
	 mOrigContext->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}

STDMETHODIMP_(void) HackerContext::ClearUnorderedAccessViewUint(THIS_
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const UINT Values[4])
{
	mOrigContext->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
}

STDMETHODIMP_(void) HackerContext::ClearUnorderedAccessViewFloat(THIS_
	/* [annotation] */
	__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const FLOAT Values[4])
{
	mOrigContext->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
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
	mOrigContext->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}

STDMETHODIMP_(void) HackerContext::GenerateMips(THIS_
	/* [annotation] */
	__in  ID3D11ShaderResourceView *pShaderResourceView)
{
	 mOrigContext->GenerateMips(pShaderResourceView);
}

STDMETHODIMP_(void) HackerContext::SetResourceMinLOD(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	FLOAT MinLOD)
{
	 mOrigContext->SetResourceMinLOD(pResource, MinLOD);
}

STDMETHODIMP_(FLOAT) HackerContext::GetResourceMinLOD(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource)
{
	return mOrigContext->GetResourceMinLOD(pResource);
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
	 mOrigContext->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource,
		Format);
}

STDMETHODIMP_(void) HackerContext::ExecuteCommandList(THIS_
	/* [annotation] */
	__in  ID3D11CommandList *pCommandList,
	BOOL RestoreContextState)
{
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
	 mOrigContext->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::HSSetShader(THIS_
	/* [annotation] */
	__in_opt  ID3D11HullShader *pHullShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("HackerContext::HSSetShader called\n");

	 mOrigContext->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) HackerContext::HSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
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
	 mOrigContext->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::DSSetShader(THIS_
	/* [annotation] */
	__in_opt  ID3D11DomainShader *pDomainShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("HackerContext::DSSetShader called\n");

	 mOrigContext->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) HackerContext::DSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
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
	 mOrigContext->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
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
	 mOrigContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

STDMETHODIMP_(void) HackerContext::CSSetShader(THIS_
	/* [annotation] */
	__in_opt  ID3D11ComputeShader *pComputeShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("HackerContext::CSSetShader called\n");

	 mOrigContext->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) HackerContext::CSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers)
{
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

	LogDebug("HackerContext::PSGetShader out: %p\n", *ppPixelShader);
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
	LogDebug("HackerContext::VSGetShader out: %p\n", *ppVertexShader);
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
}

STDMETHODIMP_(void) HackerContext::IAGetInputLayout(THIS_
	/* [annotation] */
	__out  ID3D11InputLayout **ppInputLayout)
{
	 mOrigContext->IAGetInputLayout(ppInputLayout);
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
}

STDMETHODIMP_(void) HackerContext::IAGetPrimitiveTopology(THIS_
	/* [annotation] */
	__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	 mOrigContext->IAGetPrimitiveTopology(pTopology);
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
}

STDMETHODIMP_(void) HackerContext::GetPredication(THIS_
	/* [annotation] */
	__out_opt  ID3D11Predicate **ppPredicate,
	/* [annotation] */
	__out_opt  BOOL *pPredicateValue)
{
	 mOrigContext->GetPredication(ppPredicate, pPredicateValue);
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
}

STDMETHODIMP_(void) HackerContext::OMGetDepthStencilState(THIS_
	/* [annotation] */
	__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
	/* [annotation] */
	__out_opt  UINT *pStencilRef)
{
	 mOrigContext->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}

STDMETHODIMP_(void) HackerContext::SOGetTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets)
{
	 mOrigContext->SOGetTargets(NumBuffers, ppSOTargets);
}

STDMETHODIMP_(void) HackerContext::RSGetState(THIS_
	/* [annotation] */
	__out  ID3D11RasterizerState **ppRasterizerState)
{
	 mOrigContext->RSGetState(ppRasterizerState);
}

STDMETHODIMP_(void) HackerContext::RSGetViewports(THIS_
	/* [annotation] */
	__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
	/* [annotation] */
	__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports)
{
	 mOrigContext->RSGetViewports(pNumViewports, pViewports);
}

STDMETHODIMP_(void) HackerContext::RSGetScissorRects(THIS_
	/* [annotation] */
	__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
	/* [annotation] */
	__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects)
{
	 mOrigContext->RSGetScissorRects(pNumRects, pRects);
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
}

STDMETHODIMP_(void) HackerContext::ClearState(THIS)
{
	 mOrigContext->ClearState();
}

STDMETHODIMP_(void) HackerContext::Flush(THIS)
{
	 mOrigContext->Flush();
}

STDMETHODIMP_(D3D11_DEVICE_CONTEXT_TYPE) HackerContext::GetType(THIS)
{
	return mOrigContext->GetType();
}

STDMETHODIMP_(UINT) HackerContext::GetContextFlags(THIS)
{
	return mOrigContext->GetContextFlags();
}

STDMETHODIMP HackerContext::FinishCommandList(THIS_
	BOOL RestoreDeferredContextState,
	/* [annotation] */
	__out_opt  ID3D11CommandList **ppCommandList)
{
	return mOrigContext->FinishCommandList(RestoreDeferredContextState, ppCommandList);
}


// -----------------------------------------------------------------------------------------------

// The rest of these methods are all the primary code for the tool, Direct3D calls that we override
// in order to replace or modify shaders.

STDMETHODIMP_(void) HackerContext::VSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11VertexShader *pVertexShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("HackerContext::VSSetShader called with vertexshader handle = %p\n", pVertexShader);

	bool patchedShader = false;
	if (pVertexShader)
	{
		// Store as current vertex shader. Need to do this even while
		// not hunting for ShaderOverride sections.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		VertexShaderMap::iterator i = G->mVertexShaders.find(pVertexShader);
		if (i != G->mVertexShaders.end())
		{
			G->mCurrentVertexShader = i->second;
			G->mCurrentVertexShaderHandle = pVertexShader;
			LogDebug("  vertex shader found: handle = %p, hash = %08lx%08lx\n", pVertexShader, (UINT32)(G->mCurrentVertexShader >> 32), (UINT32)G->mCurrentVertexShader);

			if (G->hunting) {
				// Add to visited vertex shaders.
				G->mVisitedVertexShaders.insert(i->second);
				patchedShader = true;
			}

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else
		{
			LogDebug("  vertex shader %p not found\n", pVertexShader);
			// G->mCurrentVertexShader = 0;
		}
	}

	if (G->hunting && pVertexShader)
	{
		// Replacement map.
		if (G->marking_mode == MARKING_MODE_ORIGINAL || !G->fix_enabled) {
			VertexShaderReplacementMap::iterator j = G->mOriginalVertexShaders.find(pVertexShader);
			if ((G->mSelectedVertexShader == G->mCurrentVertexShader || !G->fix_enabled) && j != G->mOriginalVertexShaders.end())
			{
				ID3D11VertexShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				mOrigContext->VSSetShader(shader, ppClassInstances, NumClassInstances);
				return;
			}
		}
		if (G->marking_mode == MARKING_MODE_ZERO) {
			VertexShaderReplacementMap::iterator j = G->mZeroVertexShaders.find(pVertexShader);
			if (G->mSelectedVertexShader == G->mCurrentVertexShader && j != G->mZeroVertexShaders.end())
			{
				ID3D11VertexShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				mOrigContext->VSSetShader(shader, ppClassInstances, NumClassInstances);
				return;
			}
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pVertexShader);
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL)
		{
			LogDebug("  vertex shader replaced by: %p\n", it->second.replacement);

			ID3D11VertexShader *shader = (ID3D11VertexShader*)it->second.replacement;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			mOrigContext->VSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}
	}

	if (pVertexShader) {
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	mOrigContext->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (!G->hunting || patchedShader)
	{
		if (mHackerDevice)
		{
			// Set NVidia stereo texture.
			if (mHackerDevice->mStereoResourceView)
			{
				LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

				mOrigContext->VSSetShaderResources(125, 1, &mHackerDevice->mStereoResourceView);
			}

			// Set constants from ini file if they exist
			if (mHackerDevice->mIniResourceView)
			{
				LogDebug("  adding ini constants as texture to shader resources in slot 120.\n");

				mOrigContext->VSSetShaderResources(120, 1, &mHackerDevice->mIniResourceView);
			}
		}
		else
		{
			LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
	}
}

STDMETHODIMP_(void) HackerContext::PSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	LogDebug("mOrigContext->PSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews) LogDebug("  ShaderResourceView[0] handle = %p\n", *ppShaderResourceViews);

	mOrigContext->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	// Resolve resource from resource view.
	// This is possibly no longer required as we collect stats on draw calls
	if (G->hunting && ppShaderResourceViews)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		for (UINT i = 0; i < NumViews; ++i)
		{
			void *pResource;
			int pos = StartSlot + i;

			pResource = RecordResourceViewStats(ppShaderResourceViews[i]);
			if (pResource) {
				// FIXME: Don't clobber these - it would be useful to
				// collect a set of all seen resources, e.g. for
				// matching all textures used by a shader.
				G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[pos] = pResource;
			}

		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	/*
	// Map nvidia texture slot.
	if (ppShaderResourceViews && NumViews && ppShaderResourceViews[0])
	{
	ID3D11Device *device = 0;
	GetDevice(&device);
	if (device && device->mStereoResourceView)
	{
	LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

	m_pContext->PSSetShaderResources(125, 1, &device->mStereoResourceView);
	}
	else
	{
	LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
	}
	device->Release();
	}
	*/
}

STDMETHODIMP_(void) HackerContext::PSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11PixelShader *pPixelShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("mOrigContext->PSSetShader called with pixelshader handle = %p\n", pPixelShader);

	bool patchedShader = false;
	if (pPixelShader)
	{
		// Store as current pixel shader. Need to do this even while
		// not hunting for ShaderOverride sections.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		PixelShaderMap::iterator i = G->mPixelShaders.find(pPixelShader);
		if (i != G->mPixelShaders.end())
		{
			G->mCurrentPixelShader = i->second;
			G->mCurrentPixelShaderHandle = pPixelShader;
			LogDebug("  pixel shader found: handle = %p, hash = %08lx%08lx\n", pPixelShader, (UINT32)(G->mCurrentPixelShader >> 32), (UINT32)G->mCurrentPixelShader);

			if (G->hunting) {
				// Add to visited pixel shaders.
				G->mVisitedPixelShaders.insert(i->second);
				patchedShader = true;
			}

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else
		{
			LogDebug("  pixel shader %p not found\n", pPixelShader);
		}
	}

	if (G->hunting && pPixelShader)
	{
		// Replacement map.
		if (G->marking_mode == MARKING_MODE_ORIGINAL || !G->fix_enabled) {
			PixelShaderReplacementMap::iterator j = G->mOriginalPixelShaders.find(pPixelShader);
			if ((G->mSelectedPixelShader == G->mCurrentPixelShader || !G->fix_enabled) && j != G->mOriginalPixelShaders.end())
			{
				ID3D11PixelShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				mOrigContext->PSSetShader(shader, ppClassInstances, NumClassInstances);
				return;
			}
		}
		if (G->marking_mode == MARKING_MODE_ZERO) {
			PixelShaderReplacementMap::iterator j = G->mZeroPixelShaders.find(pPixelShader);
			if (G->mSelectedPixelShader == G->mCurrentPixelShader && j != G->mZeroPixelShaders.end())
			{
				ID3D11PixelShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				mOrigContext->PSSetShader(shader, ppClassInstances, NumClassInstances);
				return;
			}
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pPixelShader);
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL)
		{
			LogDebug("  pixel shader replaced by: %p\n", it->second.replacement);

			// Todo: It might make sense to Release() the original shader, to recover memory on GPU
			ID3D11PixelShader *shader = (ID3D11PixelShader*)it->second.replacement;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			mOrigContext->PSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}
	}

	if (pPixelShader) {
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	mOrigContext->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (!G->hunting || patchedShader)
	{
		HackerDevice *device = mHackerDevice;

		if (device)
		{
			// Set NVidia stereo texture.
			if (device->mStereoResourceView)
			{
				LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

				mOrigContext->PSSetShaderResources(125, 1, &device->mStereoResourceView);
			}
			// Set constants from ini file if they exist
			if (device->mIniResourceView)
			{
				LogDebug("  adding ini constants as texture to shader resources in slot 120.\n");

				mOrigContext->PSSetShaderResources(120, 1, &device->mIniResourceView);
			}
			// Set custom depth texture.
			if (device->mZBufferResourceView)
			{
				LogDebug("  adding Z buffer to shader resources in slot 126.\n");

				mOrigContext->PSSetShaderResources(126, 1, &device->mZBufferResourceView);
			}
		}
		else
		{
			LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
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
	LogDebug("HackerContext::DrawIndexed called with IndexCount = %d, StartIndexLocation = %d, BaseVertexLocation = %d\n",
		IndexCount, StartIndexLocation, BaseVertexLocation);

	DrawContext c = BeforeDraw();
	if (!c.skip)
		 mOrigContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::Draw(THIS_
	/* [annotation] */
	__in  UINT VertexCount,
	/* [annotation] */
	__in  UINT StartVertexLocation)
{
	LogDebug("HackerContext::Draw called with VertexCount = %d, StartVertexLocation = %d\n",
		VertexCount, StartVertexLocation);

	DrawContext c = BeforeDraw();
	if (!c.skip)
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
	LogDebug("HackerContext::IASetIndexBuffer called\n");

	if (G->hunting && pIndexBuffer)
	{
		// Store as current index buffer.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			DataBufferMap::iterator i = G->mDataBuffers.find(pIndexBuffer);
			if (i != G->mDataBuffers.end())
			{
				G->mCurrentIndexBuffer = i->second;
				LogDebug("  index buffer found: handle = %p, hash = %08lx%08lx\n", pIndexBuffer, (UINT32)(G->mCurrentIndexBuffer >> 32), (UINT32)G->mCurrentIndexBuffer);

				// Add to visited index buffers.
				G->mVisitedIndexBuffers.insert(G->mCurrentIndexBuffer);

				// second try to hide index buffer.
				// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
				//	pIndexBuffer = 0;
			}
			else LogDebug("  index buffer %p not found\n", pIndexBuffer);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	mOrigContext->IASetIndexBuffer(pIndexBuffer, Format, Offset);
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
	LogDebug("HackerContext::DrawIndexedInstanced called with IndexCountPerInstance = %d, InstanceCount = %d\n",
		IndexCountPerInstance, InstanceCount);

	DrawContext c = BeforeDraw();
	if (!c.skip)
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
	LogDebug("HackerContext::DrawInstanced called with VertexCountPerInstance = %d, InstanceCount = %d\n",
		VertexCountPerInstance, InstanceCount);

	DrawContext c = BeforeDraw();
	if (!c.skip)
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
	LogDebug("HackerContext::VSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews) LogDebug("  ShaderResourceView[0] handle = %p\n", *ppShaderResourceViews);

	mOrigContext->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	// Resolve resource from resource view.
	// This is possibly no longer required as we collect stats on draw calls
	if (G->hunting && ppShaderResourceViews)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		for (UINT i = 0; i < NumViews; ++i)
		{
			void *pResource;
			int pos = StartSlot + i;

			pResource = RecordResourceViewStats(ppShaderResourceViews[i]);
			if (pResource) {
				// FIXME: Don't clobber these - it would be useful to
				// collect a set of all seen resources, e.g. for
				// matching all textures used by a shader.
				G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[pos] = pResource;
			}

		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	/*
	// Map nvidia texture slot.
	if (ppShaderResourceViews && NumViews && ppShaderResourceViews[0])
	{
	ID3D11Device *device = 0;
	GetDevice(&device);
	if (device && device->mStereoResourceView)
	{
	LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

	m_pContext->VSSetShaderResources(125, 1, &device->mStereoResourceView);
	}
	else
	{
	LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
	}
	device->Release();
	}
	*/
}

STDMETHODIMP_(void) HackerContext::OMSetRenderTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
	/* [annotation] */
	__in_ecount_opt(NumViews) ID3D11RenderTargetView *const *ppRenderTargetViews,
	/* [annotation] */
	__in_opt ID3D11DepthStencilView *pDepthStencilView)
{
	LogDebug("HackerContext::OMSetRenderTargets called with NumViews = %d\n", NumViews);

	if (G->hunting)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mCurrentRenderTargets.clear();
			G->mCurrentDepthTarget = NULL;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

		if (ppRenderTargetViews) {
			for (UINT i = 0; i < NumViews; ++i)
				RecordRenderTargetInfo(ppRenderTargetViews[i], i);
		}

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
	LogDebug("HackerContext::OMSetRenderTargetsAndUnorderedAccessViews called with NumRTVs = %d, NumUAVs = %d\n", NumRTVs, NumUAVs);

	mOrigContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

STDMETHODIMP_(void) HackerContext::DrawAuto(THIS)
{
	LogDebug("HackerContext::DrawAuto called\n");

	DrawContext c = BeforeDraw();
	if (!c.skip)
		mOrigContext->DrawAuto();
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawIndexedInstancedIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	LogDebug("HackerContext::DrawIndexedInstancedIndirect called\n");

	DrawContext c = BeforeDraw();
	if (!c.skip)
		mOrigContext->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c);
}

STDMETHODIMP_(void) HackerContext::DrawInstancedIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	LogDebug("HackerContext::DrawInstancedIndirect called\n");

	DrawContext c = BeforeDraw();
	if (!c.skip)
		mOrigContext->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c);
}

// Done at the start of each frame to clear the buffer

STDMETHODIMP_(void) HackerContext::ClearRenderTargetView(THIS_
	/* [annotation] */
	__in  ID3D11RenderTargetView *pRenderTargetView,
	/* [annotation] */
	__in  const FLOAT ColorRGBA[4])
{
	LogDebug("HackerContext::ClearRenderTargetView called with RenderTargetView=%p, color=[%f,%f,%f,%f]\n", pRenderTargetView,
		ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);

	// Update stereo parameter texture.
	LogDebug("  updating stereo parameter texture.\n");

	if (G->ENABLE_TUNE)
	{
		//device->mParamTextureManager.mSeparationModifier = gTuneValue;
		mHackerDevice->mParamTextureManager.mTuneVariable1 = G->gTuneValue[0];
		mHackerDevice->mParamTextureManager.mTuneVariable2 = G->gTuneValue[1];
		mHackerDevice->mParamTextureManager.mTuneVariable3 = G->gTuneValue[2];
		mHackerDevice->mParamTextureManager.mTuneVariable4 = G->gTuneValue[3];
		int counter = 0;
		if (counter-- < 0)
		{
			counter = 30;
			mHackerDevice->mParamTextureManager.mForceUpdate = true;
		}
	}

	mHackerDevice->mParamTextureManager.UpdateStereoTexture(mHackerDevice, this, mHackerDevice->mStereoTexture, false);

	mOrigContext->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}

