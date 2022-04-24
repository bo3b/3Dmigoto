// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.hpp"

#include "D3D11Wrapper.h"
#include "Globals.h"
#include "HackerContext.hpp"
#include "HackerDevice.hpp"
#include "Hunting.hpp"
#include "Lock.h"
#include "log.h"
#include "Profiling.hpp"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "version.h"

#include <DirectXColors.h>
#include <stdexcept>

using namespace std;

#define MAX_SIMULTANEOUS_NOTICES 10

static bool     has_notice           = false;
static unsigned notice_cleared_frame = 0;

static class Notices
{
public:
    vector<OverlayNotice> notices[static_cast<size_t>(Log_Level::num_levels)];
    CRITICAL_SECTION      lock {};

    Notices()
    {
        INIT_CRITICAL_SECTION(&lock);
    }

    ~Notices()
    {
        DeleteCriticalSection(&lock);
    }
} notices;

struct log_level_params
{
    DirectX::XMVECTORF32            colour;
    DWORD                           duration;
    bool                            hide_in_release;
    unique_ptr<DirectX::SpriteFont> Overlay::*font;
};

struct log_level_params log_levels[] = {
    { DirectX::Colors::Red, 20000, false, &Overlay::fontNotifications },        // DIRE
    { DirectX::Colors::OrangeRed, 10000, false, &Overlay::fontNotifications },  // WARNING
    { DirectX::Colors::OrangeRed, 10000, false, &Overlay::fontProfiling },      // WARNING_MONOSPACE
    { DirectX::Colors::Orange, 5000, true, &Overlay::fontNotifications },       // NOTICE
    { DirectX::Colors::LimeGreen, 2000, true, &Overlay::fontNotifications },    // INFO
};

// Side note: Not really stoked with C++ string handling.  There are like 4 or
// 5 different ways to do things, all partly compatible, none a clear winner in
// terms of simplicity and clarity.  Generally speaking we'd want to use C++
// wstring and string, but there are no good output formatters.  Maybe the
// newer iostream based pieces, but we'd still need to convert.
//
// The philosophy here and in other files, is to use whatever the API that we
// are using wants.  In this case it's a wchar_t* for DrawString, so we'll not
// do a lot of conversions and different formats, we'll just use wchar_t and its
// formatters.
//
// In particular, we also want to avoid 5 different libraries for string handling,
// Microsoft has way too many variants.  We'll use the regular C library from
// the standard c runtime, but use the _s safe versions.

// Max expected on-screen string size, used for buffer safe calls.
constexpr int maxstring = 1024;

