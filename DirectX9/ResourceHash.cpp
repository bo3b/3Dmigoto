#include "ResourceHash.h"
#include "log.h"
#include "util.h"
#include "Globals.h"
#include "Overlay.h"
#include "Main.h"
int StrResourceDesc(char *buf, size_t size, ::D3DVERTEXBUFFER_DESC *desc) {
	return _snprintf_s(buf, size, size, "type=Vertex Buffer byte_width=%u "
		"usage=\"%S\" FVF=0x%x format=\"%S\" "
		"pool=%u",
		desc->Size, TexResourceUsage(desc->Usage),
		desc->FVF, TexFormatStrDX9(desc->Format), desc->Pool);
}
int StrResourceDesc(char *buf, size_t size, ::D3DINDEXBUFFER_DESC *desc) {
	return _snprintf_s(buf, size, size, "type=Index Buffer byte_width=%u "
		"usage=\"%S\" format=\"%S\" "
		"pool=%u",
		desc->Size, TexResourceUsage(desc->Usage),TexFormatStrDX9(desc->Format), desc->Pool);
}
int StrResourceDesc(char *buf, size_t size, ::D3DSURFACE_DESC *desc) {
	return _snprintf_s(buf, size, size, "type=Surface resource_type=0x%x width=%u height=%u"
		"usage=\"%S\" format=\"%S\" "
		"pool=%u multisampling_type=%u multisampling_quality=%u", desc->Type,
		desc->Width, desc->Height, TexResourceUsage(desc->Usage),
		TexFormatStrDX9(desc->Format), desc->Pool, desc->MultiSampleType, desc->MultiSampleQuality);
}
int StrResourceDesc(char *buf, size_t size, D3D2DTEXTURE_DESC *desc) {
	return _snprintf_s(buf, size, size, "type=2DTexture resource_type=0x%x width=%u height=%u"
		"usage=\"%S\" format=\"%S\" "
		"pool=%u levels=%u multisampling_type=%u multisampling_quality=%u", desc->Type,
		desc->Width, desc->Height, TexResourceUsage(desc->Usage),
		TexFormatStrDX9(desc->Format), desc->Pool, desc->Levels, desc->MultiSampleType, desc->MultiSampleQuality);
}
int StrResourceDesc(char *buf, size_t size, D3D3DTEXTURE_DESC *desc) {
	return _snprintf_s(buf, size, size, "type=3DTexture width=%u height=%u depth=%u"
		"usage=\"%S\" format=\"%S\" "
		"pool=%u levels=%u shared_handle=%p",
		desc->Width, desc->Height, desc->Depth, TexResourceUsage(desc->Usage),
		TexFormatStrDX9(desc->Format), desc->Pool, desc->Levels);
}
int StrResourceDesc(char *buf, size_t size, struct ResourceHashInfo &info) {
	switch (info.type) {
	case ::D3DRESOURCETYPE::D3DRTYPE_VERTEXBUFFER:
		return StrResourceDesc(buf, size, &info.vbuf_desc);
	case ::D3DRESOURCETYPE::D3DRTYPE_INDEXBUFFER:
		return StrResourceDesc(buf, size, &info.ibuf_desc);
	case ::D3DRESOURCETYPE::D3DRTYPE_CUBETEXTURE:
	case ::D3DRESOURCETYPE::D3DRTYPE_TEXTURE:
	case ::D3DRESOURCETYPE::D3DRTYPE_SURFACE:
		return StrResourceDesc(buf, size, &info.desc2D);
	case ::D3DRESOURCETYPE::D3DRTYPE_VOLUMETEXTURE:
		return StrResourceDesc(buf, size, &info.desc3D);
	default:
		return _snprintf_s(buf, size, size, "type=%i", info.type);
	}
}
// Overloaded functions to log any kind of resource description (useful to call
// from templates):
template <typename DescType>
static void LogResourceDescCommon(DescType *desc)
{
	LogInfo("    Format = %s (%d)\n", TexFormatStrDX9(desc->Format), desc->Format);
	LogInfo("    Type = 0x%x\n", desc->Type);
	LogInfo("    Usage = %d\n", desc->Usage);
	LogInfo("    Pool = 0x%x\n", desc->Pool);
}
void LogResourceDesc(const ::D3DVERTEXBUFFER_DESC *desc)
{
	LogInfo("  Resource Type = Vertex Buffer\n");
	LogInfo("    ByteWidth = %d\n", desc->Size);
	LogInfo("    FVFStructureByteStride = %d\n", strideForFVF(desc->FVF));
	LogResourceDescCommon(desc);
}
void LogResourceDesc(const ::D3DINDEXBUFFER_DESC *desc)
{
	LogInfo("  Resource Type = Index Buffer\n");
	LogInfo("    ByteWidth = %d\n", desc->Size);
	LogResourceDescCommon(desc);
}
void LogResourceDesc(const D3D3DTEXTURE_DESC *desc)
{
	LogInfo("  Desc Type = Volume\n");
	LogInfo("    Width = %d\n", desc->Width);
	LogInfo("    Height = %d\n", desc->Height);
	LogInfo("    Depth = %d\n", desc->Depth);
	LogResourceDescCommon(desc);
}

