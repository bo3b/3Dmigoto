#pragma once

#include <d3d11.h>
#include <ctime>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "util.h"
#include "CommandList.h"
#include "ResourceHash.h"
#include "DLLMainHook.h"

// Resolve circular include dependency between Globals.h ->
// CommandList.h -> HackerContext.h -> Globals.h
class CommandListCommand;
typedef std::vector<std::shared_ptr<CommandListCommand>> CommandList;

enum HuntingMode {
	HUNTING_MODE_DISABLED = 0,
	HUNTING_MODE_ENABLED = 1,
	HUNTING_MODE_SOFT_DISABLED = 2,
};

const int MARKING_MODE_SKIP = 0;
const int MARKING_MODE_MONO = 1;
const int MARKING_MODE_ORIGINAL = 2;
const int MARKING_MODE_ZERO = 3;
const int MARKING_MODE_PINK = 4;

enum class ShaderHashType {
	INVALID = -1,
	FNV,
	EMBEDDED,
	BYTECODE,
};
static EnumName_t<wchar_t *, ShaderHashType> ShaderHashNames[] = {
	{L"3dmigoto", ShaderHashType::FNV},
	{L"embedded", ShaderHashType::EMBEDDED},
	{L"bytecode", ShaderHashType::BYTECODE},
	{NULL, ShaderHashType::INVALID} // End of list marker
};

// Key is index/vertex buffer, value is hash key.
typedef std::unordered_map<ID3D11Buffer *, uint32_t> DataBufferMap;

// Source compiled shaders.
typedef std::unordered_map<UINT64, std::string> CompiledShaderMap;

// Strategy: This OriginalShaderInfo record and associated map is to allow us to keep track of every
//	pixelshader and vertexshader that are compiled from hlsl text from the ShaderFixes
//	folder.  This keeps track of the original shader information using the ID3D11VertexShader*
//	or ID3D11PixelShader* as a master key to the key map.
//	We are using the base class of ID3D11DeviceChild* since both descend from that, and that allows
//	us to use the same structure for Pixel and Vertex shaders both.

// Info saved about originally overridden shaders passed in by the game in CreateVertexShader or
// CreatePixelShader that have been loaded as HLSL
//	shaderType is "vs" or "ps" or maybe later "gs" (type wstring for file name use)
//	shaderModel is only filled in when a shader is replaced.  (type string for old D3 API use)
//	linkage is passed as a parameter, seems to be rarely if ever used.
//	byteCode is the original shader byte code passed in by game, or recompiled by override.
//	timeStamp allows reloading/recompiling only modified shaders
//	replacement is either ID3D11VertexShader or ID3D11PixelShader
//  found is used to revert shaders that are deleted from ShaderFixes
//  infoText is shown in the OSD when the shader is actively selected.
struct OriginalShaderInfo
{
	UINT64 hash;
	std::wstring shaderType;
	std::string shaderModel;
	ID3D11ClassLinkage* linkage;
	ID3DBlob* byteCode;
	FILETIME timeStamp;
	ID3D11DeviceChild* replacement;
	bool found;
	std::wstring infoText;
};

// Key is the overridden shader that was given back to the game at CreateVertexShader (vs or ps)
typedef std::unordered_map<ID3D11DeviceChild *, OriginalShaderInfo> ShaderReloadMap;

// Key is vertexshader, value is hash key.
typedef std::unordered_map<ID3D11VertexShader *, UINT64> VertexShaderMap;
typedef std::unordered_map<ID3D11VertexShader *, ID3D11VertexShader *> VertexShaderReplacementMap;

// Key is pixelshader, value is hash key.
typedef std::unordered_map<ID3D11PixelShader *, UINT64> PixelShaderMap;
typedef std::unordered_map<ID3D11PixelShader *, ID3D11PixelShader *> PixelShaderReplacementMap;

