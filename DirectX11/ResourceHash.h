#pragma once

#include <d3d11_1.h>
#include <stdint.h>
#include <tuple>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <atomic>

#include "util.h"
#include "DrawCallInfo.h"

// Tracks info about specific resource instances:
struct ResourceHandleInfo
{
	D3D11_RESOURCE_DIMENSION type;
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

	ResourceHandleInfo() :
		type(D3D11_RESOURCE_DIMENSION_UNKNOWN),
		hash(0),
		orig_hash(0),
		data_hash(0)
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
		D3D11_BUFFER_DESC buf_desc;
		D3D11_TEXTURE1D_DESC tex1d_desc;
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

	struct ResourceHashInfo & operator= (D3D11_BUFFER_DESC desc)
	{
		type = D3D11_RESOURCE_DIMENSION_BUFFER;
		buf_desc = desc;
		return *this;
	}

	struct ResourceHashInfo & operator= (D3D11_TEXTURE1D_DESC desc)
	{
		type = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
		tex1d_desc = desc;
		return *this;
	}

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

typedef std::unordered_map<uint32_t, struct ResourceHashInfo> ResourceInfoMap;

// This is a COM object that can be attached to a resource via
// ID3D11DeviceChild::SetPrivateDataInterface(), so that when the resource is
// released this class will be as well, giving us a way to reliably know when a
// resource is released and purge it from G->mResources to make sure that if
// the address is later reused we won't use stale data.
//
// This approach is a bit of an experiment - we know the performance of
// Set/GetPrivateData sucks and must be avoided in fast paths, but we never
// look this up so hopefully the overhead will be limited to resource creation
// time where it should be acceptable. An alternative approach would be what we
// do for shaders - purging stale entries after every shader creation anywhere
// in 3DMigoto, but I'm hoping that this will work well enough that we can
// adapt the shaders to use it as well, since the shader approach has a high
// risk of regressions.
//
// Should fix a bug occasionally seen where a custom resource bound to the
// pipeline would act as though it was a game resource when used with
// checktextureoverride, etc.
class ResourceReleaseTracker : public IUnknown
{
	std::atomic_ulong ref;
	ID3D11Resource *resource;

public:
	ResourceReleaseTracker(ID3D11Resource *resource);

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void **ppvObject);
	ULONG STDMETHODCALLTYPE AddRef(void);
	ULONG STDMETHODCALLTYPE Release(void);
};

uint32_t CalcTexture2DDescHash(uint32_t initial_hash, const D3D11_TEXTURE2D_DESC *const_desc);
uint32_t CalcTexture3DDescHash(uint32_t initial_hash, const D3D11_TEXTURE3D_DESC *const_desc);

uint32_t CalcTexture1DDataHash(const D3D11_TEXTURE1D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);
uint32_t CalcTexture2DDataHash(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, bool zero_padding = false);
uint32_t CalcTexture2DDataHashAccurate(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);
uint32_t CalcTexture3DDataHash(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData);

ResourceHandleInfo* GetResourceHandleInfo(ID3D11Resource *resource);
uint32_t GetOrigResourceHash(ID3D11Resource *resource);
uint32_t GetResourceHash(ID3D11Resource *resource);

void MarkResourceHashContaminated(ID3D11Resource *dest, UINT DstSubresource,
		ID3D11Resource *src, UINT srcSubresource, char type,
		UINT DstX, UINT DstY, UINT DstZ, const D3D11_BOX *SrcBox);

void UpdateResourceHashFromCPU(ID3D11Resource *resource,
	const void *data, UINT rowPitch, UINT depthPitch);

void PropagateResourceHash(ID3D11Resource *dst, ID3D11Resource *src);

bool MapTrackResourceHashUpdate(ID3D11Resource *pResource, UINT Subresource);

int StrResourceDesc(char *buf, size_t size, const D3D11_BUFFER_DESC *desc);
int StrResourceDesc(char *buf, size_t size, const D3D11_TEXTURE1D_DESC *desc);
int StrResourceDesc(char *buf, size_t size, const D3D11_TEXTURE2D_DESC *desc);
int StrResourceDesc(char *buf, size_t size, const D3D11_TEXTURE3D_DESC *desc);
int StrResourceDesc(char *buf, size_t size, struct ResourceHashInfo &info);

void LogResourceDesc(const D3D11_BUFFER_DESC *desc);
void LogResourceDesc(const D3D11_TEXTURE1D_DESC *desc);
void LogResourceDesc(const D3D11_TEXTURE2D_DESC *desc);
void LogResourceDesc(const D3D11_TEXTURE3D_DESC *desc);
void LogResourceDesc(ID3D11Resource *resource);
template <typename DescType>
static void LogDebugResourceDesc(DescType *desc)
{
	if (gLogDebug)
		LogResourceDesc(desc);
}
void LogViewDesc(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc);
void LogViewDesc(const D3D11_RENDER_TARGET_VIEW_DESC *desc);
void LogViewDesc(const D3D11_DEPTH_STENCIL_VIEW_DESC *desc);
void LogViewDesc(const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc);
template <typename DescType>
static void LogDebugViewDesc(DescType *desc)
{
	if (gLogDebug)
		LogViewDesc(desc);
}


