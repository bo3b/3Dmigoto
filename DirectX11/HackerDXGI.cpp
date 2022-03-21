// Object             OS               DXGI version       Feature level
// IDXGIDevice        Win7             1.0                11.0
// IDXGIDevice1       Win7             1.0                11.0
// IDXGIDevice2       Platform update  1.2                11.1
// IDXGIDevice3       Win8.1           1.3
// IDXGIDevice4                        1.5
//
// IDXGIAdapter       Win7             1.0                11.0
// IDXGIAdapter1      Win7             1.0                11.0
// IDXGIAdapter2      Platform update  1.2                11.1
// IDXGIAdapter3                       1.3
//
// IDXGIFactory       Win7             1.0                11.0
// IDXGIFactory1      Win7             1.0                11.0
// IDXGIFactory2      Platform update  1.2                11.1
// IDXGIFactory3      Win8.1           1.3
// IDXGIFactory4                       1.4
// IDXGIFactory5                       1.5
//
// IDXGIOutput        Win7             1.0                11.0
// IDXGIOutput1       Platform update  1.2                11.1
// IDXGIOutput2       Win8.1           1.3
// IDXGIOutput3       Win8.1           1.3
// IDXGIOutput4       Win10            1.4
// IDXGIOutput5       Win10            1.5
//
// IDXGIResource      Win7             1.0                11.0
// IDXGIResource1     Platform update  1.2                11.1
//
// IDXGISwapChain     Win7             1.0                11.0
// IDXGISwapChain1    Platform update  1.2                11.1
// IDXGISwapChain2    Win8.1           1.3
// IDXGISwapChain3    Win10            1.4
// IDXGISwapChain4                     1.5

// 1-15-18: New approach is keep a strict single-layer policy when wrapping
// objects like IDXGISwapChain1.  Only the top level object we are interested
// in can successfully wrapped, because otherwise the vtable is altered from
// the DX11 definition, which led to crashes.
//
// Because of this, we are now creating only the HackerSwapChain and no
// other objects.  IDXGIFactory does not need to be wrapped, because it must
// be hooked in order to create the swap chains correctly. Device, Object,
// Adapter, Unknown were only wrapped in order to get us to the swap chain,
// and the new approach is to directly hook the CreateSwapChain and
// CreateSwapChainForHwnd, which saves a lot of complexity.
//
// We are making an IDXGISwapChain1, as the obvious descendant of SwapChain,
// and valid in platform_update scenarios.  It's only created by the
// CreateSwapChainForHwnd, but all our functionality is the same regardless.
// In the situation where the evil platform update is not installed, we will
// only create an IDXGISwapChain, but still reference it via composition in
// the HackerSwapChain.  The model is the same as that used in HackerDevice
// and HackerContext.

// Include before util.h (or any header that includes util.h) to get pretty
// version of LOCK_RESOURCE_CREATION_MODE:
#include "lock.h"

#include "HackerDXGI.hpp"
#include "HookedDevice.h"
#include "HookedDXGI.h"

#include "log.h"
#include "util.h"
#include "globals.h"
#include "Hunting.hpp"
#include "Override.hpp"
#include "IniHandler.h"
#include "CommandList.hpp"
#include "profiling.hpp"
#include "cursor.h"  // For InstallHookLate

// -----------------------------------------------------------------------------
// SetWindowPos hook, activated by full_screen=2 in d3dx.ini

static BOOL(WINAPI* fn_orig_SetWindowPos)(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) = nullptr;

