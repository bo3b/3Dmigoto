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
#include "ResourceHash.h"

// -----------------------------------------------------------------------------------------------

HackerContext::HackerContext(ID3D11Device *pDevice, ID3D11DeviceContext *pContext)
	: ID3D11DeviceContext()
{
	mOrigDevice = pDevice;
	mOrigContext = pContext;
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

ID3D11DeviceContext* HackerContext::GetOrigContext(void)
{
	return mOrigContext;
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

	// We are using the original resource hash for stat collection - things
	// get tricky otherwise
	orig_hash = GetOrigResourceHash(resource);

	resource->Release();

	if (orig_hash)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mShaderResourceInfo.insert(orig_hash);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	return resource;
}

void HackerContext::RecordShaderResourceUsage()
{
	ID3D11ShaderResourceView *ps_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	ID3D11ShaderResourceView *vs_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	ID3D11Resource *resource;
	int i;

	mOrigContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, ps_views);
	mOrigContext->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vs_views);

	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
		resource = RecordResourceViewStats(ps_views[i]);
		if (resource)
			G->mPixelShaderInfo[mCurrentPixelShader].ResourceRegisters[i].insert(resource);

		resource = RecordResourceViewStats(vs_views[i]);
		if (resource)
			G->mVertexShaderInfo[mCurrentVertexShader].ResourceRegisters[i].insert(resource);
	}
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

	// We are using the original resource hash for stat collection - things
	// get tricky otherwise
	orig_hash = GetOrigResourceHash((ID3D11Texture2D *)resource);

	resource->Release();

	if (!resource)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		mCurrentRenderTargets.push_back(resource);
		G->mVisitedRenderTargets.insert(resource);
		G->mRenderTargetInfo.insert(orig_hash);
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
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

	// We are using the original resource hash for stat collection - things
	// get tricky otherwise
	orig_hash = GetOrigResourceHash(resource);

	resource->Release();

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		mCurrentDepthTarget = resource;
		G->mDepthTargetInfo.insert(orig_hash);
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

void HackerContext::AssignDummyRenderTarget()
{
	HRESULT hr;
	ID3D11Texture2D *resource = NULL;
	ID3D11RenderTargetView *resource_view = NULL;
	D3D11_TEXTURE2D_DESC desc;
	ID3D11DepthStencilView *depth_view = NULL;
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc;
	ID3D11Texture2D *depth_resource = NULL;

	mOrigContext->OMGetRenderTargets(0, NULL, &depth_view);

	if (!depth_view) {
		// Might still be able to make a dummy render target of arbitrary size?
		return;
	}

	depth_view->GetDesc(&depth_view_desc);

	if (depth_view_desc.ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2D &&
	    depth_view_desc.ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DMS &&
	    depth_view_desc.ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DARRAY &&
	    depth_view_desc.ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY) {
		goto out;
	}

	depth_view->GetResource((ID3D11Resource**)&depth_resource);
	if (!depth_resource)
		goto out;

	depth_resource->GetDesc(&desc);

	// Adjust desc to suit a render target:
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	hr = mOrigDevice->CreateTexture2D(&desc, NULL, &resource);
	if (FAILED(hr))
		goto out1;

	hr = mOrigDevice->CreateRenderTargetView(resource, NULL, &resource_view);
	if (FAILED(hr))
		goto out2;

	mOrigContext->OMSetRenderTargets(1, &resource_view, depth_view);


	resource_view->Release();
out2:
	resource->Release();
out1:
	depth_resource->Release();
out:
	depth_view->Release();
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

	// Check index buffer filter.
	if (!shaderOverride->indexBufferFilter.empty()) {
		bool found = false;
		for (vector<UINT64>::iterator l = shaderOverride->indexBufferFilter.begin(); l != shaderOverride->indexBufferFilter.end(); ++l)
			if (mCurrentIndexBuffer == *l)
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
			if (mCurrentVertexShader != shaderOverride->partner_hash)
				use_orig = true;
		}
		else {
			if (mCurrentPixelShader != shaderOverride->partner_hash)
				use_orig = true;
		}
	}

	RunCommandList(mHackerDevice, this, &shaderOverride->command_list,
			data->VertexCount, data->IndexCount, data->InstanceCount, false);

	// TODO: Add render target filters, texture filters, etc.

	if (use_orig) {
		if (isPixelShader) {
			PixelShaderReplacementMap::iterator i = G->mOriginalPixelShaders.find(mCurrentPixelShaderHandle);
			if (i != G->mOriginalPixelShaders.end())
				data->oldPixelShader = SwitchPSShader(i->second);
		}
		else {
			VertexShaderReplacementMap::iterator i = G->mOriginalVertexShaders.find(mCurrentVertexShaderHandle);
			if (i != G->mOriginalVertexShaders.end())
				data->oldVertexShader = SwitchVSShader(i->second);
		}
	} else {
		if (shaderOverride->fake_o0)
			AssignDummyRenderTarget();
	}

}

