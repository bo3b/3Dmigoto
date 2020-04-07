#pragma once
#include "util.h"
#include <stdint.h>
#include <tuple>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <algorithm>
#include <atomic>
#include <nvapi.h>
#include "DrawCallInfo.h"
#include <d3d9.h>
namespace D3D9Wrapper {
	class IDirect3DResource9;
}
struct D3D2DTEXTURE_DESC {
	::D3DFORMAT				Format;
	::D3DRESOURCETYPE		Type;
	DWORD							Usage;
	::D3DPOOL				Pool;
	::D3DMULTISAMPLE_TYPE	MultiSampleType;
	DWORD							MultiSampleQuality;
	UINT							Width;
	UINT							Height;
	//additional
	UINT							Levels;
	D3D2DTEXTURE_DESC() {};
	D3D2DTEXTURE_DESC(::IDirect3DSurface9 *sur) {
		::D3DSURFACE_DESC desc;
		sur->GetDesc(&desc);
		Format = desc.Format;
		Type = ::D3DRESOURCETYPE::D3DRTYPE_SURFACE;
		Usage = desc.Usage;
		Pool = desc.Pool;
		MultiSampleType = desc.MultiSampleType;
		MultiSampleQuality = desc.MultiSampleQuality;
		Width = desc.Width;
		Height = desc.Height;
		Levels = 1;
	};
	D3D2DTEXTURE_DESC(::IDirect3DTexture9 *tex) {
		::D3DSURFACE_DESC desc;
		tex->GetLevelDesc(0, &desc);
		Format = desc.Format;
		Type = ::D3DRESOURCETYPE::D3DRTYPE_TEXTURE;
		Usage = desc.Usage;
		Pool = desc.Pool;
		MultiSampleType = desc.MultiSampleType;
		MultiSampleQuality = desc.MultiSampleQuality;
		Width = desc.Width;
		Height = desc.Height;
		Levels = tex->GetLevelCount();
	};
	D3D2DTEXTURE_DESC(::IDirect3DCubeTexture9 *tex) {
		::D3DSURFACE_DESC desc;
		tex->GetLevelDesc(0, &desc);
		Format = desc.Format;
		Type = ::D3DRESOURCETYPE::D3DRTYPE_CUBETEXTURE;
		Usage = desc.Usage;
		Pool = desc.Pool;
		MultiSampleType = desc.MultiSampleType;
		MultiSampleQuality = desc.MultiSampleQuality;
		Width = desc.Width;
		Height = desc.Height;
		Levels = tex->GetLevelCount();
	};
};
struct D3D3DTEXTURE_DESC {
	::D3DFORMAT				Format;
	::D3DRESOURCETYPE		Type;
	DWORD							Usage;
	::D3DPOOL				Pool;
	UINT							Width;
	UINT							Height;
	UINT							Depth;
	//additional
	UINT							Levels;
	D3D3DTEXTURE_DESC() {};
	D3D3DTEXTURE_DESC(::IDirect3DVolumeTexture9 *tex) {
		::D3DVOLUME_DESC desc;
		tex->GetLevelDesc(0, &desc);
		Format = desc.Format;
		Type = ::D3DRESOURCETYPE::D3DRTYPE_VOLUMETEXTURE;
		Usage = desc.Usage;
		Pool = desc.Pool;
		Width = desc.Width;
		Height = desc.Height;
		Depth = desc.Depth;
		Levels = tex->GetLevelCount();
	};
};
// Tracks info about specific resource instances:
struct ResourceHandleInfo
{
	::D3DRESOURCETYPE type;
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
	D3D2DTEXTURE_DESC desc2D;
	D3D3DTEXTURE_DESC desc3D;

	ResourceHandleInfo() :
		type(::D3DRESOURCETYPE(-1)),
		hash(0),
		orig_hash(0),
		data_hash(0)
	{}
};

struct CopySubresourceRegionContamination
{
	bool partial;
	::D3DBOX DstBox;
	::D3DBOX SrcBox;

	CopySubresourceRegionContamination() :
		partial(false),
		DstBox({ 0, 0, UINT_MAX, UINT_MAX, 0, UINT_MAX }),
		SrcBox({ 0, 0, UINT_MAX, UINT_MAX, 0, UINT_MAX })
	{}

