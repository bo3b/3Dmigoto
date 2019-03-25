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

#include <d3d11_1.h>
#include "nvapi.h"
#include "log.h"
#include "profiling.h"
#include "util.h"

namespace nv
{
	// -----------------------------------------------------------------------------------------------

	// This function was copied from the NV SDK file of DisplayConfiguration.cpp as a display example.
	// It's been modified to return the PathInfo for just the primary device, as the one that will be
	// used for 3D display.
	// If we fail for some reason, just set width/height to zero as it's not that significant.
	// Defined as 'static' because we only need the one implementation, and nvstereo.h is included 
	// multiple times.

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

	namespace stereo
	{
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
			leftEye[3] = -sep * 0.01f;

			leftEye[4] = tuneValue1;
			leftEye[5] = tuneValue2;
			leftEye[6] = tuneValue3;
			leftEye[7] = tuneValue4;

			leftEye[8] = screenWidth;
			leftEye[9] = screenHeight;

			rightEye[0] = -leftEye[0];
			rightEye[1] = leftEye[1];
			rightEye[2] = -leftEye[2];
			rightEye[3] = -leftEye[3];
			rightEye[4] = leftEye[4];
			rightEye[5] = leftEye[5];
			rightEye[6] = leftEye[6];
			rightEye[7] = leftEye[7];
			rightEye[8] = leftEye[8];
			rightEye[9] = leftEye[9];

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

		// The NV Stereo class, which can work for either D3D9 or D3D10, depending on which type it's specialized for
		// Note that both types can live side-by-side in two separate instances as well.
		// Also note that there are convenient typedefs below the class definition.
		template <class D3DType>
		class ParamTextureManager
		{
		public:
			typedef typename D3DType Parms;
			typedef typename D3DType::Device Device;
			typedef typename D3DType::Texture Texture;
			typedef typename D3DType::StagingResource StagingResource;
			typedef typename D3DType::Context Context;

			ParamTextureManager() :
				mEyeSeparation(0),
				mSeparation(0),
				mSeparationModifier(1),
				mTuneVariable1(1), mTuneVariable2(1), mTuneVariable3(1), mTuneVariable4(1),
				mForceUpdate(false),
				mConvergence(0),
				//mStereoHandle(0),
				mActive(false),
				mDeviceLost(true)
			{
				// mDeviceLost is set to true to initialize the texture with good data at app startup.

				// The call to CreateConfigurationProfileRegistryKey must happen BEFORE device creation.
				// Profiling::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3DType::RegistryProfileType);
			}

			// Not const because we will update the various values if an update is needed.
			bool RequiresUpdate(bool deviceLost)
			{
				if (mForceUpdate)
				{
					mForceUpdate = false;
					return true;
				}
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
				}
				else {
					eyeSep = sep = conv = 0;
					updateRequired = active != mActive;
				}

				// If the device was lost and is now restored, need to update the texture contents again.
				updateRequired = updateRequired || (!deviceLost && mDeviceLost);
				mDeviceLost = deviceLost;

				if (updateRequired) {
					LogInfo("  updating stereo texture with eyeSeparation = %e, separation = %e, convergence = %e, active = %d\n",
						eyeSep, sep, conv, active ? 1 : 0);

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


			bool UpdateStereoTexture(Device* dev, Context* context, Texture* tex, bool deviceLost)
			{
				if (!RequiresUpdate(deviceLost)) {
					return false;
				}

				StagingResource *staging = D3DType::CreateStagingResource(dev, mEyeSeparation, mSeparation * mSeparationModifier, mConvergence,
					mTuneVariable1, mTuneVariable2, mTuneVariable3, mTuneVariable4);
				if (staging)
				{
					D3DType::UpdateTextureFromStaging(context, tex, staging);
					staging->Release();
					return true;
				}
				return false;
			}

			float mEyeSeparation;
			float mSeparation;
			float mConvergence;
			float mSeparationModifier;
			float mTuneVariable1, mTuneVariable2, mTuneVariable3, mTuneVariable4;
			bool mForceUpdate;

			StereoHandle mStereoHandle;
			bool mActive;
			bool mDeviceLost;
		};

		typedef ParamTextureManager<D3D11Type> ParamTextureManagerD3D11;

	};
};

#endif /* __NVSTEREO__ */