Overlay::Overlay(
    HackerDevice*   device,
    HackerContext*  context,
    IDXGISwapChain* swap_chain) :
    resolution(),
    state()
{
    LOG_INFO("Overlay::Overlay created for %p\n", swap_chain);
    LOG_INFO("  on HackerDevice: %p, HackerContext: %p\n", device, context);

    // Drawing environment for this swap chain. This is the game environment.
    // These should specifically avoid Hacker* objects, to avoid object
    // callbacks or other problems. We just want to draw here, nothing tricky.
    // The only exception being that we need the HackerDevice in order to
    // draw the current stereoparams.
    hackerDevice  = device;
    hackerContext = context;
    origSwapChain = swap_chain;

    // Must use trampoline context to prevent 3DMigoto hunting its own overlay:
    origDevice  = hackerDevice->GetPassThroughOrigDevice1();
    origContext = context->GetPassThroughOrigContext1();

    // We are actively using the Device and Context, so we need to make
    // sure they do not get Released without us.  This happened in FFXIV.
    //
    // We do not hold a reference on the swap chain, as that would prevent
    // the swap chain from being released until the overlay is deleted, but
    // the overlay will not be deleted until the swap chain is released,
    // and since the overlay also holds references to the device and
    // context this would prevent everything from being released. So long
    // as the overlay exists we know the swap chain hasn't been released
    // yet, so this is safe.
    //
    // Alternatively, we could forgo holding pointers to these at all,
    // since we can always get access to the device and immediate context
    // via SwapChain->GetParent(ID3D11Device) and GetImmediateContext(),
    // but that would mean extra calls in a fast path.
    //
    // Note that the swap chain itself also holds references to these two
    // now, so this is technically unecessary, but since the overlay code
    // still accesses these it is more safer to leave this in place (more
    // resistant to code changes in the swap chain breaking this).
    hackerDevice->AddRef();
    hackerContext->AddRef();

    // The courierbold.spritefont is now included as binary resource data attached
    // to the d3d11.dll.  We can fetch that resource and pass it to new SpriteFont
    // to avoid having to drag around the font file. We get the module
    // handle by address to ensure we don't get some other d3d11.dll, which
    // is of particular importance when we are injected into a Windows
    // Store app and may not even be called that ourselves.
    HMODULE handle = nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(log_overlay), &handle);
    HRSRC          rc        = FindResource(handle, MAKEINTRESOURCE(IDR_COURIERBOLD), MAKEINTRESOURCE(SPRITEFONT));
    HGLOBAL        rc_data   = LoadResource(handle, rc);
    DWORD          font_size = SizeofResource(handle, rc);
    uint8_t const* font_blob = static_cast<const uint8_t*>(LockResource(rc_data));

    // We want to use the original device and original context here, because
    // these will be used by DirectXTK to generate VertexShaders and PixelShaders
    // to draw the text, and we don't want to intercept those.
    font.reset(new DirectX::SpriteFont(origDevice, font_blob, font_size));
    font->SetDefaultCharacter(L'?');

    // Courier is a nice choice for hunting status lines, and showing the
    // shader hashes since it is monospace, but for arbitrary notifications
    // we want something a little smaller, and variable width. Liberation
    // Sans has essentially the same metrics as Arial,
    // but is not encumbered.
    rc        = FindResource(handle, MAKEINTRESOURCE(IDR_ARIAL), MAKEINTRESOURCE(SPRITEFONT));
    rc_data   = LoadResource(handle, rc);
    font_size = SizeofResource(handle, rc);
    font_blob = static_cast<const uint8_t*>(LockResource(rc_data));
    fontNotifications.reset(new DirectX::SpriteFont(origDevice, font_blob, font_size));
    fontNotifications->SetDefaultCharacter(L'?');

    // Smaller monospaced font for profiling text
    rc        = FindResource(handle, MAKEINTRESOURCE(IDR_COURIERSMALL), MAKEINTRESOURCE(SPRITEFONT));
    rc_data   = LoadResource(handle, rc);
    font_size = SizeofResource(handle, rc);
    font_blob = static_cast<const uint8_t*>(LockResource(rc_data));
    fontProfiling.reset(new DirectX::SpriteFont(origDevice, font_blob, font_size));
    fontProfiling->SetDefaultCharacter(L'?');

    spriteBatch.reset(new DirectX::SpriteBatch(origContext));

    // For dark background behind notification text, following
    // https://github.com/Microsoft/DirectXTK/wiki/Simple-rendering
    states.reset(new DirectX::CommonStates(origDevice));
    effect.reset(new DirectX::BasicEffect(origDevice));

    void const* shader_byte_code;
    size_t      byte_code_length;

    effect->SetVertexColorEnabled(true);
    effect->GetVertexShaderBytecode(&shader_byte_code, &byte_code_length);

    HRESULT hr = origDevice->CreateInputLayout(DirectX::VertexPositionColor::InputElements, DirectX::VertexPositionColor::InputElementCount, shader_byte_code, byte_code_length, inputLayout.ReleaseAndGetAddressOf());
    if (FAILED(hr))
        throw std::runtime_error("CreateInputLayout failed");

    primitiveBatch.reset(new DirectX::PrimitiveBatch<DirectX::VertexPositionColor>(origContext));
}

Overlay::~Overlay()
{
    LOG_INFO("Overlay::~Overlay deleted for SwapChain %p\n", origSwapChain);
    // We Release the same interface we called AddRef on, and we use the
    // Hacker interfaces to make sure that our cleanup code is run if this
    // is the last reference.
    hackerContext->Release();
    hackerDevice->Release();
}

// -----------------------------------------------------------------------------

using namespace DirectX::SimpleMath;

// Expected to be called at DXGI::Present() to be the last thing drawn.

// Notes:
//  1) Active PS location(probably x / N format)
//  2) Active VS location(x / N format)
//  3) Current convergence and separation. (convergence, a must)
//  4) Error state of reload(syntax errors go red or something)
//  5) Duplicate Mark(maybe yellow text for location, red if Decompile failed)

//Maybe:
//  5) Other state, like show_original active.
//  6) Active toggle override.

// We need to save off everything that DirectTK will clobber and
// restore it before returning to the application. This is necessary
// to prevent rendering issues in some games like The Long Dark, and
// helps avoid introducing pipeline errors in other games like The
// Witcher 3.
// Only saving and restoring the first RenderTarget, as the only one
// we change for drawing overlay.