void LogResourceDesc(const D3D2DTEXTURE_DESC *desc)
{
	LogInfo("  Desc Type = Surface\n");
	LogInfo("    Width = %d\n", desc->Width);
	LogInfo("    Height = %d\n", desc->Height);
	LogInfo("    SampleDesc.Count = %d\n", desc->MultiSampleType);
	LogInfo("    SampleDesc.Quality = %d\n", desc->MultiSampleQuality);
	LogResourceDescCommon(desc);
}
void LogResourceDesc(::IDirect3DResource9 *resource)
{
	::D3DRESOURCETYPE type;
	::IDirect3DVertexBuffer9 *vBuffer;
	::IDirect3DIndexBuffer9 *iBuffer;
	::IDirect3DTexture9 *tex;
	::IDirect3DVolumeTexture9 *volTex;
	::IDirect3DCubeTexture9 *cubeTex;
	::IDirect3DSurface9 *sur;
	::D3DVERTEXBUFFER_DESC vBufferDesc;
	::D3DINDEXBUFFER_DESC iBufferDesc;
	type = resource->GetType();
	switch (type) {
	case ::D3DRTYPE_VERTEXBUFFER:
		vBuffer = (::IDirect3DVertexBuffer9*)resource;
		vBuffer->GetDesc(&vBufferDesc);
		return LogResourceDesc(&vBufferDesc);
	case ::D3DRTYPE_INDEXBUFFER:
		iBuffer = (::IDirect3DIndexBuffer9*)resource;
		iBuffer->GetDesc(&iBufferDesc);
		return LogResourceDesc(&iBufferDesc);
	case ::D3DRTYPE_TEXTURE:
		tex = (::IDirect3DTexture9*)resource;
		return LogResourceDesc(&D3D2DTEXTURE_DESC(tex));
	case ::D3DRTYPE_CUBETEXTURE:
		cubeTex = (::IDirect3DCubeTexture9*)resource;
		return LogResourceDesc(&D3D2DTEXTURE_DESC(cubeTex));
	case ::D3DRTYPE_VOLUMETEXTURE:
		volTex = (::IDirect3DVolumeTexture9*)resource;
		return LogResourceDesc(&D3D3DTEXTURE_DESC(volTex));
	case ::D3DRTYPE_SURFACE:
		sur = (::IDirect3DSurface9*)resource;
		return LogResourceDesc(&D3D2DTEXTURE_DESC(sur));
	}
}
// This special case of texture resolution is to improve the behavior of special
// full-screen textures.  Textures can be created dynamically of course, and some
// are set to full screen resolution.  Full screen resolution can vary between
// users and we want a way to have a stable texture hash, even while the screen
// resolution is varying.
//
// This function will modify the hashWidth and hashHeight values actually used
// in the hash calculation to be magic numbers, really just constants.  That
// will make the hash predictable and match, even if the screen resolution changes.
//
// The other variants are for *2, *4, *8, /2, as other textures seen with specific
// resolutions, but are also dynamic based on screen resolution, like 2x or 1/2 the
// resolution.
//
// ToDo: It might make more sense to avoid this altogether, and have the shaderhacker
// specify their desired texture in the d3dx.ini file by parameters, not by a single
// hash.  That would be a sequence found via the ShaderUsages that would specify all
// the parameters in something like the D3D11_TEXTURE2D_DESC.
// The only drawback here is to make it more complicated for the shaderhacker, having
// to specify the little niggly bits, and requiring them to understand and look for
// the alternate sizes.
//
// If this seems like an OK way to go, what about other interesting magic combos like
// 1.5x (720p->1080p), maybe 1080p specifically. 720/1080=2/3.
// Would it maybe make more sense to just do all the logical screen combos instead?

static void AdjustForConstResolution(UINT *hashWidth, UINT *hashHeight)
{
	int width = *hashWidth;
	int height = *hashHeight;

	if (G->mResolutionInfo.from == GetResolutionFrom::INVALID)
		return;

	if (width == G->mResolutionInfo.width && height == G->mResolutionInfo.height) {
		*hashWidth = 'SRES';
		*hashHeight = 'SRES';
	}
	else if (width == G->mResolutionInfo.width * 2 && height == G->mResolutionInfo.height * 2) {
		*hashWidth = 'SR*2';
		*hashHeight = 'SR*2';
	}
	else if (width == G->mResolutionInfo.width * 4 && height == G->mResolutionInfo.height * 4) {
		*hashWidth = 'SR*4';
		*hashHeight = 'SR*4';
	}
	else if (width == G->mResolutionInfo.width * 8 && height == G->mResolutionInfo.height * 8) {
		*hashWidth = 'SR*8';
		*hashHeight = 'SR*8';
	}
	else if (width == G->mResolutionInfo.width / 2 && height == G->mResolutionInfo.height / 2) {
		*hashWidth = 'SR/2';
		*hashHeight = 'SR/2';
	}
}
uint32_t CalcDescHash(uint32_t initial_hash, const D3D2DTEXTURE_DESC *pDesc)
{
	// It concerns me that CreateTextureND can use an override if it
	// matches screen resolution, but when we record render target / shader
	// resource stats we don't use the same override.
	//
	// For textures made with CreateTextureND and later used as a render
	// target it's probably fine since the hash will still be stored, but
	// it could be a problem if we need the hash of a render target not
	// created directly with that. I don't know enough about the DX11 API
	// to know if this is an issue, but it might be worth using the screen
	// resolution override in all cases. -DarkStarSword

	// Based on that concern, and the need to have a pointer to the
	// D3D11_TEXTURE2D_DESC struct for hash calculation, let's go ahead
	// and use the resolution override always.

	D3D2DTEXTURE_DESC* desc = const_cast<D3D2DTEXTURE_DESC*>(pDesc);

	UINT saveWidth = desc->Width;
	UINT saveHeight = desc->Height;
	AdjustForConstResolution(&desc->Width, &desc->Height);

	uint32_t hash = crc32c_hw(initial_hash, desc, sizeof(D3D2DTEXTURE_DESC));

	desc->Width = saveWidth;
	desc->Height = saveHeight;

	return hash;
}
uint32_t CalcDescHash(uint32_t initial_hash, const D3D3DTEXTURE_DESC *const_desc) {
	// Same comment as in CalcTexture2DDescHash above - concerned about
	// inconsistent use of these resolution overrides

	D3D3DTEXTURE_DESC* desc = const_cast<D3D3DTEXTURE_DESC*>(const_desc);

	UINT saveWidth = desc->Width;
	UINT saveHeight = desc->Height;
	AdjustForConstResolution(&desc->Width, &desc->Height);

	uint32_t hash = crc32c_hw(initial_hash, desc, sizeof(D3D3DTEXTURE_DESC));

	desc->Width = saveWidth;
	desc->Height = saveHeight;

	return hash;
}
static UINT CompressedFormatBlockSize(::D3DFORMAT Format)
{
	switch (Format) {
	case ::D3DFMT_DXT1:
		return 8;
	case ::D3DFMT_DXT2:
	case ::D3DFMT_DXT3:
	case ::D3DFMT_DXT4:
	case ::D3DFMT_DXT5:
		return 16;
	case ::D3DFMT_UYVY:
	case ::D3DFMT_YUY2:
		return 4;
	case ::D3DFMT_G8R8_G8B8:
	case ::D3DFMT_R8G8_B8G8:
		return 2;
	}

	return 0;
}
// -----------------------------------------------------------------------------------------------
static size_t Texture2DLength(
	const D3D2DTEXTURE_DESC *pDesc,
	const ::D3DLOCKED_BOX *pData,
	UINT level)
{
	UINT block_size, padded_width, padded_height;

	// We might simply be able to use SysMemSlicePitch. The documentation
	// indicates that it has "no meaning" for a 2D texture, but then in the
	// Remarks section indicates that it should be set to "the size of the
	// entire 2D surface in bytes"... but somehow I don't trust it - after
	// all, we don't set it when creating the stereo texture and that works!
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476220(v=vs.85).aspx

	// At the moment we are only using the first mip-map level, but this
	// should work if we wanted to use another:
	UINT mip_width = max(pDesc->Width >> level, 1);
	UINT mip_height = max(pDesc->Height >> level, 1);

	block_size = CompressedFormatBlockSize(pDesc->Format);

	if (!block_size) {
		// Uncompressed texture - use the SysMemPitch to get
		// the width (including any padding) in bytes.
		return pData->RowPitch * mip_height;
	}

	// In the case of compressed textures, we can't necessarily rely on
	// SysMemPitch because "lines" are meaningless until the texture has
	// been decompressed. Instead use the mip-map width + height padded to
	// a multiple of 4 with the 4x4 block size.

	padded_width = (mip_width + 3) & ~0x3;
	padded_height = (mip_height + 3) & ~0x3;

	return padded_width * padded_height / 16 * block_size;
}

