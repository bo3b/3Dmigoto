#pragma once

#include "Main.h"
#include "../util.h"
#include <DirectXMath.h>
#include <ctime>
#include <vector>
#include <set>
#include <unordered_map>

const int MARKING_MODE_SKIP = 0;
const int MARKING_MODE_MONO = 1;
const int MARKING_MODE_ORIGINAL = 2;
const int MARKING_MODE_ZERO = 3;

// Key is index/vertex buffer, value is hash key.
typedef std::unordered_map<D3D10Base::ID3D10Buffer *, UINT64> DataBufferMap;

// Source compiled shaders.
typedef std::unordered_map<UINT64, std::string> CompiledShaderMap;

// Strategy: This OriginalShaderInfo record and associated map is to allow us to keep track of every
//	pixelshader and vertexshader that are compiled from hlsl text from the ShaderFixes
//	folder.  This keeps track of the original shader information using the ID3D10VertexShader*
//	or ID3D10PixelShader* as a master key to the key map.
//	We are using the base class of ID3D10DeviceChild* since both descend from that, and that allows
//	us to use the same structure for Pixel and Vertex shaders both.

// Info saved about originally overridden shaders passed in by the game in CreateVertexShader or
// CreatePixelShader that have been loaded as HLSL
//	shaderType is "vs" or "ps" or maybe later "gs" (type wstring for file name use)
//	shaderModel is only filled in when a shader is replaced.  (type string for old D3 API use)
//	linkage is passed as a parameter, seems to be rarely if ever used.
//	byteCode is the original shader byte code passed in by game, or recompiled by override.
//	timeStamp allows reloading/recompiling only modified shaders
//	replacement is either ID3D10VertexShader or ID3D10PixelShader
struct OriginalShaderInfo
{
	UINT64 hash;
	std::wstring shaderType;
	std::string shaderModel;
	//D3D10Base::ID3D10ClassLinkage* linkage;
	D3D10Base::ID3DBlob* byteCode;
	FILETIME timeStamp;
	D3D10Base::ID3D10DeviceChild* replacement;
};

// Key is the overridden shader that was given back to the game at CreateVertexShader (vs or ps)
typedef std::unordered_map<D3D10Base::ID3D10DeviceChild *, OriginalShaderInfo> ShaderReloadMap;

// Key is vertexshader, value is hash key.
typedef std::unordered_map<D3D10Base::ID3D10VertexShader *, UINT64> VertexShaderMap;
typedef std::unordered_map<UINT64, D3D10Base::ID3D10VertexShader *> PreloadVertexShaderMap;
typedef std::unordered_map<D3D10Base::ID3D10VertexShader *, D3D10Base::ID3D10VertexShader *> VertexShaderReplacementMap;

// Key is pixelshader, value is hash key.
typedef std::unordered_map<D3D10Base::ID3D10PixelShader *, UINT64> PixelShaderMap;
typedef std::unordered_map<UINT64, D3D10Base::ID3D10PixelShader *> PreloadPixelShaderMap;
typedef std::unordered_map<D3D10Base::ID3D10PixelShader *, D3D10Base::ID3D10PixelShader *> PixelShaderReplacementMap;

//typedef std::unordered_map<D3D10Base::ID3D10HullShader *, UINT64> HullShaderMap;
//typedef std::unordered_map<D3D10Base::ID3D10DomainShader *, UINT64> DomainShaderMap;
//typedef std::unordered_map<D3D10Base::ID3D10ComputeShader *, UINT64> ComputeShaderMap;
typedef std::unordered_map<D3D10Base::ID3D10GeometryShader *, UINT64> GeometryShaderMap;

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
#if 0 /* Iterations are broken since we no longer use present() */
	std::vector<int> iterations; // Only for separation changes, not shaders.
#endif
	std::vector<UINT64> indexBufferFilter;
	DepthBufferFilter depth_filter;
	UINT64 partner_hash;

	ShaderOverride() :
		separation(FLT_MAX),
		convergence(FLT_MAX),
		skip(false),
		depth_filter(DepthBufferFilter::NONE),
		partner_hash(0)
	{}
};
typedef std::unordered_map<UINT64, struct ShaderOverride> ShaderOverrideMap;

struct TextureOverride {
	int stereoMode;
	int format;
#if 0 /* Iterations are broken since we no longer use present() */
	std::vector<int> iterations;
#endif

	TextureOverride() :
		stereoMode(-1),
		format(-1)
	{}
};
typedef std::unordered_map<UINT64, struct TextureOverride> TextureOverrideMap;

