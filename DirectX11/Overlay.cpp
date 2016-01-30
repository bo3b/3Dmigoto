// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>
#include <StrSafe.h>

#include "D3D11Wrapper.h"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "nvapi.h"


// Side note: Not really stoked with C++ string handling.  There are like 4 or
// 5 different ways to do things, all partly compatible, none a clear winner in
// terms of simplicity and clarity.  Generally speaking we'd want to use C++
// wstring and string, but there are no good output formatters.  Maybe the 
// newer iostream based pieces, but we'd still need to convert.
//
// The philosophy here and in other files, is to use whatever the API that we
// are using wants.  In this case it's a wchar_t* for DrawString, so we'll not
// do a lot of conversions and different formats, we'll just use wchar_t and its
// formatters.
//
// In particular, we also want to avoid 5 different libraries for string handling,
// Microsoft has way too many variants.  We'll use the regular C library from
// the standard c runtime, but use the _s safe versions.

// Max expected on-screen string size, used for buffer safe calls.
const int maxstring = 200;


Overlay::Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain)
{
	HRESULT hr;

	LogInfo("Overlay::Overlay created for %p: %s \n", pSwapChain, type_name(pSwapChain));
	LogInfo("  on HackerDevice: %p, HackerContext: %p \n", pDevice, pContext);

	// Not positive we need all of these references, but let's keep them handy.
	// We need the context at a minimum.
	mHackerDevice = pDevice;
	mHackerContext = pContext;
	mHackerSwapChain = pSwapChain;

	DXGI_SWAP_CHAIN_DESC description;
	hr = pSwapChain->GetDesc(&description);
	if (FAILED(hr))
		return;
	mResolution = DirectX::XMUINT2(description.BufferDesc.Width, description.BufferDesc.Height);

	// The courierbold.spritefont is now included as binary resource data attached
	// to the d3d11.dll.  We can fetch that resource and pass it to new SpriteFont
	// to avoid having to drag around the font file.
	HMODULE handle = GetModuleHandle(L"d3d11.dll");
	HRSRC rc = FindResource(handle, MAKEINTRESOURCE(IDR_COURIERBOLD), MAKEINTRESOURCE(SPRITEFONT));
	HGLOBAL rcData = LoadResource(handle, rc);
	DWORD fontSize = SizeofResource(handle, rc);
	uint8_t const* fontBlob = static_cast<const uint8_t*>(LockResource(rcData));

	// We want to use the original device and original context here, because
	// these will be used by DirectXTK to generate VertexShaders and PixelShaders
	// to draw the text, and we don't want to intercept those.
	mFont.reset(new DirectX::SpriteFont(pDevice->GetOrigDevice(), fontBlob, fontSize));
	mSpriteBatch.reset(new DirectX::SpriteBatch(pContext->GetOrigContext()));
}

Overlay::~Overlay()
{
}


// -----------------------------------------------------------------------------

using namespace DirectX::SimpleMath;

// Expected to be called at DXGI::Present() to be the last thing drawn.

// Notes:
	//1) Active PS location(probably x / N format)
	//2) Active VS location(x / N format)
	//3) Current convergence and separation. (convergence, a must)
	//4) Error state of reload(syntax errors go red or something)
	//5) Duplicate Mark(maybe yellow text for location, red if Decompile failed)

	//Maybe:
	//5) Other state, like show_original active.
	//6) Active toggle override.


// We need to save off everything that DirectTK will clobber and
// restore it before returning to the application. This is necessary
// to prevent rendering issues in some games like The Long Dark, and
// helps avoid introducing pipeline errors in other games like The
// Witcher 3.
// Only saving and restoring the first RenderTarget, as the only one
// we change for drawing overlay.

void Overlay::SaveState()
{
	memset(&state, 0, sizeof(state));

	ID3D11DeviceContext *context = mHackerContext->GetOrigContext();

	context->OMGetRenderTargets(1, &state.pRenderTargetView, &state.pDepthStencilView);
	state.RSNumViewPorts = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	context->RSGetViewports(&state.RSNumViewPorts, state.pViewPorts);

	context->OMGetBlendState(&state.pBlendState, state.BlendFactor, &state.SampleMask);
	context->OMGetDepthStencilState(&state.pDepthStencilState, &state.StencilRef);
	context->RSGetState(&state.pRasterizerState);
	context->PSGetSamplers(0, 1, state.samplers);
	context->IAGetPrimitiveTopology(&state.topology);
	context->IAGetInputLayout(&state.pInputLayout);
	context->VSGetShader(&state.pVertexShader, state.pVSClassInstances, &state.VSNumClassInstances);
	context->PSGetShader(&state.pPixelShader, state.pPSClassInstances, &state.PSNumClassInstances);
	context->IAGetVertexBuffers(0, 1, state.pVertexBuffers, state.Strides, state.Offsets);
	context->IAGetIndexBuffer(&state.IndexBuffer, &state.Format, &state.Offset);
	context->VSGetConstantBuffers(0, 1, state.pConstantBuffers);
	context->PSGetShaderResources(0, 1, state.pShaderResourceViews);
}