static size_t Texture3DLength(
	const D3D3DTEXTURE_DESC *pDesc,
	const ::D3DLOCKED_BOX *pData,
	UINT level)
{
	UINT block_size, padded_width, padded_height;

	// At the moment we are only using the first mip-map level, but this
	// should work if we wanted to use another:
	UINT mip_width = max(pDesc->Width >> level, 1);
	UINT mip_height = max(pDesc->Height >> level, 1);
	UINT mip_depth = max(pDesc->Depth >> level, 1);

	block_size = CompressedFormatBlockSize(pDesc->Format);

	if (!block_size) {
		// Uncompressed texture - use the SysMemSlicePitch to get the
		// width*height (including any padding) in bytes.
		return pData->SlicePitch * mip_depth;
	}

	// Not sure if SysMemSlicePitch is reliable for compressed 3D textures.
	// Use the mip-map width, height + depth padded to a multiple of 4 with
	// the 4x4 block size.

	padded_width = (mip_width + 3) & ~0x3;
	padded_height = (mip_height + 3) & ~0x3;

	return padded_width * padded_height * mip_depth / 16 * block_size;
}
inline void GetSurfaceInfo(_In_ size_t width,
	_In_ size_t height,
	_In_ ::D3DFORMAT fmt,
	_Out_opt_ size_t* outNumBytes,
	_Out_opt_ size_t* outRowBytes,
	_Out_opt_ size_t* outNumRows)
{
	size_t numBytes = 0;
	size_t rowBytes = 0;
	size_t numRows = 0;

	bool bc = false;
	bool packed = false;
	bool planar = false;
	size_t bpe = 0;
	switch (fmt)
	{
	case ::D3DFMT_DXT1:
		bc = true;
		bpe = 8;
		break;
	case ::D3DFMT_DXT2:
	case ::D3DFMT_DXT3:
	case ::D3DFMT_DXT4:
	case ::D3DFMT_DXT5:
	case ::D3DFMT_G8R8_G8B8:
	case ::D3DFMT_R8G8_B8G8:
		bc = true;
		bpe = 16;
		break;
	case ::D3DFMT_UYVY:
	case ::D3DFMT_YUY2:
		packed = true;
		bpe = 4;
		break;
	default:
		break;
	}

	if (bc)
	{
		size_t numBlocksWide = 0;
		if (width > 0)
		{
			numBlocksWide = std::max<size_t>(1, (width + 3) / 4);
		}
		size_t numBlocksHigh = 0;
		if (height > 0)
		{
			numBlocksHigh = std::max<size_t>(1, (height + 3) / 4);
		}
		rowBytes = numBlocksWide * bpe;
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else if (packed)
	{
		rowBytes = ((width + 1) >> 1) * bpe;
		numRows = height;
		numBytes = rowBytes * height;
	}
	else if (planar)
	{
		rowBytes = ((width + 1) >> 1) * bpe;
		numBytes = (rowBytes * height) + ((rowBytes * height + 1) >> 1);
		numRows = height + ((height + 1) >> 1);
	}
	else
	{
		size_t bpp = BitsPerPixel(fmt);
		rowBytes = (width * bpp + 7) / 8; // round up to nearest byte
		numRows = height;
		numBytes = rowBytes * height;
	}

	if (outNumBytes)
	{
		*outNumBytes = numBytes;
	}
	if (outRowBytes)
	{
		*outRowBytes = rowBytes;
	}
	if (outNumRows)
	{
		*outNumRows = numRows;
	}
}

static uint32_t hash_tex2d_data(uint32_t hash, const void *data, size_t length,
	const D3D2DTEXTURE_DESC *pDesc, bool zero_padding,
	bool skip_padding, UINT mapped_row_pitch)
{
	size_t row_pitch, slice_pitch, row_count;

	// Each row in a 2D texture has some alignment constraint, and the
	// unused bytes at the end of each row can be garbage, interfering with
	// the hash calculation. We should probably have always been discarding
	// these bytes for the hash calculations, but it wasn't easily apparent
	// that would be necessary and now too many fixes depend on it to just
	// change it, but we might consider adding an option to do this.
	//
	// However, these garbage bytes are proven to interfere with frame
	// analysis de-duplication - not fatally so, but they do mess up the
	// hashes on many resources so the hashes are not fully de-duped
	// (easily observable dumping HUD textures in DOAXVV twice in a row and
	// many of the de-duped hashes will have changed).
	//
	// Replacing the padding bytes with zeroes makes the hashes consistent
	// and fixes about half the hashes to match the texture hashes of those
	// that should, however the other half are still incorrect (but
	// consistent at least) and further investigation is required.
	//
	// Two possibilities come to mind to investigate:
	// - The textures may have been created with garbage in the padding
	//   bytes that we ideally should ignore.
	// - The SysMemPitch used to create the resources may not be preserved
	//   by DirectX, so the RowPitch we use here may not match leading to
	//   the zero hash being incorrect. Ideally we would skip the padding
	//   rather than replace it with zeroes.
	//
	// This is based partially from DirectXTK's SaveDDSTextureToFile, but
	// with the length capped based on our length calculation, and with the
	// padding replaced with zeroes rather than skipped.

	if (!zero_padding && !skip_padding)
		return crc32c_hw(hash, data, length);

	GetSurfaceInfo(pDesc->Width, pDesc->Height, pDesc->Format, &slice_pitch, &row_pitch, &row_count);

	uint8_t *sptr = (uint8_t*)data;
	size_t msize = min(row_pitch, mapped_row_pitch);

	signed padding = (signed)mapped_row_pitch - (signed)row_pitch;
	uint8_t *zeroes = NULL;
	if (zero_padding && padding > 0) {
		zeroes = new uint8_t[padding];
		memset(zeroes, 0, padding);
	}

	signed remaining = (signed)length;
	for (size_t h = 0; h < row_count && remaining > 0; h++) {
		hash = crc32c_hw(hash, sptr, min(msize, (unsigned)remaining));
		sptr += mapped_row_pitch;
		remaining -= (signed)msize;

		if (zeroes && remaining > 0) {
			hash = crc32c_hw(hash, zeroes, min(padding, remaining));
			remaining -= padding;
		}
	}

	delete[] zeroes;
	return hash;
}

uint32_t Calc2DDataHash(const D3D2DTEXTURE_DESC *pDesc, const ::D3DLOCKED_BOX *pLockedBox)
{
	uint32_t hash = 0;
	size_t length;

	if (!pDesc || !pLockedBox || !pLockedBox->pBits)
		return 0;

	if (G->texture_hash_version)
		return Calc2DDataHashAccurate(pDesc, pLockedBox);

	LogDebug("  Using 3DMigoto v1.2.11+ Texture2D CRC calculation\n");
	length = Texture2DLength(pDesc, &pLockedBox[0], 0);
	hash = hash_tex2d_data(hash, pLockedBox[0].pBits, length,
		pDesc, false, true, pLockedBox[0].RowPitch);

	return hash;
}
uint32_t Calc2DDataHashAccurate(const D3D2DTEXTURE_DESC *pDesc, const ::D3DLOCKED_BOX *pLockedBox)
{
	uint32_t hash = 0;

	// The regular data hash we are using is woefully innaccurate, as it
	// will not hash the entire image. Mostly OK for texture filtering, but
	// no good for frame analysis deduplication - especially evident when
	// the HUD is being rendered, as only HUD elements that alter the upper
	// third of the image cause the hash to change, while HUD elements in
	// the mid to lower half of the image don't affect the hash at all
	// (observed in DOAXVV).
	//
	// This function throws away all backwards compatibility with our
	// legacy hashing code to just try to do it right. The hashes won't
	// match those used for texture filtering at all, but it's more
	// important that this get it right.

	if (!pDesc || !pLockedBox || !pLockedBox->pBits)
		return 0;

	// Passing length=INT_MAX, since that is an upper bound and
	// hash_tex2d_data will work it out from DirectXTK
	hash = hash_tex2d_data(hash, pLockedBox[0].pBits, INT_MAX,
		pDesc, false, true, pLockedBox[0].RowPitch);

	return hash;
}
ResourceHandleInfo* GetResourceHandleInfo(D3D9Wrapper::IDirect3DResource9 *resource)
{
	return &resource->resourceHandleInfo;
}
uint32_t GetOrigResourceHash(D3D9Wrapper::IDirect3DResource9 *resource)
{
	return resource->resourceHandleInfo.orig_hash;
}

uint32_t GetResourceHash(D3D9Wrapper::IDirect3DResource9 *resource)
{
	return resource->resourceHandleInfo.hash;
}
uint32_t Calc3DDataHash(const D3D3DTEXTURE_DESC * pDesc, const ::D3DLOCKED_BOX * pLockedBox)
{
	uint32_t hash = 0;
	size_t length;

	if (!pDesc || !pLockedBox || !pLockedBox->pBits)
		return 0;
	length = Texture3DLength(pDesc, &pLockedBox[0], 0);

	LogDebug("  Using 3DMigoto v1.2.9+ Texture3D CRC calculation\n");

	hash = crc32c_hw(hash, pLockedBox[0].pBits, length);

	return hash;
}
static bool supports_hash_tracking(ResourceHandleInfo *handle_info)
{
	// We only support hash tracking and contamination detection for 2D and
	// 3D textures currently. We could probably add 1D textures relatively
	// safely, but buffers would kill performance because of how often they
	// are updated, so we're skipping them for now. If we do want to add
	// support for them later, we should add a means to turn off the
	// contamination detection on a per-resource type basis:
	return (handle_info->type == ::D3DRTYPE_TEXTURE ||
		handle_info->type == ::D3DRTYPE_VOLUMETEXTURE ||
		handle_info->type == ::D3DRTYPE_CUBETEXTURE ||
		handle_info->type == ::D3DRTYPE_SURFACE
		);
}

static bool GetResourceInfoFields(struct ResourceHashInfo *info,
	UINT *width, UINT *height, UINT *depth)
{
	switch (info->type) {
	case ::D3DRTYPE_SURFACE:
	case ::D3DRTYPE_TEXTURE:
	case ::D3DRTYPE_CUBETEXTURE:
		*width = info->desc2D.Width;
		*height = info->desc2D.Height;
		*depth = 1;
		return true;
	case ::D3DRTYPE_VOLUMETEXTURE:
		*width = info->desc3D.Width;
		*height = info->desc3D.Height;
		*depth = info->desc3D.Depth;
		return true;
	}

	return false;
}

void MarkResourceHashContaminated(D3D9Wrapper::IDirect3DResource9 *dest, UINT dstLevel,
	D3D9Wrapper::IDirect3DResource9 *src, UINT srcLevel, char type,
	::D3DBOX *DstBox, const ::D3DBOX *SrcBox)
{
	ResourceHandleInfo *dst_handle_info;
	struct ResourceHashInfo *dstInfo, *srcInfo = NULL;
	uint32_t srcHash = 0, dstHash = 0;
	UINT srcWidth = 1, srcHeight = 1, srcDepth = 1;// , srcMip = 0, srcIdx = 0, srcArraySize = 1;
	UINT dstWidth = 1, dstHeight = 1, dstDepth = 1;// , dstMip = 0, dstIdx = 0, dstArraySize = 1;
	bool partial = false;
	ResourceInfoMap::iterator info_i;
	Profiling::State profiling_state;

	if (!dest)
		return;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	dst_handle_info = GetResourceHandleInfo(dest);
	if (!dst_handle_info)
		goto out;

	if (!supports_hash_tracking(dst_handle_info))
		goto out;

	dstHash = dst_handle_info->orig_hash;
	if (!dstHash)
		goto out;

	EnterCriticalSection(&G->mCriticalSection);

	// Faster than catching an out_of_range exception from .at():
	info_i = G->mResourceInfo.find(dstHash);
	if (info_i == G->mResourceInfo.end())
		goto out_unlock;
	dstInfo = &info_i->second;

	GetResourceInfoFields(dstInfo,
		&dstWidth, &dstHeight, &dstDepth);

	if (src) {
		srcHash = GetOrigResourceHash(src);
		G->mCopiedResourceInfo.insert(srcHash);

		// Faster than catching an out_of_range exception from .at():
		info_i = G->mResourceInfo.find(srcHash);
		if (info_i != G->mResourceInfo.end()) {
			srcInfo = &info_i->second;
			GetResourceInfoFields(srcInfo,
				&srcWidth, &srcHeight, &srcDepth);

			if (dstHash != srcHash && srcInfo->initial_data_used_in_hash) {
				dstInfo->initial_data_used_in_hash = true;
				if (G->track_texture_updates == 0)
					dstInfo->hash_contaminated = true;
			}
		}
	}

	switch (type) {
	case 'L':
		dstInfo->lock_contamination.insert(dstLevel);
		dstInfo->initial_data_used_in_hash = true;
		if (G->track_texture_updates == 0)
			dstInfo->hash_contaminated = true;
		break;
	case 'C':
		dstInfo->copy_contamination.insert(srcHash);
		break;
	case 'R':

		// We especially want to know if a region copy copied
		// the entire texture, or only part of it. This may be
		// important if we end up changing the hash due to a
		// copy operation - if it copied the whole resource, we
		// can just use the hash of the source. If it only
		// copied a partial resource there's no good answer.

		partial = partial || dstWidth != srcWidth;
		partial = partial || dstHeight != srcHeight;
		partial = partial || dstDepth != srcDepth;

		if (DstBox) {
			partial = partial ||
				(DstBox->Right - DstBox->Left != dstWidth) ||
				(DstBox->Bottom - DstBox->Top != dstHeight) ||
				(DstBox->Back - DstBox->Front != dstDepth);
		}
		if (SrcBox) {
			partial = partial ||
				(SrcBox->Right - SrcBox->Left != srcWidth) ||
				(SrcBox->Bottom - SrcBox->Top != srcHeight) ||
				(SrcBox->Back - SrcBox->Front != srcDepth);
		}

		// TODO: Need to think about the implications of
		// copying between textures with > 1 array element.
		// Might want to reconsider how these are hashed (e.g.
		// hash each non-mipmap subresource separately and xor
		// the hashes together so we can efficiently change a
		// single subhash)

		dstInfo->region_contamination[
			std::make_tuple(srcHash, srcLevel, dstLevel)
		].Update(partial, DstBox, SrcBox);
	}

out_unlock:
	LeaveCriticalSection(&G->mCriticalSection);

out:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::hash_tracking_overhead);
}
void UpdateResourceHashFromCPU(D3D9Wrapper::IDirect3DResource9 * resource, ::D3DLOCKED_BOX * pLockedBox)
{
	::D3DRESOURCETYPE type;
	D3D2DTEXTURE_DESC *desc2D;
	D3D3DTEXTURE_DESC *desc3D;
	uint32_t old_data_hash, old_hash;
	ResourceHandleInfo *info = NULL;
	Profiling::State profiling_state;

	if (!resource || !pLockedBox || !pLockedBox->pBits)
		return;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	info = GetResourceHandleInfo(resource);
	if (!info)
		goto out;

	if (!supports_hash_tracking(info))
		goto out;

	// TODO: We currently store the desc structure that was originally used
	// when the resource was created. We can query the desc from the
	// resource directly to save memory, but there are some potential
	// differences between what we stored and what we get from the query.
	// MipMaps may be set to 0 at creation time, which will cause DX to
	// generate them and I presume would then be set when we query the
	// desc. Most of the other fields should be the same, but I'm not
	// positive about all the misc flags. Once we understand all possible
	// differences we could just store those instead of the whole struct.

	old_data_hash = info->data_hash;
	old_hash = info->hash;

	type = resource->GetD3DResource9()->GetType();
	switch (type) {
	case ::D3DRTYPE_SURFACE:
	case ::D3DRTYPE_TEXTURE:
	case ::D3DRTYPE_CUBETEXTURE:
		desc2D = &info->desc2D;
		info->data_hash = Calc2DDataHash(desc2D, pLockedBox);
		info->hash = CalcDescHash(info->data_hash, desc2D);
		break;
	case ::D3DRTYPE_VOLUMETEXTURE:
		desc3D = &info->desc3D;
		info->data_hash = Calc3DDataHash(desc3D, pLockedBox);
		info->hash = CalcDescHash(info->data_hash, desc3D);
	}

	LogDebug("Updated resource hash\n");
	LogDebug("  old data: %08x new data: %08x\n", old_data_hash, info->data_hash);
	LogDebug("  old hash: %08x new hash: %08x\n", old_hash, info->hash);

out:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::hash_tracking_overhead);
}
void PropagateResourceHash(D3D9Wrapper::IDirect3DResource9 *dst, D3D9Wrapper::IDirect3DResource9 *src)
{
	ResourceHandleInfo *dst_info, *src_info;
	::D3DRESOURCETYPE type;
	D3D2DTEXTURE_DESC *desc2D;
	D3D3DTEXTURE_DESC *desc3D;
	uint32_t old_data_hash, old_hash;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	dst_info = GetResourceHandleInfo(dst);
	if (!dst_info)
		goto out;

	if (!supports_hash_tracking(dst_info))
		goto out;

	src_info = GetResourceHandleInfo(src);
	if (!src_info)
		goto out;

	// If there was no initial data in either source or destination, or
	// they both contain the same data, we don't need to recalculate the
	// hash as it will not change:
	if (src_info->data_hash == dst_info->data_hash)
		goto out;

	// XXX: If the destination had an initial data but the source did not
	// we will currently discard the data part of the hash - is that the
	// best thing to do, or should we leave the destination untouched? I'm
	// going on the assumption that we care more about what the hash
	// represents *now* rather than when it was created and if a texture is
	// being dynamically updated by the GPU it doesn't make sense to use
	// the initial data hash... but I'm not certain that will be the best
	// decision for every game... We could always make it an option in the
	// d3dx.ini if need be...

	old_data_hash = dst_info->data_hash;
	old_hash = dst_info->hash;

	dst_info->data_hash = src_info->data_hash;

	type = dst->GetD3DResource9()->GetType();
	switch (type) {
	case ::D3DRTYPE_SURFACE:
	case ::D3DRTYPE_TEXTURE:
	case ::D3DRTYPE_CUBETEXTURE:
		desc2D = &dst_info->desc2D;
		dst_info->hash = CalcDescHash(dst_info->data_hash, desc2D);
		break;
	case ::D3DRTYPE_VOLUMETEXTURE:
		desc3D = &dst_info->desc3D;
		dst_info->hash = CalcDescHash(dst_info->data_hash, desc3D);
		break;
	}

	LogDebug("Propagated resource hash\n");
	LogDebug("  old data: %08x new data: %08x\n", old_data_hash, dst_info->data_hash);
	LogDebug("  old hash: %08x new hash: %08x\n", old_hash, dst_info->hash);

out:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::hash_tracking_overhead);
}