void Overlay::SaveState()
{
    memset(&state, 0, sizeof(state));

    save_om_state(origContext, &state.om_state);
    state.RSNumViewPorts = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    origContext->RSGetViewports(&state.RSNumViewPorts, state.pViewPorts);

    origContext->OMGetBlendState(&state.pBlendState, state.BlendFactor, &state.SampleMask);
    origContext->OMGetDepthStencilState(&state.pDepthStencilState, &state.StencilRef);
    origContext->RSGetState(&state.pRasterizerState);
    origContext->PSGetSamplers(0, 1, state.samplers);
    origContext->IAGetPrimitiveTopology(&state.topology);
    origContext->IAGetInputLayout(&state.pInputLayout);
    origContext->VSGetShader(&state.pVertexShader, state.pVSClassInstances, &state.VSNumClassInstances);
    origContext->PSGetShader(&state.pPixelShader, state.pPSClassInstances, &state.PSNumClassInstances);
    origContext->IAGetVertexBuffers(0, 1, state.pVertexBuffers, state.Strides, state.Offsets);
    origContext->IAGetIndexBuffer(&state.IndexBuffer, &state.Format, &state.Offset);
    origContext->VSGetConstantBuffers(0, 1, state.pVSConstantBuffers);
    origContext->PSGetConstantBuffers(0, 1, state.pPSConstantBuffers);
    origContext->PSGetShaderResources(0, 1, state.pShaderResourceViews);
}

void Overlay::RestoreState()
{
    unsigned i;

    restore_om_state(origContext, &state.om_state);

    origContext->RSSetViewports(state.RSNumViewPorts, state.pViewPorts);

    origContext->OMSetBlendState(state.pBlendState, state.BlendFactor, state.SampleMask);
    if (state.pBlendState)
        state.pBlendState->Release();

    origContext->OMSetDepthStencilState(state.pDepthStencilState, state.StencilRef);
    if (state.pDepthStencilState)
        state.pDepthStencilState->Release();

    origContext->RSSetState(state.pRasterizerState);
    if (state.pRasterizerState)
        state.pRasterizerState->Release();

    origContext->PSSetSamplers(0, 1, state.samplers);
    if (state.samplers[0])
        state.samplers[0]->Release();

    origContext->IASetPrimitiveTopology(state.topology);

    origContext->IASetInputLayout(state.pInputLayout);
    if (state.pInputLayout)
        state.pInputLayout->Release();

    origContext->VSSetShader(state.pVertexShader, state.pVSClassInstances, state.VSNumClassInstances);
    if (state.pVertexShader)
        state.pVertexShader->Release();
    for (i = 0; i < state.VSNumClassInstances; i++)
        state.pVSClassInstances[i]->Release();

    origContext->PSSetShader(state.pPixelShader, state.pPSClassInstances, state.PSNumClassInstances);
    if (state.pPixelShader)
        state.pPixelShader->Release();
    for (i = 0; i < state.PSNumClassInstances; i++)
        state.pPSClassInstances[i]->Release();

    origContext->IASetVertexBuffers(0, 1, state.pVertexBuffers, state.Strides, state.Offsets);
    if (state.pVertexBuffers[0])
        state.pVertexBuffers[0]->Release();

    origContext->IASetIndexBuffer(state.IndexBuffer, state.Format, state.Offset);
    if (state.IndexBuffer)
        state.IndexBuffer->Release();

    origContext->VSSetConstantBuffers(0, 1, state.pVSConstantBuffers);
    if (state.pVSConstantBuffers[0])
        state.pVSConstantBuffers[0]->Release();

    origContext->PSSetConstantBuffers(0, 1, state.pPSConstantBuffers);
    if (state.pPSConstantBuffers[0])
        state.pPSConstantBuffers[0]->Release();

    origContext->PSSetShaderResources(0, 1, state.pShaderResourceViews);
    if (state.pShaderResourceViews[0])
        state.pShaderResourceViews[0]->Release();
}

#ifdef NTDDI_WIN10
    #include <d3d11on12.h>
    #include <dxgi1_4.h>

