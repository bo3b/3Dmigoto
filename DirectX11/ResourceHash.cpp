#include "ResourceHash.h"

#include "log.h"
#include "util.h"
#include "globals.h"

// Overloaded functions to log any kind of resource description (useful to call
// from templates):

template <typename DescType>
static void LogResourceDescCommon(DescType *desc)
{
	LogInfo("    Usage = %d\n", desc->Usage);
	LogInfo("    BindFlags = 0x%x\n", desc->BindFlags);
	LogInfo("    CPUAccessFlags = 0x%x\n", desc->CPUAccessFlags);
	LogInfo("    MiscFlags = 0x%x\n", desc->MiscFlags);
}

void LogResourceDesc(const D3D11_BUFFER_DESC *desc)
{
	LogInfo("  Resource Type = Buffer\n");
	LogInfo("    ByteWidth = %d\n", desc->ByteWidth);
	LogResourceDescCommon(desc);
	LogInfo("    StructureByteStride = %d\n", desc->StructureByteStride);
}

void LogResourceDesc(const D3D11_TEXTURE1D_DESC *desc)
{
	LogInfo("  Resource Type = Texture1D\n");
	LogInfo("    Width = %d\n", desc->Width);
	LogInfo("    MipLevels = %d\n", desc->MipLevels);
	LogInfo("    ArraySize = %d\n", desc->ArraySize);
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	LogResourceDescCommon(desc);
}

void LogResourceDesc(const D3D11_TEXTURE2D_DESC *desc)
{
	LogInfo("  Resource Type = Texture2D\n");
	LogInfo("    Width = %d\n", desc->Width);
	LogInfo("    Height = %d\n", desc->Height);
	LogInfo("    MipLevels = %d\n", desc->MipLevels);
	LogInfo("    ArraySize = %d\n", desc->ArraySize);
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	LogInfo("    SampleDesc.Count = %d\n", desc->SampleDesc.Count);
	LogInfo("    SampleDesc.Quality = %d\n", desc->SampleDesc.Quality);
	LogResourceDescCommon(desc);
}

void LogResourceDesc(const D3D11_TEXTURE3D_DESC *desc)
{
	LogInfo("  Resource Type = Texture3D\n");
	LogInfo("    Width = %d\n", desc->Width);
	LogInfo("    Height = %d\n", desc->Height);
	LogInfo("    Depth = %d\n", desc->Depth);
	LogInfo("    MipLevels = %d\n", desc->MipLevels);
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	LogResourceDescCommon(desc);
}

void LogResourceDesc(ID3D11Resource *resource)
{
	D3D11_RESOURCE_DIMENSION dim;
	ID3D11Buffer *buffer;
	ID3D11Texture1D *tex_1d;
	ID3D11Texture2D *tex_2d;
	ID3D11Texture3D *tex_3d;
	D3D11_BUFFER_DESC buffer_desc;
	D3D11_TEXTURE1D_DESC desc_1d;
	D3D11_TEXTURE2D_DESC desc_2d;
	D3D11_TEXTURE3D_DESC desc_3d;

	resource->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			buffer = (ID3D11Buffer*)resource;
			buffer->GetDesc(&buffer_desc);
			return LogResourceDesc(&buffer_desc);
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			tex_1d = (ID3D11Texture1D*)resource;
			tex_1d->GetDesc(&desc_1d);
			return LogResourceDesc(&desc_1d);
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex_2d = (ID3D11Texture2D*)resource;
			tex_2d->GetDesc(&desc_2d);
			return LogResourceDesc(&desc_2d);
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex_3d = (ID3D11Texture3D*)resource;
			tex_3d->GetDesc(&desc_3d);
			return LogResourceDesc(&desc_3d);
	}
}

