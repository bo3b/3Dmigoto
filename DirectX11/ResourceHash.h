#pragma once

#include <d3d11.h>
#include <stdint.h>
#include <tuple>
#include <map>
#include <set>

// Tracks info about specific resource instances:
struct ResourceHandleInfo
{
	uint32_t hash;
	uint32_t orig_hash;	// Original hash at the time of creation
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
	void *diverted_map;
	size_t diverted_size;

	ResourceHandleInfo() :
		hash(0),
		orig_hash(0),
		data_hash(0),
		mapped_writable(false),
		diverted_map(NULL),
		diverted_size(0)
	{}
};

struct CopySubresourceRegionContamination
{
	bool partial;
	UINT DstX;
	UINT DstY;
	UINT DstZ;
	D3D11_BOX SrcBox;

	CopySubresourceRegionContamination() :
		partial(false),
		DstX(0),
		DstY(0),
		DstZ(0),
		SrcBox({0, 0, 0, UINT_MAX, UINT_MAX, UINT_MAX})
	{}

	void Update(bool partial, UINT DstX, UINT DstY, UINT DstZ, const D3D11_BOX *SrcBox)
	{
		this->partial = this->partial || partial;
		this->DstX = max(this->DstX, DstX);
		this->DstY = max(this->DstY, DstY);
		this->DstZ = max(this->DstZ, DstZ);
		if (SrcBox) {
			this->SrcBox.left = max(this->SrcBox.left, SrcBox->left);
			this->SrcBox.top = max(this->SrcBox.top, SrcBox->top);
			this->SrcBox.front = max(this->SrcBox.front, SrcBox->front);

			this->SrcBox.right = min(this->SrcBox.right, SrcBox->right);
			this->SrcBox.bottom = min(this->SrcBox.bottom, SrcBox->bottom);
			this->SrcBox.back = min(this->SrcBox.back, SrcBox->back);
		}
	}
};

// Create a map that uses a hashable tuple of five integers as they key (Hey C++,
// this is something Python can do with what? ... 0 lines of boilerplate?)
typedef std::tuple<uint32_t, UINT, UINT, UINT, UINT> CopySubresourceRegionContaminationMapKey;
template<> struct std::hash<CopySubresourceRegionContaminationMapKey>
{
	size_t operator()(CopySubresourceRegionContaminationMapKey const &key)
	{
		// http://stackoverflow.com/questions/4948780/magic-number-in-boosthash-combine
		size_t seed = 0;
		seed ^= std::hash<uint32_t>()(std::get<0>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<UINT>()(std::get<1>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<UINT>()(std::get<2>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<UINT>()(std::get<3>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<UINT>()(std::get<4>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}
};
typedef std::map<CopySubresourceRegionContaminationMapKey, CopySubresourceRegionContamination>
	CopySubresourceRegionContaminationMap;

// Tracks info about resources by their *original* hash. Primarily for stat collection:
struct ResourceHashInfo
{
	D3D11_RESOURCE_DIMENSION type;
	union {
		D3D11_TEXTURE2D_DESC tex2d_desc;
		D3D11_TEXTURE3D_DESC tex3d_desc;
	};

	bool initial_data_used_in_hash;
	bool hash_contaminated;
	std::set<UINT> update_contamination;
	std::set<UINT> map_contamination;
	std::set<uint32_t> copy_contamination;
	CopySubresourceRegionContaminationMap region_contamination;

	ResourceHashInfo() :
		type(D3D11_RESOURCE_DIMENSION_UNKNOWN),
		initial_data_used_in_hash(false),
		hash_contaminated(false)
	{}

	struct ResourceHashInfo & operator= (D3D11_TEXTURE2D_DESC desc)
	{
		type = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
		tex2d_desc = desc;
		return *this;
	}

	struct ResourceHashInfo & operator= (D3D11_TEXTURE3D_DESC desc)
	{
		type = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
		tex3d_desc = desc;
		return *this;
	}
};


uint32_t CalcTexture2DDescHash(uint32_t initial_hash, const D3D11_TEXTURE2D_DESC *const_desc);
uint32_t CalcTexture3DDescHash(uint32_t initial_hash, const D3D11_TEXTURE3D_DESC *const_desc);

uint32_t CalcTexture2DDataHash(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);
uint32_t CalcTexture3DDataHash(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);

uint32_t GetOrigResourceHash(ID3D11Resource *resource);
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

void LogResourceDesc(const D3D11_BUFFER_DESC *desc);
void LogResourceDesc(const D3D11_TEXTURE1D_DESC *desc);
void LogResourceDesc(const D3D11_TEXTURE2D_DESC *desc);
void LogResourceDesc(const D3D11_TEXTURE3D_DESC *desc);
template <typename DescType>
static void LogDebugResourceDesc(DescType *desc)
{
	if (gLogDebug)
		LogResourceDesc(desc);
}
