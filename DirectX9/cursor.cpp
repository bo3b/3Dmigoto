#include "cursor.h"

#include "globals.h"
#include "Overlay.h"

// DX9 Port notes:
//
// This file has both cursor and upscaling hooks mixed in. The upscaling could
// go elsewhere as it has in DX11, but there isn't an exact match for where it
// was placed in the DX11 project since DX9 lacks DXGI, and tbh the hooking
// routines didn't necessarily belong there anyway. There is some overlap
// between the cursor and window management anyway, so it can live here for the
// time being. At least this file is relatively easy to diff against the DX11
// version.
//
// This file also has some code brought in from GeDoSaTo, which is not (yet?)
// in the DX11 project, but has required us to relicense under the GPLv3.

//from GeDoSaTo: https://github.com/PeterTh/gedosato
#define GENERATE_INTERCEPT_HEADER(__name, __rettype, __convention, ...) \
typedef __rettype (__convention * __name##_FNType)(__VA_ARGS__); \
__name##_FNType True##__name, __name##Pointer; \
bool completed##__name##Detour = false; \
__rettype __convention Detoured##__name(__VA_ARGS__)
//end GeDoSaTo

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

HCURSOR current_cursor = NULL;

typedef LRESULT(WINAPI *lpfnDefWindowProc)(_In_ HWND hWnd,
	_In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam);

static lpfnDefWindowProc trampoline_DefWindowProcA = DefWindowProcA;
static lpfnDefWindowProc trampoline_DefWindowProcW = DefWindowProcW;

static HCURSOR(WINAPI *trampoline_SetCursor)(_In_opt_ HCURSOR hCursor) = SetCursor;
static HCURSOR(WINAPI *trampoline_GetCursor)(void) = GetCursor;
static BOOL(WINAPI *trampoline_GetCursorInfo)(_Inout_ PCURSORINFO pci) = GetCursorInfo;
static BOOL(WINAPI* trampoline_SetCursorPos)(_In_ int X, _In_ int Y) = SetCursorPos;
static BOOL(WINAPI* trampoline_GetCursorPos)(_Out_ LPPOINT lpPoint) = GetCursorPos;
static BOOL(WINAPI* trampoline_ScreenToClient)(_In_ HWND hWnd, LPPOINT lpPoint) = ScreenToClient;
static BOOL(WINAPI* trampoline_GetClientRect)(_In_ HWND hWnd, _Out_ LPRECT lpRect) = GetClientRect;
static BOOL(WINAPI *trampoline_GetWindowRect)(_In_ HWND hWnd, _Out_ LPRECT lpRect)= GetWindowRect;
static int(WINAPI *trampoline_MapWindowPoints)(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints,UINT cPoints) = MapWindowPoints;

static int(WINAPI *trampoline_ShowCursor)(_In_ BOOL bShow) = ShowCursor;

// This routine creates an invisible cursor that we can set whenever we are
// hiding the cursor. It is static, so will only be created the first time this
// is called.
static HCURSOR InvisibleCursor()
{
	static HCURSOR cursor = NULL;
	int width, height;
	unsigned pitch, size;
	char *and, *xor;

	if (!cursor) {
		width = GetSystemMetrics(SM_CXCURSOR);
		height = GetSystemMetrics(SM_CYCURSOR);
		pitch = ((width + 31) / 32) * 4;
		size = pitch * height;

		and = new char[size];
		xor = new char[size];

		memset(and, 0xff, size);
		memset(xor, 0x00, size);

		cursor = CreateCursor(GetModuleHandle(NULL), 0, 0, width, height, and, xor);

		delete[] and;
		delete[] xor;
	}

	return cursor;
}


static int WINAPI Hooked_ShowCursor(_In_ BOOL bShow) {
	G->SET_CURSOR_UPDATE_REQUIRED(1);
	LogDebug("  Hooked_ShowCursor called \n");
	return trampoline_ShowCursor(bShow);
}

// We hook the SetCursor call so that we can catch the current cursor that the
// game has set and return it in the GetCursorInfo call whenever the software
// cursor is visible but the hardware cursor is not.
static HCURSOR WINAPI Hooked_SetCursor(
	_In_opt_ HCURSOR hCursor)
{
	if (current_cursor != hCursor)
		G->SET_CURSOR_UPDATE_REQUIRED(1);
	current_cursor = hCursor;
	LogDebug("  Hooked_SetCursor called \n");
	if (G->hide_cursor)
		return trampoline_SetCursor(InvisibleCursor());
	else
		return trampoline_SetCursor(hCursor);
}

static HCURSOR WINAPI Hooked_GetCursor(void)
{
	LogDebug("  Hooked_GetCursor called \n");
	if (G->hide_cursor)
		return current_cursor;
	else
		return trampoline_GetCursor();
}

static BOOL WINAPI HideCursor_GetCursorInfo(
	_Inout_ PCURSORINFO pci)
{
	LogDebug(" HideCursor_GetCursorInfo called \n");
	BOOL rc = trampoline_GetCursorInfo(pci);

	if (rc && (pci->flags & CURSOR_SHOWING))
		pci->hCursor = current_cursor;

	return rc;
}

static BOOL WINAPI Hooked_GetCursorInfo(
	_Inout_ PCURSORINFO pci)
{
	LogDebug("  Hooked_GetCursorInfo called \n");
	BOOL rc = HideCursor_GetCursorInfo(pci);
	RECT client;

	if (rc && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd(), &client) && client.right && client.bottom)
	{
		pci->ptScreenPos.x = pci->ptScreenPos.x * G->GAME_INTERNAL_WIDTH() / client.right;
		pci->ptScreenPos.y = pci->ptScreenPos.y * G->GAME_INTERNAL_HEIGHT() / client.bottom;

		LogDebug("  right = %d, bottom = %d, game width = %d, game height = %d\n", client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}

	return rc;
}

BOOL WINAPI CursorUpscalingBypass_GetCursorInfo(
	_Inout_ PCURSORINFO pci)
{
	if (G->cursor_upscaling_bypass)
	{
		// Still need to process hide_cursor logic:
		return HideCursor_GetCursorInfo(pci);
	}
	return GetCursorInfo(pci);
}

static BOOL WINAPI Hooked_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint)
{
	LogDebug("Hooked_ScreenToClient called \n");
	BOOL rc;
	RECT client;
	bool translate = G->SCREEN_UPSCALING > 0 && lpPoint
		&& trampoline_GetClientRect(G->hWnd(), &client)
		&& client.right && client.bottom
		&& G->GAME_INTERNAL_WIDTH() && G->GAME_INTERNAL_HEIGHT();

	if (translate)
	{
		// Scale back to original screen coordinates:
		lpPoint->x = lpPoint->x * client.right / G->GAME_INTERNAL_WIDTH();
		lpPoint->y = lpPoint->y * client.bottom / G->GAME_INTERNAL_HEIGHT();
	}

	rc = trampoline_ScreenToClient(hWnd, lpPoint);

	if (translate)
	{
		// Now scale to fake game coordinates:
		lpPoint->x = lpPoint->x * G->GAME_INTERNAL_WIDTH() / client.right;
		lpPoint->y = lpPoint->y * G->GAME_INTERNAL_HEIGHT() / client.bottom;

		LogDebug("  translate = %d, x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", translate, lpPoint->x, lpPoint->y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}


	return rc;
}
static int WINAPI Hooked_MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints)
{
	LogDebug("  Hooked_MapWindowPoints called \n");
	int rc;
	RECT client;
	bool translate = G->adjust_map_window_points && G->SCREEN_UPSCALING > 0 && (hWndFrom == NULL || hWndFrom == HWND_DESKTOP) && hWndTo != NULL && hWndTo != HWND_DESKTOP && lpPoints
		&& trampoline_GetClientRect(G->hWnd(), &client)
		&& client.right && client.bottom
		&& G->GAME_INTERNAL_WIDTH() && G->GAME_INTERNAL_HEIGHT();

	if (translate)
	{
		//lpPoints[0].x = 1;
		// Scale back to original screen coordinates:
		lpPoints->x = lpPoints->x * client.right / G->GAME_INTERNAL_WIDTH();
		lpPoints->y = lpPoints->y * client.bottom / G->GAME_INTERNAL_HEIGHT();
	}

	rc = trampoline_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);

	if (translate)
	{
		// Now scale to fake game coordinates:
		lpPoints->x = lpPoints->x * G->GAME_INTERNAL_WIDTH() / client.right;
		lpPoints->y = lpPoints->y * G->GAME_INTERNAL_HEIGHT() / client.bottom;
		LogDebug("  translate = %d, x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", translate, lpPoints->x, lpPoints->y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}



	return rc;
}

