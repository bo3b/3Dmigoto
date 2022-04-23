#include "Cursor.h"

#include "Globals.h"
#include "log.h"
#include "Overlay.hpp"

//////////////////////////// HARDWARE MOUSE CURSOR SUPPRESSION //////////////////////////
// To suppress the hardware mouse cursor you would think we just have to call
// ShowCursor(FALSE) somewhere to decrement the count by one, but doing so from
// DLL initialization permanently disables the cursor (in Dreamfall Chapters at
// least), and calling it from RunFrameActions or elsewhere has no effect
// (though the call does not indicate an error and does seem to affect a
// counter).
//
// My first attempt to solve this was to hook ShowCursor and keep a separate
// counter for the software curser visibility, but it turns out the Steam
// Overlay also hooks this function, but has a bug where it calls the original
// vs hooked versions inconsistently when showing vs hiding the overlay,
// leading to it reading the visibility count of the *hardware* cursor when the
// overlay is shown, then setting the visibility count of the *software* cursor
// to match when the overlay is hidden, leading to the cursor disappearing.
//
// This is a second attempt to suppress the hardware cursor - this time we
// leave the visibility count alone and instead replace the cursor icon with a
// completely invisible one. Since the hardware cursor technically is
// displayed, the visibility counts for software and hardware cursors match, so
// we no longer need to manage them separately. We hook into SetCursor,
// GetCursor and GetCursorInfo to keep a handle of the cursor the game set and
// return it whenever something (including our own software mouse
// implementation) asks for it.

HCURSOR current_cursor = nullptr;

typedef LRESULT(WINAPI* lpfnDefWindowProc)(
    HWND   hWnd,
    UINT   Msg,
    WPARAM wParam,
    LPARAM lParam);

static lpfnDefWindowProc trampoline_DefWindowProcA = DefWindowProcA;
static lpfnDefWindowProc trampoline_DefWindowProcW = DefWindowProcW;

static HCURSOR(WINAPI* trampoline_SetCursor)(HCURSOR hCursor)              = SetCursor;
static HCURSOR(WINAPI* trampoline_GetCursor)()                             = GetCursor;
static BOOL(WINAPI* trampoline_GetCursorInfo)(PCURSORINFO pci)             = GetCursorInfo;
static BOOL(WINAPI* trampoline_SetCursorPos)(int X, int Y)                 = SetCursorPos;
static BOOL(WINAPI* trampoline_GetCursorPos)(LPPOINT lpPoint)              = GetCursorPos;
static BOOL(WINAPI* trampoline_ScreenToClient)(HWND hWnd, LPPOINT lpPoint) = ScreenToClient;
static BOOL(WINAPI* trampoline_GetClientRect)(HWND hWnd, LPRECT lpRect)    = GetClientRect;

// This routine creates an invisible cursor that we can set whenever we are
// hiding the cursor. It is static, so will only be created the first time this
// is called.
static HCURSOR invisible_cursor()
{
    static HCURSOR cursor = nullptr;
    int            width, height;
    unsigned       pitch, size;

    char*and;
    char* xor ;

    if (!cursor)
    {
        width  = GetSystemMetrics(SM_CXCURSOR);
        height = GetSystemMetrics(SM_CYCURSOR);
        pitch  = ((width + 31) / 32) * 4;
        size   = pitch * height;

        and = new char[size];
        xor = new char[size];

        memset(and, 0xff, size);
        memset(xor, 0x00, size);

        cursor = CreateCursor(GetModuleHandle(nullptr), 0, 0, width, height, and, xor);

        delete[]and;
        delete[] xor ;
    }

    return cursor;
}

// We hook the SetCursor call so that we can catch the current cursor that the
// game has set and return it in the GetCursorInfo call whenever the software
// cursor is visible but the hardware cursor is not.
static HCURSOR WINAPI hooked_SetCursor(
    HCURSOR hCursor)
{
    current_cursor = hCursor;

    if (G->hide_cursor)
        return trampoline_SetCursor(invisible_cursor());

    return trampoline_SetCursor(hCursor);
}

static HCURSOR WINAPI hooked_GetCursor()
{
    if (G->hide_cursor)
        return current_cursor;

    return trampoline_GetCursor();
}

static BOOL WINAPI hide_cursor_get_cursor_info(
    PCURSORINFO pci)
{
    BOOL rc = trampoline_GetCursorInfo(pci);

    if (rc && (pci->flags & CURSOR_SHOWING))
        pci->hCursor = current_cursor;

    return rc;
}

static BOOL WINAPI hooked_GetCursorInfo(
    PCURSORINFO pci)
{
    BOOL rc = hide_cursor_get_cursor_info(pci);
    RECT client;

    if (rc && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd, &client) && client.right && client.bottom)
    {
        pci->ptScreenPos.x = pci->ptScreenPos.x * G->GAME_INTERNAL_WIDTH / client.right;
        pci->ptScreenPos.y = pci->ptScreenPos.y * G->GAME_INTERNAL_HEIGHT / client.bottom;
    }

    return rc;
}

