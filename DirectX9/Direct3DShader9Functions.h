#include "d3d9Wrapper.h"
#pragma once
D3D9Wrapper::IDirect3DShader9::IDirect3DShader9(IUnknown * shader, D3D9Wrapper::IDirect3DDevice9 * hackerDevice, ShaderType shaderType)
	: D3D9Wrapper::IDirect3DUnknown(shader),
	magic(0x7da43feb),
	hackerDevice(hackerDevice),
	hash(-1),
	shaderType(shaderType),
	zeroShader(NULL),
	originalShader(NULL),
	shaderOverride(NULL),
	compiledShader(NULL),
	shaderInfo(NULL)
{
	originalShaderInfo.byteCode = NULL;
	originalShaderInfo.replacement = NULL;
}

inline void D3D9Wrapper::IDirect3DShader9::Delete()
{
	if (zeroShader) {
		--hackerDevice->migotoResourceCount;
		zeroShader->Release();
	}
	if (originalShaderInfo.replacement && originalShaderInfo.replacement != originalShader) {
		--hackerDevice->migotoResourceCount;
		originalShaderInfo.replacement->Release();
	}
	if (originalShader && originalShader != GetRealOrig()) {
		--hackerDevice->migotoResourceCount;
		originalShader->Release();
	}
	if (originalShaderInfo.byteCode) {
		originalShaderInfo.byteCode->Release();
	}

	unordered_set<D3D9Wrapper::IDirect3DShader9*>::const_iterator it = G->mReloadedShaders.find(this);
	if (it != G->mReloadedShaders.end())
		G->mReloadedShaders.erase((*it));
}