	void Update(bool partial, const ::D3DBOX *DstBox, const ::D3DBOX *SrcBox)
	{
		this->partial = this->partial || partial;
		if (DstBox) {
			this->DstBox.Left = max(this->DstBox.Left, DstBox->Left);
			this->DstBox.Top = max(this->DstBox.Top, DstBox->Top);
			this->DstBox.Front = max(this->DstBox.Front, DstBox->Front);

			this->DstBox.Right = min(this->DstBox.Right, DstBox->Right);
			this->DstBox.Bottom = min(this->DstBox.Bottom, DstBox->Bottom);
			this->DstBox.Back = min(this->DstBox.Back, DstBox->Back);
		}


		if (SrcBox) {
			this->SrcBox.Left = max(this->SrcBox.Left, SrcBox->Left);
			this->SrcBox.Top = max(this->SrcBox.Top, SrcBox->Top);
			this->SrcBox.Front = max(this->SrcBox.Front, SrcBox->Front);

			this->SrcBox.Right = min(this->SrcBox.Right, SrcBox->Right);
			this->SrcBox.Bottom = min(this->SrcBox.Bottom, SrcBox->Bottom);
			this->SrcBox.Back = min(this->SrcBox.Back, SrcBox->Back);
		}
	}
};
// Create a map that uses a hashable tuple of five integers as they key (Hey C++,
// this is something Python can do with what? ... 0 lines of boilerplate?)
typedef std::tuple<uint32_t, UINT, UINT> CopySubresourceRegionContaminationMapKey;
template<> struct std::hash<CopySubresourceRegionContaminationMapKey>
{
	size_t operator()(CopySubresourceRegionContaminationMapKey const &key)
	{
		// http://stackoverflow.com/questions/4948780/magic-number-in-boosthash-combine
		size_t seed = 0;
		seed ^= std::hash<uint32_t>()(std::get<0>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<UINT>()(std::get<1>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<UINT>()(std::get<2>(key)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}
};
typedef std::map<CopySubresourceRegionContaminationMapKey, CopySubresourceRegionContamination>
CopySubresourceRegionContaminationMap;
// Tracks info about resources by their *original* hash. Primarily for stat collection:
struct ResourceHashInfo
{
	::D3DRESOURCETYPE type;
	::D3DVERTEXBUFFER_DESC vbuf_desc;
	::D3DINDEXBUFFER_DESC ibuf_desc;
	D3D2DTEXTURE_DESC desc2D;
	D3D3DTEXTURE_DESC desc3D;

	bool initial_data_used_in_hash;
	bool hash_contaminated;
	std::set<UINT> lock_contamination;
	std::set<uint32_t> copy_contamination;
	CopySubresourceRegionContaminationMap region_contamination;

	ResourceHashInfo() :
		type((::D3DRESOURCETYPE)-1),
		initial_data_used_in_hash(false),
		hash_contaminated(false)
	{}

	struct ResourceHashInfo & operator= (::D3DVERTEXBUFFER_DESC desc)
	{
		type = ::D3DRTYPE_VERTEXBUFFER;
		vbuf_desc = desc;
		return *this;
	}
	struct ResourceHashInfo & operator= (::D3DINDEXBUFFER_DESC desc)
	{
		type = ::D3DRTYPE_INDEXBUFFER;
		ibuf_desc = desc;
		return *this;
	}

	struct ResourceHashInfo & operator= (D3D2DTEXTURE_DESC desc)
	{
		type = desc.Type;
		desc2D = desc;
		return *this;
	}

	struct ResourceHashInfo & operator= (D3D3DTEXTURE_DESC desc)
	{
		type = ::D3DRTYPE_VOLUMETEXTURE;
		desc3D = desc;
		return *this;
	}
};
typedef std::unordered_map<uint32_t, struct ResourceHashInfo> ResourceInfoMap;
uint32_t CalcDescHash(uint32_t initial_hash, const D3D2DTEXTURE_DESC *const_desc);
uint32_t CalcDescHash(uint32_t initial_hash, const D3D3DTEXTURE_DESC *const_desc);

uint32_t Calc2DDataHash(const D3D2DTEXTURE_DESC *pDesc, const ::D3DLOCKED_BOX *pLockedRect);
uint32_t Calc3DDataHash(const D3D3DTEXTURE_DESC *pDesc, const ::D3DLOCKED_BOX *pLockedBox);
uint32_t Calc2DDataHashAccurate(const D3D2DTEXTURE_DESC *pDesc, const ::D3DLOCKED_BOX *pLockedRect);

ResourceHandleInfo* GetResourceHandleInfo(D3D9Wrapper::IDirect3DResource9 *resource);
uint32_t GetOrigResourceHash(D3D9Wrapper::IDirect3DResource9  *resource);
uint32_t GetResourceHash(D3D9Wrapper::IDirect3DResource9  *resource);

void MarkResourceHashContaminated(D3D9Wrapper::IDirect3DResource9 *dest, UINT dstLevel,
	D3D9Wrapper::IDirect3DResource9 *src, UINT srcLevel, char type,
	::D3DBOX *DstBox, const ::D3DBOX *SrcBox);

void UpdateResourceHashFromCPU(D3D9Wrapper::IDirect3DResource9 *resource,
	::D3DLOCKED_BOX *pLockedRect);

void PropagateResourceHash(D3D9Wrapper::IDirect3DResource9 *dst, D3D9Wrapper::IDirect3DResource9 *src);

bool LockTrackResourceHashUpdate(D3D9Wrapper::IDirect3DResource9 *pResource, UINT Level = 0);

int StrResourceDesc(char *buf, size_t size, ::D3DVERTEXBUFFER_DESC *desc);
int StrResourceDesc(char *buf, size_t size, ::D3DINDEXBUFFER_DESC *desc);
int StrResourceDesc(char *buf, size_t size, D3D3DTEXTURE_DESC *desc);
int StrResourceDesc(char *buf, size_t size, D3D2DTEXTURE_DESC *desc);
int StrResourceDesc(char *buf, size_t size, struct ResourceHashInfo &info);

void LogResourceDesc(const ::D3DVERTEXBUFFER_DESC *desc);
void LogResourceDesc(const ::D3DINDEXBUFFER_DESC *desc);
void LogResourceDesc(const D3D2DTEXTURE_DESC *desc);
void LogResourceDesc(const D3D3DTEXTURE_DESC *desc);
template <typename DescType>
static void LogDebugResourceDesc(DescType *desc)
{
	if (gLogDebug)
		LogResourceDesc(desc);
}

// -----------------------------------------------------------------------------------------------
//                       Fuzzy Texture Override Matching Support
// -----------------------------------------------------------------------------------------------

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476202(v=vs.85).aspx
static wchar_t *ResourceType[] = {
	L"VERTEXBUFFER",
	L"INDEXBUFFER",
	L"TEXTURE",
	L"CUBETEXTURE",
	L"VOLUMETEXTURE",
	L"SURFACE",
	L"VOLUME"
};

static wchar_t *MultisampleType[] = {
	L"NONE",
	L"NONMASKABLE",
	L"2_SAMPLES",
	L"3_SAMPLES",
	L"4_SAMPLES",
	L"5_SAMPLES",
	L"6_SAMPLES",
	L"7_SAMPLES",
	L"8_SAMPLES",
	L"9_SAMPLES",
	L"10_SAMPLES",
	L"11_SAMPLES",
	L"12_SAMPLES",
	L"13_SAMPLES",
	L"14_SAMPLES",
	L"15_SAMPLES",
	L"16_SAMPLES"
};

static wchar_t *ResPools[] = {
	L"DEFAULT",
	L"MANAGED",
	L"SYSTEMMEM",
	L"SCRATCH"
};

static wchar_t *ResourceUsage[] = {
	L"AUTOGENMIPMAP",
	L"DEPTHSTENCIL",
	L"DMAP",
	L"DONOTCLIP"
	L"DYNAMIC",
	L"NONSECURE",
	L"NPATCHES",
	L"POINTS",
	L"RENDERTARGET",
	L"RTPATCHES"
	L"SOFTWAREPROCESSING",
	L"TEXTAPI",
	L"WRITEONLY",
	L"RESTRICTED_CONTENT",
	L"RESTRICT_SHARED_RESOURCE",
	L"RESTRICT_SHARED_RESOURCE_DRIVER",
};

enum class Usage {
	INVALID = 0,
	RENDERTARGET = 0x00000001L,
	DEPTHSTENCIL = 0x00000002L,
	DYNAMIC = 0x00000200L,
	NONSECURE = 0x00800000L,
	AUTOGENMIPMAP = 0x00000400L,
	DMAP = 0x00004000L,
	QUERY_LEGACYBUMPMAP = 0x00008000L,
	QUERY_SRGBREAD = 0x00010000L,
	QUERY_FILTER = 0x00020000L,
	QUERY_SRGBWRITE = 0x00040000L,
	QUERY_POSTPIXELSHADER_BLENDING = 0x00080000L,
	QUERY_VERTEXTEXTURE = 0x00100000L,
	QUERY_WRAPANDMIP = 0x00200000L,
	WRITEONLY = 0x00000008L,
	SOFTWAREPROCESSING = 0x00000010L,
	DONOTCLIP = 0x00000020L,
	POINTS = 0x00000040L,
	RTPATCHES = 0x00000080L,
	NPATCHES = 0x00000100L,
	TEXTAPI = 0x10000000L,
	RESTRICTED_CONTENT = 0x00000800L,
	RESTRICT_SHARED_RESOURCE = 0x00002000L,
	RESTRICT_SHARED_RESOURCE_DRIVER = 0x00001000L
};
static EnumName_t<const wchar_t *, Usage> UsageNames[] = {

	{ L"rendertarget", Usage::RENDERTARGET },
	{ L"depthstencil", Usage::DEPTHSTENCIL },
	{ L"dynamic", Usage::DYNAMIC },
	{ L"nonsecure", Usage::NONSECURE },
	{ L"autogenmipmap", Usage::AUTOGENMIPMAP },
	{ L"dmap", Usage::DMAP },
	{ L"query_legacybumpmap", Usage::QUERY_LEGACYBUMPMAP },
	{ L"query_srgbread", Usage::QUERY_SRGBREAD },
	{ L"query_filter", Usage::QUERY_FILTER },
	{ L"query_srgbwrite", Usage::QUERY_SRGBWRITE },
	{ L"query_postpixelshader_blending", Usage::QUERY_POSTPIXELSHADER_BLENDING },
	{ L"query_vertextexture", Usage::QUERY_VERTEXTEXTURE },
	{ L"query_wrapandmip", Usage::QUERY_WRAPANDMIP },
	{ L"writeonly", Usage::WRITEONLY },
	{ L"softwareprocessing", Usage::SOFTWAREPROCESSING },
	{ L"donotclip", Usage::DONOTCLIP },
	{ L"points", Usage::POINTS },
	{ L"rtpatches", Usage::RTPATCHES },
	{ L"npatches", Usage::NPATCHES },
	{ L"textapi", Usage::TEXTAPI },
	{ L"restricted_content", Usage::RESTRICTED_CONTENT },
	{ L"restrict_shared_resource", Usage::RESTRICT_SHARED_RESOURCE },
	{ L"restrict_shared_resource_driver", Usage::RESTRICT_SHARED_RESOURCE_DRIVER },
	{ NULL, Usage::INVALID } // End of list marker
};


static wchar_t *TexResourceUsage(UINT usage)
{
	if (usage < sizeof(ResourceUsage) / sizeof(ResourceUsage[0]))
		return ResourceUsage[usage];
	return L"UNKNOWN";
}

enum class FVFFlag {
	INVALID = 0,
	RESERVED0 = 0x001,
	POSITION_MASK = 0x400E,
	XYZ = 0x002,
	XYZRHW = 0x004,
	XYZB1 = 0x006,
	XYZB2 = 0x008,
	XYZB3 = 0x00a,
	XYZB4 = 0x00c,
	XYZB5 = 0x00e,
	XYZW = 0x4002,
	NORMAL = 0x010,
	PSIZE = 0x020,
	DIFFUSE = 0x040,
	SPECULAR = 0x080,
	TEXCOUNT_MASK = 0xf00,
	TEXCOUNT_SHIFT = 8,
	TEX0 = 0x000,
	TEX1 = 0x100,
	TEX2 = 0x200,
	TEX3 = 0x300,
	TEX4 = 0x400,
	TEX5 = 0x500,
	TEX6 = 0x600,
	TEX7 = 0x700,
	TEX8 = 0x800,
	LASTBETA_UBYTE4 = 0x1000,
	LASTBETA_D3DCOLOR = 0x8000,
	RESERVED2 = 0x6000
};
static EnumName_t<const wchar_t *, FVFFlag> FVFFlagNames[] = {
	{ L"reserved", FVFFlag::RESERVED0 },
	{ L"position_mask", FVFFlag::POSITION_MASK },
	{ L"xyz", FVFFlag::XYZ },
	{ L"xyzrhw", FVFFlag::XYZRHW },
	{ L"xyzb1", FVFFlag::XYZB1 },
	{ L"xyzb2", FVFFlag::XYZB2 },
	{ L"xyzb3", FVFFlag::XYZB3 },
	{ L"xyzb4", FVFFlag::XYZB4 },
	{ L"xyzb5", FVFFlag::XYZB5 },
	{ L"xyzw", FVFFlag::XYZW },
	{ L"normal", FVFFlag::NORMAL },
	{ L"psize", FVFFlag::PSIZE },
	{ L"diffuse", FVFFlag::DIFFUSE },
	{ L"specular", FVFFlag::SPECULAR },
	{ L"texcount_mask", FVFFlag::TEXCOUNT_MASK },
	{ L"texcount_shift", FVFFlag::TEXCOUNT_SHIFT },
	{ L"tex0", FVFFlag::TEX0 },
	{ L"tex1", FVFFlag::TEX1 },
	{ L"tex2", FVFFlag::TEX2 },
	{ L"tex3", FVFFlag::TEX3 },
	{ L"tex4", FVFFlag::TEX4 },
	{ L"tex5", FVFFlag::TEX5 },
	{ L"tex6", FVFFlag::TEX6 },
	{ L"tex7", FVFFlag::TEX7 },
	{ L"tex8", FVFFlag::TEX8 },
	{ L"lastbeta_ubyte4", FVFFlag::LASTBETA_UBYTE4 },
	{ L"lastbeta_d3dcolor", FVFFlag::LASTBETA_D3DCOLOR },
	{ L"reserved2", FVFFlag::RESERVED2 },
	{ NULL, FVFFlag::INVALID } // End of list marker
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
	RES_WIDTH,  // Useful for detecting full screen buffers
	RES_HEIGHT, // including arbitrary multiples of the resolution
};
class FuzzyMatch{
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
	bool check_common_surface_fields(const DescType *desc) const;
	template <typename DescType>
	bool check_common_buffer_fields(const DescType *desc) const;
public:
	struct TextureOverride *texture_override;

	int priority;

	bool matches_vbuffer;
	bool matches_ibuffer;
	bool matches_tex;
	bool matches_cubetex;
	bool matches_volumetex;
	bool matches_surface;

	// TODO: Consider making this a vector we iterate over so we only
	// process tests specified in this texture override
	FuzzyMatch Usage;               // Common
	FuzzyMatch Pool;				// Common
	FuzzyMatch Type;				// Common
	FuzzyMatch Size;           // iBuffer+vBuffer
	FuzzyMatch FVF;				//vBuffer
	FuzzyMatch Format;              // 1D+2D+3D
	FuzzyMatch Width;               // 1D+2D+3D
	FuzzyMatch Height;              //    2D+3D
	FuzzyMatch Depth;               //       3D
	FuzzyMatch MultiSampleType;    //    2D
	FuzzyMatch MultiSampleQuality;  //    2D    XXX Can anything change here if count=1?
	FuzzyMatch Levels;           // 1D+2D+3D XXX: Need to check what happens for resources created with mips=0 and mips generated later

	FuzzyMatchResourceDesc(std::wstring section);
	~FuzzyMatchResourceDesc();
	bool matches(const ::D3DVERTEXBUFFER_DESC *desc) const;
	bool matches(const ::D3DINDEXBUFFER_DESC *desc) const;
	bool matches(const D3D3DTEXTURE_DESC *desc) const;
	bool matches(const D3D2DTEXTURE_DESC *desc) const;
	void set_resource_type(::D3DRESOURCETYPE type);
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
void find_texture_overrides_for_resource(::IDirect3DResource9 *resource, ResourceHandleInfo *info, TextureOverrideMatches *matches, DrawCallInfo *call_info);