static ID3D11Texture2D* get_11on12_backbuffer(
    ID3D11Device*   orig_device,
    IDXGISwapChain* orig_swap_chain)
{
    ID3D12Resource*      d3d12_bb      = nullptr;
    ID3D11Texture2D*     d3d11_bb      = nullptr;
    ID3D11On12Device*    d3d11on12_dev = nullptr;
    IDXGISwapChain3*     swap_chain_3  = nullptr;
    D3D11_RESOURCE_FLAGS flags         = { D3D11_BIND_RENDER_TARGET, 0, 0, 0 };
    UINT                 bb_idx;
    HRESULT              hr;

    if (FAILED(orig_device->QueryInterface(IID_ID3D11On12Device, reinterpret_cast<void**>(&d3d11on12_dev))))
        return nullptr;
    LOG_DEBUG("  ID3D11On12Device: %p\n", d3d11on12_dev);

    // In D3D12 we need to make sure we are writing to the correct back
    // buffer for the current frame, and failing to do this will lead to a
    // DXGI_ERROR_ACCESS_DENIED. This differs from DX11 where index 0
    // always points to the current back buffer. We need the SwapChain3
    // interface to determine which back buffer is currently active:
    if (FAILED(orig_swap_chain->QueryInterface(IID_IDXGISwapChain3, reinterpret_cast<void**>(&swap_chain_3))))
        goto out;
    bb_idx = swap_chain_3->GetCurrentBackBufferIndex();
    LOG_DEBUG("  Current Back Buffer Index: %i\n", bb_idx);

    if (FAILED(orig_swap_chain->GetBuffer(bb_idx, IID_ID3D12Resource, reinterpret_cast<void**>(&d3d12_bb))))
        goto out;
    LOG_DEBUG("  ID3D12Resource: %p\n", d3d12_bb);

    // At the moment I'm creating a wrapped resource every frame, though
    // the 11on12 sample code does this once for every back buffer index
    // and uses AcquireWrappedResources() instead each frame. The sample
    // code also doesn't cope with resizing swap chains on alt+enter, so
    // I'm not at all confident that following it to the letter would work.
    // We know holding a reference to a back buffer will prevent alt+enter
    // from working - maybe Acquire gets around that, but I don't trust it.
    //
    // I'm specifying RT -> PRESENT state transitions here, which follows
    // the 11to12 sample code, but I don't think this is strictly correct -
    // surely the game should have already transitioned the back buffer to
    // the PRESENT state before calling Present(), in which case shouldn't
    // our in state be PRESENT, not RT? However, while both cases seem to
    // work just fine, in Fifa 2018 the debug layer seems to indicate both
    // are wrong - RT->PRESENT says the barrier doesn't match the current
    // resource state of RT, and PRESENT->PRESENT says it doesn't match the
    // previous barrier transitioning it to RT.
    //
    // ... I don't even ...
    //
    // ... What? ...
    //
    // ... That doesn't make sense!!! Maybe they have a game bug and left
    // it as RT by mistake, but that still doesn't make sense!!! I'm either
    // misinterpreting one of the debug messages or I'm missing something.
    //
    // AFAICT there's no way to query the current "state" to answer this
    // correctly, because despite the name there is actually no "state"
    // anywhere outside of the debug layer. D3D11 tracked this stuff and
    // automatically submitted the correct memory barrier type to the GPU,
    // but DX12 saves some CPU cycles and doesn't track anything, leaving
    // it up to the application to know which barrier to use, and the
    // "state transition" is really just a way to assist an inexperienced
    // developer in choosing the right one (that's actually kind of clever
    // - memory barriers are really easy to mess up and not realise) - so,
    // this isn't a state transition at all, it's a memory barrier.
    //
    // Okay, so it's a memory barrier - knowing that, what would be the
    // safest thing to do? Well, if the game already transitioned to
    // PRESENT, then they already did a memory barrier of their own, and so
    // long as we do another after we finish writing to the back buffer it
    // should be fine, and shouldn't matter if we start writing to it now
    // without another barrier to transition it back to RT. If the game
    // didn't transition it to PRESENT, and the last barrier they did left
    // it as RT, then (they have a bug) it won't matter if we start writing
    // to it immediately as is, but we should still do a memory barrier
    // once we are finished. Ok, either way I don't think it actually
    // matters unless they have done something very weird and we need a
    // different barrier altogether, or I haven't considered some subtlty -
    // let's go with RT->PRESENT for now and see how it goes in practice:
    hr = d3d11on12_dev->CreateWrappedResource(d3d12_bb, &flags, D3D12_RESOURCE_STATE_RENDER_TARGET, /* in "state" */ D3D12_RESOURCE_STATE_PRESENT, /* out "state" */ IID_ID3D11Texture2D, reinterpret_cast<void**>(&d3d11_bb));
    LOG_DEBUG("  ID3D11Texture2D: %p, result: 0x%x\n", d3d11_bb, hr);

out:
    if (d3d12_bb)
        d3d12_bb->Release();
    if (swap_chain_3)
        swap_chain_3->Release();
    if (d3d11on12_dev)
        d3d11on12_dev->Release();

    return d3d11_bb;
}

static void flush_d3d11on12(
    ID3D11Device*        orig_device,
    ID3D11DeviceContext* orig_context)
{
    ID3D11On12Device* d3d11on12_dev = nullptr;

    if (FAILED(orig_device->QueryInterface(IID_ID3D11On12Device, reinterpret_cast<void**>(&d3d11on12_dev))))
        return;

    // We need to tell 11on12 to release the resources it is wrapping,
    // otherwise it will still hold a reference to the back buffer which
    // will prevent alt+enter from working. This should also transition the
    // D3D12 resource state from RENDER_TARGET to PRESENT. We haven't
    // bothered keeping our own reference to the back buffer, so just have
    // it release all of them - that should be fine unless the game or
    // another overlay is also using 11on12 and for some reason did not
    // release a resource and isn't expecting to have to re-acquire it:
    d3d11on12_dev->ReleaseWrappedResources(nullptr, 0);
    d3d11on12_dev->Release();

    // D3D11 Immediate context must be flushed before any further D3D12
    // work can be performed. This should be done as late as possible to
    // ensure no commands are queued up, and in particular needs to be done
    // after we have released any references to the back buffer, including
    // unbinding it from the pipeline, and after ReleaseWrappedResources(),
    // since all the releases can be delayed:
    orig_context->Flush();
}

