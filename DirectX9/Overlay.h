#pragma once
#include <memory>
#include "D3DFont.h"
#include <gdiplus.h>
#include <DirectXMath.h>
#include "CommandList.h"

// Forward references required because of circular references from the
// other 'Hacker' objects.
#include <d3d9.h>
namespace D3D9Wrapper {
	class IDirect3DDevice9;
	class IDirect3DShader9;
}

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
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice;
	DirectX::XMUINT2 mResolution;
	::IDirect3DStateBlock9 *saved_state_block;
	void SaveState();
	void RestoreState();
	HRESULT InitDrawState(::IDirect3DSwapChain9 *pSwapChain = NULL);
	void DrawShaderInfoLine(wchar_t *type, UINT64 selectedShader, float *y);
	void DrawShaderInfoLine(wchar_t *type, D3D9Wrapper::IDirect3DShader9 *selectedShader, float *y);
	void DrawShaderInfoLine(wchar_t *osdString, float *y);
	void DrawShaderInfoLines(float *y);
	void DrawNotices(float *y);
	void DrawProfiling(float *y);
	void DrawRectangle(float x, float y, float w, float h, float r, float g, float b, float opacity);
	void DrawOutlinedString(CD3DFont *font, wchar_t const *text, ::D3DXVECTOR2 const &position, DWORD color);

	ULONG migotoResourceCount;

public:
	Overlay(D3D9Wrapper::IDirect3DDevice9 *pDevice);
	~Overlay();
	void DrawOverlay(CachedStereoValues *cachedStereoValues = NULL, ::IDirect3DSwapChain9 *pSwapChain = NULL);
	void Resize(UINT Width, UINT Height);
	ULONG ReferenceCount();

	std::unique_ptr<CD3DFont> mFont;
	std::unique_ptr<CD3DFont> mFontNotifications;
	std::unique_ptr<CD3DFont> mFontProfiling;
};

void ClearNotices();
void LogOverlayW(LogLevel level, wchar_t *fmt, ...);
void LogOverlay(LogLevel level, char *fmt, ...);