bool LockTrackResourceHashUpdate(D3D9Wrapper::IDirect3DResource9 *pResource, UINT Level)
{
	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pResource, Level, NULL, NULL, 'L', NULL, NULL);
	}

	// TODO: If track_texture_updated is disabled, but we are in hunting
	// with a reloadable config, we might consider tracking the data hash
	// updates regardless (just not the full resource hash) so the option
	// can be turned on live and work. But there's a few pieces we would
	// need for that to work so for now let's not over-complicate things.
	return G->track_texture_updates == 1 && Level == 0;
}
// -----------------------------------------------------------------------------------------------
//                       Fuzzy Texture Override Matching Support
// -----------------------------------------------------------------------------------------------

FuzzyMatch::FuzzyMatch()
{
	op = FuzzyMatchOp::ALWAYS;
	rhs_type1 = FuzzyMatchOperandType::VALUE;
	rhs_type2 = FuzzyMatchOperandType::VALUE;
	val = 0;
	mask = 0xffffffff;
	numerator = 1;
	denominator = 1;
}

static UINT get_resource_width(const ::D3DVERTEXBUFFER_DESC *desc) { return 0; }
static UINT get_resource_width(const ::D3DINDEXBUFFER_DESC *desc) { return 0; }
static UINT get_resource_width(const ::D3DSURFACE_DESC *desc) { return desc->Width; }
static UINT get_resource_width(const D3D2DTEXTURE_DESC *desc) { return desc->Width; }
static UINT get_resource_width(const D3D3DTEXTURE_DESC *desc) { return desc->Width; }
static UINT get_resource_height(const ::D3DVERTEXBUFFER_DESC *desc) { return 0; }
static UINT get_resource_height(const ::D3DINDEXBUFFER_DESC *desc) { return 0; }
static UINT get_resource_height(const ::D3DSURFACE_DESC *desc) { return desc->Height; }
static UINT get_resource_height(const D3D2DTEXTURE_DESC *desc) { return desc->Height; }
static UINT get_resource_height(const D3D3DTEXTURE_DESC *desc) { return desc->Height; }
static UINT get_resource_depth(const ::D3DVERTEXBUFFER_DESC *desc) { return 0; }
static UINT get_resource_depth(const ::D3DINDEXBUFFER_DESC *desc) { return 0; }
static UINT get_resource_depth(const ::D3DSURFACE_DESC *desc) { return 0; }
static UINT get_resource_depth(const D3D2DTEXTURE_DESC *desc) { return 0; }
static UINT get_resource_depth(const D3D3DTEXTURE_DESC *desc) { return desc->Depth; }
template <typename DescType>
static UINT eval_field(FuzzyMatchOperandType type, UINT val, const DescType *desc)
{
	switch (type) {
	case FuzzyMatchOperandType::VALUE:
		return val;
	case FuzzyMatchOperandType::WIDTH:
		return get_resource_width(desc);
	case FuzzyMatchOperandType::HEIGHT:
		return get_resource_height(desc);
	case FuzzyMatchOperandType::DEPTH:
		return get_resource_depth(desc);
	case FuzzyMatchOperandType::RES_WIDTH:
		return G->mResolutionInfo.width;
	case FuzzyMatchOperandType::RES_HEIGHT:
		return G->mResolutionInfo.height;
	};
	LogOverlay(LOG_DIRE, "BUG: Invalid fuzzy field %u\n", type);
	return val;
}
template <typename DescType>
bool FuzzyMatch::matches(UINT lhs, const DescType *desc) const
{
	UINT effective;

	// Common case:
	if (op == FuzzyMatchOp::ALWAYS)
		return true;

	effective = eval_field(rhs_type1, val, desc);
	// Second named field, for match_byte_width = res_width * res_height in RE7
	effective *= eval_field(rhs_type2, 1, desc);
	return matches_common(lhs, effective);
}