BOOL WINAPI cursor_upscaling_bypass_GetCursorInfo(
    PCURSORINFO pci)
{
    if (G->cursor_upscaling_bypass)
    {
        // Still need to process hide_cursor logic:
        return hide_cursor_get_cursor_info(pci);
    }

    return GetCursorInfo(pci);
}

static BOOL WINAPI hooked_ScreenToClient(
    HWND    hWnd,
    LPPOINT lpPoint)
{
    BOOL rc;
    RECT client;
    bool translate = G->SCREEN_UPSCALING > 0 && lpPoint && trampoline_GetClientRect(G->hWnd, &client) && client.right && client.bottom && G->GAME_INTERNAL_WIDTH && G->GAME_INTERNAL_HEIGHT;

    if (translate)
    {
        // Scale back to original screen coordinates:
        lpPoint->x = lpPoint->x * client.right / G->GAME_INTERNAL_WIDTH;
        lpPoint->y = lpPoint->y * client.bottom / G->GAME_INTERNAL_HEIGHT;
    }

    rc = trampoline_ScreenToClient(hWnd, lpPoint);

    if (translate)
    {
        // Now scale to fake game coordinates:
        lpPoint->x = lpPoint->x * G->GAME_INTERNAL_WIDTH / client.right;
        lpPoint->y = lpPoint->y * G->GAME_INTERNAL_HEIGHT / client.bottom;
    }

    return rc;
}

BOOL WINAPI cursor_upscaling_bypass_ScreenToClient(
    HWND    hWnd,
    LPPOINT lpPoint)
{
    if (G->cursor_upscaling_bypass)
        return trampoline_ScreenToClient(hWnd, lpPoint);
    return ScreenToClient(hWnd, lpPoint);
}

static BOOL WINAPI hooked_GetClientRect(
    HWND   hWnd,
    LPRECT lpRect)
{
    BOOL rc = trampoline_GetClientRect(hWnd, lpRect);

    if (G->upscaling_hooks_armed && rc && G->SCREEN_UPSCALING > 0 && lpRect != nullptr)
    {
        lpRect->right  = G->GAME_INTERNAL_WIDTH;
        lpRect->bottom = G->GAME_INTERNAL_HEIGHT;
    }

    return rc;
}

BOOL WINAPI cursor_upscaling_bypass_GetClientRect(
    HWND   hWnd,
    LPRECT lpRect)
{
    if (G->cursor_upscaling_bypass)
        return trampoline_GetClientRect(hWnd, lpRect);
    return GetClientRect(hWnd, lpRect);
}

static BOOL WINAPI hooked_GetCursorPos(
    LPPOINT lpPoint)
{
    BOOL res = trampoline_GetCursorPos(lpPoint);
    RECT client;

    if (lpPoint && res && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd, &client) && client.right && client.bottom)
    {
        // This should work with all games that uses this function to gatter the mouse coords
        // Tested with witcher 3 and dreamfall chapters
        // TODO: Maybe there is a better way than use globals for the original game resolution
        lpPoint->x = lpPoint->x * G->GAME_INTERNAL_WIDTH / client.right;
        lpPoint->y = lpPoint->y * G->GAME_INTERNAL_HEIGHT / client.bottom;
    }

    return res;
}

static BOOL WINAPI hooked_SetCursorPos(
    int X,
    int Y)
{
    RECT client;

    if (G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd, &client) && G->GAME_INTERNAL_WIDTH && G->GAME_INTERNAL_HEIGHT)
    {
        // TODO: Maybe there is a better way than use globals for the original game resolution
        const int new_x = X * client.right / G->GAME_INTERNAL_WIDTH;
        const int new_y = Y * client.bottom / G->GAME_INTERNAL_HEIGHT;
        return trampoline_SetCursorPos(new_x, new_y);
    }

    return trampoline_SetCursorPos(X, Y);
}

