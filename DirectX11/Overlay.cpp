// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>

#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "nvapi.h"


Overlay::Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain)
{
	LogInfo("Overlay::Overlay created for %p: %s \n", pSwapChain, typeid(*pSwapChain).name());

	// Not positive we need all of these references, but let's keep them handy.
	// We need the context at a minimum.
	mHackerDevice = pDevice;
	mHackerContext = pContext;
	mHackerSwapChain = pSwapChain;
	mOverlayContext = NULL;

	HRESULT hr = mHackerDevice->GetOrigDevice()->CreateDeferredContext(0, &mOverlayContext);
	if (FAILED(hr)) {
		LogInfo("Failed to create overlay context: 0x%x\n", hr);
		return;
	}

	DXGI_SWAP_CHAIN_DESC description;
	pSwapChain->GetDesc(&description);
	mResolution = DirectX::XMUINT2(description.BufferDesc.Width, description.BufferDesc.Height);

	mStereoHandle = pDevice->mStereoHandle;

	// We want to use the original device and a dedicated context here, because
	// these will be used by DirectXTK to generate VertexShaders and PixelShaders
	// to draw the text, and we don't want to intercept those. Using a dedicated
	// context simplifies saving and restoring the pipeline state to avoid the
	// overlay causing rendering issues in the game.

	mFont.reset(new DirectX::SpriteFont(pDevice->GetOrigDevice(), L"courierbold.spritefont"));
	mSpriteBatch.reset(new DirectX::SpriteBatch(mOverlayContext));
}

Overlay::~Overlay()
{
	if (mOverlayContext)
		mOverlayContext->Release();
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

void Overlay::DrawOverlay(void)
{
	ID3D11CommandList *commandList = NULL;
	HRESULT hr;
	UINT count = 1;
	D3D11_VIEWPORT viewports[1]; // Only fetching the first one
	ID3D11RenderTargetView *targets[1];

	if (!mOverlayContext)
		return;

	// We can be called super early, before a viewport is bound to the 
	// pipeline.  That's a bug in the game, but we have to work around it.
	// If there are no viewports, the SpriteBatch will throw an exception.
	mHackerContext->RSGetViewports(&count, viewports);
	if (count <= 0)
	{
		LogInfo("Overlay::DrawOverlay called with no valid viewports.");
		return;
	}
	mOverlayContext->RSSetViewports(1, viewports);

	// TODO: Could get the back buffer directly from the swap chain, but
	// would still have to set up a view for it, so for now just get the
	// render target bound to the immediate context:
	mHackerContext->OMGetRenderTargets(1, targets, NULL);
	if (!targets[0]) {
		LogInfo("Overlay::DrawOverlay called with no render targets.");
		return;
	}
	mOverlayContext->OMSetRenderTargets(1, targets, NULL);
	targets[0]->Release();


	// As primary info, we are going to draw both separation and convergence. 
	// Rather than draw graphic bars, this will just be numeric.  The reason
	// is that convergence is essentially an arbitrary number.

	float separation, convergence;
	NvAPI_Stereo_GetSeparation(mStereoHandle, &separation);
	NvAPI_Stereo_GetConvergence(mStereoHandle, &convergence);
	
	// We also want to show the count of active vertex and pixel shaders, and
	// where we are in the list.  These should all be active from the Globals.
	// 0 / 0 will mean that we are not actively searching. The position is
	// zero based, so we'll make it +1 for the humans.
	size_t vsActive = 0, psActive = 0;
	size_t vsPosition = 0, psPosition = 0;

	if (G->mSelectedVertexShaderPos >= 0)
	{
		vsActive = G->mVisitedVertexShaders.size();
		vsPosition = G->mSelectedVertexShaderPos + 1;
	}
	if (G->mSelectedPixelShaderPos >= 0)
	{
		psActive = G->mVisitedPixelShaders.size();
		psPosition = G->mSelectedPixelShaderPos + 1;
	}

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
		swprintf_s(line, maxstring, L"VS:%d/%d  PS:%d/%d", vsPosition, vsActive, psPosition, psActive);
		strSize = mFont->MeasureString(line);
		textPosition = Vector2(float(mResolution.x - strSize.x) / 2, 10);
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);

		// Desired format "Sep:85  Conv:4.5"
		swprintf_s(line, maxstring, L"Sep:%.0f  Conv:%.1f", separation, convergence);
		strSize = mFont->MeasureString(line);
		textPosition = Vector2(float(mResolution.x - strSize.x) / 2, float(mResolution.y - strSize.y - 10));
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);
	}
	mSpriteBatch->End();

	hr = mOverlayContext->FinishCommandList(FALSE, &commandList);
	if (FAILED(hr)) {
		LogInfo("Overlay context FinishCommandList failed: 0x%x", hr);
		return;
	}

	mHackerContext->ExecuteCommandList(commandList, FALSE);

	commandList->Release();
}