bool FuzzyMatch::matches_uint(UINT lhs) const
{
	// Common case:
	if (op == FuzzyMatchOp::ALWAYS)
		return true;
	if (rhs_type1 != FuzzyMatchOperandType::VALUE)
		return false;
	return matches_common(lhs, val);
}

bool FuzzyMatch::matches_common(UINT lhs, UINT effective) const
{
	// For now just supporting a single integer numerator and denominator,
	// which should be sufficient to match most aspect ratios, downsampled
	// textures and so on. TODO: Add a full expression evaluator.
	if (!denominator)
		return false;
	effective = effective * numerator / denominator;

	switch (op) {
	case FuzzyMatchOp::EQUAL:
		// Only case that the mask applies to, for flags fields
		return ((lhs & mask) == effective);
	case FuzzyMatchOp::LESS:
		return (lhs < effective);
	case FuzzyMatchOp::LESS_EQUAL:
		return (lhs <= effective);
	case FuzzyMatchOp::GREATER:
		return (lhs > effective);
	case FuzzyMatchOp::GREATER_EQUAL:
		return (lhs >= effective);
	case FuzzyMatchOp::NOT_EQUAL:
		return (lhs != effective);
	};

	return false;
}
FuzzyMatchResourceDesc::FuzzyMatchResourceDesc(std::wstring section) :
	priority(0),
	matches_vbuffer(true),
	matches_ibuffer(true),
	matches_tex(true),
	matches_volumetex(true),
	matches_cubetex(true),
	matches_surface(true)
{

	// TODO: Statically contain this once we sort out our header files:
	texture_override = new TextureOverride();
	texture_override->ini_section = section;
}