// DefWindowProc can bypass our SetCursor hook, which means that some games
// such would continue showing the hardware cursor, and our knowledge of what
// cursor was supposed to be set may be inaccurate (e.g. Akiba's Trip doesn't
// hide the cursor and sometimes the software cursor uses the busy cursor
// instead of the arrow cursor). We fix this by hooking DefWindowProc and
// processing WM_SETCURSOR message just as the original DefWindowProc would
// have done, but without bypassing our SetCursor hook.
//
// An alternative to hooking DefWindowProc in this manner might be to use
// SetWindowsHookEx since it can also hook window messages.
static LRESULT WINAPI hooked_DefWindowProc(
    HWND              hWnd,
    UINT              Msg,
    WPARAM            wParam,
    LPARAM            lParam,
    lpfnDefWindowProc trampoline_DefWindowProc)
{
    HWND    parent = nullptr;
    HCURSOR cursor = nullptr;
    LPARAM  ret    = 0;

    if (Msg == WM_SETCURSOR)
    {
        // XXX: Should we use GetParent or GetAncestor? GetParent can
        // return an "owner" window, while GetAncestor only returns
        // parents... Not sure which the official DefWindowProc uses,
        // but I suspect the answer is GetAncestor, so go with that:
        parent = GetAncestor(hWnd, GA_PARENT);

        if (parent)
        {
            // Pass the message to the parent window, just like the
            // real DefWindowProc does. This may call back in here
            // if the parent also doesn't handle this message, and
            // we stop processing if the parent handled it.
            ret = SendMessage(parent, Msg, wParam, lParam);
            if (ret)
                return ret;
        }

        // If the mouse is in the client area and the window class has
        // a cursor associated with it we set that. This will call into
        // our hooked version of SetCursor (whereas the real
        // DefWindowProc would bypass that) so that we can track the
        // current cursor set by the game and force the hardware cursor
        // to remain hidden.
        if ((lParam & 0xffff) == HTCLIENT)
        {
            cursor = reinterpret_cast<HCURSOR>(GetClassLongPtr(hWnd, GCLP_HCURSOR));
            if (cursor)
                SetCursor(cursor);
        }
        else
        {
            // Not in client area. We could continue emulating
            // DefWindowProc by setting an arrow cursor, bypassing
            // our hook to set the *real* hardware cursor, but
            // since the real DefWindowProc already bypasses our
            // hook let's just call that and allow it to take care
            // of any other edge cases we may not know about (like
            // HTERROR):
            return trampoline_DefWindowProc(hWnd, Msg, wParam, lParam);
        }

        // Return false to allow children to set their class cursor:
        return FALSE;
    }

    return trampoline_DefWindowProc(hWnd, Msg, wParam, lParam);
}

static LRESULT WINAPI hooked_DefWindowProcA(
    HWND   hWnd,
    UINT   Msg,
    WPARAM wParam,
    LPARAM lParam)
{
    return hooked_DefWindowProc(hWnd, Msg, wParam, lParam, trampoline_DefWindowProcA);
}

static LRESULT WINAPI hooked_DefWindowProcW(
    HWND   hWnd,
    UINT   Msg,
    WPARAM wParam,
    LPARAM lParam)
{
    return hooked_DefWindowProc(hWnd, Msg, wParam, lParam, trampoline_DefWindowProcW);
}

int install_hook_late(
    HINSTANCE module,
    char*     func,
    void**    trampoline,
    void*     hook)
{
    SIZE_T hook_id;
    DWORD  dw_os_err;
    void*  fn_orig;

    // Early exit with error so the caller doesn't need to explicitly deal
    // with errors getting the module handle:
    if (!module)
        return 1;

    fn_orig = NktHookLibHelpers::GetProcedureAddress(module, func);
    if (fn_orig == nullptr)
    {
        LOG_INFO("Failed to get address of %s\n", func);
        return 1;
    }

    dw_os_err = cHookMgr.Hook(&hook_id, trampoline, fn_orig, hook);
    if (dw_os_err)
    {
        LOG_INFO("Failed to hook %s: 0x%x\n", func, dw_os_err);
        return 1;
    }

    return 0;
}

void install_mouse_hooks(
    bool hide)
{
    HINSTANCE   user32;
    static bool hook_installed = false;
    int         fail           = 0;

    // Only attempt to hook it once:
    if (hook_installed)
        return;
    hook_installed = true;

    // Init our handle to the current cursor now before installing the
    // hooks, and from now on it will be kept up to date from SetCursor:
    current_cursor = GetCursor();
    if (hide)
        SetCursor(invisible_cursor());

    user32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
    fail |= install_hook_late(user32, "SetCursor", reinterpret_cast<void**>(&trampoline_SetCursor), hooked_SetCursor);
    fail |= install_hook_late(user32, "GetCursor", reinterpret_cast<void**>(&trampoline_GetCursor), hooked_GetCursor);
    fail |= install_hook_late(user32, "GetCursorInfo", reinterpret_cast<void**>(&trampoline_GetCursorInfo), hooked_GetCursorInfo);
    fail |= install_hook_late(user32, "DefWindowProcA", reinterpret_cast<void**>(&trampoline_DefWindowProcA), hooked_DefWindowProcA);
    fail |= install_hook_late(user32, "DefWindowProcW", reinterpret_cast<void**>(&trampoline_DefWindowProcW), hooked_DefWindowProcW);
    fail |= install_hook_late(user32, "SetCursorPos", reinterpret_cast<void**>(&trampoline_SetCursorPos), hooked_SetCursorPos);
    fail |= install_hook_late(user32, "GetCursorPos", reinterpret_cast<void**>(&trampoline_GetCursorPos), hooked_GetCursorPos);
    fail |= install_hook_late(user32, "ScreenToClient", reinterpret_cast<void**>(&trampoline_ScreenToClient), hooked_ScreenToClient);
    fail |= install_hook_late(user32, "GetClientRect", reinterpret_cast<void**>(&trampoline_GetClientRect), hooked_GetClientRect);

    if (fail)
    {
        log_overlay(Log_Level::dire, "Failed to hook mouse cursor functions - hide_cursor will not work\n");
        return;
    }

    LOG_INFO("Successfully hooked mouse cursor functions for hide_cursor\n");
}
