// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>

#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "nvapi.h"


Overlay::Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain)
{
	// Not positive we need all of these references, but let's keep them handy.
	// We need the context at a minimum.
	mHackerDevice = pDevice;
	mHackerContext = pContext;
	mHackerSwapChain = pSwapChain;

	DXGI_SWAP_CHAIN_DESC description;
	pSwapChain->GetDesc(&description);
	mResolution = DirectX::XMUINT2(description.BufferDesc.Width, description.BufferDesc.Height);

	mStereoHandle = pDevice->mStereoHandle;

	// We want to use the original device and original context here, because
	// these will be used by DirectXTK to generate VertexShaders and PixelShaders
	// to draw the text, and we don't want to intercept those.

	mFont.reset(new DirectX::SpriteFont(pDevice->GetOrigDevice(), L"courierbold.spritefont"));
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

void Overlay::DrawOverlay(void)
{
	// We can be called super early, before a viewport is bound to the 
	// pipeline.  That's a bug in the game, but we have to work around it.
	// If there are no viewports, the SpriteBatch will throw an exception.
	UINT count = 0;
	mHackerContext->RSGetViewports(&count, NULL);
	if (count <= 0)
		return;


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

		// Arbitrary choice, but this wants to draw the text on the left edge of the
		// screen, where longer lines don't need wrapping or centering concern.

		Vector2 textPosition(10, float(mResolution.y) / 2);

		// Desired format "Sep:85" "Conv:4.5"
		swprintf_s(line, maxstring, L"Sep:%.0f\nConv:%.1f", separation, convergence);
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);

		// Small gap between sep/conv and the shader hunting locations. Format "VS:1/15"
		textPosition.y += 2.5f * mFont->GetLineSpacing();
		swprintf_s(line, maxstring, L"VS:%d/%d\nPS:%d/%d", vsPosition, vsActive, psPosition, psActive);
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);
	}
	mSpriteBatch->End();
}