#else

static ID3D11Texture2D* get_11on12_backbuffer(ID3D11Device* origDevice, IDXGISwapChain* origSwapChain)
{
    return NULL;
}

static void flush_d3d11on12(ID3D11Device* origDevice, ID3D11DeviceContext* origContext)
{
}

#endif

// We can't trust the game to have a proper drawing environment for DirectXTK.
//
// For two games we know of (Batman Arkham Knight and Project Cars) we were not
// getting an overlay, because apparently the rendertarget was left in an odd
// state.  This adds an init to be certain that the rendertarget is the backbuffer
// so that the overlay is drawn.

HRESULT Overlay::InitDrawState()
{
    HRESULT hr;

    ID3D11Texture2D* back_buffer;
    hr = origSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&back_buffer));
    if (FAILED(hr))
    {
        // The back buffer doesn't support the D3D11 Texture2D
        // interface. Maybe this is DX12 - if we have been built with
        // the Win 10 SDK we can handle that via 11On12:
        back_buffer = get_11on12_backbuffer(origDevice, origSwapChain);
        if (!back_buffer)
            return hr;
    }

    // By doing this every frame, we are always up to date with correct size,
    // and we need the address of the BackBuffer anyway, so this is low cost.
    D3D11_TEXTURE2D_DESC description;
    back_buffer->GetDesc(&description);
    resolution = DirectX::XMUINT2(description.Width, description.Height);

    // use the back buffer address to create the render target
    ID3D11RenderTargetView* backbuffer;
    hr = origDevice->CreateRenderTargetView(back_buffer, nullptr, &backbuffer);

    back_buffer->Release();

    if (FAILED(hr))
        return hr;

    // set the first render target as the back buffer, with no stencil
    origContext->OMSetRenderTargets(1, &backbuffer, nullptr);

    // Holding onto a view of the back buffer can cause a crash on
    // ResizeBuffers, so it is very important we release it here - it will
    // still have a reference so long as it is bound to the pipeline -
    // i.e. until RestoreState() unbinds it. Holding onto this view caused
    // a crash in Mass Effect Andromeda when toggling full screen if the
    // hunting overlay had ever been displayed since launch.
    backbuffer->Release();

    // Make sure there is at least one open viewport for DirectXTK to use.
    D3D11_VIEWPORT open_view = CD3D11_VIEWPORT(0.0, 0.0, resolution.x, resolution.y);
    origContext->RSSetViewports(1, &open_view);

    return S_OK;
}

void Overlay::DrawRectangle(
    float x,
    float y,
    float w,
    float h,
    float r,
    float g,
    float b,
    float opacity)
{
    DirectX::XMVECTORF32 colour = { r, g, b, opacity };

    origContext->OMSetBlendState(states->AlphaBlend(), nullptr, 0xFFFFFFFF);
    origContext->OMSetDepthStencilState(states->DepthNone(), 0);
    origContext->RSSetState(states->CullNone());

    // Use pixel coordinates to match SpriteBatch:
    Matrix proj = Matrix::CreateScale(2.0f / resolution.x, -2.0f / resolution.y, 1) * Matrix::CreateTranslation(-1, 1, 0);
    effect->SetProjection(proj);

    // This call will change VS + PS + constant buffer state:
    effect->Apply(origContext);

    origContext->IASetInputLayout(inputLayout.Get());

    primitiveBatch->Begin();

    // DirectXTK is using 0,1,2 0,2,3, so layout the vectors clockwise:
    DirectX::VertexPositionColor v1(Vector3(x, y, 0), colour);
    DirectX::VertexPositionColor v2(Vector3(x + w, y, 0), colour);
    DirectX::VertexPositionColor v3(Vector3(x + w, y + h, 0), colour);
    DirectX::VertexPositionColor v4(Vector3(x, y + h, 0), colour);

    primitiveBatch->DrawQuad(v1, v2, v3, v4);

    primitiveBatch->End();
}

void Overlay::DrawOutlinedString(
    DirectX::SpriteFont*     use_font,
    wchar_t const*           text,
    DirectX::XMFLOAT2 const& position,
    DirectX::FXMVECTOR       color)
{
    use_font->DrawString(spriteBatch.get(), text, position + Vector2(-1, 0), DirectX::Colors::Black);
    use_font->DrawString(spriteBatch.get(), text, position + Vector2(1, 0), DirectX::Colors::Black);
    use_font->DrawString(spriteBatch.get(), text, position + Vector2(0, -1), DirectX::Colors::Black);
    use_font->DrawString(spriteBatch.get(), text, position + Vector2(0, 1), DirectX::Colors::Black);
    use_font->DrawString(spriteBatch.get(), text, position, color);
}

