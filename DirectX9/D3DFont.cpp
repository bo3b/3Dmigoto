//-----------------------------------------------------------------------------
// File: D3DFont.cpp
//
// Desc: Texture-based font class
//
// Copyright (c) 1999-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


/*
* This file has been heavily modified to accommodate for new features.
*
*      This file has been modified to include various new features such as:
*          - Batch Drawing
*          - Inline Coloring
*          - Primitive Backgrounds
*          - Direct3D9
*
*
*      Credits:
*          - Atom0s
*          - Topblast
*          - Renkokuken (Sakuri)
*          - Direct3D9 SDK
*
*      Source:
*          https://www.unknowncheats.me/forum/d3d-tutorials-and-source/74839-modified-cd3dfont-d3d9-shadows-light-effect.html
*/

//#include "stdafx.h"
#include "D3DFont.h"

__inline BOOL IsCharNumeric(char tszChar)
{
	if ((!IsCharAlpha(tszChar)) && (IsCharAlphaNumeric(tszChar)))
	{
		return TRUE;
	}
	return FALSE;
}

//
// Color Coding Extra (Can be edited to alter the default colors.)
//
// A string must be given the flag of D3DFONT_COLORTABLE in order to use
// inline coloring. Once passed this flag, a string can be colored using:
//
// ^# where # is one of the numbers below.
//
DWORD GetCustomColor(char tszColor, BYTE bAlpha)
{
	switch (tszColor)
	{
	case ('0'): // White
		return D3DCOLOR_ARGB(bAlpha, 0xFF, 0xFF, 0xFF);
		break;
	case ('1'): // Red
		return D3DCOLOR_ARGB(bAlpha, 0xFF, 0x00, 0x00);
		break;
	case ('2'): // Green
		return D3DCOLOR_ARGB(bAlpha, 0x00, 0xFF, 0x00);
		break;
	case ('3'): // Blue
		return D3DCOLOR_ARGB(bAlpha, 0x00, 0x00, 0xFF);
		break;
	case ('4'): // Yellow
		return D3DCOLOR_ARGB(bAlpha, 0xFF, 0xFF, 0x00);
		break;
	case ('5'): // Purple
		return D3DCOLOR_ARGB(bAlpha, 0x66, 0x00, 0x99);
		break;
	case ('6'): // Pink
		return D3DCOLOR_ARGB(bAlpha, 0xFF, 0x14, 0x93);
		break;
	case ('7'): // Orange
		return D3DCOLOR_ARGB(bAlpha, 0xFF, 0xA5, 0x00);
		break;
	case ('8'): // Light blue
		return D3DCOLOR_ARGB(bAlpha, 0xAD, 0xD8, 0xE6);
		break;
	case ('9'): // Black
		return D3DCOLOR_ARGB(bAlpha, 0x00, 0x00, 0x00);
		break;
	}
	return D3DCOLOR_ARGB(bAlpha, 0xFF, 0xFF, 0xFF);
}

#define colorA(col) (BYTE( ( col >> 24 ) &0xFF ) )
#define colorR(col) (BYTE( ( col >> 16 ) &0xFF ) )
#define colorG(col) (BYTE( ( col >> 8  ) &0xFF ) )
#define colorB(col) (BYTE( ( col       ) &0xFF ) )

inline DWORD CD3DFont::LightColor(DWORD color)
{
	BYTE a = colorA(color);
	BYTE r = colorR(color);
	BYTE g = colorG(color);
	BYTE b = colorB(color);

	if (r <= 0x60)
		r += 0x20;
	if (g <= 0x60)
		g += 0x20;
	if (b <= 0x60)
		b += 0x20;

	if (r <= 0x80)
		r += 0x20;
	if (g <= 0x80)
		g += 0x20;
	if (b <= 0x80)
		b += 0x20;

	if (r <= 0xA0)
		r += 0x20;
	if (g <= 0xA0)
		g += 0x20;
	if (b <= 0xA0)
		b += 0x20;

	if (r <= 0xC0)
		r += 0x1F;
	if (g <= 0xC0)
		g += 0x1F;
	if (b <= 0xC0)
		b += 0x1F;

	if (r <= 0xE0)
		r += 0x1F;
	if (g <= 0xE0)
		g += 0x1F;
	if (b <= 0xE0)
		b += 0x1F;

	return ((D3DCOLOR)(((((a)) & 0xff) << 24) | ((((r)) & 0xff) << 16) | ((((g)) & 0xff) << 8) | (((b)) & 0xff)));
}