typedef std::unordered_map<ID3D11ComputeShader *, UINT64> ComputeShaderMap;
typedef std::unordered_map<ID3D11ComputeShader *, ID3D11ComputeShader *> ComputeShaderReplacementMap;

typedef std::unordered_map<ID3D11HullShader *, UINT64> HullShaderMap;
typedef std::unordered_map<ID3D11HullShader *, ID3D11HullShader *> HullShaderReplacementMap;

typedef std::unordered_map<ID3D11DomainShader *, UINT64> DomainShaderMap;
typedef std::unordered_map<ID3D11DomainShader *, ID3D11DomainShader *> DomainShaderReplacementMap;

typedef std::unordered_map<ID3D11GeometryShader *, UINT64> GeometryShaderMap;
typedef std::unordered_map<ID3D11GeometryShader *, ID3D11GeometryShader *> GeometryShaderReplacementMap;

enum class FrameAnalysisOptions {
	INVALID         = 0,
	DUMP_RT         = 0x00000001,
	DUMP_RT_JPS     = 0x00000002,
	DUMP_RT_DDS     = 0x00000004,
	DUMP_RT_MASK    = 0x00000007,
	CLEAR_RT        = 0x00000008,
	DUMP_DEPTH      = 0x00000010,
	DUMP_DEPTH_JPS  = 0x00000020,
	DUMP_DEPTH_DDS  = 0x00000040,
	DUMP_DEPTH_MASK = 0x00000070,
	FILENAME_REG    = 0x00000080,
	DUMP_TEX        = 0x00000100,
	DUMP_TEX_JPS    = 0x00000200,
	DUMP_TEX_DDS    = 0x00000400,
	DUMP_TEX_MASK   = 0x00000700,
	DUMP_XXX        = 0x00800111,
	DUMP_XXX_JPS    = 0x00800222,
	DUMP_XXX_DDS    = 0x00800444,
	DUMP_XXX_MASK   = 0x00800777,
	PERSIST         = 0x00000800, // Used by shader/texture triggers
	STEREO          = 0x00001000,
	MONO            = 0x00002000,
	STEREO_MASK     = 0x00003000,
	DUMP_CB_BIN     = 0x00004000,
	DUMP_CB_TXT     = 0x00008000,
	DUMP_CB_MASK    = 0x0000c000,
	DUMP_VB_BIN     = 0x00010000,
	DUMP_VB_TXT     = 0x00020000,
	DUMP_VB_MASK    = 0x00030000,
	DUMP_IB_BIN     = 0x00040000,
	DUMP_IB_TXT     = 0x00080000,
	DUMP_IB_MASK    = 0x000c0000,
	DUMP_XX_BIN     = 0x00854505, // Includes anything that can be a buffer: CB, VB, IB, SRVs, RTs & UAVs
	DUMP_XX_TXT     = 0x008a8000, // Not including SRVs, RTs or UAVs for now
	FILENAME_HANDLE = 0x00100000,
	LOG             = 0x00200000,
	HOLD            = 0x00400000,
	DUMP_ON_UNMAP   = 0x00800000, // XXX: For now including in all XX masks
};
SENSIBLE_ENUM(FrameAnalysisOptions);
static EnumName_t<wchar_t *, FrameAnalysisOptions> FrameAnalysisOptionNames[] = {
	{L"dump_rt", FrameAnalysisOptions::DUMP_RT},
	{L"dump_rt_jps", FrameAnalysisOptions::DUMP_RT_JPS},
	{L"dump_rt_dds", FrameAnalysisOptions::DUMP_RT_DDS},
	{L"clear_rt", FrameAnalysisOptions::CLEAR_RT},
	{L"dump_depth", FrameAnalysisOptions::DUMP_DEPTH},
	{L"dump_depth_jps", FrameAnalysisOptions::DUMP_DEPTH_JPS}, // Doesn't work yet
	{L"dump_depth_dds", FrameAnalysisOptions::DUMP_DEPTH_DDS},
	{L"dump_tex", FrameAnalysisOptions::DUMP_TEX},
	{L"dump_tex_jps", FrameAnalysisOptions::DUMP_TEX_JPS},
	{L"dump_tex_dds", FrameAnalysisOptions::DUMP_TEX_DDS},
	{L"persist", FrameAnalysisOptions::PERSIST},
	{L"stereo", FrameAnalysisOptions::STEREO},
	{L"mono", FrameAnalysisOptions::MONO},
	{L"filename_reg", FrameAnalysisOptions::FILENAME_REG},
	{L"dump_cb", FrameAnalysisOptions::DUMP_CB_BIN},
	{L"dump_cb_txt", FrameAnalysisOptions::DUMP_CB_TXT},
	{L"dump_vb", FrameAnalysisOptions::DUMP_VB_BIN},
	{L"dump_vb_txt", FrameAnalysisOptions::DUMP_VB_TXT},
	{L"dump_ib", FrameAnalysisOptions::DUMP_IB_BIN},
	{L"dump_ib_txt", FrameAnalysisOptions::DUMP_IB_TXT},
	{L"filename_handle", FrameAnalysisOptions::FILENAME_HANDLE},
	{L"log", FrameAnalysisOptions::LOG},
	{L"hold", FrameAnalysisOptions::HOLD},
	{L"dump_on_unmap", FrameAnalysisOptions::DUMP_ON_UNMAP},
	{NULL, FrameAnalysisOptions::INVALID} // End of list marker
};

