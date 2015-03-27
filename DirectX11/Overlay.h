#pragma once

#include <memory>
#include <d3d11.h>

#include "SpriteFont.h"
#include "SpriteBatch.h"

class Overlay
{
private:
	std::unique_ptr<DirectX::SpriteFont> mFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

public:
	Overlay(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	~Overlay();

	void DrawOverlay(void);
};