static BOOL WINAPI hooked_SetWindowPos(
    HWND hWnd,
    HWND hWndInsertAfter,
    int  X,
    int  Y,
    int  cx,
    int  cy,
    UINT uFlags)
{
    if (G->SCREEN_UPSCALING != 0)
    {
        // Force desired upscaled resolution (only when desired resolution is provided!)
        if (cx != 0 && cy != 0)
        {
            cx = G->SCREEN_WIDTH;
            cy = G->SCREEN_HEIGHT;
            X  = 0;
            Y  = 0;
        }
    }
    else if (G->SCREEN_FULLSCREEN == 2)
    {
        // Do nothing - passing this call through could change the game
        // to a borderless window. Needed for The Witness.
        return true;
    }

    return fn_orig_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

void install_SetWindowPos_hook()
{
    HINSTANCE user32;
    int       fail = 0;

    // Only attempt to hook it once:
    if (fn_orig_SetWindowPos != nullptr)
        return;

    user32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
    fail |= InstallHookLate(user32, "SetWindowPos", reinterpret_cast<void**>(&fn_orig_SetWindowPos), hooked_SetWindowPos);

    if (fail)
    {
        LogOverlay(LOG_DIRE, "Failed to hook SetWindowPos for full_screen=2\n");
        return;
    }

    LOG_INFO("Successfully hooked SetWindowPos for full_screen=2\n");
    return;
}

// -----------------------------------------------------------------------------

void force_display_mode(
    DXGI_MODE_DESC* buffer_desc)
{
    // Historically we have only forced the refresh rate when full-screen.
    // I don't know if we ever had a good reason for that, but it
    // complicates forcing the refresh rate in games that start windowed
    // and later switch to full screen, so now forcing it unconditionally
    // to see how that goes. Helps Unity games work with 3D TV Play.
    //
    // UE4 does SetFullscreenState -> ResizeBuffers -> ResizeTarget
    // Unity does ResizeTarget -> SetFullscreenState -> ResizeBuffers
    if (G->SCREEN_REFRESH >= 0)
    {
        // FIXME: This may disable flipping (and use blitting instead)
        // if the forced numerator and denominator does not exactly
        // match a mode enumerated on the output. e.g. We would force
        // 60Hz as 60/1, but the display might actually use 60000/1001
        // for 60Hz and we would lose flipping and degrade performance.
        buffer_desc->RefreshRate.Numerator   = G->SCREEN_REFRESH;
        buffer_desc->RefreshRate.Denominator = 1;
        LOG_INFO("->Forcing refresh rate to = %f\n", static_cast<float>(buffer_desc->RefreshRate.Numerator) / static_cast<float>(buffer_desc->RefreshRate.Denominator));
    }
    if (G->SCREEN_WIDTH >= 0)
    {
        buffer_desc->Width = G->SCREEN_WIDTH;
        LOG_INFO("->Forcing Width to = %d\n", buffer_desc->Width);
    }
    if (G->SCREEN_HEIGHT >= 0)
    {
        buffer_desc->Height = G->SCREEN_HEIGHT;
        LOG_INFO("->Forcing Height to = %d\n", buffer_desc->Height);
    }
}

// -----------------------------------------------------------------------------

// In the Elite Dangerous case, they Release the HackerContext objects before creating the
// swap chain.  That causes problems, because we are not expecting anyone to get here without
// having a valid context.  They later call GetImmediateContext, which will generate a wrapped
// context.  So, since we need the context for our Overlay, let's do that a litte early in
// this case, which will save the reference for their GetImmediateContext call.

HackerSwapChain::HackerSwapChain(
    IDXGISwapChain1* swap_chain,
    HackerDevice*    device,
    HackerContext*   context)
{
    origSwapChain1 = swap_chain;

    hackerDevice  = device;
    hackerContext = context;

    // Bump the refcounts on the device and context to make sure they can't
    // be released as long as the swap chain is alive and we may be
    // accessing them. We probably don't actually need to do this for the
    // device, since the DirectX swap chain should already hold a reference
    // to the DirectX device, but it shouldn't hurt and makes the code more
    // semantically correct since we access the device as well. We could
    // skip both by looking them up on demand, but that would need extra
    // lookups in fast paths and there's no real need.
    //
    // The overlay also bumps these refcounts, which is technically
    // unecessary given we now do so here, but also shouldn't hurt, and is
    // safer in case we ever change this again and forget about it.

    hackerDevice->AddRef();
    if (hackerContext)
    {
        hackerContext->AddRef();
    }
    else
    {
        ID3D11DeviceContext* tmp_context = nullptr;
        // GetImmediateContext will bump the refcount for us.
        // In the case of hooking, GetImmediateContext will not return
        // a HackerContext, so we don't use it's return directly, but
        // rather just use it to make GetHackerContext valid:
        hackerDevice->GetImmediateContext(&tmp_context);
        hackerContext = hackerDevice->GetHackerContext();
    }

    hackerDevice->SetHackerSwapChain(this);

    try
    {
        // Create Overlay class that will be responsible for drawing any text
        // info over the game. Using the Hacker Device and Context we gave the game.
        overlay = new Overlay(hackerDevice, hackerContext, origSwapChain1);
    }
    catch (...)
    {
        LOG_INFO("  *** Failed to create Overlay. Exception caught.\n");
        overlay = nullptr;
    }
}

IDXGISwapChain1* HackerSwapChain::GetOrigSwapChain1()
{
    LOG_DEBUG("HackerSwapChain::GetOrigSwapChain returns %p\n", origSwapChain1);
    return origSwapChain1;
}

// -----------------------------------------------------------------------------

void HackerSwapChain::UpdateStereoParams()
{
    if (G->ENABLE_TUNE)
    {
        //device->paramTextureManager.mSeparationModifier = gTuneValue;
        hackerDevice->paramTextureManager.mTuneVariable1 = G->gTuneValue[0];
        hackerDevice->paramTextureManager.mTuneVariable2 = G->gTuneValue[1];
        hackerDevice->paramTextureManager.mTuneVariable3 = G->gTuneValue[2];
        hackerDevice->paramTextureManager.mTuneVariable4 = G->gTuneValue[3];

        int counter = 0;
        if (counter-- < 0)
        {
            counter                                        = 30;
            hackerDevice->paramTextureManager.mForceUpdate = true;
        }
    }

    // Update stereo parameter texture. It's possible to arrive here with no texture available though,
    // so we need to check first.
    if (hackerDevice->stereoTexture)
    {
        LOG_DEBUG("  updating stereo parameter texture.\n");
        hackerDevice->paramTextureManager.UpdateStereoTexture(hackerDevice, hackerContext, hackerDevice->stereoTexture, false);
    }
    else
    {
        LOG_DEBUG("  stereo parameter texture missing.\n");
    }
}

// Called at each DXGI::Present() to give us reliable time to execute user
// input and hunting commands.

void HackerSwapChain::RunFrameActions()
{
    LOG_DEBUG("Running frame actions.  Device: %p\n", hackerDevice);

    // Regardless of log settings, since this runs every frame, let's flush the log
    // so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
    if (LogFile)
        fflush(LogFile);

    // Run the command list here, before drawing the overlay so that a
    // custom shader on the present call won't remove the overlay. Also,
    // run this before most frame actions so that this can be considered as
    // a pre-present command list. We have a separate post-present command
    // list after the present call in case we need to restore state or
    // affect something at the start of the frame.
    run_command_list(hackerDevice, hackerContext, &G->present_command_list, nullptr, false);

    if (G->analyse_frame)
    {
        // We don't allow hold to be changed mid-frame due to potential
        // for filename conflicts, so use def_analyse_options:
        if (G->def_analyse_options & FrameAnalysisOptions::HOLD)
        {
            // If using analyse_options=hold we don't stop the
            // analysis at the frame boundary (it will be stopped
            // at the key up event instead), but we do increment
            // the frame count and reset the draw count:
            G->analyse_frame_no++;
        }
        else
        {
            G->analyse_frame = false;
            if (G->DumpUsage)
                DumpUsage(G->ANALYSIS_PATH);
            LogOverlay(LOG_INFO, "Frame analysis saved to %S\n", G->ANALYSIS_PATH);
        }
    }

    // NOTE: Now that key overrides can check an ini param, the ordering of
    // this and the present_command_list is significant. We might set an
    // ini param during a frame for scene detection, which is checked on
    // override activation, then cleared from the command list run on
    // present. If we ever needed to run the command list before this
    // point, we should consider making an explicit "pre" command list for
    // that purpose rather than breaking the existing behaviour.
    bool new_event = DispatchInputEvents(hackerDevice);

    current_transition.UpdatePresets(hackerDevice);
    current_transition.UpdateTransitions(hackerDevice);

    // The config file is not safe to reload from within the input handler
    // since it needs to change the key bindings, so it sets this flag
    // instead and we handle it now.
    if (G->gReloadConfigPending)
        ReloadConfig(hackerDevice);

    // Draw the on-screen overlay text with hunting and informational
    // messages, before final Present. We now do this after the shader and
    // config reloads, so if they have any notices we will see them this
    // frame (just in case we crash next frame or something).
    if (overlay && !G->suppress_overlay)
        overlay->DrawOverlay();
    G->suppress_overlay = false;

    // This must happen on the same side of the config and shader reloads
    // to ensure the config reload can't clear messages from the shader
    // reload. It doesn't really matter which side we do it on at the
    // moment, but let's do it last, because logically it makes sense to be
    // incremented when we call the original present call:
    G->frame_no++;

    // When not hunting most keybindings won't have been registered, but
    // still skip the below logic that only applies while hunting.
    if (G->hunting != HUNTING_MODE_ENABLED)
        return;

    // Update the huntTime whenever we get fresh user input.
    if (new_event)
        G->huntTime = time(nullptr);

    // Clear buffers after some user idle time.  This allows the buffers to be
    // stable during a hunt, and cleared after one minute of idle time.  The idea
    // is to make the arrays of shaders stable so that hunting up and down the arrays
    // is consistent, while the user is engaged.  After 1 minute, they are likely onto
    // some other spot, and we should start with a fresh set, to keep the arrays and
    // active shader list small for easier hunting.  Until the first keypress, the arrays
    // are cleared at each thread wake, just like before.
    // The arrays will be continually filled by the SetShader sections, but should
    // rapidly converge upon all active shaders.

    if (difftime(time(nullptr), G->huntTime) > 60)
    {
        ENTER_CRITICAL_SECTION(&G->mCriticalSection);
        {
            TimeoutHuntingBuffers();
        }
        LEAVE_CRITICAL_SECTION(&G->mCriticalSection);
    }
}

// -----------------------------------------------------------------------------
/** IUnknown **/

// In the game Elex, we see them call do the unusual SwapChain->QueryInterface(SwapChain).
// We need to return This when that happens, because otherwise they disconnect us and
// we never get calls to Present.  Rather than do just this one-off, let's always
// return This for any time this might happen, as we've seen it happen in HackerContext
// too, for Mafia 3.  So any future instances cannot leak.
//
// From: https://msdn.microsoft.com/en-us/library/windows/desktop/ms682521(v=vs.85).aspx
// And: https://blogs.msdn.microsoft.com/oldnewthing/20040326-00/?p=40033
//
//  For any one object, a specific query for the IUnknown interface on any of the object's
//    interfaces must always return the same pointer value. This enables a client to determine
//    whether two pointers point to the same component by calling QueryInterface with
//    IID_IUnknown and comparing the results.
//    It is specifically not the case that queries for interfaces other than IUnknown (even
//    the same interface through the same pointer) must return the same pointer value.
//
HRESULT STDMETHODCALLTYPE HackerSwapChain::QueryInterface(
    REFIID riid,
    void** ppvObject)
{
    LOG_INFO("HackerSwapChain::QueryInterface(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(riid).c_str());

    HRESULT hr = origSwapChain1->QueryInterface(riid, ppvObject);
    if (FAILED(hr) || !*ppvObject)
    {
        LOG_INFO("  failed result = %x for %p\n", hr, ppvObject);
        return hr;
    }

    // For TheDivision, only upon Win10, it will request these.  Even though the object
    // we would return is the exact same pointer in memory, it still calls into the object
    // with a vtable entry that does not match what they expected. Somehow they decide
    // they are on Win10, and know these APIs ought to exist.  Does not crash on Win7.
    //
    // Returning an E_NOINTERFACE here seems to work, but this does call into question our
    // entire wrapping strategy.  If the object we've wrapped is a superclass of the
    // object they desire, the vtable is not going to match.

    if (riid == __uuidof(IDXGISwapChain2))
    {
        LOG_INFO("***  returns E_NOINTERFACE as error for IDXGISwapChain2.\n");
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    if (riid == __uuidof(IDXGISwapChain3))
    {
        LOG_INFO("***  returns E_NOINTERFACE as error for IDXGISwapChain3.\n");
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    if (riid == __uuidof(IDXGISwapChain4))
    {
        LOG_INFO("***  returns E_NOINTERFACE as error for IDXGISwapChain4.\n");
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    IUnknown* unk_this = nullptr;
    HRESULT   hr_this  = origSwapChain1->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&unk_this));

    IUnknown* unk_ppv_object = nullptr;
    HRESULT   hr_ppv_object  = static_cast<IUnknown*>(*ppvObject)->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&unk_ppv_object));

    if (SUCCEEDED(hr_this) && SUCCEEDED(hr_ppv_object))
    {
        // For an actual case of this->QueryInterface(this), just return our HackerSwapChain object.
        if (unk_this == unk_ppv_object)
            *ppvObject = this;

        unk_this->Release();
        unk_ppv_object->Release();

        LOG_INFO("  return HackerSwapChain(%s@%p) wrapper of %p\n", type_name(this), this, origSwapChain1);
        return hr;
    }

    LOG_INFO("  returns result = %x for %p\n", hr, ppvObject);
    return hr;
}

ULONG STDMETHODCALLTYPE HackerSwapChain::AddRef()
{
    ULONG ul_ref = origSwapChain1->AddRef();
    LOG_INFO("HackerSwapChain::AddRef(%s@%p), counter=%d, this=%p\n", type_name(this), this, ul_ref, this);
    return ul_ref;
}

ULONG STDMETHODCALLTYPE HackerSwapChain::Release()
{
    ULONG ul_ref = origSwapChain1->Release();
    LOG_INFO("HackerSwapChain::Release(%s@%p), counter=%d, this=%p\n", type_name(this), this, ul_ref, this);

    if (ul_ref <= 0)
    {
        if (hackerDevice)
        {
            if (hackerDevice->GetHackerSwapChain() == this)
            {
                LOG_INFO("  Clearing hackerDevice->hackerSwapChain\n");
                hackerDevice->SetHackerSwapChain(nullptr);
            }
            else
            {
                LOG_INFO("  hackerDevice %p not using hackerSwapchain %p\n", hackerDevice, this);
            }
            hackerDevice->Release();
        }

        if (hackerContext)
            hackerContext->Release();

        if (overlay)
            delete overlay;

        if (last_fullscreen_swap_chain == origSwapChain1)
            last_fullscreen_swap_chain = nullptr;

        LOG_INFO("  counter=%d, this=%p, deleting self.\n", ul_ref, this);

        delete this;
        return 0L;
    }
    return ul_ref;
}

// -----------------------------------------------------------------------------
/** IDXGIObject **/

HRESULT STDMETHODCALLTYPE HackerSwapChain::SetPrivateData(
    REFGUID     Name,
    UINT        DataSize,
    const void* pData)
{
    LOG_INFO("HackerSwapChain::SetPrivateData(%s@%p) called with GUID: %s\n", type_name(this), this, name_from_IID(Name).c_str());
    LOG_INFO("  DataSize = %d\n", DataSize);

    HRESULT hr = origSwapChain1->SetPrivateData(Name, DataSize, pData);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::SetPrivateDataInterface(
    REFGUID         Name,
    const IUnknown* pUnknown)
{
    LOG_INFO("HackerSwapChain::SetPrivateDataInterface(%s@%p) called with GUID: %s\n", type_name(this), this, name_from_IID(Name).c_str());

    HRESULT hr = origSwapChain1->SetPrivateDataInterface(Name, pUnknown);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetPrivateData(
    REFGUID Name,
    UINT*   pDataSize,
    void*   pData)
{
    LOG_INFO("HackerSwapChain::GetPrivateData(%s@%p) called with GUID: %s\n", type_name(this), this, name_from_IID(Name).c_str());

    HRESULT hr = origSwapChain1->GetPrivateData(Name, pDataSize, pData);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

// More details: https://msdn.microsoft.com/en-us/library/windows/apps/hh465096.aspx
//
// This is the root class object, expected to be used for HackerDXGIAdapter, and
// HackerDXGIDevice GetParent() calls.  It would be legitimate for a caller to
// QueryInterface their objects to get the DXGIObject, and call GetParent, so
// this should be more robust.
//
// If the parent request is for the IDXGIAdapter or IDXGIFactory, that must mean
// we are taking the secret path for getting the swap chain.
//
// We no longer return wrapped objects here, because our CreateSwapChain hooks
// will correctly catch creation.

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetParent(
    REFIID riid,
    void** ppParent)
{
    LOG_INFO("HackerSwapChain::GetParent(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(riid).c_str());

    HRESULT hr = origSwapChain1->GetParent(riid, ppParent);
    if (FAILED(hr))
    {
        LOG_INFO("  failed result = %x for %p\n", hr, ppParent);
        return hr;
    }

    LOG_INFO("  returns result = %#x\n", hr);
    return hr;
}

// -----------------------------------------------------------------------------
/** IDXGIDeviceSubObject **/

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetDevice(
    REFIID riid,
    void** ppDevice)
{
    LOG_DEBUG("HackerSwapChain::GetDevice(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(riid).c_str());

    HRESULT hr = origSwapChain1->GetDevice(riid, ppDevice);
    LOG_DEBUG("  returns result = %x, handle = %p\n", hr, *ppDevice);
    return hr;
}

// -----------------------------------------------------------------------------
/** IDXGISwapChain **/

HRESULT STDMETHODCALLTYPE HackerSwapChain::Present(
    UINT SyncInterval,
    UINT Flags)
{
    Profiling::State profiling_state = {};
    bool             profiling       = false;

    LOG_DEBUG("HackerSwapChain::Present(%s@%p) called with\n", type_name(this), this);
    LOG_DEBUG("  SyncInterval = %d\n", SyncInterval);
    LOG_DEBUG("  Flags = %d\n", Flags);

    if (!(Flags & DXGI_PRESENT_TEST))
    {
        // Profiling::mode may change below, so make a copy
        profiling = Profiling::mode == Profiling::Mode::SUMMARY;
        if (profiling)
            Profiling::start(&profiling_state);

        // Every presented frame, we want to take some CPU time to run our actions,
        // which enables hunting, and snapshots, and aiming overrides and other inputs
        RunFrameActions();

        if (profiling)
            Profiling::end(&profiling_state, &Profiling::present_overhead);
    }

    HRESULT hr = origSwapChain1->Present(SyncInterval, Flags);

    if (!(Flags & DXGI_PRESENT_TEST))
    {
        if (profiling)
            Profiling::start(&profiling_state);

        // Update the stereo params texture just after the present so that
        // shaders get the new values for the current frame:
        UpdateStereoParams();

        G->bb_is_upscaling_bb = !!G->SCREEN_UPSCALING && G->upscaling_command_list_using_explicit_bb_flip;

        // Run the post present command list now, which can be used to restore
        // state changed in the pre-present command list, or to perform some
        // action at the start of a frame:
        run_command_list(hackerDevice, hackerContext, &G->post_present_command_list, nullptr, true);

        if (profiling)
            Profiling::end(&profiling_state, &Profiling::present_overhead);
    }

    LOG_DEBUG("  returns %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetBuffer(
    UINT   Buffer,
    REFIID riid,
    void** ppSurface)
{
    LOG_DEBUG("HackerSwapChain::GetBuffer(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(riid).c_str());

    HRESULT hr = origSwapChain1->GetBuffer(Buffer, riid, ppSurface);
    LOG_DEBUG("  returns %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::SetFullscreenState(
    BOOL         Fullscreen,
    IDXGIOutput* pTarget)
{
    LOG_INFO("HackerSwapChain::SetFullscreenState(%s@%p) called with\n", type_name(this), this);
    LOG_INFO("  Fullscreen = %d\n", Fullscreen);
    LOG_INFO("  Target = %p\n", pTarget);

    if (G->SCREEN_FULLSCREEN > 0)
    {
        if (G->SCREEN_FULLSCREEN == 2)
        {
            // We install this hook on demand to avoid any possible
            // issues with hooking the call when we don't need it.
            // Unconfirmed, but possibly related to:
            // https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
            install_SetWindowPos_hook();
        }

        Fullscreen = true;
        LOG_INFO("->Fullscreen forced = %d\n", Fullscreen);
    }

    //if (pTarget)
    //    hr = origSwapChain1->SetFullscreenState(Fullscreen, pTarget->m_pOutput);
    //else
    //    hr = origSwapChain1->SetFullscreenState(Fullscreen, 0);

    if (Fullscreen)
        last_fullscreen_swap_chain = origSwapChain1;

    HRESULT hr = origSwapChain1->SetFullscreenState(Fullscreen, pTarget);
    LOG_INFO("  returns %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetFullscreenState(
    BOOL*         pFullscreen,
    IDXGIOutput** ppTarget)
{
    LOG_DEBUG("HackerSwapChain::GetFullscreenState(%s@%p) called\n", type_name(this), this);

    //IDXGIOutput *origOutput;
    //HRESULT hr = origSwapChain1->GetFullscreenState(pFullscreen, &origOutput);
    //if (hr == S_OK)
    //{
    //    *ppTarget = IDXGIOutput::GetDirectOutput(origOutput);
    //    if (pFullscreen) LOG_INFO("  returns Fullscreen = %d\n", *pFullscreen);
    //    if (ppTarget) LOG_INFO("  returns target IDXGIOutput = %x, wrapper = %x\n", origOutput, *ppTarget);
    //}

    HRESULT hr = origSwapChain1->GetFullscreenState(pFullscreen, ppTarget);
    LOG_DEBUG("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetDesc(
    DXGI_SWAP_CHAIN_DESC* pDesc)
{
    LOG_DEBUG("HackerSwapChain::GetDesc(%s@%p) called\n", type_name(this), this);

    HRESULT hr = origSwapChain1->GetDesc(pDesc);

    if (hr == S_OK)
    {
        if (pDesc)
            LOG_DEBUG("  returns Windowed = %d\n", pDesc->Windowed);
        if (pDesc)
            LOG_DEBUG("  returns Width = %d\n", pDesc->BufferDesc.Width);
        if (pDesc)
            LOG_DEBUG("  returns Height = %d\n", pDesc->BufferDesc.Height);
        if (pDesc)
            LOG_DEBUG("  returns Refresh rate = %f\n", static_cast<float>(pDesc->BufferDesc.RefreshRate.Numerator) / static_cast<float>(pDesc->BufferDesc.RefreshRate.Denominator));
    }

    LOG_DEBUG("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::ResizeBuffers(
    UINT        BufferCount,
    UINT        Width,
    UINT        Height,
    DXGI_FORMAT NewFormat,
    UINT        SwapChainFlags)
{
    LOG_INFO("HackerSwapChain::ResizeBuffers(%s@%p) called\n", type_name(this), this);

    if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN)
    {
        G->mResolutionInfo.width  = Width;
        G->mResolutionInfo.height = Height;
        LOG_INFO("  Got resolution from swap chain: %ix%i\n", G->mResolutionInfo.width, G->mResolutionInfo.height);
    }

    HRESULT hr = origSwapChain1->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::ResizeTarget(
    const DXGI_MODE_DESC* pNewTargetParameters)
{
    DXGI_MODE_DESC new_desc;

    LOG_INFO("HackerSwapChain::ResizeTarget(%s@%p) called\n", type_name(this), this);
    LOG_INFO("  Width: %d, Height: %d\n", pNewTargetParameters->Width, pNewTargetParameters->Height);
    LOG_INFO("     Refresh rate = %f\n", static_cast<float>(pNewTargetParameters->RefreshRate.Numerator) / static_cast<float>(pNewTargetParameters->RefreshRate.Denominator));

    // Historically we have only forced the refresh rate when full-screen.
    // I don't know if we ever had a good reason for that, but it
    // complicates forcing the refresh rate in games that start windowed
    // and later switch to full screen, depending on the order in which
    // the game calls ResizeTarget and SetFullscreenState so now forcing it
    // unconditionally to see how that goes. Helps Unity games work with 3D
    // TV play. If we need to restore the old behaviour for some reason,
    // check the git history, but we will need further heroics.
    //
    // UE4 does SetFullscreenState -> ResizeBuffers -> ResizeTarget
    // Unity does ResizeTarget -> SetFullscreenState -> ResizeBuffers

    memcpy(&new_desc, pNewTargetParameters, sizeof(DXGI_MODE_DESC));
    force_display_mode(&new_desc);

    HRESULT hr = origSwapChain1->ResizeTarget(&new_desc);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetContainingOutput(
    IDXGIOutput** ppOutput)
{
    LOG_INFO("HackerSwapChain::GetContainingOutput(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetContainingOutput(ppOutput);
    LOG_INFO("  returns result = %#x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetFrameStatistics(
    DXGI_FRAME_STATISTICS* pStats)
{
    LOG_INFO("HackerSwapChain::GetFrameStatistics(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetFrameStatistics(pStats);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetLastPresentCount(
    UINT* pLastPresentCount)
{
    LOG_INFO("HackerSwapChain::GetLastPresentCount(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetLastPresentCount(pLastPresentCount);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

// -----------------------------------------------------------------------------
/** IDXGISwapChain1 **/

// IDXGISwapChain1 requires platform update
// IDXGISwapChain2 requires Win8.1
// IDXGISwapChain3 requires Win10

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetDesc1(
    DXGI_SWAP_CHAIN_DESC1* pDesc)
{
    LOG_INFO("HackerSwapChain::GetDesc1(%s@%p) called\n", type_name(this), this);

    HRESULT hr = origSwapChain1->GetDesc1(pDesc);
    if (hr == S_OK)
    {
        if (pDesc)
            LOG_INFO("  returns Stereo = %d\n", pDesc->Stereo);
        if (pDesc)
            LOG_INFO("  returns Width = %d\n", pDesc->Width);
        if (pDesc)
            LOG_INFO("  returns Height = %d\n", pDesc->Height);
    }
    LOG_INFO("  returns result = %x\n", hr);

    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetFullscreenDesc(
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc)
{
    LOG_INFO("HackerSwapChain::GetFullscreenDesc(%s@%p) called\n", type_name(this), this);

    HRESULT hr = origSwapChain1->GetFullscreenDesc(pDesc);
    if (hr == S_OK)
    {
        if (pDesc)
            LOG_INFO("  returns Windowed = %d\n", pDesc->Windowed);
        if (pDesc)
            LOG_INFO("  returns Refresh rate = %f\n", static_cast<float>(pDesc->RefreshRate.Numerator) / static_cast<float>(pDesc->RefreshRate.Denominator));
    }
    LOG_INFO("  returns result = %x\n", hr);

    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetHwnd(
    HWND* pHwnd)
{
    LOG_INFO("HackerSwapChain::GetHwnd(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetHwnd(pHwnd);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetCoreWindow(
    REFIID refiid,
    void** ppUnk)
{
    LOG_INFO("HackerSwapChain::GetCoreWindow(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(refiid).c_str());

    HRESULT hr = origSwapChain1->GetCoreWindow(refiid, ppUnk);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

// IDXGISwapChain1 requires the platform update, but will be the default
// swap chain we build whenever possible.
//
// ToDo: never seen this in action.  Setting to always log.  Once we see
// it in action and works OK, remove the gLogDebug sets, because debug log
// is too chatty for Present calls.

HRESULT STDMETHODCALLTYPE HackerSwapChain::Present1(
    UINT                           SyncInterval,
    UINT                           PresentFlags,
    const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
    Profiling::State profiling_state = {};
    gLogDebug                        = true;
    bool profiling                   = false;

    LOG_DEBUG("HackerSwapChain::Present1(%s@%p) called\n", type_name(this), this);
    LOG_DEBUG("  SyncInterval = %d\n", SyncInterval);
    LOG_DEBUG("  Flags = %d\n", PresentFlags);

    if (!(PresentFlags & DXGI_PRESENT_TEST))
    {
        // Profiling::mode may change below, so make a copy
        profiling = Profiling::mode == Profiling::Mode::SUMMARY;
        if (profiling)
            Profiling::start(&profiling_state);

        // Every presented frame, we want to take some CPU time to run our actions,
        // which enables hunting, and snapshots, and aiming overrides and other inputs
        RunFrameActions();

        if (profiling)
            Profiling::end(&profiling_state, &Profiling::present_overhead);
    }

    HRESULT hr = origSwapChain1->Present1(SyncInterval, PresentFlags, pPresentParameters);

    if (!(PresentFlags & DXGI_PRESENT_TEST))
    {
        if (profiling)
            Profiling::start(&profiling_state);

        // Update the stereo params texture just after the present so that we
        // get the new values for the current frame:
        UpdateStereoParams();

        G->bb_is_upscaling_bb = !!G->SCREEN_UPSCALING && G->upscaling_command_list_using_explicit_bb_flip;

        // Run the post present command list now, which can be used to restore
        // state changed in the pre-present command list, or to perform some
        // action at the start of a frame:
        run_command_list(hackerDevice, hackerContext, &G->post_present_command_list, nullptr, true);

        if (profiling)
            Profiling::end(&profiling_state, &Profiling::present_overhead);
    }

    LOG_DEBUG("  returns %x\n", hr);

    gLogDebug = false;
    return hr;
}

BOOL STDMETHODCALLTYPE HackerSwapChain::IsTemporaryMonoSupported()
{
    LOG_INFO("HackerSwapChain::IsTemporaryMonoSupported(%s@%p) called\n", type_name(this), this);
    BOOL ret = origSwapChain1->IsTemporaryMonoSupported();
    LOG_INFO("  returns %d\n", ret);
    return ret;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetRestrictToOutput(
    IDXGIOutput** ppRestrictToOutput)
{
    LOG_INFO("HackerSwapChain::GetRestrictToOutput(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetRestrictToOutput(ppRestrictToOutput);
    LOG_INFO("  returns result = %x, handle = %p\n", hr, *ppRestrictToOutput);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::SetBackgroundColor(
    const DXGI_RGBA* pColor)
{
    LOG_INFO("HackerSwapChain::SetBackgroundColor(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->SetBackgroundColor(pColor);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetBackgroundColor(
    DXGI_RGBA* pColor)
{
    LOG_INFO("HackerSwapChain::GetBackgroundColor(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetBackgroundColor(pColor);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::SetRotation(
    DXGI_MODE_ROTATION Rotation)
{
    LOG_INFO("HackerSwapChain::SetRotation(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->SetRotation(Rotation);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerSwapChain::GetRotation(
    DXGI_MODE_ROTATION* pRotation)
{
    LOG_INFO("HackerSwapChain::GetRotation(%s@%p) called\n", type_name(this), this);
    HRESULT hr = origSwapChain1->GetRotation(pRotation);
    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// HackerUpscalingSwapChain, to provide post-process upscaling to arbitrary
// resolutions.  Particularly good for 4K passive 3D.

HackerUpscalingSwapChain::HackerUpscalingSwapChain(
    IDXGISwapChain1*      swap_chain,
    HackerDevice*         hacker_device,
    HackerContext*        hacker_context,
    DXGI_SWAP_CHAIN_DESC* fake_swap_chain_desc,
    UINT                  new_width,
    UINT                  new_height) :
    HackerSwapChain(
        swap_chain,
        hacker_device,
        hacker_context),
    fakeSwapChain1(nullptr),
    fakeBackBuffer(nullptr),
    width(0),
    height(0)
{
    CreateRenderTarget(fake_swap_chain_desc);

    width  = new_width;
    height = new_height;
}

HackerUpscalingSwapChain::~HackerUpscalingSwapChain()
{
    if (fakeSwapChain1)
        fakeSwapChain1->Release();
    if (fakeBackBuffer)
        fakeBackBuffer->Release();
}

void HackerUpscalingSwapChain::CreateRenderTarget(
    DXGI_SWAP_CHAIN_DESC* fake_swap_chain_desc)
{
    HRESULT hr;

    switch (G->UPSCALE_MODE)
    {
        case 0:
        {
            // TODO: multisampled swap chain
            // TODO: multiple buffers within one spaw chain
            // ==> in this case upscale_mode = 1 should be used at the moment
            D3D11_TEXTURE2D_DESC fake_buffer_desc = {};
            fake_buffer_desc.ArraySize            = 1;
            fake_buffer_desc.MipLevels            = 1;
            fake_buffer_desc.BindFlags            = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            fake_buffer_desc.Usage                = D3D11_USAGE_DEFAULT;
            fake_buffer_desc.SampleDesc.Count     = 1;
            fake_buffer_desc.Format               = fake_swap_chain_desc->BufferDesc.Format;
            fake_buffer_desc.MiscFlags            = 0;
            fake_buffer_desc.Width                = fake_swap_chain_desc->BufferDesc.Width;
            fake_buffer_desc.Height               = fake_swap_chain_desc->BufferDesc.Height;
            fake_buffer_desc.CPUAccessFlags       = 0;

            LOCK_RESOURCE_CREATION_MODE();
            hr = hackerDevice->GetPassThroughOrigDevice1()->CreateTexture2D(&fake_buffer_desc, nullptr, &fakeBackBuffer);
            UNLOCK_RESOURCE_CREATION_MODE();
        }
        break;
        case 1:
        {
            IDXGIFactory* factory = nullptr;

            hr = origSwapChain1->GetParent(IID_PPV_ARGS(&factory));
            if (FAILED(hr))
            {
                LogOverlay(LOG_DIRE, "HackerUpscalingSwapChain::createRenderTarget failed to get DXGIFactory\n");
                // Not positive if we will be able to get an overlay to
                // display the error, so also issue an audible warning:
                beep_sad_failure();
                return;
            }
            const UINT flag_backup = fake_swap_chain_desc->Flags;

            // fake swap chain should have no influence on window
            fake_swap_chain_desc->Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            IDXGISwapChain* swap_chain;
            get_tls()->hooking_quirk_protection = true;
            factory->CreateSwapChain(hackerDevice->GetPossiblyHookedOrigDevice1(), fake_swap_chain_desc, &swap_chain);
            get_tls()->hooking_quirk_protection = false;

            factory->Release();

            HRESULT res = swap_chain->QueryInterface(IID_PPV_ARGS(&fakeSwapChain1));
            if (SUCCEEDED(res))
                swap_chain->Release();
            else
                fakeSwapChain1 = reinterpret_cast<IDXGISwapChain1*>(swap_chain);

            // restore old state in case fall back is required ToDo: Unlikely needed now.
            fake_swap_chain_desc->Flags = flag_backup;
        }
        break;
        default:
            LogOverlay(LOG_DIRE, "*** HackerUpscalingSwapChain::HackerUpscalingSwapChain() failed ==> provided upscaling mode is not valid.\n");
            // Not positive if we will be able to get an overlay to
            // display the error, so also issue an audible warning:
            beep_sad_failure();
            return;
    }

    LOG_INFO("HackerUpscalingSwapChain::HackerUpscalingSwapChain(): result %d\n", hr);

    if (FAILED(hr))
    {
        LogOverlay(LOG_DIRE, "*** HackerUpscalingSwapChain::HackerUpscalingSwapChain() failed\n");
        // Not positive if we will be able to get an overlay to
        // display the error, so also issue an audible warning:
        beep_sad_failure();
    }
}

HRESULT STDMETHODCALLTYPE HackerUpscalingSwapChain::GetBuffer(
    UINT   Buffer,
    REFIID riid,
    void** ppSurface)
{
    LOG_DEBUG("HackerUpscalingSwapChain::GetBuffer(%s@%p) called with IID: %s\n", type_name(this), this, name_from_IID(riid).c_str());

    HRESULT hr = S_OK;

    // if upscaling is on give the game fake back buffer
    if (fakeBackBuffer)
    {
        // Use QueryInterface on fakeBackBuffer, which validates that
        // the requested interface is supported, that ppSurface is not
        // NULL, and bumps the refcount if successful:
        hr = fakeBackBuffer->QueryInterface(riid, ppSurface);
    }
    else if (fakeSwapChain1)
    {
        hr = fakeSwapChain1->GetBuffer(Buffer, riid, ppSurface);
    }
    else
    {
        LOG_INFO("BUG: HackerUpscalingDXGISwapChain::GetBuffer(): Missing upscaling object\n");
        double_beep_exit();
    }

    LOG_DEBUG("  returns %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerUpscalingSwapChain::SetFullscreenState(
    BOOL         Fullscreen,
    IDXGIOutput* pTarget)
{
    LOG_INFO("HackerUpscalingSwapChain::SetFullscreenState(%s@%p) called with\n", type_name(this), this);
    LOG_INFO("  Fullscreen = %d\n", Fullscreen);
    LOG_INFO("  Target = %p\n", pTarget);

    HRESULT hr;

    BOOL         fullscreen_state = FALSE;
    IDXGIOutput* target           = nullptr;
    origSwapChain1->GetFullscreenState(&fullscreen_state, &target);

    if (target)
        target->Release();

    // dont call setfullscreenstate again to avoid starting mode switching and flooding winproc with unnecessary messages
    // can disable fullscreen mode somehow
    if (fullscreen_state && Fullscreen)
    {
        hr = S_OK;
    }
    else
    {
        if (G->SCREEN_UPSCALING == 2)
        {
            hr = origSwapChain1->SetFullscreenState(TRUE, pTarget);  // Witcher seems to require forcing the fullscreen
        }
        else
        {
            hr = origSwapChain1->SetFullscreenState(Fullscreen, pTarget);
        }
    }

    LOG_INFO("  returns %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerUpscalingSwapChain::GetDesc(
    DXGI_SWAP_CHAIN_DESC* pDesc)
{
    LOG_DEBUG("HackerUpscalingSwapChain::GetDesc(%s@%p) called\n", type_name(this), this);

    HRESULT hr = origSwapChain1->GetDesc(pDesc);

    if (hr == S_OK)
    {
        if (pDesc)
        {
            //TODO: not sure whether the upscaled resolution or game resolution should be returned
            // all tested games did not use this function only migoto does
            // I let them be the game resolution at the moment
            if (fakeBackBuffer)
            {
                D3D11_TEXTURE2D_DESC fd;
                fakeBackBuffer->GetDesc(&fd);
                pDesc->BufferDesc.Width  = fd.Width;
                pDesc->BufferDesc.Height = fd.Height;
                LOG_DEBUG("->Using fake SwapChain Sizes.\n");
            }

            if (fakeSwapChain1)
            {
                hr = fakeSwapChain1->GetDesc(pDesc);
            }
        }

        if (pDesc)
            LOG_DEBUG("  returns Windowed = %d\n", pDesc->Windowed);
        if (pDesc)
            LOG_DEBUG("  returns Width = %d\n", pDesc->BufferDesc.Width);
        if (pDesc)
            LOG_DEBUG("  returns Height = %d\n", pDesc->BufferDesc.Height);
        if (pDesc)
            LOG_DEBUG("  returns Refresh rate = %f\n", static_cast<float>(pDesc->BufferDesc.RefreshRate.Numerator) / static_cast<float>(pDesc->BufferDesc.RefreshRate.Denominator));
    }
    LOG_DEBUG("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerUpscalingSwapChain::ResizeBuffers(
    UINT        BufferCount,
    UINT        Width,
    UINT        Height,
    DXGI_FORMAT NewFormat,
    UINT        SwapChainFlags)
{
    LOG_INFO("HackerSwapChain::ResizeBuffers(%s@%p) called\n", type_name(this), this);

    // TODO: not sure if it belongs here, in the resize target function or in both
    // or maybe it is better to put it in the getviewport function?
    // Require in case the software mouse and upscaling are on at the same time
    G->GAME_INTERNAL_WIDTH  = Width;
    G->GAME_INTERNAL_HEIGHT = Height;

    if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN)
    {
        G->mResolutionInfo.width  = Width;
        G->mResolutionInfo.height = Height;
        LOG_INFO("Got resolution from swap chain: %ix%i\n", G->mResolutionInfo.width, G->mResolutionInfo.height);
    }

    HRESULT hr;

    if (fakeBackBuffer)  // UPSCALE_MODE 0
    {
        // TODO: need to consider the new code (G->gForceStereo == 2)
        // would my stuff work this way? i guess yes. What is with the games that are not calling resize buffer
        // just try to recreate texture with new game resolution
        // should be possible without any issues (texture just like the swap chain should not be used at this time point)

        D3D11_TEXTURE2D_DESC fd;
        fakeBackBuffer->GetDesc(&fd);

        if (!(fd.Width == Width && fd.Height == Height))
        {
            fakeBackBuffer->Release();

            fd.Width  = Width;
            fd.Height = Height;
            fd.Format = NewFormat;
            // just recreate texture with new width and height
            LOCK_RESOURCE_CREATION_MODE();
            hr = hackerDevice->GetPassThroughOrigDevice1()->CreateTexture2D(&fd, nullptr, &fakeBackBuffer);
            UNLOCK_RESOURCE_CREATION_MODE();
        }
        else  // nothing to resize
        {
            hr = S_OK;
        }
    }
    else if (fakeSwapChain1)  // UPSCALE_MODE 1
    {
        // the last parameter have to be zero to avoid the influence of the faked swap chain on the resize target function
        hr = fakeSwapChain1->ResizeBuffers(BufferCount, Width, Height, NewFormat, 0);
    }
    else
    {
        LOG_INFO("BUG: HackerUpscalingSwapChain::ResizeBuffers(): Missing upscaling object\n");
        double_beep_exit();
    }

    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE HackerUpscalingSwapChain::ResizeTarget(
    const DXGI_MODE_DESC* pNewTargetParameters)
{
    LOG_INFO("HackerUpscalingSwapChain::ResizeTarget(%s@%p) called\n", type_name(this), this);

    if (pNewTargetParameters != nullptr)
    {
        // TODO: not sure if it belongs here, in the resize buffers function or in both
        // or maybe it is better to put it in the getviewport function?
        // Require in case the software mouse and upscaling are on at the same time
        G->GAME_INTERNAL_WIDTH  = pNewTargetParameters->Width;
        G->GAME_INTERNAL_HEIGHT = pNewTargetParameters->Height;
    }

    // Some games like Witcher seems to drop fullscreen everytime the resizetarget is called (original one)
    // Some other games seems to require the function
    // I did it the way the faked texture mode (upscale_mode == 1) dont call resize target
    // the other mode does

    HRESULT hr;

    if (G->SCREEN_UPSCALING == 2)
    {
        DEVMODE dm_screen_settings      = {};
        dm_screen_settings.dmSize       = sizeof(dm_screen_settings);
        dm_screen_settings.dmPelsWidth  = static_cast<unsigned long>(width);
        dm_screen_settings.dmPelsHeight = static_cast<unsigned long>(height);
        dm_screen_settings.dmBitsPerPel = 32;
        dm_screen_settings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        // Change the display settings to full screen.
        LONG displ_chainge_res = ChangeDisplaySettingsEx(nullptr, &dm_screen_settings, nullptr, CDS_FULLSCREEN, nullptr);
        hr                     = displ_chainge_res == 0 ? S_OK : DXGI_ERROR_INVALID_CALL;
    }
    else if (G->SCREEN_UPSCALING == 1)
    {
        DXGI_MODE_DESC md = *pNewTargetParameters;

        // force upscaled resolution
        md.Width  = width;
        md.Height = height;

        // Temporarily disable the GetClientRect() hook since DirectX
        // itself will call that and we want it to get the real
        // resolution. Fixes upscaling in ARK: Survival Evolved
        G->upscaling_hooks_armed = false;
        hr                       = origSwapChain1->ResizeTarget(&md);
        G->upscaling_hooks_armed = true;
    }

    LOG_INFO("  returns result = %x\n", hr);
    return hr;
}