void Overlay::RestoreState()
{
	unsigned i;
	ID3D11DeviceContext *context = mHackerContext->GetOrigContext();

	context->OMSetRenderTargets(1, &state.pRenderTargetView, state.pDepthStencilView);
	if (state.pRenderTargetView)
		state.pRenderTargetView->Release();
	if (state.pDepthStencilView)
		state.pDepthStencilView->Release();

	context->RSSetViewports(state.RSNumViewPorts, state.pViewPorts);
	
	context->OMSetBlendState(state.pBlendState, state.BlendFactor, state.SampleMask);
	if (state.pBlendState)
		state.pBlendState->Release();

	context->OMSetDepthStencilState(state.pDepthStencilState, state.StencilRef);
	if (state.pDepthStencilState)
		state.pDepthStencilState->Release();

	context->RSSetState(state.pRasterizerState);
	if (state.pRasterizerState)
		state.pRasterizerState->Release();

	context->PSSetSamplers(0, 1, state.samplers);
	if (state.samplers[0])
		state.samplers[0]->Release();

	context->IASetPrimitiveTopology(state.topology);

	context->IASetInputLayout(state.pInputLayout);
	if (state.pInputLayout)
		state.pInputLayout->Release();

	context->VSSetShader(state.pVertexShader, state.pVSClassInstances, state.VSNumClassInstances);
	if (state.pVertexShader)
		state.pVertexShader->Release();
	for (i = 0; i < state.VSNumClassInstances; i++)
		state.pVSClassInstances[i]->Release();

	context->PSSetShader(state.pPixelShader, state.pPSClassInstances, state.PSNumClassInstances);
	if (state.pPixelShader)
		state.pPixelShader->Release();
	for (i = 0; i < state.PSNumClassInstances; i++)
		state.pPSClassInstances[i]->Release();

	context->IASetVertexBuffers(0, 1, state.pVertexBuffers, state.Strides, state.Offsets);
	if (state.pVertexBuffers[0])
		state.pVertexBuffers[0]->Release();

	context->IASetIndexBuffer(state.IndexBuffer, state.Format, state.Offset);
	if (state.IndexBuffer)
		state.IndexBuffer->Release();

	context->VSSetConstantBuffers(0, 1, state.pConstantBuffers);
	if (state.pConstantBuffers[0])
		state.pConstantBuffers[0]->Release();

	context->PSSetShaderResources(0, 1, state.pShaderResourceViews);
	if (state.pShaderResourceViews[0])
		state.pShaderResourceViews[0]->Release();
}

// We can't trust the game to have a proper drawing environment for DirectXTK.
//
// For two games we know of (Batman Arkham Knight and Project Cars) we were not
// getting an overlay, because apparently the rendertarget was left in an odd
// state.  This adds an init to be certain that the rendertarget is the backbuffer
// so that the overlay is drawn. 

HRESULT Overlay::InitDrawState()
{
	HRESULT hr;

	ID3D11Texture2D *pBackBuffer;
	hr = mHackerSwapChain->GetOrigSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	// use the back buffer address to create the render target
	ID3D11RenderTargetView *backbuffer;
	hr = mHackerDevice->GetOrigDevice()->CreateRenderTargetView(pBackBuffer, NULL, &backbuffer);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// set the first render target as the back buffer, with no stencil
	mHackerContext->GetOrigContext()->OMSetRenderTargets(1, &backbuffer, NULL);

	// Make sure there is at least one open viewport for DirectXTK to use.
	D3D11_VIEWPORT openView = CD3D11_VIEWPORT(0.0, 0.0, float(mResolution.x), float(mResolution.y));
	mHackerContext->GetOrigContext()->RSSetViewports(1, &openView);

	return S_OK;
}

// -----------------------------------------------------------------------------

// The active shader will show where we are in each list. / 0 / 0 will mean that we are not 
// actively searching. 

static void AppendShaderText(wchar_t *fullLine, wchar_t *type, int pos, size_t size)
{
	if (size == 0)
		return;

	// The position is zero based, so we'll make it +1 for the humans.
	if (++pos == 0)
		size = 0;

	// Format: "VS:1/15"
	wchar_t append[maxstring];
	swprintf_s(append, maxstring, L"%ls:%d/%Iu ", type, pos, size);

	wcscat_s(fullLine, maxstring, append);
}


// We also want to show the count of active vertex, pixel, compute, geometry, domain, hull
// shaders, that are active in the frame.  Any that have a zero count will be stripped, to
// keep it from being too busy looking.