void LogViewDesc(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc)
{
	LogInfo("  View Type = Shader Resource\n");
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	switch (desc->ViewDimension) {
		case D3D11_SRV_DIMENSION_UNKNOWN:
			LogInfo("    ViewDimension = UNKNOWN\n");
			break;
		case D3D11_SRV_DIMENSION_BUFFER:
			LogInfo("    ViewDimension = BUFFER\n");
			LogInfo("      Buffer.FirstElement/NumElements = %u\n", desc->Buffer.FirstElement);
			LogInfo("      Buffer.ElementOffset/ElementWidth = %u\n", desc->Buffer.ElementOffset);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE1D:
			LogInfo("    ViewDimension = TEXTURE1D\n");
			LogInfo("      Texture1D.MostDetailedMip = %u\n", desc->Texture1D.MostDetailedMip);
			LogInfo("      Texture1D.MipLevels = %d\n", desc->Texture1D.MipLevels);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
			LogInfo("    ViewDimension = TEXTURE1DARRAY\n");
			LogInfo("      Texture1DArray.MostDetailedMip = %u\n", desc->Texture1DArray.MostDetailedMip);
			LogInfo("      Texture1DArray.MipLevels = %d\n", desc->Texture1DArray.MipLevels);
			LogInfo("      Texture1DArray.FirstArraySlice = %u\n", desc->Texture1DArray.FirstArraySlice);
			LogInfo("      Texture1DArray.ArraySize = %u\n", desc->Texture1DArray.ArraySize);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2D:
			LogInfo("    ViewDimension = TEXTURE2D\n");
			LogInfo("      Texture2D.MostDetailedMip = %u\n", desc->Texture2D.MostDetailedMip);
			LogInfo("      Texture2D.MipLevels = %d\n", desc->Texture2D.MipLevels);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
			LogInfo("    ViewDimension = TEXTURE2DARRAY\n");
			LogInfo("      Texture2DArray.MostDetailedMip = %u\n", desc->Texture2DArray.MostDetailedMip);
			LogInfo("      Texture2DArray.MipLevels = %d\n", desc->Texture2DArray.MipLevels);
			LogInfo("      Texture2DArray.FirstArraySlice = %u\n", desc->Texture2DArray.FirstArraySlice);
			LogInfo("      Texture2DArray.ArraySize = %u\n", desc->Texture2DArray.ArraySize);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DMS:
			LogInfo("    ViewDimension = TEXTURE2DMS\n");
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
			LogInfo("    ViewDimension = TEXTURE2DMSARRAY\n");
			LogInfo("      Texture2DMSArray.FirstArraySlice = %u\n", desc->Texture2DMSArray.FirstArraySlice);
			LogInfo("      Texture2DMSArray.ArraySize = %u\n", desc->Texture2DMSArray.ArraySize);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE3D:
			LogInfo("    ViewDimension = TEXTURE3D\n");
			LogInfo("      Texture3D.MostDetailedMip = %u\n", desc->Texture3D.MostDetailedMip);
			LogInfo("      Texture3D.MipLevels = %d\n", desc->Texture3D.MipLevels);
			break;
		case D3D11_SRV_DIMENSION_TEXTURECUBE:
			LogInfo("    ViewDimension = TEXTURECUBE\n");
			LogInfo("      TextureCube.MostDetailedMip = %u\n", desc->TextureCube.MostDetailedMip);
			LogInfo("      TextureCube.MipLevels = %d\n", desc->TextureCube.MipLevels);
			break;
		case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
			LogInfo("    ViewDimension = TEXTURECUBEARRAY\n");
			LogInfo("      TextureCubeArray.MostDetailedMip = %u\n", desc->TextureCubeArray.MostDetailedMip);
			LogInfo("      TextureCubeArray.MipLevels = %d\n", desc->TextureCubeArray.MipLevels);
			LogInfo("      TextureCubeArray.First2DArrayFace = %u\n", desc->TextureCubeArray.First2DArrayFace);
			LogInfo("      TextureCubeArray.NumCubes = %u\n", desc->TextureCubeArray.NumCubes);
			break;
		case D3D11_SRV_DIMENSION_BUFFEREX:
			LogInfo("    ViewDimension = BUFFEREX\n");
			LogInfo("      BufferEx.FirstElement = %u\n", desc->BufferEx.FirstElement);
			LogInfo("      BufferEx.NumElements = %u\n", desc->BufferEx.NumElements);
			LogInfo("      BufferEx.Flags = 0x%x\n", desc->BufferEx.Flags);
			break;
	}
}

