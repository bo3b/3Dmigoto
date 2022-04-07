#pragma once

#include <Windows.h>

// These functions will bypass our hooks *if* the option to do so has been enabled:
BOOL WINAPI cursor_upscaling_bypass_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect);
BOOL WINAPI cursor_upscaling_bypass_GetCursorInfo(_Inout_ PCURSORINFO pci);
BOOL WINAPI cursor_upscaling_bypass_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint);

int  install_hook_late(HINSTANCE module, char* func, void** trampoline, void* hook);
void install_mouse_hooks(bool hide);