void HackerContext::BeforeDraw(DrawContext &data)
{
	float separationValue = FLT_MAX, convergenceValue = FLT_MAX;

	// Skip?
	data.skip = false;

	// If we are not hunting shaders, we should skip all of this shader management for a performance bump.
	if (G->hunting == HUNTING_MODE_ENABLED)
	{
		UINT selectedRenderTargetPos;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		{
			// Stats
			if (mCurrentVertexShader && mCurrentPixelShader)
			{
				G->mVertexShaderInfo[mCurrentVertexShader].PartnerShader.insert(mCurrentPixelShader);
				G->mPixelShaderInfo[mCurrentPixelShader].PartnerShader.insert(mCurrentVertexShader);
			}
			if (mCurrentPixelShader) {
				for (selectedRenderTargetPos = 0; selectedRenderTargetPos < mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
					std::vector<std::set<ID3D11Resource *>> &targets = G->mPixelShaderInfo[mCurrentPixelShader].RenderTargets;

					if (selectedRenderTargetPos >= targets.size())
						targets.push_back(std::set<ID3D11Resource *>());

					targets[selectedRenderTargetPos].insert(mCurrentRenderTargets[selectedRenderTargetPos]);
				}
				if (mCurrentDepthTarget)
					G->mPixelShaderInfo[mCurrentPixelShader].DepthTargets.insert(mCurrentDepthTarget);
			}

			// Maybe make this optional if it turns out to have a
			// significant performance impact:
			RecordShaderResourceUsage();

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
				if (G->marking_mode == MARKING_MODE_MONO)
				{
					data.override = true;
					separationValue = 0;
				}
				else if (G->marking_mode == MARKING_MODE_SKIP)
				{
					data.skip = true;
				}
				else if (G->marking_mode == MARKING_MODE_PINK)
				{
					if (G->mPinkingShader)
						data.oldPixelShader = SwitchPSShader(G->mPinkingShader);
				}
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (!G->fix_enabled)
		return;

	// Override settings?
	if (!G->mShaderOverrideMap.empty()) {
		ShaderOverrideMap::iterator i;

		i = G->mShaderOverrideMap.find(mCurrentVertexShader);
		if (i != G->mShaderOverrideMap.end()) {
			data.post_commands[0] = &i->second.post_command_list;
			ProcessShaderOverride(&i->second, false, &data, &separationValue, &convergenceValue);
		}

		if (mCurrentHullShader) {
			i = G->mShaderOverrideMap.find(mCurrentHullShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[1] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data, &separationValue, &convergenceValue);
			}
		}

		if (mCurrentDomainShader) {
			i = G->mShaderOverrideMap.find(mCurrentDomainShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[2] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data, &separationValue, &convergenceValue);
			}
		}

		if (mCurrentGeometryShader) {
			i = G->mShaderOverrideMap.find(mCurrentGeometryShader);
			if (i != G->mShaderOverrideMap.end()) {
				data.post_commands[3] = &i->second.post_command_list;
				ProcessShaderOverride(&i->second, false, &data, &separationValue, &convergenceValue);
			}
		}

		i = G->mShaderOverrideMap.find(mCurrentPixelShader);
		if (i != G->mShaderOverrideMap.end()) {
			data.post_commands[4] = &i->second.post_command_list;
			ProcessShaderOverride(&i->second, true, &data, &separationValue, &convergenceValue);
		}
	}

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
	return;
}