inline DWORD CD3DFont::DarkColor(DWORD color)
{
	BYTE a = colorA(color);
	BYTE r = colorR(color);
	BYTE g = colorG(color);
	BYTE b = colorB(color);

	if (r >= 0x70)
		r -= 0x10;
	if (g >= 0x70)
		g -= 0x10;
	if (b >= 0x70)
		b -= 0x10;

	if (r >= 0x60)
		r -= 0x20;
	if (g >= 0x60)
		g -= 0x20;
	if (b >= 0x60)
		b -= 0x20;

	if (r >= 0x40)
		r -= 0x1F;
	if (g >= 0x40)
		g -= 0x1F;
	if (b >= 0x40)
		b -= 0x1F;

	if (r >= 0x20)
		r -= 0x1F;
	if (g >= 0x20)
		g -= 0x1F;
	if (b >= 0x20)
		b -= 0x1F;

	return ((D3DCOLOR)(((((a)) & 0xff) << 24) | ((((r)) & 0xff) << 16) | ((((g)) & 0xff) << 8) | (((b)) & 0xff)));
}

//-----------------------------------------------------------------------------
// Name: CD3DFont()
// Desc: Font class constructor
//-----------------------------------------------------------------------------
CD3DFont::CD3DFont(const char* strFontName, DWORD dwHeight, DWORD dwFlags)
{
	strcpy_s(m_strFontName, 80 * sizeof(char), strFontName);
	m_fFontHeight = static_cast<float>(dwHeight);
	m_dwFontFlags = dwFlags;

	m_pD3DDevice = NULL;
	m_pTexture = NULL;

	m_pVB = NULL;
	m_dwUsedFontVerts = 0L;

	m_pEffectVB = NULL;
	m_dwUsedEffectVerts = 0L;

	m_pFontVertices = NULL;
	m_pEffectVertices = NULL;

	m_pStateBlockSaved = NULL;
	m_pStateBlockDrawText = NULL;
}



//-----------------------------------------------------------------------------
// Name: ~CD3DFont()
// Desc: Font class destructor
//-----------------------------------------------------------------------------
CD3DFont::~CD3DFont(void)
{
	InvalidateDeviceObjects();
	DeleteDeviceObjects();
}