BOOL WINAPI CursorUpscalingBypass_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint)
{
	if (G->cursor_upscaling_bypass)
		return trampoline_ScreenToClient(hWnd, lpPoint);
	return ScreenToClient(hWnd, lpPoint);
}

static BOOL WINAPI Hooked_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect)
{
	BOOL rc = trampoline_GetClientRect(hWnd, lpRect);

	if (G->upscaling_hooks_armed && rc && G->SCREEN_UPSCALING > 0 && lpRect != NULL && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
		lpRect->right = G->GAME_INTERNAL_WIDTH();
		lpRect->bottom = G->GAME_INTERNAL_HEIGHT();
	}
	LogDebug("  Hooked_GetClientRect called right = %d, bottom = %d\n", lpRect->right, lpRect->bottom);

	return rc;
}
static BOOL WINAPI Hooked_GetWindowRect(_In_ HWND hWnd, _Out_ LPRECT lpRect)
{

	BOOL rc = trampoline_GetWindowRect(hWnd, lpRect);

	if (G->adjust_get_window_rect && G->upscaling_hooks_armed && rc && G->SCREEN_UPSCALING > 0 && lpRect != NULL && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
		lpRect->right = G->GAME_INTERNAL_WIDTH();
		lpRect->bottom = G->GAME_INTERNAL_HEIGHT();
	}
	LogDebug("  Hooked_GetWindowRect called right = %d, bottom = %d\n", lpRect->right, lpRect->bottom);

	return rc;
}