FuzzyMatchResourceDesc::~FuzzyMatchResourceDesc()
{
	delete texture_override;
}
template <typename DescType>
bool FuzzyMatchResourceDesc::check_common_resource_fields(const DescType *desc) const
{
	if (!Type.matches(desc->Type, desc))
		return false;
	if (!Format.matches(desc->Format, desc))
		return false;
	if (!Usage.matches(desc->Usage, desc))
		return false;
	if (!Pool.matches(desc->Pool, desc))
		return false;
	return true;
}
template <typename DescType>
bool FuzzyMatchResourceDesc::check_common_surface_fields(const DescType *desc) const
{
	if (!Height.matches(desc->Height, desc))
		return false;
	if (!Width.matches(desc->Width, desc))
		return false;
	return true;
}
template <typename DescType>
bool FuzzyMatchResourceDesc::check_common_buffer_fields(const DescType *desc) const
{
	if (!Size.matches(desc->Size, desc))
		return false;
	return true;
}
bool FuzzyMatchResourceDesc::matches(const ::D3DVERTEXBUFFER_DESC *desc) const
{
	if (!matches_vbuffer)
		return false;

	if (!check_common_resource_fields(desc))
		return false;
	if (!check_common_buffer_fields(desc))
		return false;
	if (!FVF.matches(desc->FVF, desc))
		return false;
	return true;
}

