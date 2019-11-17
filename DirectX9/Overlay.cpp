// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"
#include "Main.h"
#include <DirectXColors.h>
#include <StrSafe.h>
#include <comdef.h>
#define MAX_SIMULTANEOUS_NOTICES 10
static std::vector<OverlayNotice> notices[NUM_LOG_LEVELS];
static bool has_notice = false;
static unsigned notice_cleared_frame = 0;

struct LogLevelParams {
	DWORD colour;
	DWORD duration;
	bool hide_in_release;
	std::unique_ptr<CD3DFont> Overlay::*font;
};

struct LogLevelParams log_levels[] = {
	{ Gdiplus::Color::Red,       20000, false, &Overlay::mFontNotifications }, // DIRE
{ Gdiplus::Color::OrangeRed, 10000, false, &Overlay::mFontNotifications }, // WARNING
{ Gdiplus::Color::OrangeRed, 10000, false, &Overlay::mFontProfiling }, // WARNING_MONOSPACE
{ Gdiplus::Color::Orange,     5000,  true, &Overlay::mFontNotifications }, // NOTICE
{ Gdiplus::Color::LimeGreen,  2000,  true, &Overlay::mFontNotifications }, // INFO
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
const int maxstring = 1024;


Overlay::Overlay(D3D9Wrapper::IDirect3DDevice9 *pDevice)
{
	migotoResourceCount = 0;
	LogInfo("Overlay::Overlay created for HackerDevice: %p\n", pDevice);

	mHackerDevice = pDevice;
	saved_state_block = NULL;

	mFont.reset(new CD3DFont("Courier", 10, D3DFONT_BOLD));
	mFontNotifications.reset(new CD3DFont("Arial", 10, D3DFONT_BOLD));
	mFontProfiling.reset(new CD3DFont("Courier", 5, D3DFONT_BOLD));

	mFont->InitializeDeviceObjects(mHackerDevice->GetD3D9Device());
	mFontNotifications->InitializeDeviceObjects(mHackerDevice->GetD3D9Device());
	mFontProfiling->InitializeDeviceObjects(mHackerDevice->GetD3D9Device());

	mFont->RestoreDeviceObjects();
	mFontNotifications->RestoreDeviceObjects();
	mFontProfiling->RestoreDeviceObjects();
	if (mFont.get())
		migotoResourceCount += 3;
	if (mFontNotifications.get())
		migotoResourceCount += 3;
	if (mFontProfiling.get())
		migotoResourceCount += 3;
}

Overlay::~Overlay()
{
	if (saved_state_block) {
		--migotoResourceCount;
		saved_state_block->Release();
		saved_state_block = NULL;
	}

	if (mFont.get())
		migotoResourceCount -= 3;
	if (mFontNotifications.get())
		migotoResourceCount -= 3;
	if (mFontProfiling.get())
		migotoResourceCount -= 3;
}

void Overlay::SaveState()
{

	if (saved_state_block == NULL) {
		mHackerDevice->GetD3D9Device()->CreateStateBlock(::D3DSBT_ALL, &saved_state_block);
		++migotoResourceCount;
	}
	saved_state_block->Capture();
}

void Overlay::RestoreState()
{
	if (saved_state_block != NULL)
	{
		saved_state_block->Apply();
	}
}

// We can't trust the game to have a proper drawing environment for DirectXTK.
//
// For two games we know of (Batman Arkham Knight and Project Cars) we were not
// getting an overlay, because apparently the rendertarget was left in an odd
// state.  This adds an init to be certain that the rendertarget is the backbuffer
// so that the overlay is drawn.
HRESULT Overlay::InitDrawState(::IDirect3DSwapChain9 *pSwapChain)
{

	HRESULT hr;

	// Get back buffer
	::IDirect3DSurface9 *back_buffer = NULL;
	if (pSwapChain) {
		hr = pSwapChain->GetBackBuffer(0, ::D3DBACKBUFFER_TYPE_MONO, &back_buffer);
		if (FAILED(hr))
			return hr;
	}
	else {
		hr = mHackerDevice->GetD3D9Device()->GetBackBuffer(0, 0, ::D3DBACKBUFFER_TYPE_MONO, &back_buffer);
		if (FAILED(hr))
			return hr;
	}

	// By doing this every frame, we are always up to date with correct size,
	// and we need the address of the BackBuffer anyway, so this is low cost.
	::D3DSURFACE_DESC description;
	back_buffer->GetDesc(&description);
	mResolution = DirectX::XMUINT2(description.Width, description.Height);
	hr = mHackerDevice->GetD3D9Device()->SetRenderTarget(0, back_buffer);
	if (FAILED(hr))
		return hr;
	::D3DVIEWPORT9 viewport;
	hr = mHackerDevice->GetD3D9Device()->GetViewport(&viewport);
	if (FAILED(hr))
		return hr;
	if (viewport.Width != (mResolution.x) || viewport.Height != (mResolution.y))
	{
		viewport.X = 0;
		viewport.Y = 0;
		viewport.Width = (mResolution.x);
		viewport.Height = (mResolution.y);
		viewport.MinZ = 0.0f;
		viewport.MaxZ = 1.0f;
	}
	// Release temporary resources
	back_buffer->Release();
	return S_OK;
}
// -----------------------------------------------------------------------------
#define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
struct CUSTOMVERTEX
{
	FLOAT x, y, z, rhw;    // from the D3DFVF_XYZRHW flag
	D3DCOLOR colour;    // from the D3DFVF_DIFFUSE flag
};
void Overlay::DrawRectangle(float x, float y, float w, float h, float r, float g, float b, float opacity)
{
	int iopacity, ir, ig, ib;
	iopacity = (int)(opacity * 255.0f);
	ir = (int)(r * 255.0f);
	ig = (int)(g * 255.0f);
	ib = (int)(b * 255.0f);
	D3DCOLOR colour = D3DCOLOR_ARGB(iopacity, ir, ig, ib);
	CUSTOMVERTEX quad[] = {
		{ x,  y, 1.0f, 1.0f,  colour },
	{ x + w,  y, 1.0f, 1.0f,  colour },
	{ x, y + h, 1.0f, 1.0f,  colour },
	{ x + w, y + h, 1.0f, 1.0f, colour },
	};
	mHackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ALPHABLENDENABLE, TRUE);
	mHackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_BLENDOP, ::D3DBLENDOP_ADD);
	mHackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_SRCBLEND, ::D3DBLEND_SRCALPHA);
	mHackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_DESTBLEND, ::D3DBLEND_INVSRCALPHA);
	mHackerDevice->GetD3D9Device()->SetFVF(CUSTOMFVF);
	mHackerDevice->GetD3D9Device()->DrawPrimitiveUP(::D3DPT_TRIANGLESTRIP, 2, quad, sizeof(CUSTOMVERTEX));
}