enum class DepthBufferFilter {
	INVALID = -1,
	NONE,
	DEPTH_ACTIVE,
	DEPTH_INACTIVE,
};
static EnumName_t<wchar_t *, DepthBufferFilter> DepthBufferFilterNames[] = {
	{L"none", DepthBufferFilter::NONE},
	{L"depth_active", DepthBufferFilter::DEPTH_ACTIVE},
	{L"depth_inactive", DepthBufferFilter::DEPTH_INACTIVE},
	{NULL, DepthBufferFilter::INVALID} // End of list marker
};

struct ShaderOverride {
	float separation;
	float convergence;
	bool skip;
	std::vector<int> iterations; // Only for separation changes, not shaders.
	std::vector<UINT64> indexBufferFilter;
	DepthBufferFilter depth_filter;
	UINT64 partner_hash;
	FrameAnalysisOptions analyse_options;
	char model[20]; // More than long enough for even ps_4_0_level_9_0

	CommandList command_list;
	CommandList post_command_list;

	ShaderOverride() :
		separation(FLT_MAX),
		convergence(FLT_MAX),
		skip(false),
		depth_filter(DepthBufferFilter::NONE),
		partner_hash(0),
		analyse_options(FrameAnalysisOptions::INVALID)
	{
		model[0] = '\0';
	}
};
typedef std::unordered_map<UINT64, struct ShaderOverride> ShaderOverrideMap;

struct TextureOverride {
	int stereoMode;
	int format;
	int width;
	int height;
	std::vector<int> iterations;
	FrameAnalysisOptions analyse_options;
	bool expand_region_copy;
	bool deny_cpu_read;
	float filter_index;

	CommandList command_list;
	CommandList post_command_list;

	TextureOverride() :
		stereoMode(-1),
		format(-1),
		width(-1),
		height(-1),
		analyse_options(FrameAnalysisOptions::INVALID),
		expand_region_copy(false),
		deny_cpu_read(false),
		filter_index(1.0)
	{}
};
typedef std::unordered_map<uint32_t, struct TextureOverride> TextureOverrideMap;

struct ShaderInfoData
{
	// All are std::map or std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<int, std::set<ID3D11Resource *>> ResourceRegisters;
	std::set<UINT64> PartnerShader;
	std::vector<std::set<ID3D11Resource *>> RenderTargets;
	std::set<ID3D11Resource *> DepthTargets;
};

