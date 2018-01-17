#pragma once

#include <memory>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include "SpriteFont.h"
#include "SpriteBatch.h"


class Overlay
{
private:
	IDXGISwapChain* mOrigSwapChain;
	ID3D11Device* mOrigDevice;
	ID3D11DeviceContext* mOrigContext;

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

		ID3D11RenderTargetView  *pRenderTargetView;
		ID3D11DepthStencilView  *pDepthStencilView;

		D3D11_VIEWPORT pViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT RSNumViewPorts;

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
	HRESULT InitDrawState();
	void DrawShaderInfoLine(char *type, UINT64 selectedShader, int *y, bool shader);
	void DrawShaderInfoLines();

public:
	Overlay(ID3D11Device *pDevice, ID3D11DeviceContext *pContext, IDXGISwapChain *pSwapChain);
	~Overlay();

	void DrawOverlay(void);
	void Resize(UINT Width, UINT Height);
};

