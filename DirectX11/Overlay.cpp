// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>

#include "SimpleMath.h"
#include "SpriteBatch.h"

// We want to use the original device and original context here, because
// these will be used by DirectXTK to generate VertexShaders and PixelShaders
// to draw the text, and we don't want to intercept those.

Overlay::Overlay(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	mFont.reset(new DirectX::SpriteFont(pDevice, L"courierbold.spritefont"));
	mSpriteBatch.reset(new DirectX::SpriteBatch(pContext));
}

Overlay::~Overlay()
{
}

// Expected to be called at DXGI::Present() to be the last thing drawn,
// but we are presently drawing it more often via the Draw() code override.

void Overlay::DrawOverlay(void)
{
	DirectX::SimpleMath::Vector2 fontPos(50, 1000);

	std::wstring output = std::wstring(L"Hello") + std::wstring(L" World");

	mSpriteBatch->Begin();
	{
		//mFont->DrawString(mSpriteBatch.get(), L"Hello, world!", DirectX::XMFLOAT2(50, 1000));
		mFont->DrawString(mSpriteBatch.get(), output.c_str(), fontPos, DirectX::Colors::LimeGreen);
	}
	mSpriteBatch->End();
}