enum class GetResolutionFrom {
	INVALID       = -1,
	SWAP_CHAIN,
	DEPTH_STENCIL,
};
static EnumName_t<wchar_t *, GetResolutionFrom> GetResolutionFromNames[] = {
	{L"swap_chain", GetResolutionFrom::SWAP_CHAIN},
	{L"depth_stencil", GetResolutionFrom::DEPTH_STENCIL},
	{NULL, GetResolutionFrom::INVALID} // End of list marker
};

struct ResolutionInfo
{
	int width, height;
	GetResolutionFrom from;

	ResolutionInfo() :
		from(GetResolutionFrom::INVALID),
		width(-1),
		height(-1)
	{}
};

enum class AsyncQueryType
{
	QUERY,
	PREDICATE,
	COUNTER,
};

struct Globals
{
	bool gInitialized;
	bool gReloadConfigPending;
	bool gLogInput;
	bool dump_all_profiles;

	wchar_t SHADER_PATH[MAX_PATH];
	wchar_t SHADER_CACHE_PATH[MAX_PATH];
	wchar_t CHAIN_DLL_PATH[MAX_PATH];

	EnableHooks enable_hooks;
	
	bool enable_check_interface;
	int enable_dxgi1_2;
	int enable_create_device;
	bool enable_platform_update;

	int SCREEN_WIDTH;
	int SCREEN_HEIGHT;
	int SCREEN_REFRESH;
	int SCREEN_FULLSCREEN;
	int FILTER_REFRESH[11];
	bool SCREEN_ALLOW_COMMANDS;

	int marking_mode;
	int mark_snapshot;
	int gForceStereo;
	bool gCreateStereoProfile;
	int gSurfaceCreateMode;
	int gSurfaceSquareCreateMode;
	bool gForceNoNvAPI;

	UINT hunting;
	bool fix_enabled;
	bool config_reloadable;
	bool show_original_enabled;
	time_t huntTime;

	bool deferred_enabled;

	unsigned analyse_frame;
	unsigned analyse_frame_no;
	wchar_t ANALYSIS_PATH[MAX_PATH];
	FrameAnalysisOptions def_analyse_options, cur_analyse_options;
	std::unordered_set<void*> frame_analysis_seen_rts;

	ShaderHashType shader_hash_type;
	int EXPORT_HLSL;		// 0=off, 1=HLSL only, 2=HLSL+OriginalASM, 3= HLSL+OriginalASM+recompiledASM
	bool EXPORT_SHADERS, EXPORT_FIXED, EXPORT_BINARY, CACHE_SHADERS, SCISSOR_DISABLE;
	bool track_texture_updates;
	char ZRepair_DepthTextureReg1, ZRepair_DepthTextureReg2;
	std::string ZRepair_DepthTexture1, ZRepair_DepthTexture2;
	std::vector<std::string> ZRepair_Dependencies1, ZRepair_Dependencies2;
	std::string ZRepair_ZPosCalc1, ZRepair_ZPosCalc2;
	std::string ZRepair_PositionTexture, ZRepair_WorldPosCalc;
	std::vector<std::string> InvTransforms;
	std::string BackProject_Vector1, BackProject_Vector2;
	std::string ObjectPos_ID1, ObjectPos_ID2, ObjectPos_MUL1, ObjectPos_MUL2;
	std::string MatrixPos_ID1, MatrixPos_MUL1;
	uint32_t ZBufferHashToInject;
	bool FIX_SV_Position;
	bool FIX_Light_Position;
	bool FIX_Recompile_VS;
	bool DumpUsage;
	bool ENABLE_TUNE;
	float gTuneValue[4], gTuneStep;

	DirectX::XMFLOAT4 iniParams[INI_PARAMS_SIZE];
	int StereoParamsReg;
	int IniParamsReg;

	ResolutionInfo mResolutionInfo;
	CommandList present_command_list;
	CommandList post_present_command_list;
	unsigned frame_no;