void Overlay::DrawOutlinedString(CD3DFont *font, wchar_t const *text, ::D3DXVECTOR2 const &position, DWORD color)
{
	char c[MAX_PATH];
	wcstombs(c, text, MAX_PATH);

	font->DrawTextW(position.x - 1, position.y, Gdiplus::Color::Black, c, 0, 0);
	font->DrawTextW(position.x + 1, position.y, Gdiplus::Color::Black, c, 0, 0);
	font->DrawTextW(position.x, position.y - 1, Gdiplus::Color::Black, c, 0, 0);
	font->DrawTextW(position.x - 1, position.y + 1, Gdiplus::Color::Black, c, 0, 0);
	font->DrawTextW(position.x, position.y, color, c, 0, 0);
}
// The active shader will show where we are in each list. / 0 / 0 will mean that we are not
// actively searching.

static void AppendShaderText(wchar_t *fullLine, wchar_t *type, int pos, size_t size)
{
	if (size == 0)
		return;

	// The position is zero based, so we'll make it +1 for the humans.
	if (++pos == 0)
		size = 0;

	wchar_t append[maxstring];
	swprintf_s(append, maxstring, L"%ls:%d/%Iu ", type, pos, size);
	wcscat_s(fullLine, maxstring, append);
}


// We also want to show the count of active vertex, pixel, compute, geometry, domain, hull
// shaders, that are active in the frame.  Any that have a zero count will be stripped, to
// keep it from being too busy looking.