//-----------------------------------------------------------------------------
// Name: InitializeDeviceObjects()
// Desc: Initializes device-dependent objects, including the vertex buffer used
//       for rendering text and the texture map which stores the font image.
//-----------------------------------------------------------------------------
HRESULT CD3DFont::InitializeDeviceObjects(LPDIRECT3DDEVICE9 pd3dDevice)
{
	HRESULT hr;

	// Keep a local copy of the device
	this->m_pD3DDevice = NULL;
	this->m_pD3DDevice = pd3dDevice;

	// Establish the font and texture size
	m_fTextScale = 1.0f; // Draw fonts into texture without scaling

						 // Large fonts need larger textures
	if (m_fFontHeight > 60)
		m_dwTexWidth = m_dwTexHeight = 2048;
	else if (m_fFontHeight > 30)
		m_dwTexWidth = m_dwTexHeight = 1024;
	else if (m_fFontHeight > 15)
		m_dwTexWidth = m_dwTexHeight = 512;
	else
		m_dwTexWidth = m_dwTexHeight = 256;

	// If requested texture is too big, use a smaller texture and smaller font,
	// and scale up when rendering.
	D3DCAPS9 d3dCaps;
	m_pD3DDevice->GetDeviceCaps(&d3dCaps);

	if (m_dwTexWidth > d3dCaps.MaxTextureWidth)
	{
		m_fTextScale = (FLOAT)d3dCaps.MaxTextureWidth / (FLOAT)m_dwTexWidth;
		m_dwTexWidth = m_dwTexHeight = d3dCaps.MaxTextureWidth;
	}

	// Create a new texture for the font
	hr = m_pD3DDevice->CreateTexture(m_dwTexWidth, m_dwTexHeight, 1,
		0, D3DFMT_A4R4G4B4,
		D3DPOOL_MANAGED, &m_pTexture, NULL);
	if (FAILED(hr))
		return hr;

	// Prepare to create a bitmap
	DWORD* pBitmapBits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (int)m_dwTexWidth;
	bmi.bmiHeader.biHeight = -(int)m_dwTexHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 32;

	// Create a DC and a bitmap for the font
	HDC hDC = CreateCompatibleDC(NULL);
	HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS,
		(void**)&pBitmapBits, NULL, 0);
	SetMapMode(hDC, MM_TEXT);

	// Create a font.  By specifying ANTIALIASED_QUALITY, we might get an
	// antialiased font, but this is not guaranteed.
	INT nHeight = -MulDiv((int)m_fFontHeight,
		(INT)(GetDeviceCaps(hDC, LOGPIXELSY) * (m_fTextScale)),
		72);
	DWORD dwBold = (m_dwFontFlags&D3DFONT_BOLD) ? FW_BOLD : FW_NORMAL;
	DWORD dwItalic = (m_dwFontFlags&D3DFONT_ITALIC) ? TRUE : FALSE;
	HFONT hFont = CreateFontA(nHeight, 0, 0, 0, dwBold, dwItalic,
		FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		VARIABLE_PITCH, m_strFontName);
	if (NULL == hFont)
		return E_FAIL;

	SelectObject(hDC, hbmBitmap);
	SelectObject(hDC, hFont);

	// Set text properties
	SetTextColor(hDC, RGB(255, 255, 255));
	SetBkColor(hDC, 0x00000000);
	SetTextAlign(hDC, TA_TOP);

	// Loop through all printable character and output them to the bitmap..
	// Meanwhile, keep track of the corresponding tex coords for each character.
	DWORD x = 0;
	DWORD y = 0;
	char str[2] = ("x");
	SIZE size;

	// Calculate the spacing between characters based on line height
	GetTextExtentPoint32(hDC, TEXT(" "), 1, &size);
	x = m_dwSpacing = (DWORD)ceil(size.cy / 1000.0f);

	for (char c = 32; c < 127; c++)
	{
		str[0] = c;
		GetTextExtentPoint32A(hDC, str, 1, &size);

		if ((DWORD)(x + size.cx + m_dwSpacing) > m_dwTexWidth)
		{
			x = m_dwSpacing;
			y += size.cy + 1;
		}

		ExtTextOutA(hDC, x + 0, y + 0, ETO_OPAQUE, NULL, str, 1, NULL);

		m_fTexCoords[c - 32][0] = ((FLOAT)(x + 0 - m_dwSpacing)) / m_dwTexWidth;
		m_fTexCoords[c - 32][1] = ((FLOAT)(y + 0 + 0)) / m_dwTexHeight;
		m_fTexCoords[c - 32][2] = ((FLOAT)(x + size.cx + m_dwSpacing)) / m_dwTexWidth;
		m_fTexCoords[c - 32][3] = ((FLOAT)(y + size.cy + 0)) / m_dwTexHeight;

		x += size.cx + (2 * m_dwSpacing);
	}

	// Lock the surface and write the alpha values for the set pixels
	D3DLOCKED_RECT d3dlr;
	m_pTexture->LockRect(0, &d3dlr, 0, 0);
	BYTE* pDstRow = (BYTE*)d3dlr.pBits;
	WORD* pDst16;
	BYTE bAlpha; // 4-bit measure of pixel intensity

	for (y = 0; y < m_dwTexHeight; y++)
	{
		pDst16 = (WORD*)pDstRow;
		for (x = 0; x < m_dwTexWidth; x++)
		{
			bAlpha = (BYTE)((pBitmapBits[m_dwTexWidth*y + x] & 0xff) >> 4);
			if (bAlpha > 0)
			{
				*pDst16++ = (WORD)((bAlpha << 12) | 0x0fff);
			}
			else
			{
				*pDst16++ = 0x0000;
			}
		}
		pDstRow += d3dlr.Pitch;
	}

	// Done updating texture, so clean up used objects
	m_pTexture->UnlockRect(0);
	DeleteObject(hbmBitmap);
	DeleteDC(hDC);
	DeleteObject(hFont);

	return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CD3DFont::RestoreDeviceObjects(void)
{
	HRESULT hResult = E_FAIL;

	// Create vertex buffer for the letters
	//if( m_pVB )
	//{
	//    SAFE_RELEASE( m_pVB );
	//}
	if (FAILED(hResult = m_pD3DDevice->CreateVertexBuffer(MAX_NUM_VERTICES * sizeof(FONT2DVERTEX),
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		0,
		D3DPOOL_DEFAULT,
		&m_pVB, NULL)))
	{
		return hResult;
	}
	// Create vertex buffer for the effects
	//if( m_pEffectVertices )
	//{
	//    SAFE_RELEASE( m_pEffectVB );
	//}
	if (FAILED(hResult = m_pD3DDevice->CreateVertexBuffer(MAX_NUM_VERTICES * sizeof(FONTEFFECTVERTEX),
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		0,
		D3DPOOL_DEFAULT,
		&m_pEffectVB, NULL)))
		return hResult;

	// Create the state blocks for rendering text
	for (int nIndex = 0; nIndex < 2; nIndex++)
	{
		m_pD3DDevice->BeginStateBlock();
		m_pD3DDevice->SetTexture(0, m_pTexture);

		m_pD3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		m_pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		m_pD3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
		m_pD3DDevice->SetRenderState(D3DRS_ALPHAREF, 0x08);
		m_pD3DDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		m_pD3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		m_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		m_pD3DDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		m_pD3DDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
		//m_pD3DDevice->SetRenderState( D3DRS_EDGEANTIALIAS, FALSE );
		m_pD3DDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
		m_pD3DDevice->SetRenderState(D3DRS_VERTEXBLEND, FALSE);
		m_pD3DDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);

		m_pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		m_pD3DDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		m_pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		m_pD3DDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);

		m_pD3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		m_pD3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_pD3DDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		m_pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		m_pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		m_pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		m_pD3DDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		m_pD3DDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		m_pD3DDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		m_pD3DDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

		if (nIndex == 0)
			m_pD3DDevice->EndStateBlock(&m_pStateBlockSaved);
		else
			m_pD3DDevice->EndStateBlock(&m_pStateBlockDrawText);
	}
	return S_OK;
}



