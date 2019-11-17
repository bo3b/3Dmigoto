// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE,
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

#pragma once

#ifndef __NVSTEREO__
#define __NVSTEREO__ 1

/**
 * How to use this file:
 *   1) You will also need the NVAPI. Any version will suffice, the public version is
 *      available at http://developer.nvidia.com/object/nvapi.html
 *
 *   2) Include this file from somewhere in your project. You can define
 *      NO_STEREO_D3D9 to disable Direct3D9 functionality, or
 *      NO_STEREO_D3D10 to disable Direct3D10 functionality.
 *
 *   3) Declare a variable mStereoParamMgr of the appropriate type (ParamTextureManagerD3D9 or ParamTextureManagerD3D10)
 *      somewhere in your application. (The object that owns a pointer to your IDirect3DDevice9 or
 *      ID3DDevice10 is a reasonable owner for this variable as well).
 *
 *   4) Create the instance of this class (or classes, if your engine supports both D3D9 and D3D10 at the
 *      same time) prior to creating the respective D3D9 or D3D10 device.
 *
 *   5) Create a texture to be stereoized, using the parameters specified by StereoTexWidth, StereoTexHeight and StereoTexFormat.
 *
 *   6) Once per frame, call ParamTextureManager::UpdateStereoTexture with the texture created in step 5.
 *
 *   7) Bind this texture as you would bind any other texture for shader input.
 *
 *   8) The expected shader usage of this texture is shown in section 2.3 of the whitepaper
 *      "NVIDIA 3D Vision Automatic Best Practices Guide."
 *
 **/

// The upstream version of this file can be found in the NVIDIA SDK 11:
// http://developer.nvidia.com/sites/default/files/akamai/gamedev/files/sdk/11/NVIDIA_SDK11_Direct3D_11.00.0328.2105.exe

// vvvvv 3DMIGOTO ADDITION vvvvv
#ifdef MIGOTO_DX
#if MIGOTO_DX != 9
#define NO_STEREO_D3D9
#endif
#if MIGOTO_DX != 10
// NOTE THAT DX10 project is currently using its own version of this file,
// though with some tweaking it shouldn't need to.
#define NO_STEREO_D3D10
#endif
#if MIGOTO_DX != 11
#define NO_STEREO_D3D11
#endif
#endif // MIGOTO_DX

#include <d3d11_1.h>
// ^^^^^ 3DMIGOTO ADDITION ^^^^^

#include "nvapi.h"

// vvvvv 3DMIGOTO ADDITION vvvvv
#include "log.h"
#include "profiling.h"
#include "util.h"
// ^^^^^ 3DMIGOTO ADDITION ^^^^^

namespace nv {
	// vvvvv 3DMIGOTO ADDITION vvvvv
#ifndef NO_STEREO_D3D11
	// -----------------------------------------------------------------------------------------------

	// This function was copied from the NV SDK file of DisplayConfiguration.cpp as a display example.
	// It's been modified to return the PathInfo for just the primary device, as the one that will be
	// used for 3D display.
	// If we fail for some reason, just set width/height to zero as it's not that significant.
	// Defined as 'static' because we only need the one implementation, and nvstereo.h is included
	// multiple times.
	//
	// DSS NOTE: Don't use this. We are leaving it in for backwards compatibility only.
	// - It gets the display resolution, but more often than not that is not what you want
	// - It does not vary by eye and so is not appropriate to place in the stereo texture
	// - If stereo is disabled the stereo texture won't be updated and this will be invalid

