#pragma once
//-----------------------------------------------------------------------------
// File: D3DFont.h
//
// Desc: Texture-based font class
//
// Copyright (c) 1999-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


/*
* This file has been heavily modified to accommodate for new features, originally by Renkokuken (Sakuri) then by Topblast
*
*      This file has been modified to include various new features such as:
*          - Batch Drawing
*          - Inline Coloring
*          - Primitive Backgrounds
*
* This file was then modified to add new features and change the framework.
*
*    Features include:
*        Fix / Added shadow font
*        Added Right Align
*        Added Light Effect
*        D3D9 Framework
*        Adjust Spacing
*        Fix RenderState issue
*
*
*    Credits:
*        Atom0s
*        Topblast
*        Direct3D 9 SDK
*        Renkokuken  (Sakuri)
*
*    Source:
*        https://www.unknowncheats.me/forum/d3d-tutorials-and-source/74839-modified-cd3dfont-d3d9-shadows-light-effect.html
*/

#pragma once

#ifndef __D3DFONT_HEADER__
#define __D3DFONT_HEADER__

#include <windows.h>
#include <string>
#include <stdio.h>
#include <d3d9.h>
#include <d3dx9.h>

//-----------------------------------------------------------------------------
// Custom vertex types for rendering text
//-----------------------------------------------------------------------------
#define D3DFVF_FONT2DVERTEX ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#define D3DFVF_FONTEFFECTVERTEX ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE )

#define MAX_NUM_VERTICES 4096
#define MAX( A, B ) ( A > B ? A : B )

struct FONT2DVERTEX {
	D3DXVECTOR4 vXYZRHW;
	DWORD dwColor;
	FLOAT TU, TV;
};
struct FONTEFFECTVERTEX {
	D3DXVECTOR4 vXYZRHW;
	DWORD dwColor;
};

inline FONT2DVERTEX InitFont2DVertex(const D3DXVECTOR4& pVertex, D3DCOLOR dwColor, FLOAT TU, FLOAT TV)
{
	FONT2DVERTEX textVertex;

	textVertex.vXYZRHW = pVertex;
	textVertex.dwColor = dwColor;
	textVertex.TU = TU;
	textVertex.TV = TV;

	return textVertex;
}

inline FONTEFFECTVERTEX InitFontEffectVertex(const D3DXVECTOR4& pVertex, D3DCOLOR dwColor)
{
	FONTEFFECTVERTEX fxVertex;

	fxVertex.vXYZRHW = pVertex;
	fxVertex.dwColor = dwColor;

	return fxVertex;
}

#define SAFE_RELEASE( p ) if( p ){ p->Release(); p = NULL; }


// Font creation flags
#define D3DFONT_BOLD        0x0001
#define D3DFONT_ITALIC      0x0002
#define D3DFONT_ZENABLE     0x0004

// Font rendering flags
#define D3DFONT_CENTERED    0x0001
#define D3DFONT_FILTERED    0x0002
#define D3DFONT_BACKGROUND  0x0004
#define D3DFONT_COLORTABLE  0x0008
#define D3DFONT_RIGHT       0x0010
#define D3DFONT_SHADOW      0x0020
#define D3DFONT_LIGHTEFFECT 0x0040


//-----------------------------------------------------------------------------
// Name: class CD3DFont
// Desc: Texture-based font class for doing text in a 3D scene.
//-----------------------------------------------------------------------------
class CD3DFont {
public:

	// Constructor / destructor
	CD3DFont(const char* strFontName, DWORD dwHeight, DWORD dwFlags = 0L);
	~CD3DFont(void);

	// Getter
	float CD3DFont::GetFontHeight(void); // Added by Sean Pesce

										 // Begin/End batch drawing
	HRESULT BeginDrawing(void);
	HRESULT EndDrawing(void);

	// 2D text drawing functions
	HRESULT DrawText(float fXPos, float fYPos, DWORD dwColor, const char* strText, DWORD dwFlags = 0L, DWORD dwBackgroundColor = 0L);

	// Function to get extent of text
	HRESULT GetTextExtent(const char* strText, SIZE* pSize);

	// Static drawing functions
	HRESULT BeginStatic(void);
	HRESULT AddStaticText(float fXPos, float fYPos, DWORD dwColor, const char* strText, DWORD dwFlags = 0L, DWORD dwBackgroundColor = 0L);
	HRESULT EndStatic(void);
	HRESULT RenderStaticPrimitives(void);
	HRESULT ClearStaticBuffer(void);

	// Initializing and destroying device-dependent objects
	HRESULT InitializeDeviceObjects(LPDIRECT3DDEVICE9 pD3DDevice);
	HRESULT RestoreDeviceObjects(void);
	HRESULT InvalidateDeviceObjects(void);
	HRESULT DeleteDeviceObjects(void);

	// Stateblocks for setting and restoring render states
	LPDIRECT3DSTATEBLOCK9 m_pStateBlockSaved;
	LPDIRECT3DSTATEBLOCK9 m_pStateBlockDrawText;

	inline DWORD LightColor(DWORD color);
	inline DWORD DarkColor(DWORD  color);

protected:

	char m_strFontName[80]; // Font properties

	float m_fFontHeight;
	DWORD m_dwFontFlags;

	LPDIRECT3DDEVICE9 m_pD3DDevice; // A D3DDevice used for rendering
	LPDIRECT3DTEXTURE9 m_pTexture; // The d3d texture for this font

	DWORD m_dwUsedFontVerts;
	LPDIRECT3DVERTEXBUFFER9 m_pVB; // VertexBuffer for rendering text

	DWORD   m_dwSpacing;                  // Character pixel spacing per side
	DWORD m_dwUsedEffectVerts;
	LPDIRECT3DVERTEXBUFFER9 m_pEffectVB;

	FONT2DVERTEX* m_pFontVertices;
	FONTEFFECTVERTEX* m_pEffectVertices;

	DWORD m_dwTexWidth; // Texture dimensions
	DWORD m_dwTexHeight;

	float m_fTextScale;
	float m_fTexCoords[96][4];
};

#endif // __D3DFONT_HEADER__
