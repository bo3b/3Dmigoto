#pragma once

#include "CommonStates.h"
#include "Effects.h"
#include "PrimitiveBatch.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "util.h"
#include "VertexTypes.h"

#include <d3d11_1.h>
#include <memory>
#include <wrl.h>

// Using forward references for these needed initializer objects. We specifically
// do not want to include HackerDevice.hpp or HackerContext.hpp here because it
// sets up annoying and complicated circular dependencies.  For situations where
// we access objects via pointers, we don't need the #includes, and as a general
// rule it's better to use forward references to avoid accidental includes that
// obfuscate definitions and usage.
class HackerContext;
class HackerDevice;

const enum class Log_Level
{
    dire,
    warning,
    warning_monospace,
    notice,
    info,

    num_levels
};

class OverlayNotice
{
public:
    std::wstring message;
    DWORD        timestamp;

    OverlayNotice(std::wstring message);
};

class Overlay
{
private:
    IDXGISwapChain*      origSwapChain;
    ID3D11Device*        origDevice;
    ID3D11DeviceContext* origContext;
    HackerDevice*        hackerDevice;
    HackerContext*       hackerContext;

    DirectX::XMUINT2                                                       resolution;
    std::unique_ptr<DirectX::SpriteBatch>                                  spriteBatch;
    std::unique_ptr<DirectX::CommonStates>                                 states;
    std::unique_ptr<DirectX::BasicEffect>                                  effect;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> primitiveBatch;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>                              inputLayout;

    // These are all state that we save away before drawing the overlay and
    // restore again afterwards. Basically everything that DirectTK
    // SimpleSprite may clobber:
    struct
    {
        ID3D11BlendState* pBlendState;
        FLOAT             BlendFactor[4];
        UINT              SampleMask;

        om_state om_state;

        D3D11_VIEWPORT pViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT           RSNumViewPorts;

        ID3D11DepthStencilState* pDepthStencilState;
        UINT                     StencilRef;

        ID3D11RasterizerState* pRasterizerState;

        ID3D11SamplerState* samplers[1];

        D3D11_PRIMITIVE_TOPOLOGY topology;

        ID3D11InputLayout* pInputLayout;

        ID3D11VertexShader*  pVertexShader;
        ID3D11ClassInstance* pVSClassInstances[256];
        UINT                 VSNumClassInstances;
        ID3D11Buffer*        pVSConstantBuffers[1];

        ID3D11PixelShader*   pPixelShader;
        ID3D11ClassInstance* pPSClassInstances[256];
        UINT                 PSNumClassInstances;
        ID3D11Buffer*        pPSConstantBuffers[1];

        ID3D11Buffer* pVertexBuffers[1];
        UINT          Strides[1];
        UINT          Offsets[1];

        ID3D11Buffer* IndexBuffer;
        DXGI_FORMAT   Format;
        UINT          Offset;

        ID3D11ShaderResourceView* pShaderResourceViews[1];
    } state;

    void    SaveState();
    void    RestoreState();
    HRESULT InitDrawState();
    void    DrawShaderInfoLine(char* type, UINT64 selected_shader, float* y, bool shader);
    void    DrawShaderInfoLines(float* y);
    void    DrawNotices(float* y);
    void    DrawProfiling(float* y);
    void    DrawRectangle(float x, float y, float w, float h, float r, float g, float b, float opacity);
    void    DrawOutlinedString(DirectX::SpriteFont* font, wchar_t const* text, DirectX::XMFLOAT2 const& position, DirectX::FXMVECTOR color);

public:
    std::unique_ptr<DirectX::SpriteFont> font;
    std::unique_ptr<DirectX::SpriteFont> fontNotifications;
    std::unique_ptr<DirectX::SpriteFont> fontProfiling;

    Overlay(HackerDevice* device, HackerContext* context, IDXGISwapChain* swap_chain);
    ~Overlay();

    void DrawOverlay();
};

void clear_notices();
void log_overlay_w(Log_Level level, wchar_t* fmt, ...);
void log_overlay(Log_Level level, char* fmt, ...);
