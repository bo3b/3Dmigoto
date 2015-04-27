#pragma once

#include <memory>
#include <d3d11.h>

#include "SpriteFont.h"
#include "SpriteBatch.h"

#include "HackerDevice.h"
#include "HackerContext.h"
#include "HackerDXGI.h"
#include "nvapi.h"


// Forward references required because of circular references from the
// other 'Hacker' objects.

class HackerDevice;
class HackerContext;
class HackerDXGISwapChain;

class Overlay
{
private:
	DirectX::XMUINT2 mResolution;
	StereoHandle mStereoHandle;
	std::unique_ptr<DirectX::SpriteFont> mFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

public:
	Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain);
	~Overlay();

	void DrawOverlay(void);
};