BOOL WINAPI CursorUpscalingBypass_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect)
{
	if (G->cursor_upscaling_bypass)
		return trampoline_GetClientRect(hWnd, lpRect);
	return GetClientRect(hWnd, lpRect);
}

static BOOL WINAPI Hooked_GetCursorPos(_Out_ LPPOINT lpPoint)
{
	LogDebug("Hooked_GetCursorPos called \n");
	BOOL res = trampoline_GetCursorPos(lpPoint);
	RECT client;

	if (G->adjust_cursor_pos && lpPoint && res && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd(), &client) && client.right && client.bottom && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
		// This should work with all games that uses this function to gatter the mouse coords
		// Tested with witcher 3 and dreamfall chapters
		// TODO: Maybe there is a better way than use globals for the original game resolution
		lpPoint->x = lpPoint->x * G->GAME_INTERNAL_WIDTH() / client.right;
		lpPoint->y = lpPoint->y * G->GAME_INTERNAL_HEIGHT() / client.bottom;
		LogDebug("  x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", lpPoint->x, lpPoint->y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}

	return res;
}

static BOOL WINAPI Hooked_SetCursorPos(_In_ int X, _In_ int Y)
{
	RECT client;

	if (G->adjust_cursor_pos && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd(), &client) && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
		// TODO: Maybe there is a better way than use globals for the original game resolution
		const int new_x = X * client.right / G->GAME_INTERNAL_WIDTH();
		const int new_y = Y * client.bottom / G->GAME_INTERNAL_HEIGHT();
		LogDebug("  Hooked_SetCursorPos called x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", new_x, new_y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
		return trampoline_SetCursorPos(new_x, new_y);
	}
	else
		LogDebug("  Hooked_SetCursorPos called x = %d, y = %d", X, Y);
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
static LRESULT WINAPI Hooked_DefWindowProc(
	_In_ HWND   hWnd,
	_In_ UINT   Msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam,
	lpfnDefWindowProc trampoline_DefWindowProc)
{

	HWND parent = NULL;
	HCURSOR cursor = NULL;
	LPARAM ret = 0;

	if (Msg == WM_SETCURSOR) {
		// XXX: Should we use GetParent or GetAncestor? GetParent can
		// return an "owner" window, while GetAncestor only returns
		// parents... Not sure which the official DefWindowProc uses,
		// but I suspect the answer is GetAncestor, so go with that:
		parent = GetAncestor(hWnd, GA_PARENT);

		if (parent) {
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
		if ((lParam & 0xffff) == HTCLIENT) {
			cursor = (HCURSOR)GetClassLongPtr(hWnd, GCLP_HCURSOR);
			if (cursor)
				SetCursor(cursor);
		} else {
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

static LRESULT WINAPI Hooked_DefWindowProcA(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	return Hooked_DefWindowProc(hWnd, Msg, wParam, lParam, trampoline_DefWindowProcA);
}

static LRESULT WINAPI Hooked_DefWindowProcW(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	return Hooked_DefWindowProc(hWnd, Msg, wParam, lParam, trampoline_DefWindowProcW);
}


int InstallHookLate(HINSTANCE module, char *func, void **trampoline, void *hook)
{
	SIZE_T hook_id;
	DWORD dwOsErr;
	void *fnOrig;

	// Early exit with error so the caller doesn't need to explicitly deal
	// with errors getting the module handle:
	if (!module)
		return 1;

	fnOrig = NktHookLibHelpers::GetProcedureAddress(module, func);
	if (fnOrig == NULL) {
		LogInfo("Failed to get address of %s\n", func);
		return 1;
	}

	dwOsErr = cHookMgr.Hook(&hook_id, trampoline, fnOrig, hook);
	if (dwOsErr) {
		LogInfo("Failed to hook %s: 0x%x\n", func, dwOsErr);
		return 1;
	}

	return 0;
}

//from GeDoSaTo: https://github.com/PeterTh/gedosato
// Display Changing /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	template<typename DEVM>
	void fixDevMode(DEVM* lpDevMode) {
		if (lpDevMode->dmPelsWidth == G->GAME_INTERNAL_WIDTH() && lpDevMode->dmPelsHeight == G->GAME_INTERNAL_HEIGHT()) {
			LogDebug(" -> Overriding\n");
			if (G->SCREEN_WIDTH != -1)
				lpDevMode->dmPelsWidth = G->SCREEN_WIDTH;
			if (G->SCREEN_HEIGHT != -1)
				lpDevMode->dmPelsHeight = G->SCREEN_HEIGHT;
			if (G->SCREEN_REFRESH != -1)
				lpDevMode->dmDisplayFrequency = G->SCREEN_REFRESH;
		}
	}
}

GENERATE_INTERCEPT_HEADER(ChangeDisplaySettingsExA, LONG, WINAPI, _In_opt_ LPCSTR lpszDeviceName, _In_opt_ DEVMODEA* lpDevMode, _Reserved_ HWND hwnd, _In_ DWORD dwflags, _In_opt_ LPVOID lParam) {
	LogDebug("ChangeDisplaySettingsExA\n");
	if (!G->adjust_display_settings || !(G->SCREEN_UPSCALING > 0)) return TrueChangeDisplaySettingsExA(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
	if (lpDevMode == NULL) return TrueChangeDisplaySettingsExA(lpszDeviceName, NULL, hwnd, dwflags, lParam);
	DEVMODEA copy = *lpDevMode;
	fixDevMode(&copy);
	return TrueChangeDisplaySettingsExA(lpszDeviceName, &copy, hwnd, dwflags, lParam);
}
GENERATE_INTERCEPT_HEADER(ChangeDisplaySettingsExW, LONG, WINAPI, _In_opt_ LPCWSTR lpszDeviceName, _In_opt_ DEVMODEW* lpDevMode, _Reserved_ HWND hwnd, _In_ DWORD dwflags, _In_opt_ LPVOID lParam) {
	LogDebug("ChangeDisplaySettingsExW\n");
	if (!G->adjust_display_settings || !(G->SCREEN_UPSCALING > 0)) return TrueChangeDisplaySettingsExW(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
	if (lpDevMode == NULL) return TrueChangeDisplaySettingsExW(lpszDeviceName, NULL, hwnd, dwflags, lParam);
	DEVMODEW copy = *lpDevMode;
	fixDevMode(&copy);
	return TrueChangeDisplaySettingsExW(lpszDeviceName, &copy, hwnd, dwflags, lParam);
}
// System Metrics ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* SystemMetricToString(int metric) {
	switch (metric) {
	case SM_CXSCREEN: return "SM_CXSCREEN";
	case SM_CYSCREEN: return "SM_CYSCREEN";
	case SM_CXVSCROLL: return "SM_CXVSCROLL";
	case SM_CYHSCROLL: return "SM_CYHSCROLL";
	case SM_CYCAPTION: return "SM_CYCAPTION";
	case SM_CXBORDER: return "SM_CXBORDER";
	case SM_CYBORDER: return "SM_CYBORDER";
	case SM_CXDLGFRAME: return "SM_CXDLGFRAME";
	case SM_CYDLGFRAME: return "SM_CYDLGFRAME";
	case SM_CYVTHUMB: return "SM_CYVTHUMB";
	case SM_CXHTHUMB: return "SM_CXHTHUMB";
	case SM_CXICON: return "SM_CXICON";
	case SM_CYICON: return "SM_CYICON";
	case SM_CXCURSOR: return "SM_CXCURSOR";
	case SM_CYCURSOR: return "SM_CYCURSOR";
	case SM_CYMENU: return "SM_CYMENU";
	case SM_CXFULLSCREEN: return "SM_CXFULLSCREEN";
	case SM_CYFULLSCREEN: return "SM_CYFULLSCREEN";
	case SM_CYKANJIWINDOW: return "SM_CYKANJIWINDOW";
	case SM_MOUSEPRESENT: return "SM_MOUSEPRESENT";
	case SM_CYVSCROLL: return "SM_CYVSCROLL";
	case SM_CXHSCROLL: return "SM_CXHSCROLL";
	case SM_DEBUG: return "SM_DEBUG";
	case SM_SWAPBUTTON: return "SM_SWAPBUTTON";
	case SM_RESERVED1: return "SM_RESERVED1";
	case SM_RESERVED2: return "SM_RESERVED2";
	case SM_RESERVED3: return "SM_RESERVED3";
	case SM_RESERVED4: return "SM_RESERVED4";
	case SM_CXMIN: return "SM_CXMIN";
	case SM_CYMIN: return "SM_CYMIN";
	case SM_CXSIZE: return "SM_CXSIZE";
	case SM_CYSIZE: return "SM_CYSIZE";
	case SM_CXFRAME: return "SM_CXFRAME";
	case SM_CYFRAME: return "SM_CYFRAME";
	case SM_CXMINTRACK: return "SM_CXMINTRACK";
	case SM_CYMINTRACK: return "SM_CYMINTRACK";
	case SM_CXDOUBLECLK: return "SM_CXDOUBLECLK";
	case SM_CYDOUBLECLK: return "SM_CYDOUBLECLK";
	case SM_CXICONSPACING: return "SM_CXICONSPACING";
	case SM_CYICONSPACING: return "SM_CYICONSPACING";
	case SM_MENUDROPALIGNMENT: return "SM_MENUDROPALIGNMENT";
	case SM_PENWINDOWS: return "SM_PENWINDOWS";
	case SM_DBCSENABLED: return "SM_DBCSENABLED";
	case SM_CMOUSEBUTTONS: return "SM_CMOUSEBUTTONS";
	case SM_SECURE: return "SM_SECURE";
	case SM_CXEDGE: return "SM_CXEDGE";
	case SM_CYEDGE: return "SM_CYEDGE";
	case SM_CXMINSPACING: return "SM_CXMINSPACING";
	case SM_CYMINSPACING: return "SM_CYMINSPACING";
	case SM_CXSMICON: return "SM_CXSMICON";
	case SM_CYSMICON: return "SM_CYSMICON";
	case SM_CYSMCAPTION: return "SM_CYSMCAPTION";
	case SM_CXSMSIZE: return "SM_CXSMSIZE";
	case SM_CYSMSIZE: return "SM_CYSMSIZE";
	case SM_CXMENUSIZE: return "SM_CXMENUSIZE";
	case SM_CYMENUSIZE: return "SM_CYMENUSIZE";
	case SM_ARRANGE: return "SM_ARRANGE";
	case SM_CXMINIMIZED: return "SM_CXMINIMIZED";
	case SM_CYMINIMIZED: return "SM_CYMINIMIZED";
	case SM_CXMAXTRACK: return "SM_CXMAXTRACK";
	case SM_CYMAXTRACK: return "SM_CYMAXTRACK";
	case SM_CXMAXIMIZED: return "SM_CXMAXIMIZED";
	case SM_CYMAXIMIZED: return "SM_CYMAXIMIZED";
	case SM_NETWORK: return "SM_NETWORK";
	case SM_CLEANBOOT: return "SM_CLEANBOOT";
	case SM_CXDRAG: return "SM_CXDRAG";
	case SM_CYDRAG: return "SM_CYDRAG";
	case SM_SHOWSOUNDS: return "SM_SHOWSOUNDS";
	case SM_CXMENUCHECK: return "SM_CXMENUCHECK";
	case SM_CYMENUCHECK: return "SM_CYMENUCHECK";
	case SM_SLOWMACHINE: return "SM_SLOWMACHINE";
	case SM_MIDEASTENABLED: return "SM_MIDEASTENABLED";
	case SM_MOUSEWHEELPRESENT: return "SM_MOUSEWHEELPRESENT";
	case SM_XVIRTUALSCREEN: return "SM_XVIRTUALSCREEN";
	case SM_YVIRTUALSCREEN: return "SM_YVIRTUALSCREEN";
	case SM_CXVIRTUALSCREEN: return "SM_CXVIRTUALSCREEN";
	case SM_CYVIRTUALSCREEN: return "SM_CYVIRTUALSCREEN";
	case SM_CMONITORS: return "SM_CMONITORS";
	case SM_SAMEDISPLAYFORMAT: return "SM_SAMEDISPLAYFORMAT";
	case SM_IMMENABLED: return "SM_IMMENABLED";
	case SM_CXFOCUSBORDER: return "SM_CXFOCUSBORDER";
	case SM_CYFOCUSBORDER: return "SM_CYFOCUSBORDER";
	case SM_TABLETPC: return "SM_TABLETPC";
	case SM_MEDIACENTER: return "SM_MEDIACENTER";
	case SM_STARTER: return "SM_STARTER";
	case SM_SERVERR2: return "SM_SERVERR2";
	case SM_REMOTESESSION: return "SM_REMOTESESSION";
	case SM_SHUTTINGDOWN: return "SM_SHUTTINGDOWN";
	}
	return "Unknown Metric!";
}
GENERATE_INTERCEPT_HEADER(GetSystemMetrics, int, WINAPI, _In_ int nIndex) {
	LogDebug("DetouredGetSystemMetrics %d - %s\n", nIndex, SystemMetricToString(nIndex));
	int ret = TrueGetSystemMetrics(nIndex);
	if (G->adjust_system_metrics && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		switch (nIndex) {
		case SM_CXSCREEN:
			ret = G->GAME_INTERNAL_WIDTH();
			break;
		case SM_CYSCREEN:
			ret = G->GAME_INTERNAL_HEIGHT();
			break;
		case SM_CXVIRTUALSCREEN:
			ret = G->GAME_INTERNAL_WIDTH();
			break;
		case SM_CYVIRTUALSCREEN:
			ret = G->GAME_INTERNAL_HEIGHT();
			break;
		}
	}
	LogDebug(" -> %d\n", ret);
	return ret;
}

namespace {
	void adjustMonitorInfo(LPMONITORINFO lpmi) {
		lpmi->rcMonitor.right = lpmi->rcMonitor.left + G->GAME_INTERNAL_WIDTH();
		lpmi->rcMonitor.bottom = lpmi->rcMonitor.top + G->GAME_INTERNAL_HEIGHT();
	}
}

GENERATE_INTERCEPT_HEADER(GetMonitorInfoA, BOOL, WINAPI, _In_ HMONITOR hMonitor, _Inout_ LPMONITORINFO lpmi) {
	LogDebug("DetouredGetMonitorInfoA %p\n", hMonitor);
	BOOL ret = TrueGetMonitorInfoA(hMonitor, lpmi);
	if (G->adjust_monitor_info && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
		adjustMonitorInfo(lpmi);
	return ret;
}
GENERATE_INTERCEPT_HEADER(GetMonitorInfoW, BOOL, WINAPI, _In_ HMONITOR hMonitor, _Inout_ LPMONITORINFO lpmi) {
	LogDebug("DetouredGetMonitorInfoW %p\n", hMonitor);
	BOOL ret = TrueGetMonitorInfoW(hMonitor, lpmi);
	if (G->adjust_monitor_info && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
		adjustMonitorInfo(lpmi);
	return ret;
}
// Mouse stuff //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GENERATE_INTERCEPT_HEADER(ClipCursor, BOOL, WINAPI, __in_opt CONST RECT *lpRect) {
	LogDebug("ClipCursor\n");
	if (G->adjust_clip_cursor && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> Overriding\n");
		return TrueClipCursor(NULL);
	}
	return TrueClipCursor(lpRect);
}

GENERATE_INTERCEPT_HEADER(WindowFromPoint, HWND, WINAPI, _In_ POINT Point) {
	LogDebug("DetouredWindowFromPointA\n");
	if (G->adjust_window_from_point && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		LogDebug("-> Adjusting position\n");
		Point.x = Point.x * G->SCREEN_WIDTH / G->GAME_INTERNAL_WIDTH();
		Point.y = Point.y * G->SCREEN_HEIGHT / G->GAME_INTERNAL_HEIGHT();
	}
	return TrueWindowFromPoint(Point);
}
// Messages /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

GENERATE_INTERCEPT_HEADER(PeekMessageA, BOOL, WINAPI, _Out_ LPMSG lpMsg, _In_opt_ HWND hWnd, _In_ UINT wMsgFilterMin, _In_ UINT wMsgFilterMax, _In_ UINT wRemoveMsg) {
	LogDebug("DetouredPeekMessageA\n");
	BOOL ret = TruePeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
	if (ret && G->adjust_message_pt && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		LogDebug("-> Adjusting position\n");
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_WIDTH() / G->SCREEN_WIDTH;
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_HEIGHT() / G->SCREEN_HEIGHT;
	}
	return ret;
}
GENERATE_INTERCEPT_HEADER(GetMessageA, BOOL, WINAPI, _Out_ LPMSG lpMsg, _In_opt_ HWND hWnd, _In_ UINT wMsgFilterMin, _In_ UINT wMsgFilterMax) {
	LogDebug("DetouredGetMessageA\n");
	BOOL ret = TrueGetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
	if (ret && G->adjust_message_pt && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		LogDebug("-> Adjusting position\n");
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_WIDTH() / G->SCREEN_WIDTH;
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_HEIGHT() / G->SCREEN_HEIGHT;
	}
	return ret;
}

GENERATE_INTERCEPT_HEADER(GetMessagePos, DWORD, WINAPI) {
	LogDebug("DetouredGetMessagePos\n");
	DWORD ret = TrueGetMessagePos();
	return ret;
}

// WindowProc ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* WindowLongOffsetToString(int nIndex) {
	switch (nIndex) {
	case GWLP_WNDPROC: return "GWL_WNDPROC";
	case GWLP_HINSTANCE: return "GWL_HINSTANCE";
	case GWLP_HWNDPARENT: return "GWL_HWNDPARENT";
	case GWL_STYLE: return "GWL_STYLE";
	case GWL_EXSTYLE: return "GWL_EXSTYLE";
	case GWLP_USERDATA: return "GWL_USERDATA";
	case GWLP_ID: return "GWL_ID";
	}
	return "Unknown Offset!";
}
#pragma region WindowProc
namespace {
	std::map<HWND, WNDPROC> prevWndProcs;
	LRESULT CALLBACK InterceptWindowProc(_In_  HWND hwnd, _In_  UINT uMsg, _In_  WPARAM wParam, _In_  LPARAM lParam) {
		LogDebug("InterceptWindowProc hwnd: %p\n", hwnd);
		if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) {
			POINTS p = MAKEPOINTS(lParam);
			p.x = p.x * (SHORT)G->GAME_INTERNAL_WIDTH() / G->SCREEN_WIDTH;
			p.y = p.y * (SHORT)G->GAME_INTERNAL_HEIGHT() / G->SCREEN_HEIGHT;
			return CallWindowProc(prevWndProcs[hwnd], hwnd, uMsg, wParam, MAKELPARAM(p.x, p.y));
		}
		LogDebug(" -> calling original: %p\n", prevWndProcs[hwnd]);
		LRESULT res = CallWindowProc(prevWndProcs[hwnd], hwnd, uMsg, wParam, lParam);
		return res;
	}
}
#ifndef _WIN64
GENERATE_INTERCEPT_HEADER(GetWindowLongA, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongA hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongA(hWnd, nIndex);
}
GENERATE_INTERCEPT_HEADER(GetWindowLongW, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongW hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongW(hWnd, nIndex);
}

GENERATE_INTERCEPT_HEADER(SetWindowLongA, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG dwNewLong) {
	LogDebug("DetouredSetWindowLongA hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG ret = TrueSetWindowLongA(hWnd, nIndex, dwNewLong);
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongA(hWnd, GWL_WNDPROC, (LONG)&InterceptWindowProc);
	}
	return ret;
}
GENERATE_INTERCEPT_HEADER(SetWindowLongW, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG dwNewLong) {
	LogDebug("DetouredSetWindowLongW hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG ret = TrueSetWindowLongW(hWnd, nIndex, dwNewLong);
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongW(hWnd, GWL_WNDPROC, (LONG)&InterceptWindowProc);
	}
	return ret;
}
#else
GENERATE_INTERCEPT_HEADER(GetWindowLongPtrA, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongA hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWLP_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG_PTR)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongPtrA(hWnd, nIndex);
}
GENERATE_INTERCEPT_HEADER(GetWindowLongPtrW, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongW hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWLP_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG_PTR)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongPtrW(hWnd, nIndex);
}

GENERATE_INTERCEPT_HEADER(SetWindowLongPtrA, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG_PTR dwNewLong) {
	LogDebug("DetouredSetWindowLongA hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG_PTR ret = TrueSetWindowLongPtrA(hWnd, nIndex, dwNewLong);
	if (nIndex == GWLP_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)&InterceptWindowProc);
	}
	return ret;
}
GENERATE_INTERCEPT_HEADER(SetWindowLongPtrW, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG_PTR dwNewLong) {
	LogDebug("DetouredSetWindowLongW hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG_PTR ret = TrueSetWindowLongPtrW(hWnd, nIndex, dwNewLong);
	if (nIndex == GWLP_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)&InterceptWindowProc);
	}
	return ret;
}
#endif // _WIN64