//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CD3DFont::InvalidateDeviceObjects(void)
{
	if (m_pVB)
	{
		//m_pVB->Release();
		SAFE_RELEASE(m_pVB);
	}

	if (m_pEffectVB)
	{
		//m_pEffectVB->Release();
		SAFE_RELEASE(m_pEffectVB);
	}

	// Delete the state blocks
	if (m_pD3DDevice)
	{
		SAFE_RELEASE(m_pStateBlockSaved);
		SAFE_RELEASE(m_pStateBlockDrawText);
	}

	m_dwUsedEffectVerts = 0L;

	return S_OK;
}



//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DeleteDeviceObjects(void)
{
	//m_pTexture->Release();
	SAFE_RELEASE(m_pTexture);
	m_pD3DDevice = NULL;
	return S_OK;
}



//-----------------------------------------------------------------------------
// Name: GetTextExtent()
// Desc: Get the dimensions of a text string
//-----------------------------------------------------------------------------
HRESULT CD3DFont::GetTextExtent(const char* strText, SIZE* pSize)
{
	if ((strText == NULL) || (pSize == NULL))
		return E_FAIL;

	float fRowWidth = 0.0f;
	float fRowHeight = (m_fTexCoords[0][3] - m_fTexCoords[0][1]) * m_dwTexHeight;
	float fWidth = 0.0f;
	float fHeight = fRowHeight;

	BOOL bSkipNext = FALSE;

	while (*strText)
	{
		char tchLetter = *strText++;

		if (bSkipNext)
		{
			bSkipNext = FALSE;
			continue;
		}

		if (tchLetter == ('^'))
		{
			if (IsCharNumeric(*strText))
			{
				bSkipNext = TRUE;
				continue;
			}
		}

		if (tchLetter == ('\n'))
		{
			fRowWidth = 0.0f;
			fHeight += fRowHeight;
		}

		if (tchLetter < (' '))
			continue;

		FLOAT tx1 = m_fTexCoords[tchLetter - 32][0];
		FLOAT tx2 = m_fTexCoords[tchLetter - 32][2];

		fRowWidth += (tx2 - tx1) * m_dwTexWidth;

		if (fRowWidth > fWidth)
			fWidth = fRowWidth;
	}

	pSize->cx = static_cast<int>(fWidth);
	pSize->cy = static_cast<int>(fHeight);

	return S_OK;
}

HRESULT CD3DFont::BeginDrawing(void)
{
	if (m_pD3DDevice == NULL)
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	if (FAILED(hResult = m_pEffectVB->Lock(0, 0, (void**)(&m_pEffectVertices), D3DLOCK_DISCARD)))
		return hResult;

	if (FAILED(hResult = m_pVB->Lock(0, 0, (void**)(&m_pFontVertices), D3DLOCK_DISCARD)))
		return hResult;

	return S_OK;
}