// -----------------------------------------------------------------------------------------------
//                       Fuzzy Texture Override Matching Support
// -----------------------------------------------------------------------------------------------

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476202(v=vs.85).aspx
static wchar_t *ResourceDimensions[] = {
	L"UNKNOWN",
	L"BUFFER",
	L"TEXTURE1D",
	L"TEXTURE2D",
	L"TEXTURE3D",
};

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476259(v=vs.85).aspx
static wchar_t *ResourceUsage[] = {
	L"DEFAULT",
	L"IMMUTABLE",
	L"DYNAMIC",
	L"STAGING"
};
static wchar_t *TexResourceUsage(UINT usage)
{
	if (usage < sizeof(ResourceUsage) / sizeof(ResourceUsage[0]))
		return ResourceUsage[usage];
	return L"UNKNOWN";
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476106(v=vs.85).aspx
enum class ResourceCPUAccessFlags {
	INVALID = 0,
	WRITE   = 0x00010000,
	READ    = 0x00020000,
};
SENSIBLE_ENUM(ResourceCPUAccessFlags);
static EnumName_t<const wchar_t *, ResourceCPUAccessFlags> ResourceCPUAccessFlagNames[] = {
	{L"write", ResourceCPUAccessFlags::WRITE},
	{L"read", ResourceCPUAccessFlags::READ},
	{NULL, ResourceCPUAccessFlags::INVALID} // End of list marker
};

enum class ResourceMiscFlags {
	INVALID                          = 0,
	GENERATE_MIPS                    = 0x00000001,
	SHARED                           = 0x00000002,
	TEXTURECUBE                      = 0x00000004,
	DRAWINDIRECT_ARGS                = 0x00000010,
	BUFFER_ALLOW_RAW_VIEWS           = 0x00000020,
	BUFFER_STRUCTURED                = 0x00000040,
	RESOURCE_CLAMP                   = 0x00000080,
	SHARED_KEYEDMUTEX                = 0x00000100,
	GDI_COMPATIBLE                   = 0x00000200,
	SHARED_NTHANDLE                  = 0x00000800,
	RESTRICTED_CONTENT               = 0x00001000,
	RESTRICT_SHARED_RESOURCE         = 0x00002000,
	RESTRICT_SHARED_RESOURCE_DRIVER  = 0x00004000,
	GUARDED                          = 0x00008000,
	TILE_POOL                        = 0x00020000,
	TILED                            = 0x00040000,
	HW_PROTECTED                     = 0x00080000,
};
SENSIBLE_ENUM(ResourceMiscFlags);
static EnumName_t<const wchar_t *, ResourceMiscFlags> ResourceMiscFlagNames[] = {
	{L"generate_mips", ResourceMiscFlags::GENERATE_MIPS},
	{L"shared", ResourceMiscFlags::SHARED},
	{L"texturecube", ResourceMiscFlags::TEXTURECUBE},
	{L"drawindirect_args", ResourceMiscFlags::DRAWINDIRECT_ARGS},
	{L"buffer_allow_raw_views", ResourceMiscFlags::BUFFER_ALLOW_RAW_VIEWS},
	{L"buffer_structured", ResourceMiscFlags::BUFFER_STRUCTURED},
	{L"resource_clamp", ResourceMiscFlags::RESOURCE_CLAMP},
	{L"shared_keyedmutex", ResourceMiscFlags::SHARED_KEYEDMUTEX},
	{L"gdi_compatible", ResourceMiscFlags::GDI_COMPATIBLE},
	{L"shared_nthandle", ResourceMiscFlags::SHARED_NTHANDLE},
	{L"restricted_content", ResourceMiscFlags::RESTRICTED_CONTENT},
	{L"restrict_shared_resource", ResourceMiscFlags::RESTRICT_SHARED_RESOURCE},
	{L"restrict_shared_resource_driver", ResourceMiscFlags::RESTRICT_SHARED_RESOURCE_DRIVER},
	{L"guarded", ResourceMiscFlags::GUARDED},
	{L"tile_pool", ResourceMiscFlags::TILE_POOL},
	{L"tiled", ResourceMiscFlags::TILED},
	{L"hw_protected", ResourceMiscFlags::HW_PROTECTED},
	{NULL, ResourceMiscFlags::INVALID} // End of list marker
};

enum class FuzzyMatchOp {
	ALWAYS,
	EQUAL,
	LESS,
	LESS_EQUAL,
	GREATER,
	GREATER_EQUAL,
	NOT_EQUAL,
};

enum class FuzzyMatchOperandType {
	VALUE,
	WIDTH,      // Width, Height & Depth useful for checking
	HEIGHT,     // for square/cube/rectangular textures.
	DEPTH,
	ARRAY,      // Probably not useful, but similar to depth
	RES_WIDTH,  // Useful for detecting full screen buffers
	RES_HEIGHT, // including arbitrary multiples of the resolution
};

class FuzzyMatch {
	bool matches_common(UINT lhs, UINT effective) const;
public:
	FuzzyMatchOp op;
	FuzzyMatchOperandType rhs_type1;
	FuzzyMatchOperandType rhs_type2;

	// TODO: Support more operand types, such as texture/resolution
	// width/height. Maybe for advanced usage even allow an operand to be
	// an ini param so it can be changed on the fly (might be useful for
	// MEA to replace the mid-game profile switch, but I'd be surprised if
	// there isn't a better way to achieve that).
	UINT val;
	UINT mask;
	UINT numerator;
	UINT denominator;

	FuzzyMatch();
	template <typename DescType>
	bool matches(UINT lhs, const DescType *desc) const;
	bool matches_uint(UINT lhs) const;
};

// Forward declaration to resolve circular dependency. One of these days we
// really need to start splitting everything out of globals and making an
// effort to reduce our cyclic dependencies. Downside of this is it
// necessitates using a pointer for the textureoverride contained in the below
// struct since the definition needs to be known to statically contain one, but
// we'll hold off until post 1.3 since the FrameAnalysisOptions needs to go in
// FrameAnalysis.h to make that work, and that is an area that diverged from 1.2:
struct TextureOverride;

class FuzzyMatchResourceDesc {
private:
	template <typename DescType>
	bool check_common_resource_fields(const DescType *desc) const;
	template <typename DescType>
	bool check_common_texture_fields(const DescType *desc) const;
public:
	struct TextureOverride *texture_override;

	bool matches_buffer;
	bool matches_tex1d;
	bool matches_tex2d;
	bool matches_tex3d;

	// TODO: Consider making this a vector we iterate over so we only
	// process tests specified in this texture override
	FuzzyMatch Usage;               // Common
	FuzzyMatch BindFlags;           // Common
	FuzzyMatch CPUAccessFlags;      // Common
	FuzzyMatch MiscFlags;           // Common
	FuzzyMatch ByteWidth;           // Buffer+StructuredBuffer
	FuzzyMatch StructureByteStride; //        StructuredBuffer XXX: I think I may have seen this later set to 0 if it was initially set on a regular buffer?
	FuzzyMatch MipLevels;           // 1D+2D+3D XXX: Need to check what happens for resources created with mips=0 and mips generated later
	FuzzyMatch Format;              // 1D+2D+3D
	FuzzyMatch Width;               // 1D+2D+3D
	FuzzyMatch Height;              //    2D+3D
	FuzzyMatch Depth;               //       3D
	FuzzyMatch ArraySize;           // 1D+2D
	FuzzyMatch SampleDesc_Count;    //    2D
	FuzzyMatch SampleDesc_Quality;  //    2D    XXX Can anything change here if count=1?

	FuzzyMatchResourceDesc(std::wstring section);
	~FuzzyMatchResourceDesc();
	bool matches(const D3D11_BUFFER_DESC *desc) const;
	bool matches(const D3D11_TEXTURE1D_DESC *desc) const;
	bool matches(const D3D11_TEXTURE2D_DESC *desc) const;
	bool matches(const D3D11_TEXTURE3D_DESC *desc) const;

	void set_resource_type(D3D11_RESOURCE_DIMENSION type);
	bool update_types_matched();
};
bool TextureOverrideLess(const struct TextureOverride &lhs, const struct TextureOverride &rhs);
struct FuzzyMatchResourceDescLess {
	bool operator() (const std::shared_ptr<FuzzyMatchResourceDesc> &lhs, const std::shared_ptr<FuzzyMatchResourceDesc> &rhs) const;
};
// This set is sorted because multiple fuzzy texture overrides may match a
// given resource, but we want to make sure we always process them in the same
// order for consistent results.
typedef std::set<std::shared_ptr<FuzzyMatchResourceDesc>, FuzzyMatchResourceDescLess> FuzzyTextureOverrides;

typedef std::vector<TextureOverride*> TextureOverrideMatches;

template <typename DescType>
void find_texture_overrides(uint32_t hash, const DescType *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info);
void find_texture_overrides_for_resource(ID3D11Resource *resource, TextureOverrideMatches *matches, DrawCallInfo *call_info);