bool FuzzyMatchResourceDesc::matches(const ::D3DINDEXBUFFER_DESC *desc) const
{
	if (!matches_ibuffer)
		return false;

	if (!check_common_resource_fields(desc))
		return false;

	if (!check_common_buffer_fields(desc))
		return false;
	return true;
}
bool FuzzyMatchResourceDesc::matches(const D3D2DTEXTURE_DESC *desc) const
{
	if (!matches_surface && !matches_tex && !matches_cubetex)
		return false;

	if (!check_common_resource_fields(desc))
		return false;
	if (!check_common_surface_fields(desc))
		return false;
	if (!MultiSampleType.matches(desc->MultiSampleType, desc))
		return false;
	if (!MultiSampleQuality.matches(desc->MultiSampleQuality, desc))
		return false;
	return true;
}
bool FuzzyMatchResourceDesc::matches(const D3D3DTEXTURE_DESC *desc) const
{
	if (!matches_volumetex)
		return false;
	if (!check_common_resource_fields(desc))
		return false;
	if (!check_common_surface_fields(desc))
		return false;
	if (!Depth.matches(desc->Depth, desc))
		return false;
	return true;
}
void FuzzyMatchResourceDesc::set_resource_type(::D3DRESOURCETYPE type)
{
	switch (type) {
	case ::D3DRTYPE_VERTEXBUFFER:
		matches_tex = matches_volumetex = matches_cubetex = matches_ibuffer = matches_surface = false;
		return;
	case ::D3DRTYPE_INDEXBUFFER:
		matches_tex = matches_volumetex = matches_cubetex = matches_vbuffer = matches_surface = false;
		return;
	case ::D3DRTYPE_TEXTURE:
		matches_ibuffer = matches_volumetex = matches_cubetex = matches_vbuffer = matches_surface = false;
		return;
	case ::D3DRTYPE_VOLUMETEXTURE:
		matches_ibuffer = matches_tex = matches_cubetex = matches_vbuffer = matches_surface = false;
		return;
	case ::D3DRTYPE_CUBETEXTURE:
		matches_ibuffer = matches_tex = matches_volumetex = matches_vbuffer = matches_surface = false;
		return;
	case ::D3DRTYPE_SURFACE:
		matches_ibuffer = matches_tex = matches_volumetex = matches_vbuffer = matches_cubetex = false;
		return;
	}
}

bool FuzzyMatchResourceDesc::update_types_matched()
{
	// This will remove the flags for types of resources we cannot match
	// based on what fields we are matching and what resource types they
	// apply to. We do not set a flag if it was already cleared, since the
	// user may have already specified a specific resource type. If we are
	// left with no possible resource types we can match we will return
	// false so that the caller knows this is invalid.

	if (FuzzyMatchOp::ALWAYS != FVF.op)
		matches_ibuffer = matches_tex = matches_volumetex = matches_cubetex = matches_surface = false;

	if (FuzzyMatchOp::ALWAYS != Size.op)
		matches_tex = matches_volumetex = matches_cubetex = matches_surface = false;

	if (FuzzyMatchOp::ALWAYS != Levels.op)
		matches_ibuffer = matches_vbuffer = matches_surface = false;

	if (FuzzyMatchOp::ALWAYS != Width.op || FuzzyMatchOp::ALWAYS != Height.op)
		matches_vbuffer = matches_ibuffer = false;

	if (FuzzyMatchOp::ALWAYS != Depth.op)
		matches_tex = matches_ibuffer = matches_vbuffer = matches_cubetex = matches_surface = false;

	if (FuzzyMatchOp::ALWAYS != MultiSampleType.op
		|| FuzzyMatchOp::ALWAYS != MultiSampleQuality.op)
		matches_vbuffer = matches_ibuffer = matches_tex = matches_cubetex = matches_volumetex = false;

	return matches_vbuffer || matches_ibuffer || matches_tex || matches_volumetex || matches_cubetex || matches_surface;
}
static bool matches_draw_info(TextureOverride *tex_override, DrawCallInfo *call_info)
{
	if (!tex_override->has_draw_context_match)
		return true;
	if (!call_info)
		return false;
	if (!tex_override->match_first_vertex.matches_uint(call_info->StartVertex))
		return false;
	if (!tex_override->match_first_index.matches_uint(call_info->StartIndex))
		return false;
	if (!tex_override->match_vertex_count.matches_uint(DrawPrimitiveCountToVerticesCount(call_info->PrimitiveCount, call_info->primitive_type)))
		return false;
	if (!tex_override->match_index_count.matches_uint(DrawPrimitiveCountToVerticesCount(call_info->PrimitiveCount, call_info->primitive_type)))
		return false;
	return true;
}
static void find_texture_override_for_hash(uint32_t hash, TextureOverrideMatches *matches, DrawCallInfo *call_info)
{
	TextureOverrideMap::iterator i;
	TextureOverrideList::iterator j;
	i = lookup_textureoverride(hash);
	if (i == G->mTextureOverrideMap.end())
		return;
	for (j = i->second.begin(); j != i->second.end(); j++) {
		if (matches_draw_info(&(*j), call_info))
			matches->push_back(&(*j));
	}
}