//-----------------------------------------------------------------------------
// Name: DrawText()
// Desc: Draws 2D text
//-----------------------------------------------------------------------------
HRESULT CD3DFont::DrawText(float fXPos, float fYPos, DWORD dwColor, const char* strText, DWORD dwFlags, DWORD dwBackgroundColor)
{
	if ((m_pD3DDevice == NULL) || (strText == NULL) || (m_pEffectVertices == NULL) || (m_pFontVertices == NULL))
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	if (dwFlags & D3DFONT_BACKGROUND)
	{
		SIZE szFontBox = { 0 };

		if (FAILED(GetTextExtent(strText, &szFontBox)))
			return E_FAIL;

		// Set filter states

		szFontBox.cx += static_cast<LONG>(fXPos);
		szFontBox.cy += static_cast<LONG>(fYPos);



		m_pEffectVertices[m_dwUsedEffectVerts] = InitFontEffectVertex(D3DXVECTOR4(fXPos - 2.0f, fYPos - 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Top-Left

		m_pEffectVertices[m_dwUsedEffectVerts + 1] = InitFontEffectVertex(D3DXVECTOR4(static_cast<float>(szFontBox.cx) + 2.0f, fYPos - 1.0f, 1.0f, 1.0f), dwBackgroundColor);
		m_pEffectVertices[m_dwUsedEffectVerts + 4] = InitFontEffectVertex(D3DXVECTOR4(static_cast<float>(szFontBox.cx) + 2.0f, fYPos - 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Top-Right

		m_pEffectVertices[m_dwUsedEffectVerts + 2] = InitFontEffectVertex(D3DXVECTOR4(fXPos - 2.0f, szFontBox.cy + 1.0f, 1.0f, 1.0f), dwBackgroundColor);
		m_pEffectVertices[m_dwUsedEffectVerts + 3] = InitFontEffectVertex(D3DXVECTOR4(fXPos - 2.0f, szFontBox.cy + 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Bottom-Left

		m_pEffectVertices[m_dwUsedEffectVerts + 5] = InitFontEffectVertex(D3DXVECTOR4(static_cast<float>(szFontBox.cx) + 2.0f, szFontBox.cy + 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Bottom-Right

		m_dwUsedEffectVerts += 6;

		if (m_dwUsedEffectVerts > (MAX_NUM_VERTICES - 6))
		{
			if (FAILED(hResult = m_pStateBlockSaved->Capture()))
				return hResult;

			if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetPixelShader(NULL)))
				return hResult;

			if (FAILED(hResult = m_pEffectVB->Unlock()))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONTEFFECTVERTEX)))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pEffectVB, 0, sizeof(FONTEFFECTVERTEX))))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, (m_dwUsedEffectVerts / 3))))
				return hResult;

			if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetTexture(0, m_pTexture)))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONT2DVERTEX)))
				return hResult;

			if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pVB, 0, sizeof(FONT2DVERTEX))))
				return hResult;

			m_pEffectVertices = NULL;
			m_dwUsedEffectVerts = 0L;

			if (FAILED(hResult = m_pEffectVB->Lock(0, 0, (void**)(&m_pEffectVertices), D3DLOCK_DISCARD)))
				return hResult;


			if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
				return hResult;
		}
	}

	if (dwFlags & D3DFONT_RIGHT)
	{
		SIZE sz;
		GetTextExtent(strText, &sz);
		fXPos -= (FLOAT)sz.cx;
	}
	else if (dwFlags & D3DFONT_CENTERED)
	{
		SIZE sz;
		GetTextExtent(strText, &sz);
		fXPos -= (FLOAT)sz.cx / 2.0f;
	}
	// Set filter states
	if (dwFlags & D3DFONT_FILTERED)
	{
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}
	size_t len = strlen(strText);
	DWORD strStart = (DWORD)strText;
	float fStartX = fXPos;
	DWORD dwCustomColor = 0xFFFFFFFF;

	while (*strText)
	{
		char tszChr = *strText++;
		DWORD strCur = (DWORD)strText;
		if (tszChr == ('^'))
		{
			if (IsCharNumeric(*strText))
			{
				char tszValue = *strText++;
				dwCustomColor = GetCustomColor(tszValue, (dwColor >> 24));
				continue;
			}
		}

		if (tszChr == ('\n'))
		{
			fXPos = fStartX;
			fYPos += (m_fTexCoords[0][3] - m_fTexCoords[0][1]) * m_dwTexHeight;
		}

		if (tszChr < (' '))
			continue;

		float tx1 = m_fTexCoords[tszChr - 32][0];
		float ty1 = m_fTexCoords[tszChr - 32][1];
		float tx2 = m_fTexCoords[tszChr - 32][2];
		float ty2 = m_fTexCoords[tszChr - 32][3];

		float fWidth = (tx2 - tx1) *  m_dwTexWidth / m_fTextScale;
		float fHeight = (ty2 - ty1) * m_dwTexHeight / m_fTextScale;

		if (tszChr != (' '))
		{
			if (dwFlags & D3DFONT_COLORTABLE)
				dwColor = dwCustomColor;

			if (dwFlags & D3DFONT_SHADOW)
			{
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 + 1.0f, fYPos + fHeight + 1.f, 1.0f, 1.0f), 0x9a000000, tx1, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 + 1.0f, fYPos + 0 + 1.f, 1.0f, 1.0f), 0x9a000000, tx1, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth + 1.0f, fYPos + fHeight + 1.f, 1.0f, 1.0f), 0x9a000000, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth + 1.0f, fYPos + 0 + 1.f, 1.0f, 1.0f), 0x9a000000, tx2, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth + 1.0f, fYPos + fHeight + 1.f, 1.0f, 1.0f), 0x9a000000, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 + 1.0f, fYPos + 0 + 1.f, 1.0f, 1.0f), 0x9a000000, tx1, ty1);
				m_dwUsedFontVerts += 2;
			}
			if (dwFlags & D3DFONT_LIGHTEFFECT)
			{
				if ((float)(strCur - strStart) / (float)len < float(1.0 / 3.0))
				{
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx1, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
				}
				else if ((float)(strCur - strStart) / (float)len > float(2.0 / 3.0))
				{
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx1, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), DarkColor(dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), DarkColor(dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
				}
				else
					goto normalText;
			}
			else
			{
			normalText:
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
			}

			m_dwUsedFontVerts += 2;

			if (m_dwUsedFontVerts * 3 > (MAX_NUM_VERTICES - 6))
			{
				// Unlock, render, and relock the vertex buffer

				if (FAILED(hResult = m_pStateBlockSaved->Capture()))
					return hResult;

				if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
					return hResult;

				if (FAILED(hResult = m_pD3DDevice->SetPixelShader(NULL)))
					return hResult;

				if (FAILED(hResult = m_pD3DDevice->SetTexture(0, m_pTexture)))
					return hResult;

				if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONT2DVERTEX)))
					return hResult;

				if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pVB, 0, sizeof(FONT2DVERTEX))))
					return hResult;

				if (FAILED(hResult = m_pVB->Unlock()))
					return hResult;

				if (FAILED(hResult = m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, m_dwUsedFontVerts)))
					return hResult;

				if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
					return hResult;

				if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
					return hResult;
				m_pFontVertices = NULL;
				m_dwUsedFontVerts = 0L;

				if (FAILED(hResult = m_pVB->Lock(0, 0, (void**)(&m_pFontVertices), D3DLOCK_DISCARD)))
					return hResult;
			}
		}
		fXPos += fWidth;
	}
	return S_OK;
}



