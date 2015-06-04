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
	HackerDXGISwapChain *mHackerSwapChain;
	HackerDevice *mHackerDevice;
	HackerContext *mHackerContext;

	DirectX::XMUINT2 mResolution;
	std::unique_ptr<DirectX::SpriteFont> mFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

	// These are all state that we save away before drawing the overlay and
	// restore again afterwards. Basically everything that DirectTK
	// SimpleSprite may clobber:
	struct {
		ID3D11BlendState *pBlendState;
		FLOAT BlendFactor[4];
		UINT SampleMask;

		ID3D11DepthStencilState *pDepthStencilState;
		UINT StencilRef;

		ID3D11RasterizerState *pRasterizerState;

		ID3D11SamplerState *samplers[1];

		D3D11_PRIMITIVE_TOPOLOGY topology;

		ID3D11InputLayout *pInputLayout;

		ID3D11VertexShader *pVertexShader;
		ID3D11ClassInstance *pVSClassInstances[256];
		UINT VSNumClassInstances;

		ID3D11PixelShader *pPixelShader;
		ID3D11ClassInstance *pPSClassInstances[256];
		UINT PSNumClassInstances;

		ID3D11Buffer *pVertexBuffers[1];
		UINT Strides[1];
		UINT Offsets[1];

		ID3D11Buffer *IndexBuffer;
		DXGI_FORMAT Format;
		UINT Offset;

		ID3D11Buffer *pConstantBuffers[1];

		ID3D11ShaderResourceView *pShaderResourceViews[1];
	} state;

	void SaveState();
	void RestoreState();


public:
	Overlay(HackerDevice *pDevice, HackerContext *pContext, HackerDXGISwapChain *pSwapChain);
	~Overlay();

	void DrawOverlay(void);
};