static void find_texture_override_for_resource_by_hash(ResourceHandleInfo *info, TextureOverrideMatches *matches, DrawCallInfo *call_info)
{
	uint32_t hash = 0;

	if (!info)
		return;

	if (G->mTextureOverrideMap.empty())
		return;

	if (!info->hash)
		return;

	find_texture_override_for_hash(info->hash, matches, call_info);
}

template <typename DescType>
static void find_texture_overrides_for_desc(const DescType *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info)
{
	FuzzyTextureOverrides::iterator i;

	for (i = G->mFuzzyTextureOverrides.begin(); i != G->mFuzzyTextureOverrides.end(); i++) {
		if ((*i)->matches(desc) && matches_draw_info((*i)->texture_override, call_info))
			matches->push_back((*i)->texture_override);
	}
}

template <typename DescType>
void find_texture_overrides(uint32_t hash, const DescType *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info)
{
	find_texture_override_for_hash(hash, matches, call_info);
	if (!matches->empty()) {
		// If we got a result it was matched by hash - that's an exact
		// match and we don't process any fuzzy matches
		return;
	}

	find_texture_overrides_for_desc(desc, matches, call_info);
}
// Explicit template expansion is necessary to generate these functions for
// the compiler to generate them so they can be used from other source files:
template void find_texture_overrides<::D3DVERTEXBUFFER_DESC>(uint32_t hash, const ::D3DVERTEXBUFFER_DESC *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info);
template void find_texture_overrides<::D3DINDEXBUFFER_DESC>(uint32_t hash, const ::D3DINDEXBUFFER_DESC *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info);
template void find_texture_overrides<D3D2DTEXTURE_DESC>(uint32_t hash, const D3D2DTEXTURE_DESC *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info);
template void find_texture_overrides<D3D3DTEXTURE_DESC>(uint32_t hash, const D3D3DTEXTURE_DESC *desc, TextureOverrideMatches *matches, DrawCallInfo *call_info);
void find_texture_overrides_for_resource(::IDirect3DResource9 *resource, ResourceHandleInfo *info, TextureOverrideMatches *matches, DrawCallInfo *call_info)
{
	::D3DRESOURCETYPE type;
	::IDirect3DVertexBuffer9 *vbuf = NULL;
	::IDirect3DIndexBuffer9 *ibuf = NULL;
	::IDirect3DSurface9 *sur = NULL;
	::IDirect3DVolume9 *vol = NULL;
	::IDirect3DTexture9 *tex2d = NULL;
	::IDirect3DCubeTexture9 *texCube = NULL;
	::IDirect3DVolumeTexture9 *tex3d = NULL;
	::D3DVERTEXBUFFER_DESC vbuf_desc;
	::D3DINDEXBUFFER_DESC ibuf_desc;
	find_texture_override_for_resource_by_hash(info, matches, call_info);
	if (!matches->empty()) {
		// If we got a result it was matched by hash - that's an exact
		// match and we don't process any fuzzy matches
		return;
	}

	type = resource->GetType();
	switch (type) {
	case ::D3DRTYPE_VERTEXBUFFER:
		vbuf = (::IDirect3DVertexBuffer9*)resource;
		vbuf->GetDesc(&vbuf_desc);
		return find_texture_overrides_for_desc(&vbuf_desc, matches, call_info);
	case ::D3DRTYPE_INDEXBUFFER:
		ibuf = (::IDirect3DIndexBuffer9*)resource;
		ibuf->GetDesc(&ibuf_desc);
		return find_texture_overrides_for_desc(&ibuf_desc, matches, call_info);
	case ::D3DRTYPE_SURFACE:
		sur = (::IDirect3DSurface9*)resource;
		return find_texture_overrides_for_desc(&D3D2DTEXTURE_DESC(sur), matches, call_info);
	case ::D3DRTYPE_TEXTURE:
		tex2d = (::IDirect3DTexture9*)resource;
		return find_texture_overrides_for_desc(&D3D2DTEXTURE_DESC(tex2d), matches, call_info);
	case ::D3DRTYPE_CUBETEXTURE:
		texCube = (::IDirect3DCubeTexture9*)resource;
		return find_texture_overrides_for_desc(&D3D2DTEXTURE_DESC(texCube), matches, call_info);
	case ::D3DRTYPE_VOLUMETEXTURE:
		tex3d = (::IDirect3DVolumeTexture9*)resource;
		return find_texture_overrides_for_desc(&D3D3DTEXTURE_DESC(tex3d), matches, call_info);
	}
}

bool TextureOverrideLess(const struct TextureOverride &lhs, const struct TextureOverride &rhs)
{
	// For texture create time overrides we want the highest priority
	// texture override to take precedence, which will happen if it is
	// processed last. Same goes for texture filtering. If the priorities
	// are equal, we use the ini section name for sorting to make sure that
	// we get consistent results.

	if (lhs.priority != rhs.priority)
		return lhs.priority < rhs.priority;
	return lhs.ini_section < rhs.ini_section;
}
bool FuzzyMatchResourceDescLess::operator() (const std::shared_ptr<FuzzyMatchResourceDesc> &lhs, const std::shared_ptr<FuzzyMatchResourceDesc> &rhs) const
{
	return TextureOverrideLess(*lhs->texture_override, *rhs->texture_override);
}