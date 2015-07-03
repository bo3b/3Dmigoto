// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>
#include <StrSafe.h>

#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "nvapi.h"


Overlay::Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain)
{
	LogInfo("Overlay::Overlay created for %p: %s \n", pSwapChain, typeid(*pSwapChain).name());
	LogInfo("  on HackerDevice: %p, HackerContext: %p \n", pDevice, pContext);

	// Not positive we need all of these references, but let's keep them handy.
	// We need the context at a minimum.
	mHackerDevice = pDevice;
	mHackerContext = pContext;
	mHackerSwapChain = pSwapChain;

	DXGI_SWAP_CHAIN_DESC description;
	pSwapChain->GetDesc(&description);
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

void Overlay::SaveState()
{
	// We need to save off everything that DirectTK will clobber and
	// restore it before returning to the application. This is necessary
	// to prevent rendering issues in some games like The Long Dark, and
	// helps avoid introducing pipeline errors in other games like The
	// Witcher 3.

	memset(&state, 0, sizeof(state));

	ID3D11DeviceContext *context = mHackerContext->GetOrigContext();

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

static void AppendShaderOverlayText(wstring *line, wchar_t *type, int pos, std::set<UINT64> *visited)
{
	wchar_t buf[32];
	size_t size = visited->size();
	if (!size)
		return;

	if (++pos == 0)
		size = 0;

	StringCchPrintf(buf, 32, L"%ls:%d/%d ", type, pos, size);
	line->append(buf);
}

void Overlay::DrawOverlay(void)
{
	wstring shader_line;

	// We can be called super early, before a viewport is bound to the 
	// pipeline.  That's a bug in the game, but we have to work around it.
	// If there are no viewports, the SpriteBatch will throw an exception.
	UINT count = 0;
	mHackerContext->RSGetViewports(&count, NULL);
	if (count <= 0)
	{
		LogInfo("Overlay::DrawOverlay called with no valid viewports.");
		return;
	}


	// As primary info, we are going to draw both separation and convergence. 
	// Rather than draw graphic bars, this will just be numeric.  The reason
	// is that convergence is essentially an arbitrary number.

	float separation, convergence;
	NvU8 stereo = false;
	NvAPI_Stereo_IsEnabled(&stereo);
	if (stereo)
	{
		NvAPI_Stereo_IsActivated(mHackerDevice->mStereoHandle, &stereo);
		if (stereo)
		{
			NvAPI_Stereo_GetSeparation(mHackerDevice->mStereoHandle, &separation);
			NvAPI_Stereo_GetConvergence(mHackerDevice->mStereoHandle, &convergence);
		}
	}

	// We also want to show the count of active vertex and pixel shaders, and
	// where we are in the list.  These should all be active from the Globals.
	// 0 / 0 will mean that we are not actively searching. The position is
	// zero based, so we'll make it +1 for the humans.
	AppendShaderOverlayText(&shader_line, L"VS", G->mSelectedVertexShaderPos, &G->mVisitedVertexShaders);
	AppendShaderOverlayText(&shader_line, L"PS", G->mSelectedPixelShaderPos, &G->mVisitedPixelShaders);
	AppendShaderOverlayText(&shader_line, L"CS", G->mSelectedComputeShaderPos, &G->mVisitedComputeShaders);
	AppendShaderOverlayText(&shader_line, L"GS", G->mSelectedGeometryShaderPos, &G->mVisitedGeometryShaders);
	AppendShaderOverlayText(&shader_line, L"DS", G->mSelectedDomainShaderPos, &G->mVisitedDomainShaders);
	AppendShaderOverlayText(&shader_line, L"HS", G->mSelectedHullShaderPos, &G->mVisitedHullShaders);

	SaveState();
	mSpriteBatch->Begin();
	{
		const int maxstring = 200;
		wchar_t line[maxstring];
		Vector2 strSize;
		Vector2 textPosition;

		// Arbitrary choice, but this wants to draw the text on the left edge of the
		// screen, where longer lines don't need wrapping or centering concern.
		// Tried that, didn't really like it.  Let's try moving sep/conv at bottom middle,
		// and shader counts in top middle.

		// Small gap between sep/conv and the shader hunting locations. Format "VS:1/15"
		strSize = mFont->MeasureString(shader_line.c_str());
		textPosition = Vector2(float(mResolution.x - strSize.x) / 2, 10);
		mFont->DrawString(mSpriteBatch.get(), shader_line.c_str(), textPosition, DirectX::Colors::LimeGreen);

		// Desired format "Sep:85  Conv:4.5"
		if (stereo)
			swprintf_s(line, maxstring, L"Sep:%.0f  Conv:%.1f", separation, convergence);
		else
			swprintf_s(line, maxstring, L"Stereo disabled");
		strSize = mFont->MeasureString(line);
		textPosition = Vector2(float(mResolution.x - strSize.x) / 2, float(mResolution.y - strSize.y - 10));
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);
	}
	mSpriteBatch->End();
	RestoreState();
}