	static void GetPrimaryDisplayResolution(float &pWidth, float &pHeight)
	{
		NvAPI_Status ret;
		pWidth = 0;
		pHeight = 0;

		// First retrieve the display path count
		NvU32 pathCount = 0;
		NV_DISPLAYCONFIG_PATH_INFO *pathInfo = NULL;

		// Pass 1: Fetch pathCount, # of displays
		ret = Profiling::NvAPI_DISP_GetDisplayConfig(&pathCount, NULL);
		if (ret != NVAPI_OK)
		{
			LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: %d  ***\n", ret);
			return;
		}

		pathInfo = (NV_DISPLAYCONFIG_PATH_INFO*)malloc(pathCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
		if (!pathInfo)
		{
			LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: Out of memory  ***\n");
			return;
		}

		// These version params must be set, or it will crash.
		memset(pathInfo, 0, pathCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
		for (NvU32 i = 0; i < pathCount; i++)
		{
			pathInfo[i].version = NV_DISPLAYCONFIG_PATH_INFO_VER;

			// Allocation for the source mode info.
			pathInfo[i].sourceModeInfo = (NV_DISPLAYCONFIG_SOURCE_MODE_INFO*)malloc(sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
			if (pathInfo[i].sourceModeInfo == NULL)
			{
				LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: Out of memory  ***\n");
				return;
			}
			memset(pathInfo[i].sourceModeInfo, 0, sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
		}


		// Pass 2: Retrieve the targetInfo struct count, and the source mode info, which is what we are interested in.
		ret = Profiling::NvAPI_DISP_GetDisplayConfig(&pathCount, pathInfo);
		if (ret != NVAPI_OK)
		{
			LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: %d  ***\n", ret);
			return;
		}

		// We don't need this 3rd pass, because we don't need targetInfo.
		// Pass 3: Fetch targetInfo structs
		//for (NvU32 i = 0; i < pathCount; i++)
		//{
		//	// Allocation for the source mode info.
		//	pathInfo[i].sourceModeInfo = (NV_DISPLAYCONFIG_SOURCE_MODE_INFO*)malloc(sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
		//	if (pathInfo[i].sourceModeInfo == NULL)
		//	{
		//		LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: Out of memory  ***\n", ret);
		//		return;
		//	}
		//	memset(pathInfo[i].sourceModeInfo, 0, sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));

		//	// Allocate the target array.
		//	pathInfo[i].targetInfo = (NV_DISPLAYCONFIG_PATH_TARGET_INFO*)malloc(pathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
		//	if (pathInfo[i].targetInfo == NULL)
		//	{
		//		LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: Out of memory  ***\n", ret);
		//		return;
		//	}

		//	memset(pathInfo[i].targetInfo, 0, pathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
		//	for (NvU32 j = 0; j < pathInfo[i].targetInfoCount; j++)
		//	{
		//		pathInfo[i].targetInfo[j].details = (NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO*)malloc(sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
		//		memset(pathInfo[i].targetInfo[j].details, 0, sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
		//		pathInfo[i].targetInfo[j].details->version = NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO_VER;
		//	}
		//}

		// Memory allocated for return results, retrieve the full path info, filling in those targetModeInfo structs.
		//ret = Profiling::NvAPI_DISP_GetDisplayConfig(&pathCount, pathInfo);
		//if (ret != NVAPI_OK)
		//{
		//	LogInfo(" *** NvAPI_DISP_GetDisplayConfig failed: %d  ***\n", ret);
		//	return;
		//}


		// Look through the N adapters for the primary display to get resolution.
		for (NvU32 i = 0; i < pathCount; i++)
		{
			if (pathInfo[i].sourceModeInfo->bGDIPrimary)
			{
				pWidth = (float)pathInfo[i].sourceModeInfo->resolution.width;
				pHeight = (float)pathInfo[i].sourceModeInfo->resolution.height;
			}
			free(pathInfo[i].sourceModeInfo);
		}
		free(pathInfo);

		LogInfo("  nvapi fetched screen width: %f, height: %f\n", pWidth, pHeight);
	}


	// -----------------------------------------------------------------------------------------------
#endif // NO_STEREO_D3D11
	// ^^^^^ 3DMIGOTO ADDITION ^^^^^

	namespace stereo {
		typedef struct  _Nv_Stereo_Image_Header
		{
			unsigned int    dwSignature;
			unsigned int    dwWidth;
			unsigned int    dwHeight;
			unsigned int    dwBPP;
			unsigned int    dwFlags;
		} NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;

#define     NVSTEREO_IMAGE_SIGNATURE 0x4433564e // NV3D
#define     NVSTEREO_SWAP_EYES 0x00000001

		inline void PopulateTextureData(float* leftEye, float* rightEye, LPNVSTEREOIMAGEHEADER header, unsigned int width, unsigned int height, unsigned int pixelBytes, float eyeSep, float sep, float conv,
			float tuneValue1, float tuneValue2, float tuneValue3, float tuneValue4, float screenWidth, float screenHeight)
		{
			// Separation is the separation value and eyeSeparation a magic system value.
			// Normally sep is in [0, 100], and we want the fractional part of 1.
			float finalSeparation = eyeSep * sep * 0.01f;
			leftEye[0] = -finalSeparation;
			leftEye[1] = conv;
			leftEye[2] = 1.0f;
			// vvvvv 3DMIGOTO ADDITION vvvvv
			leftEye[3] = -sep * 0.01f;

#ifndef NO_STEREO_D3D11
			// Only leaving these in DX11 for backwards compatibility. Don't
			// use - they are all broken in some way, and not what you want.
			leftEye[4] = tuneValue1;
			leftEye[5] = tuneValue2;
			leftEye[6] = tuneValue3;
			leftEye[7] = tuneValue4;

			leftEye[8] = screenWidth;
			leftEye[9] = screenHeight;
#endif
			// ^^^^^ 3DMIGOTO ADDITION ^^^^^

			rightEye[0] = -leftEye[0];
			rightEye[1] = leftEye[1];
			rightEye[2] = -leftEye[2];
			// vvvvv 3DMIGOTO ADDITION vvvvv
			rightEye[3] = -leftEye[3];
#ifndef NO_STEREO_D3D11
			rightEye[4] = leftEye[4];
			rightEye[5] = leftEye[5];
			rightEye[6] = leftEye[6];
			rightEye[7] = leftEye[7];
			rightEye[8] = leftEye[8];
			rightEye[9] = leftEye[9];
#endif
			// ^^^^^ 3DMIGOTO ADDITION ^^^^^

			// Fill the header
			header->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
			header->dwWidth = width;
			header->dwHeight = height;
			header->dwBPP = pixelBytes;
			header->dwFlags = 0;
		}

		inline bool IsStereoEnabled()
		{
			NvU8 stereoEnabled = 0;
			if (NVAPI_OK != Profiling::NvAPI_Stereo_IsEnabled(&stereoEnabled)) {
				return false;
			}

			return stereoEnabled != 0;
		}

#ifndef NO_STEREO_D3D9
		// The D3D9 "Driver" for stereo updates, encapsulates the logic that is Direct3D9 specific.
		struct D3D9Type
		{
			typedef IDirect3DDevice9 Device;
			typedef IDirect3DDevice9 Context; // 3DMIGOTO ADDITION - Use device as context in DX < 11
			typedef IDirect3DTexture9 Texture;
			typedef IDirect3DSurface9 StagingResource;

			static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX9_REGISTRY_PROFILE;

			// Note that the texture must be at least 20 bytes wide to handle the stereo header.
			static const int StereoTexWidth = 8;
			static const int StereoTexHeight = 1;
			static const D3DFORMAT StereoTexFormat = D3DFMT_A32B32G32R32F;
			static const int StereoBytesPerPixel = 16;

			static StagingResource* CreateStagingResource(Device* pDevice, float eyeSep, float sep, float conv)
			{
				StagingResource* staging = 0;
				unsigned int stagingWidth = StereoTexWidth * 2;
				unsigned int stagingHeight = StereoTexHeight + 1;

				pDevice->CreateOffscreenPlainSurface(stagingWidth, stagingHeight, StereoTexFormat, D3DPOOL_SYSTEMMEM, &staging, NULL);

				if (!staging) {
					return 0;
				}

				D3DLOCKED_RECT lr;
				staging->LockRect(&lr, NULL, 0);
				unsigned char* sysData = (unsigned char *) lr.pBits;
				unsigned int sysMemPitch = stagingWidth * StereoBytesPerPixel;

				float* leftEyePtr = (float*)sysData;
				float* rightEyePtr = leftEyePtr + StereoTexWidth * StereoBytesPerPixel / sizeof(float);
				LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)(sysData + sysMemPitch * StereoTexHeight);
				PopulateTextureData(leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv);
				staging->UnlockRect();

				return staging;
			}

			static void UpdateTextureFromStaging(Device* pDevice, Texture* tex, StagingResource* staging)
			{
				RECT stereoSrcRect;
				stereoSrcRect.top = 0;
				stereoSrcRect.bottom = StereoTexHeight;
				stereoSrcRect.left = 0;
				stereoSrcRect.right = StereoTexWidth;

				POINT stereoDstPoint;
				stereoDstPoint.x = 0;
				stereoDstPoint.y = 0;

				IDirect3DSurface9* texSurface;
				tex->GetSurfaceLevel( 0, &texSurface );

				pDevice->UpdateSurface(staging, &stereoSrcRect, texSurface, &stereoDstPoint);
				texSurface->Release();
			}
		};
#endif // NO_STEREO_D3D9

#ifndef NO_STEREO_D3D10
		// The D3D10 "Driver" for stereo updates, encapsulates the logic that is Direct3D10 specific.
		struct D3D10Type
		{
			typedef ID3D10Device Device;
			typedef ID3D10Device Context; // 3DMIGOTO ADDITION - Use device as context in DX < 11
			typedef ID3D10Texture2D Texture;
			typedef ID3D10Texture2D StagingResource;

			static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX10_REGISTRY_PROFILE;

			// Note that the texture must be at least 20 bytes wide to handle the stereo header.
			static const int StereoTexWidth = 8;
			static const int StereoTexHeight = 1;
			static const DXGI_FORMAT StereoTexFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
			static const int StereoBytesPerPixel = 16;

			static StagingResource* CreateStagingResource(Device* pDevice, float eyeSep, float sep, float conv)
			{
				StagingResource* staging = 0;
				unsigned int stagingWidth = StereoTexWidth * 2;
				unsigned int stagingHeight = StereoTexHeight + 1;

				// Allocate the buffer sys mem data to write the stereo tag and stereo params
				D3D10_SUBRESOURCE_DATA sysData;
				sysData.SysMemPitch = StereoBytesPerPixel * stagingWidth;
				sysData.pSysMem = new unsigned char[sysData.SysMemPitch * stagingHeight];

				float* leftEyePtr = (float*)sysData.pSysMem;
				float* rightEyePtr = leftEyePtr + StereoTexWidth * StereoBytesPerPixel / sizeof(float);
				LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)((unsigned char*)sysData.pSysMem + sysData.SysMemPitch * StereoTexHeight);
				PopulateTextureData(leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv);

				D3D10_TEXTURE2D_DESC desc;
				memset(&desc, 0, sizeof(D3D10_TEXTURE2D_DESC));
				desc.Width = stagingWidth;
				desc.Height = stagingHeight;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = StereoTexFormat;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D10_USAGE_DEFAULT;
				desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				pDevice->CreateTexture2D(&desc, &sysData, &staging);
				delete [] sysData.pSysMem;
				return staging;
			}

			static void UpdateTextureFromStaging(Device* pDevice, Texture* tex, StagingResource* staging)
			{
				pDevice; tex; staging;
				D3D10_BOX stereoSrcBox;
				stereoSrcBox.front = 0;
				stereoSrcBox.back = 1;
				stereoSrcBox.top = 0;
				stereoSrcBox.bottom = StereoTexHeight;
				stereoSrcBox.left = 0;
				stereoSrcBox.right = StereoTexWidth;

				pDevice->CopySubresourceRegion(tex, 0, 0, 0, 0, staging, 0, &stereoSrcBox);
			}
		};
#endif // NO_STEREO_D3D10

#ifndef NO_STEREO_D3D11 // 3DMIGOTO ADDITION
		struct D3D11Type
		{
			typedef ID3D11Device Device;
			typedef ID3D11Texture2D Texture;
			typedef ID3D11Texture2D StagingResource;
			typedef ID3D11DeviceContext Context;

			static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX10_REGISTRY_PROFILE;

			// Note that the texture must be at least 20 bytes wide to handle the stereo header.
			static const int StereoTexWidth = 8;
			static const int StereoTexHeight = 1;
			static const DXGI_FORMAT StereoTexFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
			static const int StereoBytesPerPixel = 16;

			static StagingResource* CreateStagingResource(Device* pDevice, float eyeSep, float sep, float conv,
				float tuneValue1, float tuneValue2, float tuneValue3, float tuneValue4)
			{
				float screenWidth, screenHeight;

				// Get screen resolution here, since it becomes part of the stereo texture structure.
				// This was previously fetched from the swap chain.
				GetPrimaryDisplayResolution(screenWidth, screenHeight);

				StagingResource* staging = 0;
				unsigned int stagingWidth = StereoTexWidth * 2;
				unsigned int stagingHeight = StereoTexHeight + 1;

				// Allocate the buffer sys mem data to write the stereo tag and stereo params
				D3D11_SUBRESOURCE_DATA sysData;
				sysData.SysMemPitch = StereoBytesPerPixel * stagingWidth;
				sysData.pSysMem = new unsigned char[sysData.SysMemPitch * stagingHeight];

				float* leftEyePtr = (float*)sysData.pSysMem;
				float* rightEyePtr = leftEyePtr + StereoTexWidth * StereoBytesPerPixel / sizeof(float);
				LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)((unsigned char*)sysData.pSysMem + sysData.SysMemPitch * StereoTexHeight);
				PopulateTextureData(leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv,
					tuneValue1, tuneValue2, tuneValue3, tuneValue4,
					screenWidth, screenHeight);

				D3D11_TEXTURE2D_DESC desc;
				memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
				desc.Width = stagingWidth;
				desc.Height = stagingHeight;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = StereoTexFormat;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				LockResourceCreationMode();
				HRESULT ret = pDevice->CreateTexture2D(&desc, &sysData, &staging);
				UnlockResourceCreationMode();
				delete sysData.pSysMem;
				if (ret != S_OK)
				{
					LogInfo("    error during creation of stereo staging texture. result = %x.\n", ret);
					return NULL;
				}
				return staging;
			}

			static void UpdateTextureFromStaging(Context* context, Texture* tex, StagingResource* staging)
			{
				D3D11_BOX stereoSrcBox;
				stereoSrcBox.front = 0;
				stereoSrcBox.back = 1;
				stereoSrcBox.top = 0;
				stereoSrcBox.bottom = StereoTexHeight;
				stereoSrcBox.left = 0;
				stereoSrcBox.right = StereoTexWidth;

				context->CopySubresourceRegion(tex, 0, 0, 0, 0, staging, 0, &stereoSrcBox);
			}
		};
#endif // NO_STEREO_D3D11

		// The NV Stereo class, which can work for either D3D9 or D3D10, depending on which type it's specialized for
		// Note that both types can live side-by-side in two seperate instances as well.
		// Also note that there are convenient typedefs below the class definition.
		template <class D3DType>
		class ParamTextureManager
		{
		public:
			typedef typename D3DType Parms;
			typedef typename D3DType::Device Device;
			typedef typename D3DType::Texture Texture;
			typedef typename D3DType::StagingResource StagingResource;
			typedef typename D3DType::Context Context; // 3DMIGOTO ADDITION

			ParamTextureManager() :
				mEyeSeparation(0),
				mSeparation(0),
				// vvvvv 3DMIGOTO ADDITION vvvvv
				mSeparationModifier(1),
				mTuneVariable1(1), mTuneVariable2(1), mTuneVariable3(1), mTuneVariable4(1),
				mForceUpdate(false),
				// ^^^^^ 3DMIGOTO ADDITION ^^^^^
				mConvergence(0),
				mStereoHandle(0),
				mActive(false),
				mDeviceLost(true)
			{
				// mDeviceLost is set to true to initialize the texture with good data at app startup.

				// The call to CreateConfigurationProfileRegistryKey must happen BEFORE device creation.
				// Profiling::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3DType::RegistryProfileType);
			}

			// 3DMIGOTO REMOVAL ~ParamTextureManager()
			// 3DMIGOTO REMOVAL {
			// 3DMIGOTO REMOVAL 	if (mStereoHandle) {
			// 3DMIGOTO REMOVAL 		NvAPI_Stereo_DestroyHandle(mStereoHandle);
			// 3DMIGOTO REMOVAL 		mStereoHandle = 0;
			// 3DMIGOTO REMOVAL 	}
			// 3DMIGOTO REMOVAL }
			// 3DMIGOTO REMOVAL
			// 3DMIGOTO REMOVAL void Init(Device* dev)
			// 3DMIGOTO REMOVAL {
			// 3DMIGOTO REMOVAL 	NvAPI_Stereo_CreateHandleFromIUnknown(dev, &mStereoHandle);
			// 3DMIGOTO REMOVAL
			// 3DMIGOTO REMOVAL 	// Set that we've initialized regardless--we'll only try to init once.
			// 3DMIGOTO REMOVAL 	mInitialized = true;
			// 3DMIGOTO REMOVAL }

			// Not const because we will update the various values if an update is needed.
			bool RequiresUpdate(bool deviceLost)
			{
				// vvvvv 3DMIGOTO ADDITION vvvvv
				if (mForceUpdate)
				{
					mForceUpdate = false;
					return true;
				}
				// ^^^^^ 3DMIGOTO ADDITION ^^^^^
				bool active = IsStereoActive();
				bool updateRequired;
				float eyeSep, sep, conv;
				if (active) {
					if (NVAPI_OK != Profiling::NvAPI_Stereo_GetEyeSeparation(mStereoHandle, &eyeSep))
						return false;
					if (NVAPI_OK != Profiling::NvAPI_Stereo_GetSeparation(mStereoHandle, &sep))
						return false;
					if (NVAPI_OK != Profiling::NvAPI_Stereo_GetConvergence(mStereoHandle, &conv))
						return false;

					updateRequired = (eyeSep != mEyeSeparation)
						|| (sep != mSeparation)
						|| (conv != mConvergence)
						|| (active != mActive);
				} else {
					eyeSep = sep = conv = 0;
					updateRequired = active != mActive;
				}

				// If the device was lost and is now restored, need to update the texture contents again.
				updateRequired = updateRequired || (!deviceLost && mDeviceLost);
				mDeviceLost = deviceLost;

				if (updateRequired) {
					// vvvvv 3DMIGOTO ADDITION vvvvv
					LogInfo("  updating stereo texture with eyeSeparation = %e, separation = %e, convergence = %e, active = %d\n",
						eyeSep, sep, conv, active ? 1 : 0);
					// ^^^^^ 3DMIGOTO ADDITION ^^^^^

					mEyeSeparation = eyeSep;
					mSeparation = sep;
					mConvergence = conv;
					mActive = active;
					return true;
				}

				return false;
			}

			bool IsStereoActive() const
			{
				NvU8 stereoActive = 0;
				if (NVAPI_OK != Profiling::NvAPI_Stereo_IsActivated(mStereoHandle, &stereoActive)) {
					return false;
				}

				return stereoActive != 0;
			}

			float GetConvergenceDepth() const { return mActive ? mConvergence : 1.0f; }


			// 3DMIGOTO ADDITION: In DX9 or DX10 pass the device a second time as the context
			bool UpdateStereoTexture(Device* dev, Context* context, Texture* tex, bool deviceLost)
			{
				// 3DMIGOTO REMOVAL if (!mInitialized) {
				// 3DMIGOTO REMOVAL 	Init(dev);
				// 3DMIGOTO REMOVAL }

				if (!RequiresUpdate(deviceLost)) {
					return false;
				}

				StagingResource *staging = D3DType::CreateStagingResource(dev, mEyeSeparation, mSeparation * mSeparationModifier, mConvergence,
					mTuneVariable1, mTuneVariable2, mTuneVariable3, mTuneVariable4);
				if (staging) {
					D3DType::UpdateTextureFromStaging(context, tex, staging);
					staging->Release();
					return true;
				}
				return false;
			}

		// 3DMIGOTO REMOVAL private:
			float mEyeSeparation;
			float mSeparation;
			float mConvergence;
			// vvvvv 3DMIGOTO ADDITION vvvvv
			float mSeparationModifier;
			float mTuneVariable1, mTuneVariable2, mTuneVariable3, mTuneVariable4;
			bool mForceUpdate;
			// ^^^^^ 3DMIGOTO ADDITION ^^^^^

			StereoHandle mStereoHandle;
			// 3DMIGITO REMOVAL bool mInitialized;
			bool mActive;
			bool mDeviceLost;
		};

#ifndef NO_STEREO_D3D9
		typedef ParamTextureManager<D3D9Type> ParamTextureManagerD3D9;
#endif

#ifndef NO_STEREO_D3D10
		typedef ParamTextureManager<D3D10Type> ParamTextureManagerD3D10;
#endif

#ifndef NO_STEREO_D3D11 // 3DMIGOTO ADDITION
		typedef ParamTextureManager<D3D11Type> ParamTextureManagerD3D11;
#endif
	};
};

#endif /* __NVSTEREO__ */