struct ShaderInfoData
{
	// All are std::map or std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<int, void *> ResourceRegisters;
	std::set<UINT64> PartnerShader;
	std::vector<std::set<void *>> RenderTargets;
	std::set<void *> DepthTargets;
};
struct SwapChainInfo
{
	int width, height;
};

struct ResourceInfo
{
	D3D10Base::D3D10_RESOURCE_DIMENSION type;
	union {
		D3D10Base::D3D10_TEXTURE2D_DESC tex2d_desc;
		D3D10Base::D3D10_TEXTURE3D_DESC tex3d_desc;
	};

	ResourceInfo() :
		type(D3D10Base::D3D10_RESOURCE_DIMENSION_UNKNOWN)
	{}

	struct ResourceInfo & operator= (D3D10Base::D3D10_TEXTURE2D_DESC desc)
	{
		type = D3D10Base::D3D10_RESOURCE_DIMENSION_TEXTURE2D;
		tex2d_desc = desc;
		return *this;
	}

	struct ResourceInfo & operator= (D3D10Base::D3D10_TEXTURE3D_DESC desc)
	{
		type = D3D10Base::D3D10_RESOURCE_DIMENSION_TEXTURE3D;
		tex3d_desc = desc;
		return *this;
	}
};

struct Globals
{
	wchar_t SHADER_PATH[MAX_PATH];
	wchar_t SHADER_CACHE_PATH[MAX_PATH];

	int SCREEN_WIDTH;
	int SCREEN_HEIGHT;
	int SCREEN_REFRESH;
	int SCREEN_FULLSCREEN;
	int marking_mode;
	bool gForceStereo;
	bool gCreateStereoProfile;
	int gSurfaceCreateMode;
	int gSurfaceSquareCreateMode;

	bool hunting;
	bool fix_enabled;
	bool config_reloadable;
	time_t huntTime;

	int EXPORT_HLSL;		// 0=off, 1=HLSL only, 2=HLSL+OriginalASM, 3= HLSL+OriginalASM+recompiledASM
	bool EXPORT_SHADERS, EXPORT_FIXED, CACHE_SHADERS, PRELOAD_SHADERS, SCISSOR_DISABLE;
	char ZRepair_DepthTextureReg1, ZRepair_DepthTextureReg2;
	std::string ZRepair_DepthTexture1, ZRepair_DepthTexture2;
	std::vector<std::string> ZRepair_Dependencies1, ZRepair_Dependencies2;
	std::string ZRepair_ZPosCalc1, ZRepair_ZPosCalc2;
	std::string ZRepair_PositionTexture, ZRepair_WorldPosCalc;
	std::vector<std::string> InvTransforms;
	std::string BackProject_Vector1, BackProject_Vector2;
	std::string ObjectPos_ID1, ObjectPos_ID2, ObjectPos_MUL1, ObjectPos_MUL2;
	std::string MatrixPos_ID1, MatrixPos_MUL1;
	UINT64 ZBufferHashToInject;
	bool FIX_SV_Position;
	bool FIX_Light_Position;
	bool FIX_Recompile_VS;
	bool DumpUsage;
	bool ENABLE_TUNE;
	float gTuneValue[4], gTuneStep;

	DirectX::XMFLOAT4 iniParams;

	SwapChainInfo mSwapChainInfo;

	ThreadSafePointerSet m_AdapterList;
	CRITICAL_SECTION mCriticalSection;
	bool ENABLE_CRITICAL_SECTION;

	DataBufferMap mDataBuffers;
	UINT64 mCurrentIndexBuffer;
	std::set<UINT64> mVisitedIndexBuffers; // std::set is sorted for consistent order while hunting
	UINT64 mSelectedIndexBuffer;
	unsigned int mSelectedIndexBufferPos;
	std::set<UINT64> mSelectedIndexBuffer_VertexShader; // std::set so that shaders used with an index buffer will be sorted in log when marked
	std::set<UINT64> mSelectedIndexBuffer_PixelShader; // std::set so that shaders used with an index buffer will be sorted in log when marked

	CompiledShaderMap mCompiledShaderMap;

	VertexShaderMap mVertexShaders;							// All shaders ever registered with CreateVertexShader
	PreloadVertexShaderMap mPreloadedVertexShaders;			// All shaders that were preloaded as .bin
	VertexShaderReplacementMap mOriginalVertexShaders;		// When MarkingMode=Original, switch to original
	VertexShaderReplacementMap mZeroVertexShaders;			// When MarkingMode=zero.
	UINT64 mCurrentVertexShader;							// Shader currently live in GPU pipeline.
	D3D10Base::ID3D10VertexShader *mCurrentVertexShaderHandle;			// Shader currently live in GPU pipeline.
	std::set<UINT64> mVisitedVertexShaders;				// Only shaders seen since last hunting timeout; std::set for consistent order whiel hunting
	UINT64 mSelectedVertexShader;							// Shader selected using XInput
	unsigned int mSelectedVertexShaderPos;
	std::set<UINT64> mSelectedVertexShader_IndexBuffer; // std::set so that index buffers used with a shader will be sorted in log when marked