HRESULT CD3DFont::EndDrawing(void)
{
	if (m_pD3DDevice == NULL)
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	// Setup renderstate
	if (FAILED(hResult = m_pStateBlockSaved->Capture()))
		return hResult;

	if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
		return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetPixelShader(NULL)))
		return hResult;

	if (FAILED(hResult = m_pEffectVB->Unlock()))
		return hResult;

	if (m_dwUsedEffectVerts)
	{
		if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONTEFFECTVERTEX)))
			return hResult;

		if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pEffectVB, 0, sizeof(FONTEFFECTVERTEX))))
			return hResult;

		if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
			return hResult;

		if (FAILED(hResult = m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, (m_dwUsedEffectVerts / 3))))
			return hResult;
	}

	if (FAILED(hResult = m_pD3DDevice->SetTexture(0, m_pTexture)))
		return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONT2DVERTEX)))
		return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pVB, 0, sizeof(FONT2DVERTEX))))
		return hResult;

	// Unlock and render the vertex buffer

	if (FAILED(hResult = m_pVB->Unlock()))
		return hResult;

	if (m_dwUsedFontVerts > 0)
		if (FAILED(hResult = m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, m_dwUsedFontVerts)))
			return hResult;

	// Restore the modified renderstates
	if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
		return hResult;


	if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
		return hResult;

	m_dwUsedFontVerts = 0L;
	m_dwUsedEffectVerts = 0L;

	m_pFontVertices = NULL;
	m_pEffectVertices = NULL;

	return S_OK;
}

HRESULT CD3DFont::BeginStatic(void)
{
	if ((m_pD3DDevice == NULL) || (m_pEffectVB == NULL) || (m_pVB == NULL))
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	if (FAILED(hResult = m_pEffectVB->Lock(0, 0, (void**)(&m_pEffectVertices), D3DLOCK_NOOVERWRITE)))
		return hResult;

	if (FAILED(hResult = m_pVB->Lock(0, 0, (void**)(&m_pFontVertices), D3DLOCK_NOOVERWRITE)))
		return hResult;

	return S_OK;
}