// -----------------------------------------------------------------------------

// The active shader will show where we are in each list. / 0 / 0 will mean that we are not
// actively searching.

static void append_shader_text(
    wchar_t* full_line,
    wchar_t* type,
    int      pos,
    size_t   size)
{
    if (size == 0)
        return;

    // The position is zero based, so we'll make it +1 for the humans.
    if (++pos == 0)
        size = 0;

    // Format: "VS:1/15"
    wchar_t append[maxstring];
    swprintf_s(append, maxstring, L"%ls:%d/%Iu ", type, pos, size);

    wcscat_s(full_line, maxstring, append);
}

// We also want to show the count of active vertex, pixel, compute, geometry, domain, hull
// shaders, that are active in the frame.  Any that have a zero count will be stripped, to
// keep it from being too busy looking.

static void create_shader_count_string(
    wchar_t* counts)
{
    const wchar_t* marking_mode;

    wcscpy_s(counts, maxstring, L"");
    // The order here more or less follows how important these are for
    // shaderhacking. VS and PS are the absolute most important, CS is
    // pretty important, GS and DS show up from time to time and HS is not
    // important at all since we have never needed to fix one.
    append_shader_text(counts, L"VS", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
    append_shader_text(counts, L"PS", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
    append_shader_text(counts, L"CS", G->mSelectedComputeShaderPos, G->mVisitedComputeShaders.size());
    append_shader_text(counts, L"GS", G->mSelectedGeometryShaderPos, G->mVisitedGeometryShaders.size());
    append_shader_text(counts, L"DS", G->mSelectedDomainShaderPos, G->mVisitedDomainShaders.size());
    append_shader_text(counts, L"HS", G->mSelectedHullShaderPos, G->mVisitedHullShaders.size());

    // TODO: Are these correct? mSelected* are unit_32 so never -1.
    if (G->mSelectedVertexBuffer != -1)
        append_shader_text(counts, L"VB", G->mSelectedVertexBufferPos, G->mVisitedVertexBuffers.size());
    if (G->mSelectedIndexBuffer != -1)
        append_shader_text(counts, L"IB", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
    if (G->mSelectedRenderTarget != reinterpret_cast<ID3D11Resource*>(-1))
        append_shader_text(counts, L"RT", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());

    marking_mode = lookup_enum_name(MarkingModeNames, G->marking_mode);
    if (marking_mode)
        wcscat_s(counts, maxstring, marking_mode);
}

// Need to convert from the current selection, mSelectedVertexShader as hash, and
// find the OriginalShaderInfo that matches.  This is a linear search instead of a
// hash lookup, because we don't have the ID3D11DeviceChild*.

static bool find_info_text(
    wchar_t* info,
    UINT64   selected_shader)
{
    for (auto& loaded : G->mReloadedShaders)
    //for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> loaded in G->mReloadedShaders)
    {
        if ((loaded.second.hash == selected_shader) && !loaded.second.infoText.empty())
        {
            // We now use wcsncat_s instead of wcscat_s here,
            // because the later will terminate us if the resulting
            // string would overflow the destination buffer (or
            // fail with EINVAL if we change the parameter
            // validation so it doesn't terminate us). wcsncat_s
            // has a _TRUNCATE option that tells it to fill up as
            // much of the buffer as possible without overflowing
            // and will still NULL terminate the resulting string,
            // which will work fine for this case since that will
            // be more than we can fit on the screen anyway.
            // wcsncat would also work, but its count field is
            // silly (maxstring-strlen(info)-1) and VS complains.
            //
            // Skip past first two characters, which will always be //
            wcsncat_s(info, maxstring, loaded.second.infoText.c_str() + 2, _TRUNCATE);
            return true;
        }
    }
    return false;
}

// This is for a line of text as info about the currently selected shader.
// The line is pulled out of the header of the HLSL text file, and can be
// anything. Since there can be multiple shaders selected, VS and PS and HS for
// example, we'll show one line for each, but only those that are present
// in ShaderFixes and have something other than a blank line at the top.

void Overlay::DrawShaderInfoLine(
    char*  type,
    UINT64 selected_shader,
    float* y,
    bool   shader)
{
    wchar_t osd_string[maxstring];
    Vector2 str_size;
    Vector2 text_position;
    float   x = 0;

    if (shader)
    {
        if (selected_shader == -1)
            return;

        if (G->verbose_overlay)
            swprintf_s(osd_string, maxstring, L"%S %016llx:", type, selected_shader);
        else
            swprintf_s(osd_string, maxstring, L"%S:", type);

        if (!find_info_text(osd_string, selected_shader) && !G->verbose_overlay)
            return;
    }
    else
    {
        if (selected_shader == 0xffffffff || !G->verbose_overlay)
            return;

        swprintf_s(osd_string, maxstring, L"%S %08llx", type, selected_shader);
    }

    str_size = font->MeasureString(osd_string);

    if (!G->verbose_overlay)
        x = max((resolution.x - str_size.x) / 2.0f, 0.0f);

    text_position = Vector2(x, *y);
    *y += str_size.y;
    DrawOutlinedString(font.get(), osd_string, text_position, DirectX::Colors::LimeGreen);
}

void Overlay::DrawShaderInfoLines(
    float* y)
{
    // Order is the same as the pipeline... Not quite the same as the count
    // summary line, which is sorted by "the order in which we added them"
    // (which to be fair, is pretty much their order of importance for our
    // purposes). Since these only show up while hunting, it is better to
    // have them reflect the actual order that they are run in. The summary
    // line can stay in order of importance since it is always shown.
    DrawShaderInfoLine("VB", G->mSelectedVertexBuffer, y, false);
    DrawShaderInfoLine("IB", G->mSelectedIndexBuffer, y, false);
    DrawShaderInfoLine("VS", G->mSelectedVertexShader, y, true);
    DrawShaderInfoLine("HS", G->mSelectedHullShader, y, true);
    DrawShaderInfoLine("DS", G->mSelectedDomainShader, y, true);
    DrawShaderInfoLine("GS", G->mSelectedGeometryShader, y, true);
    DrawShaderInfoLine("PS", G->mSelectedPixelShader, y, true);
    DrawShaderInfoLine("CS", G->mSelectedComputeShader, y, true);
    // FIXME? This one is stored as a handle, not a hash:
    if (G->mSelectedRenderTarget != reinterpret_cast<ID3D11Resource*>(-1))
        DrawShaderInfoLine("RT", GetOrigResourceHash(G->mSelectedRenderTarget), y, false);
}

void Overlay::DrawNotices(
    float* y)
{
    vector<OverlayNotice>::iterator notice;
    DWORD                           time = GetTickCount();
    Vector2                         str_size;
    int                             level, displayed = 0;

    ENTER_CRITICAL_SECTION(&notices.lock);
    {
        has_notice = false;
        for (level = 0; level < static_cast<int>(Log_Level::num_levels); level++)
        {
            if (log_levels[level].hide_in_release && G->hunting == Hunting_Mode::disabled)
                continue;

            for (notice = notices.notices[level].begin(); notice != notices.notices[level].end() && displayed < MAX_SIMULTANEOUS_NOTICES;)
            {
                if (!notice->timestamp)
                {
                    // Set the timestamp on the first present call
                    // that we display the message after it was
                    // issued. Means messages won't be missed if
                    // they exceed the maximum we can display
                    // simultaneously, and helps combat messages
                    // disappearing too quickly if there have been
                    // no present calls for a while after they were
                    // issued.
                    notice->timestamp = time;
                }
                else if ((time - notice->timestamp) > log_levels[level].duration)
                {
                    notice = notices.notices[level].erase(notice);
                    continue;
                }

                str_size = (this->*log_levels[level].font)->MeasureString(notice->message.c_str());

                DrawRectangle(0, *y, str_size.x + 3, str_size.y, 0, 0, 0, 0.75);

                (this->*log_levels[level].font)->DrawString(spriteBatch.get(), notice->message.c_str(), Vector2(0, *y), log_levels[level].colour);
                *y += str_size.y + 5;

                has_notice = true;
                notice++;
                displayed++;
            }
        }
    }
    LEAVE_CRITICAL_SECTION(&notices.lock);
}

void Overlay::DrawProfiling(
    float* y)
{
    Vector2 str_size;

    Profiling::update_txt();

    str_size = fontProfiling->MeasureString(Profiling::text.c_str());
    DrawRectangle(0, *y, str_size.x + 3, str_size.y, 0, 0, 0, 0.75);

    fontProfiling->DrawString(spriteBatch.get(), Profiling::text.c_str(), Vector2(0, *y), DirectX::Colors::Goldenrod);
}

// Create a string for display on the bottom edge of the screen, that contains the current
// stereo info of separation and convergence.
// Desired format: "Sep:85  Conv:4.5"

static void create_stereo_info_string(
    StereoHandle stereo_handle,
    wchar_t*     info)
{
    // Rather than draw graphic bars, this will just be numeric.  Because
    // convergence is essentially an arbitrary number.

    float separation, convergence;
    NvU8  stereo = !!stereo_handle;
    if (stereo)
    {
        nvapi_override();
        Profiling::NvAPI_Stereo_IsEnabled(&stereo);
        if (stereo)
        {
            Profiling::NvAPI_Stereo_IsActivated(stereo_handle, &stereo);
            if (stereo)
            {
                Profiling::NvAPI_Stereo_GetSeparation(stereo_handle, &separation);
                Profiling::NvAPI_Stereo_GetConvergence(stereo_handle, &convergence);
            }
        }
    }

    if (stereo)
        swprintf_s(info, maxstring, L"Sep:%.0f  Conv:%.2f", separation, convergence);
    else
        swprintf_s(info, maxstring, L"Stereo disabled");
}

void Overlay::DrawOverlay()
{
    Profiling::State profiling_state {};
    HRESULT          hr;

    if (G->hunting != Hunting_Mode::enabled && !has_notice && Profiling::mode == Profiling::Mode::NONE)
        return;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    // Since some games did not like having us change their drawing state from
    // SpriteBatch, we now save and restore all state information for the GPU
    // around our drawing.
    SaveState();
    {
        hr = InitDrawState();
        if (FAILED(hr))
            goto fail_restore;

        spriteBatch->Begin();
        {
            wchar_t osd_string[maxstring];
            Vector2 str_size;
            Vector2 text_position;
            float   y = 10.0f;

            if (G->hunting == Hunting_Mode::enabled)
            {
                // Top of screen
                create_shader_count_string(osd_string);
                str_size      = font->MeasureString(osd_string);
                text_position = Vector2((resolution.x - str_size.x) / 2.0f, y);
                DrawOutlinedString(font.get(), osd_string, text_position, DirectX::Colors::LimeGreen);
                y += str_size.y;

                DrawShaderInfoLines(&y);

                // Bottom of screen
                create_stereo_info_string(hackerDevice->stereoHandle, osd_string);
                str_size      = font->MeasureString(osd_string);
                text_position = Vector2((resolution.x - str_size.x) / 2.0f, resolution.y - str_size.y - 10.0f);
                DrawOutlinedString(font.get(), osd_string, text_position, DirectX::Colors::LimeGreen);
            }

            if (has_notice)
                DrawNotices(&y);

            if (Profiling::mode != Profiling::Mode::NONE)
                DrawProfiling(&y);
        }
        spriteBatch->End();
    }
fail_restore:
    RestoreState();

    flush_d3d11on12(origDevice, origContext);

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::overlay_overhead);
}

OverlayNotice::OverlayNotice(
    wstring message) :
    message(message),
    timestamp(0)
{}

void clear_notices()
{
    int level;

    if (notice_cleared_frame == G->frame_no)
        return;

    ENTER_CRITICAL_SECTION(&notices.lock);
    {
        for (level = 0; level < static_cast<int>(Log_Level::num_levels); level++)
            notices.notices[level].clear();

        notice_cleared_frame = G->frame_no;
        has_notice           = false;
    }
    LEAVE_CRITICAL_SECTION(&notices.lock);
}

void log_overlay_w(
    Log_Level level,
    wchar_t*  fmt,
    ...)
{
    wchar_t msg[maxstring];
    va_list ap;

    va_start(ap, fmt);
    LOG_INFO_W_V(fmt, ap);

    // Using _vsnwprintf_s so we don't crash if the message is too long for
    // the buffer, and truncate it instead - unless we can automatically
    // wrap the message, which DirectXTK doesn't appear to support, who
    // cares if it gets cut off somewhere off screen anyway?
    _vsnwprintf_s(msg, maxstring, _TRUNCATE, fmt, ap);

    ENTER_CRITICAL_SECTION(&notices.lock);
    {
        size_t i = static_cast<size_t>(level);
        notices.notices[i].emplace_back(msg);
        has_notice = true;
    }
    LEAVE_CRITICAL_SECTION(&notices.lock);

    va_end(ap);
}

// ASCII version of the above. DirectXTK only understands wide strings, so we
// need to convert it to that, but we can't just convert the format and hand it
// to log_overlay_w, because that would reverse the meaning of %s and %S in the
// format string. Instead we do our own LOG_INFO_V and _vsnprintf_s to handle the
// format string correctly and convert the result to a wide string.
void log_overlay(
    Log_Level level,
    char*     fmt,
    ...)
{
    char    amsg[maxstring];
    wchar_t wmsg[maxstring];
    va_list ap;
    size_t  i = static_cast<size_t>(level);

    va_start(ap, fmt);
    LOG_INFO_V(fmt, ap);

    if (!log_levels[i].hide_in_release || hunting_enabled())
    {
        // Using _vsnprintf_s so we don't crash if the message is too long for
        // the buffer, and truncate it instead - unless we can automatically
        // wrap the message, which DirectXTK doesn't appear to support, who
        // cares if it gets cut off somewhere off screen anyway?
        _vsnprintf_s(amsg, maxstring, _TRUNCATE, fmt, ap);
        mbstowcs(wmsg, amsg, maxstring);

        ENTER_CRITICAL_SECTION(&notices.lock);
        {
            notices.notices[i].emplace_back(wmsg);
            has_notice = true;
        }
        LEAVE_CRITICAL_SECTION(&notices.lock);
    }

    va_end(ap);
}