	PixelShaderMap mPixelShaders;							// All shaders ever registered with CreatePixelShader
	PreloadPixelShaderMap mPreloadedPixelShaders;
	PixelShaderReplacementMap mOriginalPixelShaders;
	PixelShaderReplacementMap mZeroPixelShaders;
	UINT64 mCurrentPixelShader;
	D3D10Base::ID3D10PixelShader *mCurrentPixelShaderHandle;
	std::set<UINT64> mVisitedPixelShaders; // std::set is sorted for consistent order while hunting
	UINT64 mSelectedPixelShader;
	unsigned int mSelectedPixelShaderPos;
	std::set<UINT64> mSelectedPixelShader_IndexBuffer; // std::set so that index buffers used with a shader will be sorted in log when marked

	ShaderReloadMap mReloadedShaders;						// Shaders that were reloaded live from ShaderFixes

	GeometryShaderMap mGeometryShaders;
	//ComputeShaderMap mComputeShaders;
	//DomainShaderMap mDomainShaders;
	//HullShaderMap mHullShaders;

	ShaderOverrideMap mShaderOverrideMap;
	TextureOverrideMap mTextureOverrideMap;

	// Statistics
	std::unordered_map<void *, UINT64> mRenderTargets;
	std::map<UINT64, struct ResourceInfo> mRenderTargetInfo; // std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, struct ResourceInfo> mDepthTargetInfo; // std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<void *> mVisitedRenderTargets; // std::set is sorted for consistent order while hunting
	std::vector<void *> mCurrentRenderTargets;
	void *mSelectedRenderTarget;
	unsigned int mSelectedRenderTargetPos;
	void *mCurrentDepthTarget;
	// Snapshot of all targets for selection.
	void *mSelectedRenderTargetSnapshot;
	std::set<void *> mSelectedRenderTargetSnapshotList; // std::set so that render targets will be sorted in log when marked
	// Relations
	std::unordered_map<D3D10Base::ID3D10Texture2D *, UINT64> mTexture2D_ID;
	std::unordered_map<D3D10Base::ID3D10Texture3D *, UINT64> mTexture3D_ID;
	std::map<UINT64, ShaderInfoData> mVertexShaderInfo; // std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mPixelShaderInfo; // std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)

	Globals() :
		mSelectedRenderTargetSnapshot(0),
		mSelectedRenderTargetPos(0),
		mSelectedRenderTarget((void *)1),
		mCurrentDepthTarget(0),
		mCurrentPixelShader(0),
		mCurrentPixelShaderHandle(NULL),
		mSelectedPixelShader(1),
		mSelectedPixelShaderPos(0),
		mCurrentVertexShader(0),
		mCurrentVertexShaderHandle(NULL),
		mSelectedVertexShader(1),
		mSelectedVertexShaderPos(0),
		mCurrentIndexBuffer(0),
		mSelectedIndexBuffer(1),
		mSelectedIndexBufferPos(0),

		hunting(false),
		fix_enabled(true),
		config_reloadable(false),
		huntTime(0),

		EXPORT_SHADERS(false),
		EXPORT_HLSL(0),
		EXPORT_FIXED(false),
		CACHE_SHADERS(false),
		PRELOAD_SHADERS(false),
		FIX_SV_Position(false),
		FIX_Light_Position(false),
		FIX_Recompile_VS(false),
		DumpUsage(false),
		ENABLE_TUNE(false),
		gTuneStep(0.001f),

		iniParams{ FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX },

		ENABLE_CRITICAL_SECTION(false),
		SCREEN_WIDTH(-1),
		SCREEN_HEIGHT(-1),
		SCREEN_REFRESH(-1),
		SCREEN_FULLSCREEN(-1),
		marking_mode(-1),
		gForceStereo(false),
		gCreateStereoProfile(false),
		gSurfaceCreateMode(-1),
		gSurfaceSquareCreateMode(-1),
		ZBufferHashToInject(0),
		SCISSOR_DISABLE(0)
	{
		SHADER_PATH[0] = 0; 
		SHADER_CACHE_PATH[0] = 0;
		mSwapChainInfo.width = -1;
		mSwapChainInfo.height = -1;

		for (int i = 0; i < 4; i++)
			gTuneValue[i] = 1.0f;
	}
};

extern Globals *G;
