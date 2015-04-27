// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>

#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "nvapi.h"

// We want to use the original device and original context here, because
// these will be used by DirectXTK to generate VertexShaders and PixelShaders
// to draw the text, and we don't want to intercept those.

Overlay::Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain)
{
	DXGI_SWAP_CHAIN_DESC description;
	pSwapChain->GetDesc(&description);
	mResolution = DirectX::XMUINT2(description.BufferDesc.Width, description.BufferDesc.Height);

	mStereoHandle = pDevice->mStereoHandle;

	mFont.reset(new DirectX::SpriteFont(pDevice->GetOrigDevice(), L"courierbold.spritefont"));
	mSpriteBatch.reset(new DirectX::SpriteBatch(pContext->GetOrigContext()));
}

Overlay::~Overlay()
{
}


// -----------------------------------------------------------------------------

using namespace DirectX::SimpleMath;

// Expected to be called at DXGI::Present() to be the last thing drawn.

void Overlay::DrawOverlay(void)
{

	// As primary info, we are going to draw both separation and convergence. 
	// Rather than draw graphic bars, this will just be numeric.  The reason
	// is that convergence is essentially an arbitrary number.

	float separation, convergence;
	NvAPI_Stereo_GetSeparation(mStereoHandle, &separation);
	NvAPI_Stereo_GetConvergence(mStereoHandle, &convergence);



	mSpriteBatch->Begin();
	{
		const int maxstring = 200;
		wchar_t line[maxstring];

		// Arbitrary choice, but this wants to draw the text on the left edge of the
		// screen, where longer lines don't need wrapping or centering concern.

		Vector2 textPosition(10, float(mResolution.y) / 2);

		swprintf_s(line, maxstring, L" Sep:%5.1f", separation);
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);

		textPosition.y += mFont->GetLineSpacing();
		swprintf_s(line, maxstring, L"Conv:%5.1f", convergence);
		mFont->DrawString(mSpriteBatch.get(), line, textPosition, DirectX::Colors::LimeGreen);
	}
	mSpriteBatch->End();
}