void HackerContext::AfterDraw(DrawContext &data)
{
	int i;

	if (G->analyse_frame)
		FrameAnalysisAfterDraw(false);

	if (data.skip)
		return;

	for (i = 0; i < 5; i++) {
		if (data.post_commands[i]) {
			RunCommandList(mHackerDevice, this, data.post_commands[i],
					data.VertexCount, data.IndexCount, data.InstanceCount, true);
		}
	}

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

HRESULT STDMETHODCALLTYPE HackerContext::QueryInterface(
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerContext::QueryInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigContext->QueryInterface(riid, ppvObject);

	LogDebug("  returns result = %x for %p \n", hr, ppvObject);
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
	LogDebug("HackerContext::GetDevice(%s@%p) returns %p \n", typeid(*this).name(), this, mHackerDevice);

	// Fix ref counting bug that slowly eats away at the device until we
	// crash. In FC4 this can happen after about 10 minutes, or when
	// running in windowed mode during launch.
	
	// Follow our rule of always calling the original call first to ensure that
	// any side-effects (including ref counting) are activated.
	mOrigContext->GetDevice(ppDevice);

	// Return our wrapped device though.
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
	LogInfo("HackerContext::GetPrivateData(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());

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
	LogInfo("HackerContext::SetPrivateData(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());
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
	LogInfo("HackerContext::SetPrivateDataInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());

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

HRESULT HackerContext::MapDenyCPURead(
	ID3D11Resource *pResource,
	UINT Subresource,
	D3D11_MAP MapType,
	UINT MapFlags,
	D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	ID3D11Texture2D *tex = (ID3D11Texture2D*)pResource;
	D3D11_TEXTURE2D_DESC desc;
	D3D11_RESOURCE_DIMENSION dim;
	uint32_t hash;
	TextureOverrideMap::iterator i;
	HRESULT hr;
	UINT replace_size;
	void *replace;

	if (!pResource || (MapType != D3D11_MAP_READ && MapType != D3D11_MAP_READ_WRITE))
		return E_FAIL;

	pResource->GetType(&dim);
	if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return E_FAIL;

	if (G->mTextureOverrideMap.empty())
		return E_FAIL;

	tex->GetDesc(&desc);
	hash = GetResourceHash(tex);

	LogDebug("Map Texture2D %08lx (%ux%u) Subresource=%u MapType=%i MapFlags=%u\n",
			hash, desc.Width, desc.Height, Subresource, MapType, MapFlags);

	// Currently only replacing first subresource to simplify map type, and
	// only on read access as it is unclear how to handle a read/write access.
	// Still log others in case we find we need them later.
	if (Subresource != 0 || MapType != D3D11_MAP_READ)
		return E_FAIL;

	i = G->mTextureOverrideMap.find(hash);
	if (i == G->mTextureOverrideMap.end())
		return E_FAIL;

	if (!i->second.deny_cpu_read)
		return E_FAIL;

	// TODO: We can probably skip the original map call altogether avoiding
	// the latency so long as the D3D11_MAPPED_SUBRESOURCE we return is sane.
	// Best to call original in case their are unknown side-effects, including
	// other wrappers who expect to get called.
	hr = mOrigContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);

	if (SUCCEEDED(hr) && pMappedResource->pData) {
		replace_size = pMappedResource->RowPitch * desc.Height;
		replace = malloc(replace_size);
		if (!replace) {
			LogDebug("deny_cpu_read out of memory\n");
			return E_OUTOFMEMORY;
		}
		memset(replace, 0, replace_size);
		mDeniedMaps[pResource] = replace;
		LogDebug("deny_cpu_read replaced mapping from 0x%p with %u bytes of 0s at 0x%p\n",
				pMappedResource->pData, replace_size, replace);
		pMappedResource->pData = replace;
	}

	return hr;
}

void HackerContext::FreeDeniedMapping(ID3D11Resource *pResource, UINT Subresource)
{
	if (Subresource != 0)
		return;

	if (G->mTextureOverrideMap.empty())
		return;

	DeniedMap::iterator i;
	i = mDeniedMaps.find(pResource);
	if (i == mDeniedMaps.end())
		return;

	LogDebug("deny_cpu_read freeing map at 0x%p\n", i->second);

	free(i->second);
	mDeniedMaps.erase(i);
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
	HRESULT hr = MapDenyCPURead(pResource, Subresource, MapType, MapFlags, pMappedResource);
	if (SUCCEEDED(hr))
		return hr;

	hr = mOrigContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);

	if (SUCCEEDED(hr))
		MapTrackResourceHashUpdate(pResource, Subresource, MapType, MapFlags, pMappedResource);

	return hr;
}

STDMETHODIMP_(void) HackerContext::Unmap(THIS_
	/* [annotation] */
	__in ID3D11Resource *pResource,
	/* [annotation] */
	__in  UINT Subresource)
{
	MapUpdateResourceHash(pResource, Subresource);
	FreeDeniedMapping(pResource, Subresource);
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
	FrameAnalysisLog("PSSetConstantBuffers(StartSlot:%u, NumBuffers:%u, ppConstantBuffers:0x%p)\n",
			StartSlot, NumBuffers, ppConstantBuffers);
	FrameAnalysisLogResourceArray(StartSlot, NumBuffers, (ID3D11Resource *const *)ppConstantBuffers);

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
		 &G->mGeometryShaders,
		 &G->mOriginalGeometryShaders,
		 NULL /* TODO: &G->mZeroGeometryShaders */,
		 &G->mVisitedGeometryShaders,
		 G->mSelectedGeometryShader,
		 &mCurrentGeometryShader,
		 &mCurrentGeometryShaderHandle);

	FrameAnalysisLog("GSSetShader(pShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pShader, ppClassInstances, NumClassInstances, mCurrentGeometryShader);

	if (pShader)
		BindStereoResources<&ID3D11DeviceContext::GSSetShaderResources>();
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
	LogDebug("HackerContext::Begin(%s@%p) \n", typeid(*this).name(), this);
	
	mOrigContext->Begin(pAsync);
}

STDMETHODIMP_(void) HackerContext::End(THIS_
	/* [annotation] */
	__in  ID3D11Asynchronous *pAsync)
{
	LogDebug("HackerContext::End(%s@%p) \n", typeid(*this).name(), this);

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
	FrameAnalysisLog("GSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);

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

bool HackerContext::BeforeDispatch(DispatchContext *context)
{
	if (G->hunting == HUNTING_MODE_ENABLED) {
		// TODO: Collect stats on assigned UAVs

		if (mCurrentComputeShader == G->mSelectedComputeShader) {
			if (G->marking_mode == MARKING_MODE_SKIP)
				return false;
		}
	}

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
			RunCommandList(mHackerDevice, this, &i->second.command_list, 0, 0, 0, false);
		}
	}

	return true;
}

void HackerContext::AfterDispatch(DispatchContext *context)
{
	if (G->analyse_frame)
		FrameAnalysisAfterDraw(true);

	if (context->post_commands)
		RunCommandList(mHackerDevice, this, context->post_commands, 0, 0, 0, true);
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

	FrameAnalysisLog("Dispatch(ThreadGroupCountX:%u, ThreadGroupCountY:%u, ThreadGroupCountZ:%u)\n",
			ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if (BeforeDispatch(&context))
		mOrigContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	AfterDispatch(&context);
}

STDMETHODIMP_(void) HackerContext::DispatchIndirect(THIS_
	/* [annotation] */
	__in  ID3D11Buffer *pBufferForArgs,
	/* [annotation] */
	__in  UINT AlignedByteOffsetForArgs)
{
	DispatchContext context;

	FrameAnalysisLog("DispatchIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);

	if (BeforeDispatch(&context))
		mOrigContext->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDispatch(&context);
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
	srcHash = GetResourceHash(srcTex);
	dstHash = GetResourceHash(dstTex);

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
		UpdateResourceHashFromCPU(pDstResource, NULL, pSrcData, SrcRowPitch, SrcDepthPitch);
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
	if (G->deferred_enabled)
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

	SetShader<ID3D11HullShader, &ID3D11DeviceContext::HSSetShader>
		(pHullShader, ppClassInstances, NumClassInstances,
		 &G->mHullShaders,
		 &G->mOriginalHullShaders,
		 NULL /* TODO: &G->mZeroHullShaders */,
		 &G->mVisitedHullShaders,
		 G->mSelectedHullShader,
		 &mCurrentHullShader,
		 &mCurrentHullShaderHandle);

	FrameAnalysisLog("HSSetShader(pHullShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pHullShader, ppClassInstances, NumClassInstances, mCurrentHullShader);

	if (pHullShader)
		BindStereoResources<&ID3D11DeviceContext::HSSetShaderResources>();
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

	SetShader<ID3D11DomainShader, &ID3D11DeviceContext::DSSetShader>
		(pDomainShader, ppClassInstances, NumClassInstances,
		 &G->mDomainShaders,
		 &G->mOriginalDomainShaders,
		 NULL /* &G->mZeroDomainShaders */,
		 &G->mVisitedDomainShaders,
		 G->mSelectedDomainShader,
		 &mCurrentDomainShader,
		 &mCurrentDomainShaderHandle);

	FrameAnalysisLog("DSSetShader(pDomainShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pDomainShader, ppClassInstances, NumClassInstances, mCurrentDomainShader);

	if (pDomainShader)
		BindStereoResources<&ID3D11DeviceContext::DSSetShaderResources>();
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
	FrameAnalysisLog("CSSetUnorderedAccessViews(StartSlot:%u, NumUAVs:%u, ppUnorderedAccessViews:0x%p, pUAVInitialCounts:0x%p)\n",
			StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
	FrameAnalysisLogViewArray(0, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);

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
	std::unordered_map<ID3D11Shader *, UINT64> *registered,
	std::unordered_map<ID3D11Shader *, ID3D11Shader *> *originalShaders,
	std::unordered_map<ID3D11Shader *, ID3D11Shader *> *zeroShaders,
	std::set<UINT64> *visitedShaders,
	UINT64 selectedShader,
	UINT64 *currentShaderHash,
	ID3D11Shader **currentShaderHandle)
{
	ID3D11Shader *repl_shader = pShader;

	if (pShader) {
		// Store as current shader. Need to do this even while
		// not hunting for ShaderOverride section in BeforeDraw
		// As an optimization, we can skip the lookup if there are no ShaderOverride
		// The lookup/find takes measurable amounts of CPU time.
		if (!G->mShaderOverrideMap.empty() || (G->hunting == HUNTING_MODE_ENABLED)) {
			std::unordered_map<ID3D11Shader *, UINT64>::iterator i = registered->find(pShader);
			if (i != registered->end()) {
				*currentShaderHash = i->second;
				*currentShaderHandle = pShader;
				LogDebug("  shader found: handle = %p, hash = %016I64x\n", *currentShaderHandle, *currentShaderHash);

				if ((G->hunting == HUNTING_MODE_ENABLED) && visitedShaders) {
					if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
					visitedShaders->insert(i->second);
					if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				}
			}
			else
				LogDebug("  shader %p not found\n", pShader);
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		// No longer conditional on G->hunting now that hunting may be soft enabled via key binding
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pShader);
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL) {
			LogDebug("  shader replaced by: %p\n", it->second.replacement);

			// Todo: It might make sense to Release() the original shader, to recover memory on GPU
			repl_shader = (ID3D11Shader*)it->second.replacement;
		}

		if (G->hunting == HUNTING_MODE_ENABLED) {
			// Replacement map.
			if (G->marking_mode == MARKING_MODE_ORIGINAL || !G->fix_enabled) {
				std::unordered_map<ID3D11Shader *, ID3D11Shader *>::iterator j = originalShaders->find(pShader);
				if ((selectedShader == *currentShaderHash || !G->fix_enabled) && j != originalShaders->end()) {
					repl_shader = j->second;
				}
			}
			if (G->marking_mode == MARKING_MODE_ZERO) {
				std::unordered_map<ID3D11Shader *, ID3D11Shader *>::iterator j = zeroShaders->find(pShader);
				if (selectedShader == *currentShaderHash && j != zeroShaders->end()) {
					repl_shader = j->second;
				}
			}
		}

	} else {
		*currentShaderHash = 0;
		*currentShaderHandle = NULL;
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
	LogDebug("HackerContext::CSSetShader called with computeshader handle = %p\n", pComputeShader);

	SetShader<ID3D11ComputeShader, &ID3D11DeviceContext::CSSetShader>
		(pComputeShader, ppClassInstances, NumClassInstances,
		 &G->mComputeShaders,
		 &G->mOriginalComputeShaders,
		 NULL /* TODO (if it makes sense): &G->mZeroComputeShaders */,
		 &G->mVisitedComputeShaders,
		 G->mSelectedComputeShader,
		 &mCurrentComputeShader,
		 &mCurrentComputeShaderHandle);

	FrameAnalysisLog("CSSetShader(pComputeShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pComputeShader, ppClassInstances, NumClassInstances, mCurrentComputeShader);

	if (pComputeShader)
		BindStereoResources<&ID3D11DeviceContext::CSSetShaderResources>();
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

	LogDebug("HackerContext::PSGetShader out: %p\n", *ppPixelShader);
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
	FrameAnalysisLogViewArray(0, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);
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

	SetShader<ID3D11VertexShader, &ID3D11DeviceContext::VSSetShader>
		(pVertexShader, ppClassInstances, NumClassInstances,
		 &G->mVertexShaders,
		 &G->mOriginalVertexShaders,
		 &G->mZeroVertexShaders,
		 &G->mVisitedVertexShaders,
		 G->mSelectedVertexShader,
		 &mCurrentVertexShader,
		 &mCurrentVertexShaderHandle);

	FrameAnalysisLog("VSSetShader(pVertexShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pVertexShader, ppClassInstances, NumClassInstances, mCurrentVertexShader);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (pVertexShader)
		BindStereoResources<&ID3D11DeviceContext::VSSetShaderResources>();
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
	LogDebug("HackerContext::PSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews) LogDebug("  ShaderResourceView[0] handle = %p\n", *ppShaderResourceViews);

	mOrigContext->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) HackerContext::PSSetShader(THIS_
	/* [annotation] */
	__in_opt ID3D11PixelShader *pPixelShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("HackerContext::PSSetShader called with pixelshader handle = %p\n", pPixelShader);

	SetShader<ID3D11PixelShader, &ID3D11DeviceContext::PSSetShader>
		(pPixelShader, ppClassInstances, NumClassInstances,
		 &G->mPixelShaders,
		 &G->mOriginalPixelShaders,
		 &G->mZeroPixelShaders,
		 &G->mVisitedPixelShaders,
		 G->mSelectedPixelShader,
		 &mCurrentPixelShader,
		 &mCurrentPixelShaderHandle);

	FrameAnalysisLog("PSSetShader(pPixelShader:0x%p, ppClassInstances:0x%p, NumClassInstances:%u) hash=%016I64x\n",
			pPixelShader, ppClassInstances, NumClassInstances, mCurrentPixelShader);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (pPixelShader) {
		BindStereoResources<&ID3D11DeviceContext::PSSetShaderResources>();

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
	FrameAnalysisLog("DrawIndexed(IndexCount:%u, StartIndexLocation:%u, BaseVertexLocation:%u)\n",
			IndexCount, StartIndexLocation, BaseVertexLocation);
	LogDebug("HackerContext::DrawIndexed called with IndexCount = %d, StartIndexLocation = %d, BaseVertexLocation = %d\n",
		IndexCount, StartIndexLocation, BaseVertexLocation);

	DrawContext c = DrawContext(0, IndexCount, 0);
	BeforeDraw(c);
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
	FrameAnalysisLog("Draw(VertexCount:%u, StartVertexLocation:%u)\n",
			VertexCount, StartVertexLocation);
	LogDebug("HackerContext::Draw called with VertexCount = %d, StartVertexLocation = %d\n",
		VertexCount, StartVertexLocation);

	DrawContext c = DrawContext(VertexCount, 0, 0);
	BeforeDraw(c);
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

	mOrigContext->IASetIndexBuffer(pIndexBuffer, Format, Offset);

	// When hunting, save this as a visited index buffer to cycle through.
	if (pIndexBuffer && !G->mDataBuffers.empty() && G->hunting == HUNTING_MODE_ENABLED) {
		DataBufferMap::iterator i = G->mDataBuffers.find(pIndexBuffer);
		if (i != G->mDataBuffers.end()) {
			mCurrentIndexBuffer = i->second;
			LogDebug("  index buffer found: handle = %p, hash = %08lx \n", pIndexBuffer, mCurrentIndexBuffer);

			if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				G->mVisitedIndexBuffers.insert(mCurrentIndexBuffer);
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else {
			LogDebug("  index buffer %p not found\n", pIndexBuffer);
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
	FrameAnalysisLog("DrawIndexedInstanced(IndexCountPerInstance:%u, InstanceCount:%u, StartIndexLocation:%u, BaseVertexLocation:%i, StartInstanceLocation:%u)\n",
			IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	LogDebug("HackerContext::DrawIndexedInstanced called with IndexCountPerInstance = %d, InstanceCount = %d\n",
		IndexCountPerInstance, InstanceCount);

	DrawContext c = DrawContext(0, IndexCountPerInstance, InstanceCount);
	BeforeDraw(c);
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
	FrameAnalysisLog("DrawInstanced(VertexCountPerInstance:%u, InstanceCount:%u, StartVertexLocation:%u, StartInstanceLocation:%u)\n",
			VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	LogDebug("HackerContext::DrawInstanced called with VertexCountPerInstance = %d, InstanceCount = %d\n",
		VertexCountPerInstance, InstanceCount);

	DrawContext c = DrawContext(VertexCountPerInstance, 0, InstanceCount);
	BeforeDraw(c);
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
	FrameAnalysisLog("VSSetShaderResources(StartSlot:%u, NumViews:%u, ppShaderResourceViews:0x%p)\n",
			StartSlot, NumViews, ppShaderResourceViews);
	FrameAnalysisLogViewArray(StartSlot, NumViews, (ID3D11View *const *)ppShaderResourceViews);
	LogDebug("HackerContext::VSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews) LogDebug("  ShaderResourceView[0] handle = %p\n", *ppShaderResourceViews);

	mOrigContext->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
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
	LogDebug("HackerContext::OMSetRenderTargets called with NumViews = %d\n", NumViews);

	if (G->hunting == HUNTING_MODE_ENABLED) {
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			mCurrentRenderTargets.clear();
			mCurrentDepthTarget = NULL;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

		if (ppRenderTargetViews) {
			for (UINT i = 0; i < NumViews; ++i) {
				if (!ppRenderTargetViews[i])
					continue;
				RecordRenderTargetInfo(ppRenderTargetViews[i], i);
				FrameAnalysisClearRT(ppRenderTargetViews[i]);
			}
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
	FrameAnalysisLog("OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs:%i, ppRenderTargetViews:0x%p, pDepthStencilView:0x%p, UAVStartSlot:%i, NumUAVs:%u, ppUnorderedAccessViews:0x%p, pUAVInitialCounts:0x%p)\n",
			NumRTVs, ppRenderTargetViews, pDepthStencilView,
			UAVStartSlot, NumUAVs, ppUnorderedAccessViews,
			pUAVInitialCounts);
	FrameAnalysisLogViewArray(0, NumRTVs, (ID3D11View *const *)ppRenderTargetViews);
	FrameAnalysisLogView(-1, "D", pDepthStencilView);
	FrameAnalysisLogViewArray(UAVStartSlot, NumUAVs, (ID3D11View *const *)ppUnorderedAccessViews);
	LogDebug("HackerContext::OMSetRenderTargetsAndUnorderedAccessViews called with NumRTVs = %d, NumUAVs = %d\n", NumRTVs, NumUAVs);

	if (G->hunting == HUNTING_MODE_ENABLED) {
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			mCurrentRenderTargets.clear();
			mCurrentDepthTarget = NULL;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

		if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
			if (ppRenderTargetViews) {
				for (UINT i = 0; i < NumRTVs; ++i) {
					if (!ppRenderTargetViews[i])
						continue;
					RecordRenderTargetInfo(ppRenderTargetViews[i], i);
					FrameAnalysisClearRT(ppRenderTargetViews[i]);
				}
			}

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
	FrameAnalysisLog("DrawAuto()\n");
	LogDebug("HackerContext::DrawAuto called\n");

	DrawContext c = DrawContext(0, 0, 0);
	BeforeDraw(c);
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
	FrameAnalysisLog("DrawIndexedInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);
	LogDebug("HackerContext::DrawIndexedInstancedIndirect called\n");

	DrawContext c = DrawContext(0, 0, 0);
	BeforeDraw(c);
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
	FrameAnalysisLog("DrawInstancedIndirect(pBufferForArgs:0x%p, AlignedByteOffsetForArgs:%u)\n",
			pBufferForArgs, AlignedByteOffsetForArgs);
	LogDebug("HackerContext::DrawInstancedIndirect called\n");

	DrawContext c = DrawContext(0, 0, 0);
	BeforeDraw(c);
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

	// Update stereo parameter texture. It's possible to arrive here with no texture available though,
	// so we need to check first.
	if (mHackerDevice->mStereoTexture)
	{
		LogDebug("  updating stereo parameter texture.\n");
		mHackerDevice->mParamTextureManager.UpdateStereoTexture(mHackerDevice, this, mHackerDevice->mStereoTexture, false);
	}
	else
	{
		LogDebug("  stereo parameter texture missing.\n");
	}

	mOrigContext->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}


// -----------------------------------------------------------------------------
// HackerContext1
//	Not positive we need this now, but makes it possible to wrap Device1 for 
//	systems with platform update installed.

HackerContext1::HackerContext1(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext)
	: HackerContext(pDevice1, pContext)
{
	mOrigDevice1 = pDevice1;
	mOrigContext1 = pContext;
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
	mOrigContext1->UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);

	// TODO: Track resource hash updates
}


void STDMETHODCALLTYPE HackerContext1::DiscardResource(
	/* [annotation] */
	_In_  ID3D11Resource *pResource)
{
	mOrigContext1->DiscardResource(pResource);
}

void STDMETHODCALLTYPE HackerContext1::DiscardView(
	/* [annotation] */
	_In_  ID3D11View *pResourceView)
{
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
	mOrigContext1->SwapDeviceContextState(pState, ppPreviousState);
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
	mOrigContext1->ClearView(pView, Color, pRect, NumRects);
}


void STDMETHODCALLTYPE HackerContext1::DiscardView1(
	/* [annotation] */
	_In_  ID3D11View *pResourceView,
	/* [annotation] */
	_In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
	UINT NumRects)
{
	mOrigContext1->DiscardView1(pResourceView, pRects, NumRects);
}