void LogViewDesc(const D3D11_RENDER_TARGET_VIEW_DESC *desc)
{
	LogInfo("  View Type = Render Target\n");
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	switch (desc->ViewDimension) {
		case D3D11_RTV_DIMENSION_UNKNOWN:
			LogInfo("    ViewDimension = UNKNOWN\n");
			break;
		case D3D11_RTV_DIMENSION_BUFFER:
			LogInfo("    ViewDimension = BUFFER\n");
			LogInfo("      Buffer.FirstElement/NumElements = %u\n", desc->Buffer.FirstElement);
			LogInfo("      Buffer.ElementOffset/ElementWidth = %u\n", desc->Buffer.ElementOffset);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE1D:
			LogInfo("    ViewDimension = TEXTURE1D\n");
			LogInfo("      Texture1D.MipSlice = %u\n", desc->Texture1D.MipSlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
			LogInfo("    ViewDimension = TEXTURE1DARRAY\n");
			LogInfo("      Texture1DArray.MipSlice = %u\n", desc->Texture1DArray.MipSlice);
			LogInfo("      Texture1DArray.FirstArraySlice = %u\n", desc->Texture1DArray.FirstArraySlice);
			LogInfo("      Texture1DArray.ArraySize = %u\n", desc->Texture1DArray.ArraySize);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2D:
			LogInfo("    ViewDimension = TEXTURE2D\n");
			LogInfo("      Texture2D.MipSlice = %u\n", desc->Texture2D.MipSlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
			LogInfo("    ViewDimension = TEXTURE2DARRAY\n");
			LogInfo("      Texture2DArray.MipSlice = %u\n", desc->Texture2DArray.MipSlice);
			LogInfo("      Texture2DArray.FirstArraySlice = %u\n", desc->Texture2DArray.FirstArraySlice);
			LogInfo("      Texture2DArray.ArraySize = %u\n", desc->Texture2DArray.ArraySize);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DMS:
			LogInfo("    ViewDimension = TEXTURE2DMS\n");
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
			LogInfo("    ViewDimension = TEXTURE2DMSARRAY\n");
			LogInfo("      Texture2DMSArray.FirstArraySlice = %u\n", desc->Texture2DMSArray.FirstArraySlice);
			LogInfo("      Texture2DMSArray.ArraySize = %u\n", desc->Texture2DMSArray.ArraySize);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE3D:
			LogInfo("    ViewDimension = TEXTURE3D\n");
			LogInfo("      Texture3D.MipSlice = %u\n", desc->Texture3D.MipSlice);
			LogInfo("      Texture3D.FirstWSlice = %u\n", desc->Texture3D.FirstWSlice);
			LogInfo("      Texture3D.WSize = %u\n", desc->Texture3D.WSize);
			break;
	}
}

void LogViewDesc(const D3D11_DEPTH_STENCIL_VIEW_DESC *desc)
{
	LogInfo("  View Type = Depth Stencil\n");
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	LogInfo("    Flags = 0x%x\n", desc->Flags);
	switch (desc->ViewDimension) {
		case D3D11_DSV_DIMENSION_UNKNOWN:
			LogInfo("    ViewDimension = UNKNOWN\n");
			break;
		case D3D11_DSV_DIMENSION_TEXTURE1D:
			LogInfo("    ViewDimension = TEXTURE1D\n");
			LogInfo("      Texture1D.MipSlice = %u\n", desc->Texture1D.MipSlice);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
			LogInfo("    ViewDimension = TEXTURE1DARRAY\n");
			LogInfo("      Texture1DArray.MipSlice = %u\n", desc->Texture1DArray.MipSlice);
			LogInfo("      Texture1DArray.FirstArraySlice = %u\n", desc->Texture1DArray.FirstArraySlice);
			LogInfo("      Texture1DArray.ArraySize = %u\n", desc->Texture1DArray.ArraySize);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2D:
			LogInfo("    ViewDimension = TEXTURE2D\n");
			LogInfo("      Texture2D.MipSlice = %u\n", desc->Texture2D.MipSlice);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
			LogInfo("    ViewDimension = TEXTURE2DARRAY\n");
			LogInfo("      Texture2DArray.MipSlice = %u\n", desc->Texture2DArray.MipSlice);
			LogInfo("      Texture2DArray.FirstArraySlice = %u\n", desc->Texture2DArray.FirstArraySlice);
			LogInfo("      Texture2DArray.ArraySize = %u\n", desc->Texture2DArray.ArraySize);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2DMS:
			LogInfo("    ViewDimension = TEXTURE2DMS\n");
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
			LogInfo("    ViewDimension = TEXTURE2DMSARRAY\n");
			LogInfo("      Texture2DMSArray.FirstArraySlice = %u\n", desc->Texture2DMSArray.FirstArraySlice);
			LogInfo("      Texture2DMSArray.ArraySize = %u\n", desc->Texture2DMSArray.ArraySize);
			break;
	}
}

void LogViewDesc(const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc)
{
	LogInfo("  View Type = Unordered Access\n");
	LogInfo("    Format = %s (%d)\n", TexFormatStr(desc->Format), desc->Format);
	switch (desc->ViewDimension) {
		case D3D11_UAV_DIMENSION_UNKNOWN:
			LogInfo("    ViewDimension = UNKNOWN\n");
			break;
		case D3D11_UAV_DIMENSION_BUFFER:
			LogInfo("    ViewDimension = BUFFER\n");
			LogInfo("      Buffer.FirstElement = %u\n", desc->Buffer.FirstElement);
			LogInfo("      Buffer.NumElements = %u\n", desc->Buffer.NumElements);
			LogInfo("      Buffer.Flags = 0x%x\n", desc->Buffer.Flags);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE1D:
			LogInfo("    ViewDimension = TEXTURE1D\n");
			LogInfo("      Texture1D.MipSlice = %u\n", desc->Texture1D.MipSlice);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
			LogInfo("    ViewDimension = TEXTURE1DARRAY\n");
			LogInfo("      Texture1DArray.MipSlice = %u\n", desc->Texture1DArray.MipSlice);
			LogInfo("      Texture1DArray.FirstArraySlice = %u\n", desc->Texture1DArray.FirstArraySlice);
			LogInfo("      Texture1DArray.ArraySize = %u\n", desc->Texture1DArray.ArraySize);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2D:
			LogInfo("    ViewDimension = TEXTURE2D\n");
			LogInfo("      Texture2D.MipSlice = %u\n", desc->Texture2D.MipSlice);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
			LogInfo("    ViewDimension = TEXTURE2DARRAY\n");
			LogInfo("      Texture2DArray.MipSlice = %u\n", desc->Texture2DArray.MipSlice);
			LogInfo("      Texture2DArray.FirstArraySlice = %u\n", desc->Texture2DArray.FirstArraySlice);
			LogInfo("      Texture2DArray.ArraySize = %u\n", desc->Texture2DArray.ArraySize);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE3D:
			LogInfo("    ViewDimension = TEXTURE3D\n");
			LogInfo("      Texture3D.MipSlice = %u\n", desc->Texture3D.MipSlice);
			LogInfo("      Texture3D.FirstWSlice = %u\n", desc->Texture3D.FirstWSlice);
			LogInfo("      Texture3D.WSize = %u\n", desc->Texture3D.WSize);
			break;
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

uint32_t CalcTexture2DDescHash(uint32_t initial_hash, const D3D11_TEXTURE2D_DESC *const_desc)
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

	D3D11_TEXTURE2D_DESC* desc = const_cast<D3D11_TEXTURE2D_DESC*>(const_desc);

	UINT saveWidth = desc->Width;
	UINT saveHeight = desc->Height;
	AdjustForConstResolution(&desc->Width, &desc->Height);

	uint32_t hash = crc32c_hw(initial_hash, desc, sizeof(D3D11_TEXTURE2D_DESC));

	desc->Width = saveWidth;
	desc->Height = saveHeight;

	return hash;
}

uint32_t CalcTexture3DDescHash(uint32_t initial_hash, const D3D11_TEXTURE3D_DESC *const_desc)
{
	// Same comment as in CalcTexture2DDescHash above - concerned about
	// inconsistent use of these resolution overrides

	D3D11_TEXTURE3D_DESC* desc = const_cast<D3D11_TEXTURE3D_DESC*>(const_desc);

	UINT saveWidth = desc->Width;
	UINT saveHeight = desc->Height;
	AdjustForConstResolution(&desc->Width, &desc->Height);

	uint32_t hash = crc32c_hw(initial_hash, desc, sizeof(D3D11_TEXTURE3D_DESC));

	desc->Width = saveWidth;
	desc->Height = saveHeight;

	return hash;
}

// -----------------------------------------------------------------------------------------------

static UINT CompressedFormatBlockSize(DXGI_FORMAT Format)
{
	switch (Format) {
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			return 8;

		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return 16;
	}

	return 0;
}

static size_t Texture2DLength(
	const D3D11_TEXTURE2D_DESC *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
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
		return pInitialData->SysMemPitch * mip_height;
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
	const D3D11_TEXTURE3D_DESC *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData,
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
		return pInitialData->SysMemSlicePitch * mip_depth;
	}

	// Not sure if SysMemSlicePitch is reliable for compressed 3D textures.
	// Use the mip-map width, height + depth padded to a multiple of 4 with
	// the 4x4 block size.

	padded_width = (mip_width + 3) & ~0x3;
	padded_height = (mip_height + 3) & ~0x3;

	return padded_width * padded_height * mip_depth / 16 * block_size;
}


uint32_t CalcTexture2DDataHash(
	const D3D11_TEXTURE2D_DESC *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData)
{
	uint32_t hash = 0;
	size_t length_v12;
	size_t length;
	UINT item = 0, level = 0, index;

	if (!pDesc || !pInitialData || !pInitialData->pSysMem)
		return 0;

	// In 3DMigoto v1.2, this is what we were using as the length of the
	// buffer in bytes. Unfortunately this is not right since pDesc->Width
	// is in texels, not bytes, and if pDesc->ArraySize was greater than 1
	// it signifies that there are additional separate buffers to consider,
	// while we treated it as making the first buffer longer. Additionally,
	// compressed textures complicate the buffer size calculation further.
	//
	// The result is that we might not consider the entire buffer when
	// calculating the hash (which may not be ideal, but it is generally
	// acceptable), or we might overflow the buffer. If we overflow we
	// might get an exception if there is nothing mapped after the buffer
	// (which we catch and log), but we could just as easily process
	// gargage after the buffer as being part of the texture, which would
	// lead to us creating unpredictable hashes.
	length_v12 = pDesc->Width * pDesc->Height * pDesc->ArraySize;

	// Compare the old broken length to the length of just the first item.
	// If the broken length is shorter, we will just use that and skip
	// considering additional entries in the array. While not ideal, this
	// will minimise the pain of changing the texture hash so soon after
	// the last time.
	//
	// TODO: We might consider an ini setting to disable this fallback for
	// new games, or possibly to force it for old games.
	length = Texture2DLength(pDesc, &pInitialData[0], 0);
	LogDebug("  Texture2D length: %Iu bad v1.2.1 length: %Iu\n", length, length_v12);
	if (length_v12 <= length) {
		if (length_v12 < length || pDesc->ArraySize > 1) {
			LogDebug("  Using 3DMigoto v1.2.1 compatible Texture2D CRC calculation\n");
		}
		return crc32c_hw(hash, pInitialData[0].pSysMem, length_v12);
	}

	// If we are here it means the old length had overflowed the buffer,
	// which means we could not rely on it being a consistent value unless
	// we got lucky and the memory following the buffer was always
	// consistent (and even if we did, can we be sure every player will,
	// and that it won't change when the game is updated?).
	//
	// In that case, let's do it right... and hopefully this will be the
	// last time we need to change this.

	LogDebug("  Using 3DMigoto v1.2.11+ Texture2D CRC calculation\n");

	// We are no longer taking multiple subresources into account in the
	// hash. We did for a short time between 3DMigoto 1.2.9 and 1.2.10, but
	// then it was discovered that some games are updating resources which
	// made matching their hash impossible without tracking the updates.
	//
	// This complicates matters if a multi-element resource gets an update
	// to only a single subresource. In that case, we would have no way to
	// recalculate the hash from all subresources (well, not unless we pull
	// the other subresources back from the GPU and kill performance) and
	// would not be able to track them.
	//
	// For now, we are solving this dilemma by only using the hash from the
	// first subresource for all subresources in the texture. In the
	// future, we could consider an alternate approach that calculates
	// individual hashes for each subresource and xors them all together
	// for the texture as a whole, thereby allowing each subresource to be
	// tracked individually, but this would be a pretty fundamental change
	// since we would probably want to change all hashes, not just those of
	// multi-element resources - so not something we would do unless
	// necessary.
	//
	// 3DMigoto 1.2.9 already changed the hash of multi-element resources,
	// but none of our fixes were reported broken by that change. Therefore
	// I am fairly confident that there won't be any impact to this change
	// either.
	//
	//for (item = 0; item < pDesc->ArraySize; item++) {
		// We could potentially consider multiple mip-map levels, but
		// they are unlikely to differentiate any textures that the
		// main mip-map level alone could not, and few games hand them
		// to us anyway. Alternatively, using only a smaller mip-map
		// could potentially be used to improve performance if the
		// largest mip-map level was enormous (but again, only if the
		// game actually handed them to us).
		// for (level = 0; level < pDesc->MipLevels; level++) {

		index = D3D11CalcSubresource(level, item, max(pDesc->MipLevels, 1));
		length = Texture2DLength(pDesc, &pInitialData[index], level);
		hash = crc32c_hw(hash, pInitialData[index].pSysMem, length);
	//}

	return hash;
}

// Must be called with the critical section held to protect mResources against
// simultaneous reads & modifications (hmm, tempted to implement a lock free
// map given that it's add only, or use RCU). Is there anything on Windows like
// lockdep to statically prove this is called with the lock held?
uint32_t GetOrigResourceHash(ID3D11Resource *resource)
{
	std::unordered_map<ID3D11Resource *, ResourceHandleInfo>::iterator j;

	j = G->mResources.find(resource);
	if (j != G->mResources.end())
		return j->second.orig_hash;

	return 0;
}

// Must be called with the critical section held to protect mResources against
// simultaneous reads & modifications (hmm, tempted to implement a lock free
// map given that it's add only, or use RCU). Is there anything on Windows like
// lockdep to statically prove this is called with the lock held?
uint32_t GetResourceHash(ID3D11Resource *resource)
{
	std::unordered_map<ID3D11Resource *, ResourceHandleInfo>::iterator j;

	j = G->mResources.find(resource);
	if (j != G->mResources.end())
		return j->second.hash;

	// We can get here for a few legitimate reasons where a resource has
	// not been hashed. 1D and buffer resources are currently not hashed,
	// resources created by 3DMigoto bypass the CreateTexture wrapper and
	// are not hashed, and the swap chain's back buffer will not have been
	// hashed. We used to hash these on demand here, but it's not clear
	// that we ever needed their hashes - if we ever do we could consider
	// hashing them at the time of creation instead.
	//
	// Return a 0 so it is obvious that this resource has not been hashed.

	return 0;
}

uint32_t CalcTexture3DDataHash(
	const D3D11_TEXTURE3D_DESC *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData)
{
	uint32_t hash = 0;
	size_t length_v12;
	size_t length;

	if (!pDesc || !pInitialData || !pInitialData->pSysMem)
		return 0;

	// In 3DMigoto v1.2, this is what we were using as the length of the
	// buffer in bytes. Unfortunately this is not right since pDesc->Width
	// is in texels, not bytes. Additionally, compressed textures
	// complicate the buffer size calculation further.
	//
	// The result is that we might not consider the entire buffer when
	// calculating the hash (which may not be ideal, but it is generally
	// acceptable), or we might overflow the buffer. If we overflow we
	// might get an exception if there is nothing mapped after the buffer
	// (which we catch and log), but we could just as easily process
	// gargage after the buffer as being part of the texture, which would
	// lead to us creating unpredictable hashes.
	length_v12 = pDesc->Width * pDesc->Height * pDesc->Depth;

	// Compare the old broken length to the actual length. If the broken
	// length is shorter, we will just use that. While not ideal, this will
	// minimise the pain of changing the texture hash so soon after the
	// last time.
	//
	// TODO: We might consider an ini setting to disable this fallback for
	// new games, or possibly to force it for old games.
	length = Texture3DLength(pDesc, &pInitialData[0], 0);
	LogDebug("  Texture3D length: %Iu bad v1.2.1 length: %Iu\n", length, length_v12);
	if (length_v12 <= length) {
		if (length_v12 < length) {
			LogDebug("  Using 3DMigoto v1.2.1 compatible Texture3D CRC calculation\n");
		}
		return crc32c_hw(hash, pInitialData[0].pSysMem, length_v12);
	}

	// If we are here it means the old length had overflowed the buffer,
	// which means we could not rely on it being a consistent value unless
	// we got lucky and the memory following the buffer was always
	// consistent (and even if we did, can we be sure every player will,
	// and that it won't change when the game is updated?).
	//
	// In that case, let's do it right... and hopefully this will be the
	// last time we need to change this.

	LogDebug("  Using 3DMigoto v1.2.9+ Texture3D CRC calculation\n");

	// As above, we could potentially consider multiple mip-map levels blah
	// blah blah... Difference is, there can only be one array entry in a
	// 3D texture

	hash = crc32c_hw(hash, pInitialData[0].pSysMem, length);

	return hash;
}

static bool GetResourceInfoFields(struct ResourceHashInfo *info, UINT subresource,
		UINT *width, UINT *height, UINT *depth,
		UINT *idx, UINT *mip, UINT *array_size)
{
	UINT mips;
	switch (info->type) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			mips = max(info->tex2d_desc.MipLevels, 1);
			*idx = subresource / mips;
			*mip = subresource % mips;
			*width = max(info->tex2d_desc.Width >> *mip, 1);
			*height = max(info->tex2d_desc.Height >> *mip, 1);
			*depth = 1;
			*array_size = info->tex2d_desc.ArraySize;
			return true;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			mips = max(info->tex3d_desc.MipLevels, 1);
			*idx = subresource / mips;
			*mip = subresource % mips;
			*width = max(info->tex3d_desc.Width >> *mip, 1);
			*height = max(info->tex3d_desc.Height >> *mip, 1);
			*depth = max(info->tex3d_desc.Depth >> *mip, 1);
			*array_size = 1;
			return true;
	}
	return false;
}

void MarkResourceHashContaminated(ID3D11Resource *dest, UINT DstSubresource,
		ID3D11Resource *src, UINT srcSubresource, char type,
		UINT DstX, UINT DstY, UINT DstZ, const D3D11_BOX *SrcBox)
{
	struct ResourceHashInfo *dstInfo, *srcInfo = NULL;
	uint32_t srcHash = 0, dstHash = 0;
	UINT srcWidth = 1, srcHeight = 1, srcDepth = 1, srcMip = 0, srcIdx = 0, srcArraySize = 1;
	UINT dstWidth = 1, dstHeight = 1, dstDepth = 1, dstMip = 0, dstIdx = 0, dstArraySize = 1;
	bool partial = false;

	if (!dest)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	dstHash = GetOrigResourceHash(dest);
	if (!dstHash)
		goto out_unlock;

	try {
		dstInfo = &G->mResourceInfo.at(dstHash);
	} catch (std::out_of_range) {
		goto out_unlock;
	}

	GetResourceInfoFields(dstInfo, DstSubresource,
			&dstWidth, &dstHeight, &dstDepth,
			&dstIdx, &dstMip, &dstArraySize);

	// We don't care if a mip-map has been updated since we don't hash those.
	// We could collect info about the copy anyway (below code will work to
	// do so), but it adds a lot of irrelevant noise to the ShaderUsage.txt
	if (dstMip)
		goto out_unlock;

	if (src) {
		srcHash = GetOrigResourceHash(src);
		G->mCopiedResourceInfo.insert(srcHash);

		try {
			srcInfo = &G->mResourceInfo.at(srcHash);
			GetResourceInfoFields(srcInfo, srcSubresource,
					&srcWidth, &srcHeight, &srcDepth,
					&srcIdx, &srcMip, &srcArraySize);

			if (dstHash != srcHash && srcInfo->initial_data_used_in_hash) {
				dstInfo->initial_data_used_in_hash = true;
				if (!G->track_texture_updates)
					dstInfo->hash_contaminated = true;
			}
		} catch (std::out_of_range) {
		}
	}

	switch (type) {
		case 'U':
			dstInfo->update_contamination.insert(DstSubresource);
			dstInfo->initial_data_used_in_hash = true;
			if (!G->track_texture_updates)
				dstInfo->hash_contaminated = true;
			break;
		case 'M':
			dstInfo->map_contamination.insert(DstSubresource);
			dstInfo->initial_data_used_in_hash = true;
			if (!G->track_texture_updates)
				dstInfo->hash_contaminated = true;
			break;
		case 'C':
			dstInfo->copy_contamination.insert(srcHash);
			break;
		case 'S':

			// We especially want to know if a region copy copied
			// the entire texture, or only part of it. This may be
			// important if we end up changing the hash due to a
			// copy operation - if it copied the whole resource, we
			// can just use the hash of the source. If it only
			// copied a partial resource there's no good answer.

			partial = partial || dstWidth != srcWidth;
			partial = partial || dstHeight != srcHeight;
			partial = partial || dstDepth != srcDepth;

			partial = partial || DstX || DstY || DstZ;
			if (SrcBox) {
				partial = partial ||
					(SrcBox->right - SrcBox->left != dstWidth) ||
					(SrcBox->bottom - SrcBox->top != dstHeight) ||
					(SrcBox->back - SrcBox->front != dstDepth);
			}

			// TODO: Need to think about the implications of
			// copying between textures with > 1 array element.
			// Might want to reconsider how these are hashed (e.g.
			// hash each non-mipmap subresource separately and xor
			// the hashes together so we can efficiently change a
			// single subhash)
			partial = partial || dstArraySize > 1 || srcArraySize > 1;

			dstInfo->region_contamination[
					std::make_tuple(srcHash, dstIdx, dstMip, srcIdx, srcMip)
				].Update(partial, DstX, DstY, DstZ, SrcBox);
	}

out_unlock:
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

void UpdateResourceHashFromCPU(ID3D11Resource *resource,
	ResourceHandleInfo *info,
	const void *data, UINT rowPitch, UINT depthPitch)
{
	D3D11_RESOURCE_DIMENSION dim;
	D3D11_SUBRESOURCE_DATA initialData;
	ID3D11Texture2D *tex2D;
	ID3D11Texture3D *tex3D;
	D3D11_TEXTURE2D_DESC *desc2D;
	D3D11_TEXTURE3D_DESC *desc3D;
	uint32_t old_data_hash, old_hash;

	if (!resource || !data)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	if (!info) {
		try {
			info = &G->mResources.at(resource);
		} catch (std::out_of_range) {
			goto out_unlock;
		}
	}

	// Ever noticed that D3D11_SUBRESOURCE_DATA is binary identical to
	// D3D11_MAPPED_SUBRESOURCE but they changed all the names around?
	initialData.pSysMem = data;
	initialData.SysMemPitch = rowPitch;
	initialData.SysMemSlicePitch = depthPitch;

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

	resource->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex2D = (ID3D11Texture2D*)resource;

			desc2D = &info->desc2D;
			// TODO: tex2D->GetDesc(&desc2D); then fix up mip-maps if necessary

			info->data_hash = CalcTexture2DDataHash(desc2D, &initialData);
			info->hash = CalcTexture2DDescHash(info->data_hash, desc2D);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex3D = (ID3D11Texture3D*)resource;

			desc3D = &info->desc3D;
			// TODO: tex3D->GetDesc(&desc3D); then fix up mip-maps if necessary

			info->data_hash = CalcTexture3DDataHash(desc3D, &initialData);
			info->hash = CalcTexture3DDescHash(info->data_hash, desc3D);
			break;
	}

	LogDebug("Updated resource hash\n");
	LogDebug("  old data: %08x new data: %08x\n", old_data_hash, info->data_hash);
	LogDebug("  old hash: %08x new hash: %08x\n", old_hash, info->hash);

out_unlock:
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

void PropagateResourceHash(ID3D11Resource *dst, ID3D11Resource *src)
{
	ResourceHandleInfo *dst_info, *src_info;
	D3D11_RESOURCE_DIMENSION dim;
	D3D11_TEXTURE2D_DESC *desc2D;
	D3D11_TEXTURE3D_DESC *desc3D;
	uint32_t old_data_hash, old_hash;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	try {
		src_info = &G->mResources.at(src);
	} catch (std::out_of_range) {
		goto out_unlock;
	}

	try {
		dst_info = &G->mResources.at(dst);
	} catch (std::out_of_range) {
		goto out_unlock;
	}

	// If there was no initial data in either source or destination, or
	// they both contain the same data, we don't need to recalculate the
	// hash as it will not change:
	if (src_info->data_hash == dst_info->data_hash)
		goto out_unlock;

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

	dst->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			desc2D = &dst_info->desc2D;
			// TODO: tex2D->GetDesc(&desc2D); then fix up mip-maps if necessary

			dst_info->hash = CalcTexture2DDescHash(dst_info->data_hash, desc2D);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			desc3D = &dst_info->desc3D;
			// TODO: tex3D->GetDesc(&desc3D); then fix up mip-maps if necessary

			dst_info->hash = CalcTexture3DDescHash(dst_info->data_hash, desc3D);
			break;
	}

	LogDebug("Propagated resource hash\n");
	LogDebug("  old data: %08x new data: %08x\n", old_data_hash, dst_info->data_hash);
	LogDebug("  old hash: %08x new hash: %08x\n", old_hash, dst_info->hash);

out_unlock:
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

void MapTrackResourceHashUpdate(
	ID3D11Resource *pResource,
	UINT Subresource,
	D3D11_MAP MapType,
	UINT MapFlags,
	D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	D3D11_RESOURCE_DIMENSION dim;
	ResourceHandleInfo *info;
	bool divert = false;
	void *replace;

	if (!pResource)
		return;

	switch (MapType) {
		case D3D11_MAP_WRITE_DISCARD:
			divert = true;
			// Fall through
		case D3D11_MAP_WRITE:
		case D3D11_MAP_WRITE_NO_OVERWRITE:
			break;
		default:
			return;
	}

	if (G->hunting) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pResource, Subresource, NULL, 0, 'M', 0, 0, 0, NULL);
	}

	// TODO: If track_texture_updated is disabled, but we are in hunting
	// with a reloadable config, we might consider tracking the data hash
	// updates regardless (just not the full resource hash) so the option
	// can be turned on live and work. But there's a few pieces we would
	// need for that to work so for now let's not over-complicate things.
	if (!G->track_texture_updates || Subresource != 0 || !pMappedResource)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	try {
		info = &G->mResources.at(pResource);
	} catch (std::out_of_range) {
		goto out_unlock;
	}

	if (divert) {
		pResource->GetType(&dim);
		switch (dim) {
			case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
				info->diverted_size = pMappedResource->RowPitch * info->desc2D.Height;
				break;
			case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
				info->diverted_size = pMappedResource->DepthPitch * info->desc3D.Depth;
				break;
			default:
				goto out_unlock;
		}

		replace = malloc(info->diverted_size);
		if (!replace) {
			LogInfo("MapTrackResourceHashUpdate out of memory\n");
			goto out_unlock;
		}
		memset(replace, 0, info->diverted_size);

		info->diverted_map = pMappedResource->pData;
		pMappedResource->pData = replace;
	} else {
		info->diverted_map = NULL;
	}

	memcpy(&info->map, pMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	info->mapped_writable = true;

out_unlock:
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

void MapUpdateResourceHash(ID3D11Resource *pResource, UINT Subresource)
{
	ResourceHandleInfo *info;

	if (!G->track_texture_updates || Subresource != 0)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	try {
		info = &G->mResources.at(pResource);
	} catch (std::out_of_range) {
		goto out_unlock;
	}

	if (!info->mapped_writable)
		goto out_unlock;
	info->mapped_writable = false;

	UpdateResourceHashFromCPU(pResource, info, info->map.pData, info->map.RowPitch, info->map.DepthPitch);

	// TODO: Measure performance vs. not diverting
	if (info->diverted_map) {
		memcpy(info->diverted_map, info->map.pData, info->diverted_size);
		free(info->map.pData);
	}

out_unlock:
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}
