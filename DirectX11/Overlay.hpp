#pragma once

#include <memory>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl.h>

#include "SpriteFont.h"
#include "SpriteBatch.h"
#include "PrimitiveBatch.h"
#include "CommonStates.h"
#include "Effects.h"
#include "VertexTypes.h"

#include "HackerDevice.hpp"
#include "HackerContext.hpp"

class HackerSwapChain;

enum LogLevel {
    LOG_DIRE,
    LOG_WARNING,
    LOG_WARNING_MONOSPACE,
    LOG_NOTICE,
    LOG_INFO,

    NUM_LOG_LEVELS
};

class OverlayNotice {
public:
    std::wstring message;
    DWORD timestamp;

    OverlayNotice(std::wstring message);
};

class Overlay
{
private:
    IDXGISwapChain* origSwapChain;
    ID3D11Device* origDevice;
    ID3D11DeviceContext* origContext;
    HackerDevice* hackerDevice;
    HackerContext* hackerContext;

    DirectX::XMUINT2 resolution;
    std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
    std::unique_ptr<DirectX::CommonStates> states;
    std::unique_ptr<DirectX::BasicEffect> effect;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> primitiveBatch;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

    // These are all state that we save away before drawing the overlay and
    // restore again afterwards. Basically everything that DirectTK
    // SimpleSprite may clobber:
    struct {
        ID3D11BlendState *pBlendState;
        FLOAT BlendFactor[4];
        UINT SampleMask;

        OMState om_state;

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
        ID3D11Buffer *pVSConstantBuffers[1];

        ID3D11PixelShader *pPixelShader;
        ID3D11ClassInstance *pPSClassInstances[256];
        UINT PSNumClassInstances;
        ID3D11Buffer *pPSConstantBuffers[1];

        ID3D11Buffer *pVertexBuffers[1];
        UINT Strides[1];
        UINT Offsets[1];

        ID3D11Buffer *IndexBuffer;
        DXGI_FORMAT Format;
        UINT Offset;

        ID3D11ShaderResourceView *pShaderResourceViews[1];
    } state;

    void SaveState();
    void RestoreState();
    HRESULT InitDrawState();
    void DrawShaderInfoLine(char *type, UINT64 selectedShader, float *y, bool shader);
    void DrawShaderInfoLines(float *y);
    void DrawNotices(float *y);
    void DrawProfiling(float *y);
    void DrawRectangle(float x, float y, float w, float h, float r, float g, float b, float opacity);
    void DrawOutlinedString(DirectX::SpriteFont *font, wchar_t const *text, DirectX::XMFLOAT2 const &position, DirectX::FXMVECTOR color);

public:
    std::unique_ptr<DirectX::SpriteFont> font;
    std::unique_ptr<DirectX::SpriteFont> fontNotifications;
    std::unique_ptr<DirectX::SpriteFont> fontProfiling;

    Overlay(HackerDevice *pDevice, HackerContext *pContext, IDXGISwapChain *pSwapChain);
    ~Overlay();

    void DrawOverlay(void);
};

void ClearNotices();
void LogOverlayW(LogLevel level, wchar_t *fmt, ...);
void LogOverlay(LogLevel level, char *fmt, ...);