static void CreateShaderCountString(wchar_t *counts)
{
	const wchar_t *marking_mode;
	wcscpy_s(counts, maxstring, L"");
	// The order here more or less follows how important these are for
	// shaderhacking. VS and PS are the absolute most important, CS is
	// pretty important, GS and DS show up from time to time and HS is not
	// important at all since we have never needed to fix one.
	AppendShaderText(counts, L"VS", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	AppendShaderText(counts, L"PS", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	if (G->mSelectedVertexBuffer != -1)
		AppendShaderText(counts, L"VB", G->mSelectedVertexBufferPos, G->mVisitedVertexBuffers.size());
	if (G->mSelectedIndexBuffer != -1)
		AppendShaderText(counts, L"IB", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	if (G->mSelectedRenderTarget != (D3D9Wrapper::IDirect3DSurface9 *)-1)
		AppendShaderText(counts, L"RT", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());

	marking_mode = lookup_enum_name(MarkingModeNames, G->marking_mode);
	if (marking_mode)
		wcscat_s(counts, maxstring, marking_mode);
}


// Need to convert from the current selection, mSelectedVertexShader as hash, and
// find the OriginalShaderInfo that matches.  This is a linear search instead of a
// hash lookup, because we don't have the ID3D11DeviceChild*.

static bool FindInfoText(wchar_t *info, D3D9Wrapper::IDirect3DShader9 *selectedShader)
{
	if (!selectedShader->originalShaderInfo.infoText.empty())
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
		wcsncat_s(info, maxstring, selectedShader->originalShaderInfo.infoText.c_str() + 2, _TRUNCATE);
		return true;
	}
	return false;
}


// This is for a line of text as info about the currently selected shader.
// The line is pulled out of the header of the HLSL text file, and can be
// anything. Since there can be multiple shaders selected, VS and PS and HS for
// example, we'll show one line for each, but only those that are present
// in ShaderFixes and have something other than a blank line at the top.

void Overlay::DrawShaderInfoLine(wchar_t *osdString, float *y) {
	SIZE strSize;
	::D3DXVECTOR2 textPosition;
	float x = 0;
	char c[MAX_PATH];
	wcstombs(c, osdString, MAX_PATH);

	mFont->GetTextExtent(c, &strSize);

	if (!G->verbose_overlay)
		x = max(float(mResolution.x - strSize.cx) / 2, float(0));

	textPosition = ::D3DXVECTOR2(x, 10 + ((*y)++ * (float)strSize.cy));
	mFont->DrawTextW(textPosition.x, textPosition.y, Gdiplus::Color::LimeGreen, c, 0, 0);
}

void Overlay::DrawShaderInfoLine(wchar_t *type, UINT64 selectedShader, float *y) {
	wchar_t osdString[maxstring];
	if (selectedShader == 0xffffffff || !G->verbose_overlay)
		return;

	swprintf_s(osdString, maxstring, L"%S %08llx", type, selectedShader);

	DrawShaderInfoLine(osdString, y);

}
void Overlay::DrawShaderInfoLine(wchar_t *type, D3D9Wrapper::IDirect3DShader9 *selectedShader, float *y)
{
	wchar_t osdString[maxstring];
	if (selectedShader->hash == -1)
		return;

	if (G->verbose_overlay)
		swprintf_s(osdString, maxstring, L"%S %016llx:", type, selectedShader->hash);
	else
		swprintf_s(osdString, maxstring, L"%S:", type);

	if (!FindInfoText(osdString, selectedShader) && !G->verbose_overlay)
		return;

	DrawShaderInfoLine(osdString, y);
}

void Overlay::DrawShaderInfoLines(float *y)
{
	// Order is the same as the pipeline... Not quite the same as the count
	// summary line, which is sorted by "the order in which we added them"
	// (which to be fair, is pretty much their order of importance for our
	// purposes). Since these only show up while hunting, it is better to
	// have them reflect the actual order that they are run in. The summary
	// line can stay in order of importance since it is always shown.
	DrawShaderInfoLine(L"VB", G->mSelectedVertexBuffer, y);
	DrawShaderInfoLine(L"IB", G->mSelectedIndexBuffer, y);
	DrawShaderInfoLine(L"VS", G->mSelectedVertexShader, y);
	DrawShaderInfoLine(L"PS", G->mSelectedPixelShader, y);
	// FIXME? This one is stored as a handle, not a hash:
	if (G->mSelectedRenderTarget != (D3D9Wrapper::IDirect3DSurface9 *)-1)
		DrawShaderInfoLine(L"RT", GetOrigResourceHash(G->mSelectedRenderTarget), y);
}


void Overlay::DrawNotices(float *y)
{
	std::vector<OverlayNotice>::iterator notice;
	DWORD time = GetTickCount();
	::D3DXVECTOR2 textPosition;
	SIZE strSize;
	int level, displayed = 0;

	EnterCriticalSection(&G->mCriticalSection);

	has_notice = false;
	for (level = 0; level < NUM_LOG_LEVELS; level++) {
		if (log_levels[level].hide_in_release && G->hunting == HUNTING_MODE_DISABLED)
			continue;

		for (notice = notices[level].begin(); notice != notices[level].end() && displayed < MAX_SIMULTANEOUS_NOTICES; ) {
			if (!notice->timestamp) {
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
			else if ((time - notice->timestamp) > log_levels[level].duration) {
				notice = notices[level].erase(notice);
				continue;
			}
			char c[MAX_PATH];
			wcstombs(c, notice->message.substr(0, (MAX_PATH  -1)).c_str(), MAX_PATH);
			(this->*log_levels[level].font)->GetTextExtent(c, &strSize);

			DrawRectangle(0, *y, (float)(strSize.cx + 3.0f), (float)strSize.cy, 0, 0, 0, 0.75);
			(this->*log_levels[level].font)->DrawTextW(0, *y, log_levels[level].colour, c, 0, 0);
			*y += strSize.cy + 5;

			has_notice = true;
			notice++;
			displayed++;
		}
	}

	LeaveCriticalSection(&G->mCriticalSection);
}

void Overlay::DrawProfiling(float *y)
{
	SIZE strSize;

	Profiling::update_txt();

	char c[MAX_PATH];
	wcstombs(c, Profiling::text.substr(0, (MAX_PATH - 1)).c_str(), MAX_PATH);

	mFontProfiling->GetTextExtent(c, &strSize);
	DrawRectangle(0, *y, (float)(strSize.cx + 3), (float)strSize.cy, 0, 0, 0, 0.75);
	mFontProfiling->DrawTextW(0, *y, Gdiplus::Color::Goldenrod, c, 0, 0);
}
// Create a string for display on the bottom edge of the screen, that contains the current
// stereo info of separation and convergence.
// Desired format: "Sep:85  Conv:4.5"

static void CreateStereoInfoString(D3D9Wrapper::IDirect3DDevice9 *hackerDevice, wchar_t *info, CachedStereoValues *cachedStereoValues)
{
	// Rather than draw graphic bars, this will just be numeric.  Because
	// convergence is essentially an arbitrary number.
	float separation, convergence;
	bool stereo = !!hackerDevice->mStereoHandle;
	if (stereo)
	{
		NvAPIOverride();
		GetStereoEnabled(cachedStereoValues, &stereo);
		if (stereo)
		{
			GetStereoActive(hackerDevice, cachedStereoValues, &stereo);
			if (stereo)
			{
				GetSeparation(hackerDevice, cachedStereoValues, &separation);
				GetConvergence(hackerDevice, cachedStereoValues, &convergence);
			}
		}
	}

	if (stereo)
		swprintf_s(info, maxstring, L"Sep:%.0f  Conv:%.2f", separation, convergence);
	else
		swprintf_s(info, maxstring, L"Stereo disabled");
}

void Overlay::Resize(UINT Width, UINT Height)
{
	mResolution.x = Width;
	mResolution.y = Height;
}

ULONG Overlay::ReferenceCount()
{
	UINT ref = migotoResourceCount;
	if (mFont.get()) {
		if (mFont->m_pStateBlockDrawText)
			ref++;
		if (mFont->m_pStateBlockSaved)
			ref++;
	}

	if (mFontNotifications.get()) {
		if (mFontNotifications->m_pStateBlockDrawText)
			ref++;
		if (mFontNotifications->m_pStateBlockSaved)
			ref++;
	}
	if (mFontProfiling.get()) {
		if (mFontProfiling->m_pStateBlockDrawText)
			ref++;
		if (mFontProfiling->m_pStateBlockSaved)
			ref++;
	}
	return ref;
}

void Overlay::DrawOverlay(CachedStereoValues *cachedStereoValues, ::IDirect3DSwapChain9 *pSwapChain)
{
	Profiling::State profiling_state;
	HRESULT hr;

	if (G->hunting != HUNTING_MODE_ENABLED && !has_notice && Profiling::mode == Profiling::Mode::NONE)
		return;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);
	// Since some games did not like having us change their drawing state from
	// SpriteBatch, we now save and restore all state information for the GPU
	// around our drawing.
	SaveState();
	{
		hr = InitDrawState(pSwapChain);
		if (FAILED(hr))
			goto restore_end;
		{
			wchar_t osdString[maxstring];
			SIZE strSize;
			::D3DXVECTOR2 textPosition;
			float y = 10.0f;

			if (G->hunting == HUNTING_MODE_ENABLED) {
				hr = mFont->BeginDrawing();
				if (FAILED(hr))
					goto restore_end;
				// Top of screen
				CreateShaderCountString(osdString);
				char c[MAX_PATH];
				wcstombs(c, osdString, MAX_PATH);

				hr = mFont->GetTextExtent(c, &strSize);
				if (FAILED(hr))
					goto drawing_end;
				textPosition = ::D3DXVECTOR2(float(mResolution.x - strSize.cx) / 2, y);
				DrawOutlinedString(mFont.get(), osdString, textPosition, Gdiplus::Color::LimeGreen);
				y += strSize.cy;

				DrawShaderInfoLines(&y);

				// Bottom of screen
				CreateStereoInfoString(mHackerDevice, osdString, cachedStereoValues);
				wcstombs(c, osdString, MAX_PATH);
				hr = mFont->GetTextExtent(c, &strSize);
				if (FAILED(hr))
					goto drawing_end;
				textPosition = ::D3DXVECTOR2(float(mResolution.x - strSize.cx) / 2, float(mResolution.y - strSize.cy - 10));
				DrawOutlinedString(mFont.get(), osdString, textPosition, Gdiplus::Color::LimeGreen);
			drawing_end:
				mFont->EndDrawing();
			}

			if (has_notice) {
				hr = mFontNotifications->BeginDrawing();
				if (FAILED(hr))
					goto restore_end;
				hr = mFontProfiling->BeginDrawing();
				if (FAILED(hr)) {
					mFontNotifications->EndDrawing();
					goto restore_end;
				}
				DrawNotices(&y);
				mFontNotifications->EndDrawing();
				if (Profiling::mode != Profiling::Mode::NONE)
					DrawProfiling(&y);
				mFontProfiling->EndDrawing();
			}
			else {
				hr = mFontProfiling->BeginDrawing();
				if (FAILED(hr))
					goto restore_end;
				if (Profiling::mode != Profiling::Mode::NONE)
					DrawProfiling(&y);
				mFontProfiling->EndDrawing();
			}
		}
	}
	restore_end:
		RestoreState();
		if (Profiling::mode == Profiling::Mode::SUMMARY)
			Profiling::end(&profiling_state, &Profiling::overlay_overhead);

}
OverlayNotice::OverlayNotice(std::wstring message) :
	message(message),
	timestamp(0)
{
}

void ClearNotices()
{
	int level;

	if (notice_cleared_frame == G->frame_no)
		return;

	EnterCriticalSection(&G->mCriticalSection);

	for (level = 0; level < NUM_LOG_LEVELS; level++)
		notices[level].clear();

	notice_cleared_frame = G->frame_no;
	has_notice = false;

	LeaveCriticalSection(&G->mCriticalSection);
}

void LogOverlayW(LogLevel level, wchar_t *fmt, ...)
{
	wchar_t msg[maxstring];
	va_list ap;

	va_start(ap, fmt);
	vLogInfoW(fmt, ap);

	// Using _vsnwprintf_s so we don't crash if the message is too long for
	// the buffer, and truncate it instead - unless we can automatically
	// wrap the message, which DirectXTK doesn't appear to support, who
	// cares if it gets cut off somewhere off screen anyway?
	_vsnwprintf_s(msg, maxstring, _TRUNCATE, fmt, ap);

	EnterCriticalSection(&G->mCriticalSection);

	notices[level].emplace_back(msg);
	has_notice = true;

	LeaveCriticalSection(&G->mCriticalSection);

	va_end(ap);
}

// ASCII version of the above. DirectXTK only understands wide strings, so we
// need to convert it to that, but we can't just convert the format and hand it
// to LogOverlayW, because that would reverse the meaning of %s and %S in the
// format string. Instead we do our own vLogInfo and _vsnprintf_s to handle the
// format string correctly and convert the result to a wide string.
void LogOverlay(LogLevel level, char *fmt, ...)
{
	char amsg[maxstring];
	wchar_t wmsg[maxstring];
	va_list ap;

	va_start(ap, fmt);
	vLogInfo(fmt, ap);

	if (!log_levels[level].hide_in_release || G->hunting) {
		// Using _vsnprintf_s so we don't crash if the message is too long for
		// the buffer, and truncate it instead - unless we can automatically
		// wrap the message, which DirectXTK doesn't appear to support, who
		// cares if it gets cut off somewhere off screen anyway?
		_vsnprintf_s(amsg, maxstring, _TRUNCATE, fmt, ap);
		mbstowcs(wmsg, amsg, maxstring);

		EnterCriticalSection(&G->mCriticalSection);

		notices[level].emplace_back(wmsg);
		has_notice = true;

		LeaveCriticalSection(&G->mCriticalSection);
	}

	va_end(ap);
}