static void CreateShaderCountString(wchar_t *counts)
{
	wcscpy_s(counts, maxstring, L"");
	AppendShaderText(counts, L"VS", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	AppendShaderText(counts, L"PS", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	AppendShaderText(counts, L"CS", G->mSelectedComputeShaderPos, G->mVisitedComputeShaders.size());
	AppendShaderText(counts, L"GS", G->mSelectedGeometryShaderPos, G->mVisitedGeometryShaders.size());
	AppendShaderText(counts, L"DS", G->mSelectedDomainShaderPos, G->mVisitedDomainShaders.size());
	AppendShaderText(counts, L"HS", G->mSelectedHullShaderPos, G->mVisitedHullShaders.size());
}


// Need to convert from the current selection, mSelectedVertexShader as hash, and
// find the OriginalShaderInfo that matches.  This is a linear search instead of a
// hash lookup, because we don't have the ID3D11DeviceChild*.

static bool FindInfoText(wchar_t *info, UINT64 selectedShader)
{
	if (selectedShader != -1)
	{
		for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> loaded in G->mReloadedShaders)
		{
			if ((loaded.second.hash == selectedShader) && !loaded.second.infoText.empty())
			{
				// Skip past first two characters, which will always be //
				wcscpy_s(info, maxstring, loaded.second.infoText.c_str() + 2);
				return true;
			}
		}
	}
	return false;
}


// This is for a line of text as info about the currently selected shader.  The line is 
// pulled out of the header of the HLSL text file, and can be anything.
// Since there can be multiple shaders selected, VS and PS and HS for example,
// we'll exit once any string is found, rather than show multiple lines.

static void CreateShaderInfoString(wchar_t *info)
{
	if (FindInfoText(info, G->mSelectedVertexShader))
		return;
	if (FindInfoText(info, G->mSelectedPixelShader))
		return;
	if (FindInfoText(info, G->mSelectedComputeShader))
		return;
	if (FindInfoText(info, G->mSelectedGeometryShader))
		return;
	if (FindInfoText(info, G->mSelectedDomainShader))
		return;
	if (FindInfoText(info, G->mSelectedHullShader))
		return;

	wcscpy_s(info, maxstring, L"");
}


// Create a string for display on the bottom edge of the screen, that contains the current
// stereo info of separation and convergence. 
// Desired format: "Sep:85  Conv:4.5"

static void CreateStereoInfoString(StereoHandle stereoHandle, wchar_t *info)
{
	// Rather than draw graphic bars, this will just be numeric.  Because
	// convergence is essentially an arbitrary number.

	float separation, convergence;
	NvU8 stereo = false;
	NvAPIOverride();
	NvAPI_Stereo_IsEnabled(&stereo);
	if (stereo)
	{
		NvAPI_Stereo_IsActivated(stereoHandle, &stereo);
		if (stereo)
		{
			NvAPI_Stereo_GetSeparation(stereoHandle, &separation);
			NvAPI_Stereo_GetConvergence(stereoHandle, &convergence);
		}
	}

	if (stereo)
		swprintf_s(info, maxstring, L"Sep:%.0f  Conv:%.1f", separation, convergence);
	else
		swprintf_s(info, maxstring, L"Stereo disabled");
}


void Overlay::DrawOverlay(void)
{
	HRESULT hr;

	// Since some games did not like having us change their drawing state from
	// SpriteBatch, we now save and restore all state information for the GPU
	// around our drawing.  
	SaveState();
	{
		hr = InitDrawState();
		if (FAILED(hr))
			goto fail_restore;

		mSpriteBatch->Begin();
		{
			wchar_t osdString[maxstring];
			Vector2 strSize;
			Vector2 textPosition;

			// Top of screen
			CreateShaderCountString(osdString);
			strSize = mFont->MeasureString(osdString);
			textPosition = Vector2(float(mResolution.x - strSize.x) / 2, 10);
			mFont->DrawString(mSpriteBatch.get(), osdString, textPosition, DirectX::Colors::LimeGreen);

			CreateShaderInfoString(osdString);
			strSize = mFont->MeasureString(osdString);
			textPosition = Vector2(float(mResolution.x - strSize.x) / 2, 10 + strSize.y);
			mFont->DrawString(mSpriteBatch.get(), osdString, textPosition, DirectX::Colors::LimeGreen);

			// Bottom of screen
			CreateStereoInfoString(mHackerDevice->mStereoHandle, osdString);
			strSize = mFont->MeasureString(osdString);
			textPosition = Vector2(float(mResolution.x - strSize.x) / 2, float(mResolution.y - strSize.y - 10));
			mFont->DrawString(mSpriteBatch.get(), osdString, textPosition, DirectX::Colors::LimeGreen);
		}
		mSpriteBatch->End();
	}
fail_restore:
	RestoreState();
}