#pragma endregion
//end GeDoSaTo: https://github.com/PeterTh/gedosato

void InstallMouseHooks(bool hide)
{
	HINSTANCE hUser32;
	static bool hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	//InstallCreateWindowHook();
	// Init our handle to the current cursor now before installing the
	// hooks, and from now on it will be kept up to date from SetCursor:
	current_cursor = GetCursor();
	if (hide)
		SetCursor(InvisibleCursor());

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHookLate(hUser32, "SetCursor", (void**)&trampoline_SetCursor, Hooked_SetCursor);
	fail |= InstallHookLate(hUser32, "GetCursor", (void**)&trampoline_GetCursor, Hooked_GetCursor);

	//fail |= InstallHookLate(hUser32, "ShowCursor", (void**)&trampoline_ShowCursor, Hooked_ShowCursor);

	fail |= InstallHookLate(hUser32, "GetCursorInfo", (void**)&trampoline_GetCursorInfo, Hooked_GetCursorInfo);
	fail |= InstallHookLate(hUser32, "DefWindowProcA", (void**)&trampoline_DefWindowProcA, Hooked_DefWindowProcA);
	fail |= InstallHookLate(hUser32, "DefWindowProcW", (void**)&trampoline_DefWindowProcW, Hooked_DefWindowProcW);

	fail |= InstallHookLate(hUser32, "SetCursorPos", (void**)&trampoline_SetCursorPos, Hooked_SetCursorPos);
	fail |= InstallHookLate(hUser32, "GetCursorPos", (void**)&trampoline_GetCursorPos, Hooked_GetCursorPos);

	fail |= InstallHookLate(hUser32, "ScreenToClient", (void**)&trampoline_ScreenToClient, Hooked_ScreenToClient);
	fail |= InstallHookLate(hUser32, "MapWindowPoints", (void**)&trampoline_MapWindowPoints, Hooked_MapWindowPoints);
	fail |= InstallHookLate(hUser32, "WindowFromPoint", (void**)&TrueWindowFromPoint, DetouredWindowFromPoint);

	fail |= InstallHookLate(hUser32, "GetClientRect", (void**)&trampoline_GetClientRect, Hooked_GetClientRect);
	fail |= InstallHookLate(hUser32, "GetWindowRect", (void**)&trampoline_GetWindowRect, Hooked_GetWindowRect);
	fail |= InstallHookLate(hUser32, "ClipCursor", (void**)&TrueClipCursor, DetouredClipCursor);

#ifndef _WIN64
	fail |= InstallHookLate(hUser32, "SetWindowLongA", (void**)&TrueSetWindowLongA, DetouredSetWindowLongA);
	fail |= InstallHookLate(hUser32, "SetWindowLongW", (void**)&TrueSetWindowLongW, DetouredSetWindowLongW);
	fail |= InstallHookLate(hUser32, "GetWindowLongA", (void**)&TrueGetWindowLongA, DetouredGetWindowLongA);
	fail |= InstallHookLate(hUser32, "GetWindowLongW", (void**)&TrueGetWindowLongW, DetouredGetWindowLongW);
#else
	fail |= InstallHookLate(hUser32, "SetWindowLongPtrA", (void**)&TrueSetWindowLongPtrA, DetouredSetWindowLongPtrA);
	fail |= InstallHookLate(hUser32, "SetWindowLongPtrW", (void**)&TrueSetWindowLongPtrW, DetouredSetWindowLongPtrW);
	fail |= InstallHookLate(hUser32, "GetWindowLongPtrA", (void**)&TrueGetWindowLongPtrA, DetouredGetWindowLongPtrA);
	fail |= InstallHookLate(hUser32, "GetWindowLongPtrW", (void**)&TrueGetWindowLongPtrW, DetouredGetWindowLongPtrW);
#endif
	fail |= InstallHookLate(hUser32, "PeekMessageA", (void**)&TruePeekMessageA, DetouredPeekMessageA);
	fail |= InstallHookLate(hUser32, "GetMessageA", (void**)&TrueGetMessageA, DetouredGetMessageA);
	fail |= InstallHookLate(hUser32, "GetMessagePos", (void**)&TrueGetMessagePos, DetouredGetMessagePos);

	fail |= InstallHookLate(hUser32, "ChangeDisplaySettingsExA", (void**)&TrueChangeDisplaySettingsExA, DetouredChangeDisplaySettingsExA);
	fail |= InstallHookLate(hUser32, "ChangeDisplaySettingsExW", (void**)&TrueChangeDisplaySettingsExW, DetouredChangeDisplaySettingsExW);

	fail |= InstallHookLate(hUser32, "GetSystemMetrics", (void**)&TrueGetSystemMetrics, DetouredGetSystemMetrics);

	fail |= InstallHookLate(hUser32, "GetMonitorInfoA", (void**)&TrueGetMonitorInfoA, DetouredGetMonitorInfoA);
	fail |= InstallHookLate(hUser32, "GetMonitorInfoW", (void**)&TrueGetMonitorInfoW, DetouredGetMonitorInfoW);

	if (fail) {
		LogInfo("Failed to hook mouse cursor functions - hide_cursor will not work\n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked mouse cursor functions for hide_cursor\n");
}

static BOOL(WINAPI *trampoline_SetWindowPos)(_In_ HWND hWnd, _In_opt_ HWND hWndInsertAfter,
		_In_ int X, _In_ int Y, _In_ int cx, _In_ int cy, _In_ UINT uFlags)
	= SetWindowPos;

static BOOL WINAPI Hooked_SetWindowPos(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags)
{
	LogDebug("  Hooked_SetWindowPos called, width = %d, height = %d \n", cx, cy);

	if (G->SCREEN_UPSCALING != 0) {
		if (cx != 0 && cy != 0) {
			cx = G->SCREEN_WIDTH;
			cy = G->SCREEN_HEIGHT;
			X = 0;
			Y = 0;
		}
	}
	else if (G->SCREEN_FULLSCREEN == 2) {
		// Do nothing - passing this call through could change the game
		// to a borderless window. Needed for The Witness.
		return true;
	}

	return trampoline_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

void InstallSetWindowPosHook()
{
	HINSTANCE hUser32;
	static bool hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHookLate(hUser32, "SetWindowPos", (void**)&trampoline_SetWindowPos, Hooked_SetWindowPos);

	if (fail) {
		LogInfo("Failed to hook SetWindowPos for full_screen=2\n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked SetWindowPos for full_screen=2\n");
	return;
}

static HWND(WINAPI *trampoline_CreateWindowEx)(_In_     DWORD     dwExStyle,
	_In_opt_ LPCTSTR   lpClassName,
	_In_opt_ LPCTSTR   lpWindowName,
	_In_     DWORD     dwStyle,
	_In_     int       x,
	_In_     int       y,
	_In_     int       nWidth,
	_In_     int       nHeight,
	_In_opt_ HWND      hWndParent,
	_In_opt_ HMENU     hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID    lpParam) = CreateWindowExW;

static HWND WINAPI Hooked_CreateWindowEx(
	_In_     DWORD     dwExStyle,
	_In_opt_ LPCTSTR   lpClassName,
	_In_opt_ LPCTSTR   lpWindowName,
	_In_     DWORD     dwStyle,
	_In_     int       x,
	_In_     int       y,
	_In_     int       nWidth,
	_In_     int       nHeight,
	_In_opt_ HWND      hWndParent,
	_In_opt_ HMENU     hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID    lpParam)
{
	LogDebug("  Hooked_CreateWindowEx called, width = %d, height = %d \n", nWidth, nHeight);
	return trampoline_CreateWindowEx(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}
//
void InstallCreateWindowHook()
{
	HINSTANCE hUser32;
	static bool create_window_hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (create_window_hook_installed)
		return;
	create_window_hook_installed = true;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHookLate(hUser32, "CreateWindowExW", (void**)&trampoline_CreateWindowEx, Hooked_CreateWindowEx);
	if (fail) {
		LogInfo("Failed to hook create window \n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked create window \n");
}