	CRITICAL_SECTION mCriticalSection;
	bool ENABLE_CRITICAL_SECTION;

	DataBufferMap mDataBuffers;
	std::set<uint32_t> mVisitedIndexBuffers;				// std::set is sorted for consistent order while hunting
	uint32_t mSelectedIndexBuffer;
	int mSelectedIndexBufferPos;
	std::set<UINT64> mSelectedIndexBuffer_VertexShader;		// std::set so that shaders used with an index buffer will be sorted in log when marked
	std::set<UINT64> mSelectedIndexBuffer_PixelShader;		// std::set so that shaders used with an index buffer will be sorted in log when marked

	CompiledShaderMap mCompiledShaderMap;

	VertexShaderMap mVertexShaders;							// All shaders ever registered with CreateVertexShader
	VertexShaderReplacementMap mOriginalVertexShaders;		// When MarkingMode=Original, switch to original
	VertexShaderReplacementMap mZeroVertexShaders;			// When MarkingMode=zero.
	std::set<UINT64> mVisitedVertexShaders;					// Only shaders seen since last hunting timeout; std::set for consistent order while hunting
	UINT64 mSelectedVertexShader;				 			// Hash.  -1 now for unselected state. The shader selected using Input object.
	int mSelectedVertexShaderPos;							// -1 for unselected state.
	std::set<uint32_t> mSelectedVertexShader_IndexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked

	PixelShaderMap mPixelShaders;							// All shaders ever registered with CreatePixelShader
	PixelShaderReplacementMap mOriginalPixelShaders;
	PixelShaderReplacementMap mZeroPixelShaders;
	std::set<UINT64> mVisitedPixelShaders;					// std::set is sorted for consistent order while hunting
	UINT64 mSelectedPixelShader;							// Hash.  -1 now for unselected state.
	int mSelectedPixelShaderPos;							// -1 for unselected state.
	std::set<uint32_t> mSelectedPixelShader_IndexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked
	ID3D11PixelShader* mPinkingShader;						// Special pixels shader to mark a selection with hot pink.

	ShaderReloadMap mReloadedShaders;						// Shaders that were reloaded live from ShaderFixes

	ComputeShaderMap mComputeShaders;
	ComputeShaderReplacementMap mOriginalComputeShaders;
	std::set<UINT64> mVisitedComputeShaders;
	UINT64 mSelectedComputeShader;
	int mSelectedComputeShaderPos;

	GeometryShaderMap mGeometryShaders;
	GeometryShaderReplacementMap mOriginalGeometryShaders;
	std::set<UINT64> mVisitedGeometryShaders;
	UINT64 mSelectedGeometryShader;
	int mSelectedGeometryShaderPos;

	DomainShaderMap mDomainShaders;
	DomainShaderReplacementMap mOriginalDomainShaders;
	std::set<UINT64> mVisitedDomainShaders;
	UINT64 mSelectedDomainShader;
	int mSelectedDomainShaderPos;

	HullShaderMap mHullShaders;
	HullShaderReplacementMap mOriginalHullShaders;
	std::set<UINT64> mVisitedHullShaders;
	UINT64 mSelectedHullShader;
	int mSelectedHullShaderPos;

	ShaderOverrideMap mShaderOverrideMap;
	TextureOverrideMap mTextureOverrideMap;

	// Statistics
	std::unordered_map<ID3D11Resource *, ResourceHandleInfo> mResources;
	std::unordered_map<ID3D11Asynchronous*, AsyncQueryType> mQueryTypes;