HRESULT CD3DFont::AddStaticText(float fXPos, float fYPos, DWORD dwColor, const char* strText, DWORD dwFlags, DWORD dwBackgroundColor)
{
	if ((m_pD3DDevice == NULL) || (strText == NULL) || (m_pEffectVertices == NULL) || (m_pFontVertices == NULL))
		return E_FAIL;

	if (dwFlags & D3DFONT_BACKGROUND)
	{
		if (m_dwUsedEffectVerts > (MAX_NUM_VERTICES - 6))
			return E_FAIL;

		SIZE szFontBox = { 0 };

		if (FAILED(GetTextExtent(strText, &szFontBox)))
			return E_FAIL;

		szFontBox.cx += static_cast<LONG>(fXPos);
		szFontBox.cy += static_cast<LONG>(fYPos);

		m_pEffectVertices[m_dwUsedEffectVerts] = InitFontEffectVertex(D3DXVECTOR4(fXPos - 2.0f, fYPos - 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Top-Left

		m_pEffectVertices[m_dwUsedEffectVerts + 1] = InitFontEffectVertex(D3DXVECTOR4(static_cast<float>(szFontBox.cx) + 2.0f, fYPos - 1.0f, 1.0f, 1.0f), dwBackgroundColor);
		m_pEffectVertices[m_dwUsedEffectVerts + 4] = InitFontEffectVertex(D3DXVECTOR4(static_cast<float>(szFontBox.cx) + 2.0f, fYPos - 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Top-Right

		m_pEffectVertices[m_dwUsedEffectVerts + 2] = InitFontEffectVertex(D3DXVECTOR4(fXPos - 2.0f, szFontBox.cy + 1.0f, 1.0f, 1.0f), dwBackgroundColor);
		m_pEffectVertices[m_dwUsedEffectVerts + 3] = InitFontEffectVertex(D3DXVECTOR4(fXPos - 2.0f, szFontBox.cy + 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Bottom-Left

		m_pEffectVertices[m_dwUsedEffectVerts + 5] = InitFontEffectVertex(D3DXVECTOR4(static_cast<float>(szFontBox.cx) + 2.0f, szFontBox.cy + 1.0f, 1.0f, 1.0f), dwBackgroundColor); // Bottom-Right

		m_dwUsedEffectVerts += 6;
	}

	if (dwFlags & D3DFONT_RIGHT)
	{
		SIZE sz;
		GetTextExtent(strText, &sz);
		fXPos -= (FLOAT)sz.cx;
	}
	else if (dwFlags & D3DFONT_CENTERED)
	{
		SIZE sz;
		GetTextExtent(strText, &sz);
		fXPos -= (FLOAT)sz.cx / 2.0f;
	}
	size_t len = strlen(strText);
	DWORD strStart = (DWORD)strText;
	// Set filter states
	if (dwFlags & D3DFONT_FILTERED)
	{
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}

	float fStartX = fXPos;
	DWORD dwCustomColor = 0xFFFFFFFF;

	while (*strText)
	{
		if (m_dwUsedFontVerts * 3 > (MAX_NUM_VERTICES - 6))
			return E_FAIL;

		char tszChr = *strText++;
		DWORD strCur = (DWORD)strText;
		if (tszChr == ('^'))
		{
			if (IsCharNumeric(*strText))
			{
				char tszValue = *strText++;
				dwCustomColor = GetCustomColor(tszValue, (dwColor >> 24));
				continue;
			}
		}

		if (tszChr == ('\n'))
		{
			fXPos = fStartX;
			fYPos += (m_fTexCoords[0][3] - m_fTexCoords[0][1]) * m_dwTexHeight;
		}

		if (tszChr < (' '))
			continue;

		float tx1 = m_fTexCoords[tszChr - 32][0];
		float ty1 = m_fTexCoords[tszChr - 32][1];
		float tx2 = m_fTexCoords[tszChr - 32][2];
		float ty2 = m_fTexCoords[tszChr - 32][3];

		float fWidth = (tx2 - tx1) *  m_dwTexWidth / m_fTextScale;
		float fHeight = (ty2 - ty1) * m_dwTexHeight / m_fTextScale;

		if (tszChr != (' '))
		{
			if (dwFlags & D3DFONT_COLORTABLE)
				dwColor = dwCustomColor;
			if (dwFlags & D3DFONT_SHADOW)
			{
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 + 1.0f, fYPos + fHeight + 1.f, 1.0f, 1.0f), 0x9a000000, tx1, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 + 1.0f, fYPos + 0 + 1.f, 1.0f, 1.0f), 0x9a000000, tx1, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth + 1.0f, fYPos + fHeight + 1.f, 1.0f, 1.0f), 0x9a000000, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth + 1.0f, fYPos + 0 + 1.f, 1.0f, 1.0f), 0x9a000000, tx2, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth + 1.0f, fYPos + fHeight + 1.f, 1.0f, 1.0f), 0x9a000000, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 + 1.0f, fYPos + 0 + 1.f, 1.0f, 1.0f), 0x9a000000, tx1, ty1);
				m_dwUsedFontVerts += 2;
			}
			if (dwFlags & D3DFONT_LIGHTEFFECT)
			{
				if ((float)(strCur - strStart) / (float)len < float(1.0 / 3.0))
				{
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx1, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
				}
				else if ((float)(strCur - strStart) / (float)len > float(2.0 / 3.0))
				{
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), (dwColor), tx1, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), DarkColor(dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), (dwColor), tx2, ty1);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), DarkColor(dwColor), tx2, ty2);
					*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), LightColor(dwColor), tx1, ty1);
				}
				else
					goto normalText;
			}
			else
			{
			normalText:
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty1);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + fWidth - 0.5f, fYPos + fHeight - 0.5f, 1.0f, 1.0f), dwColor, tx2, ty2);
				*m_pFontVertices++ = InitFont2DVertex(D3DXVECTOR4(fXPos + 0 - 0.5f, fYPos + 0 - 0.5f, 1.0f, 1.0f), dwColor, tx1, ty1);
			}

			m_dwUsedFontVerts += 2;
		}

		fXPos += fWidth;
	}

	return S_OK;
}

