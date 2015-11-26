#pragma once

#include <d3d11.h>
#include <stdint.h>

// Tracks info about specific resource instances:
struct ResourceHandleInfo
{
	uint32_t hash;
	uint32_t data_hash;	// Just the data hash for track_texture_updates

	// TODO: If we are sure we understand all possible differences between
	// the original desc and that obtained by querying the resource we
	// probably don't need to store these. One possible difference is the
	// MipMaps field, which can be set to 0 at creation time to tell DX to
	// create the mip-maps, and I presume it will be filled in by the time
	// we query the desc. Most of the other fields shouldn't change, but
	// I'm not positive about all the misc flags. For now, storing this
	// copy is safer but wasteful.
	union {
		D3D11_TEXTURE2D_DESC desc2D;
		D3D11_TEXTURE3D_DESC desc3D;
	};

	D3D11_MAPPED_SUBRESOURCE map;
	bool mapped_writable;

	ResourceHandleInfo() :
		hash(0),
		data_hash(0),
		mapped_writable(false)
	{}
};

uint32_t CalcTexture2DDescHash(uint32_t initial_hash, const D3D11_TEXTURE2D_DESC *const_desc);
uint32_t CalcTexture3DDescHash(uint32_t initial_hash, const D3D11_TEXTURE3D_DESC *const_desc);

uint32_t CalcTexture2DDataHash(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);
uint32_t CalcTexture3DDataHash(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);

uint32_t GetResourceHash(ID3D11Resource *resource);

void MarkResourceHashContaminated(ID3D11Resource *dest, UINT DstSubresource,
		ID3D11Resource *src, UINT srcSubresource, char type,
		UINT DstX, UINT DstY, UINT DstZ, const D3D11_BOX *SrcBox);

void UpdateResourceHashFromCPU(ID3D11Resource *resource,
	ResourceHandleInfo *info,
	const void *data, UINT rowPitch, UINT depthPitch);

void PropagateResourceHash(ID3D11Resource *dst, ID3D11Resource *src);

void MapTrackResourceHashUpdate(ID3D11Resource *pResource, UINT Subresource,
	D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource);
void MapUpdateResourceHash(ID3D11Resource *pResource, UINT Subresource);