	// These five items work with the *original* resource hash:
	std::unordered_map<uint32_t, struct ResourceHashInfo> mResourceInfo;
	std::set<uint32_t> mRenderTargetInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mDepthTargetInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mShaderResourceInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mCopiedResourceInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)

	std::set<ID3D11Resource *> mVisitedRenderTargets;						// std::set is sorted for consistent order while hunting
	ID3D11Resource *mSelectedRenderTarget;
	int mSelectedRenderTargetPos;
	// Snapshot of all targets for selection.
	ID3D11Resource *mSelectedRenderTargetSnapshot;
	std::set<ID3D11Resource *> mSelectedRenderTargetSnapshotList;			// std::set so that render targets will be sorted in log when marked
	// Relations
	std::map<UINT64, ShaderInfoData> mVertexShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mPixelShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)

	Globals() :
		mSelectedRenderTargetSnapshot(0),
		mSelectedRenderTargetPos(-1),
		mSelectedRenderTarget((ID3D11Resource *)-1),
		mSelectedPixelShader(-1),
		mSelectedPixelShaderPos(-1),
		mSelectedVertexShader(-1),
		mSelectedVertexShaderPos(-1),
		mSelectedIndexBuffer(1),
		mSelectedIndexBufferPos(-1),
		mSelectedComputeShader(-1),
		mSelectedComputeShaderPos(-1),
		mSelectedGeometryShader(-1),
		mSelectedGeometryShaderPos(-1),
		mSelectedDomainShader(-1),
		mSelectedDomainShaderPos(-1),
		mSelectedHullShader(-1),
		mSelectedHullShaderPos(-1),
		mPinkingShader(0),

		hunting(HUNTING_MODE_DISABLED),
		fix_enabled(true),
		config_reloadable(false),
		show_original_enabled(false),
		huntTime(0),

		deferred_enabled(true),

		analyse_frame(0),
		analyse_frame_no(0),
		def_analyse_options(FrameAnalysisOptions::INVALID),
		cur_analyse_options(FrameAnalysisOptions::INVALID),

		shader_hash_type(ShaderHashType::FNV),
		EXPORT_SHADERS(false),
		EXPORT_HLSL(0),
		EXPORT_FIXED(false),
		EXPORT_BINARY(false),
		CACHE_SHADERS(false),
		FIX_SV_Position(false),
		FIX_Light_Position(false),
		FIX_Recompile_VS(false),
		DumpUsage(false),
		ENABLE_TUNE(false),
		gTuneStep(0.001f),

		StereoParamsReg(125),
		IniParamsReg(120),

		frame_no(0),

		ENABLE_CRITICAL_SECTION(false),
		SCREEN_WIDTH(-1),
		SCREEN_HEIGHT(-1),
		SCREEN_REFRESH(-1),
		SCREEN_FULLSCREEN(0),
		SCREEN_ALLOW_COMMANDS(false),

		marking_mode(-1),
		mark_snapshot(2),
		gForceStereo(0),
		gCreateStereoProfile(false),
		gSurfaceCreateMode(-1),
		gSurfaceSquareCreateMode(-1),
		gForceNoNvAPI(false),
		ZBufferHashToInject(0),
		SCISSOR_DISABLE(0),

		enable_hooks(EnableHooks::INVALID),
		enable_check_interface(false),
		enable_dxgi1_2(0),
		enable_create_device(0),
		enable_platform_update(false),
		gInitialized(false),
		gReloadConfigPending(false),
		gLogInput(false),
		dump_all_profiles(false)

	{
		int i;

		SHADER_PATH[0] = 0;
		SHADER_CACHE_PATH[0] = 0;
		CHAIN_DLL_PATH[0] = 0;

		ANALYSIS_PATH[0] = 0;

		for (i = 0; i < 4; i++)
			gTuneValue[i] = 1.0f;

		for (i = 0; i < 11; i++)
			FILTER_REFRESH[i] = 0;

		for (i = 0; i < INI_PARAMS_SIZE; i++) {
			iniParams[i].x = FLT_MAX;
			iniParams[i].y = FLT_MAX;
			iniParams[i].z = FLT_MAX;
			iniParams[i].w = FLT_MAX;
		}
	}
};

extern Globals *G;