HRESULT CD3DFont::EndStatic(void)
{
	if ((m_pD3DDevice == NULL) || (m_pEffectVB == NULL) || (m_pVB))
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	if (FAILED(hResult = m_pEffectVB->Unlock()))
		return hResult;

	if (FAILED(hResult = m_pVB->Unlock()))
		return hResult;

	return S_OK;
}

HRESULT CD3DFont::RenderStaticPrimitives(void)
{
	if ((m_pD3DDevice == NULL) || (m_pEffectVB == NULL) || (m_pVB == NULL) || (m_dwUsedFontVerts == 0))
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	if (FAILED(hResult = m_pStateBlockSaved->Capture()))
		return hResult;

	if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
		return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetPixelShader(NULL)))
		return hResult;


	if (m_dwUsedEffectVerts)
	{

		if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONTEFFECTVERTEX)))
			return hResult;

		if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pEffectVB, 0, sizeof(FONTEFFECTVERTEX))))
			return hResult;

		if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
			return hResult;

		if (FAILED(hResult = m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, (m_dwUsedEffectVerts / 3))))
			return hResult;
	}

	if (FAILED(hResult = m_pD3DDevice->SetTexture(0, m_pTexture)))
		return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetFVF(D3DFVF_FONT2DVERTEX)))
		return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetStreamSource(0, m_pVB, 0, sizeof(FONT2DVERTEX))))
		return hResult;

	if (m_dwUsedFontVerts > 0)
		if (FAILED(hResult = m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, m_dwUsedFontVerts)))
			return hResult;

	if (FAILED(hResult = m_pStateBlockDrawText->Apply()))
		return hResult;

	//if (FAILED(hResult = m_pStateBlockSaved->Apply()))
	//    return hResult;

	if (FAILED(hResult = m_pD3DDevice->SetTexture(0, NULL)))
		return hResult;

	return S_OK;
}

HRESULT CD3DFont::ClearStaticBuffer(void)
{
	if ((m_pD3DDevice == NULL) || (m_pEffectVB == NULL) || (m_pVB == NULL))
		return E_FAIL;

	HRESULT hResult = E_FAIL;

	if (FAILED(hResult = m_pEffectVB->Lock(0, 0, (void**)(&m_pEffectVertices), D3DLOCK_DISCARD)))
		return hResult;

	if (FAILED(hResult = m_pVB->Lock(0, 0, (void**)(&m_pFontVertices), D3DLOCK_DISCARD)))
		return hResult;

	if (FAILED(hResult = m_pEffectVB->Unlock()))
		return hResult;

	if (FAILED(hResult = m_pVB->Unlock()))
		return hResult;

	m_dwUsedFontVerts = 0L;
	m_dwUsedEffectVerts = 0L;

	m_pFontVertices = NULL;
	m_pEffectVertices = NULL;

	return S_OK;
}

float CD3DFont::GetFontHeight(void)
{
	return m_fFontHeight;
}
