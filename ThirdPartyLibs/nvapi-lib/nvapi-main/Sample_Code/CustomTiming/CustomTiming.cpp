/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

////////////////////////////////////////////////////////////////////////////////////
// @brief:  This is the entry point for the console application.
//          This program applies a fixed custom resolution to the attached monitor
//          It applies the custom settings each active display in the following sequence:
//          NvAPI_DISP_TryCustomDisplay... Wait for 5 seconds
//          NvAPI_DISP_SaveCustomDisplay... Wait for 5 seconds
//          NvAPI_DISP_RevertCustomDisplay... Wait for 5 seconds
//
// @assumptions: This code is designed for Win7+ operating systems. It assumes that 
//               the system has atleast one active display.
//
// @driver support: R313+
////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include "nvapi.h"

// Link the lib file.
// The following line is needed if we are building the code using the command line compiler.
// If we are building using the Visual Studio, we can point to the lib file using the UI.
#pragma comment(lib, "nvapi64.lib")

// This function applies the loaded custom timings on all available displays
NvAPI_Status ApplyCustomDisplay();

// This function enumerates the display Ids of all the connected displays
NvAPI_Status GetConnectedDisplays(NvU32 *displayIds, NvU32 *noDisplays);

// This function applies custom display settings, saves them and reverts back.
void loadCustomDisplay(NV_CUSTOM_DISPLAY *customDisplay);

int _tmain(int argc, _TCHAR *argv[])
{
    NvAPI_Status ret = NVAPI_OK;

    ret = NvAPI_Initialize();
    if(ret != NVAPI_OK)
    {
        printf("NvAPI_Initialize() failed = 0x%x", ret);
        return 1; // Initialization failed
    }

    for(NvU32 q = 0; q < 50; q++)
    {
        printf("/");
    }
    printf("\n");

    ret = ApplyCustomDisplay();
    if(ret != NVAPI_OK)
    {
        getchar();
        return 1; // Failed to apply custom display
    }
    printf("\n");

    for(NvU32 q = 0; q < 50; q++)
    {
        printf("/");
    }
    printf("\n");

    printf("\nCustom_Timing successful!\nPress any key to exit...\n");
    getchar();
    return 0;
}

void loadCustomDisplay(NV_CUSTOM_DISPLAY *cd)
{
    cd->version         = NV_CUSTOM_DISPLAY_VER;
    cd->width           = 1024;
    cd->height          = 999;
    cd->depth           = 32;
    cd->colorFormat     = NV_FORMAT_A8R8G8B8;
    cd->srcPartition.x  = 0;
    cd->srcPartition.y  = 0;
    cd->srcPartition.w  = 1;
    cd->srcPartition.h  = 1;
    cd->xRatio          = 1;
    cd->yRatio          = 1;
}

NvAPI_Status GetConnectedDisplays(NvU32 *displayIds, NvU32 *noDisplays)
{
    NvAPI_Status ret = NVAPI_OK;

    NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
    NvU32 gpuCount = 0;
    NvU32 noDisplay = 0;
    
    // Get all the Physical GPU Handles
    ret = NvAPI_EnumPhysicalGPUs(nvGPUHandle, &gpuCount);
    if(ret != NVAPI_OK)
    {
        return ret;
    }

    for(NvU32 Count = 0; Count < gpuCount; Count++) // iterating per Physical GPU Handle call
    {
        NvU32 dispIdCount = 0;
        // First call to get the no. of displays connected by passing NULL
        if(NvAPI_GPU_GetConnectedDisplayIds(nvGPUHandle[Count], NULL, &dispIdCount, 0) != NVAPI_OK)
        {
            return NVAPI_ERROR;
        }

        if(dispIdCount > 0) // If no. of displays connected > 0 we can proceed to check if its active
        {
            // alocations for the display ids
            NV_GPU_DISPLAYIDS *dispIds = (NV_GPU_DISPLAYIDS *)malloc( sizeof(NV_GPU_DISPLAYIDS)*dispIdCount );
            
            for(NvU32 dispIndex = 0; dispIndex < dispIdCount; dispIndex++)
            {
                dispIds[dispIndex].version = NV_GPU_DISPLAYIDS_VER; // adding the correct version information
            }

            // second call to get the display ids
            if(NvAPI_GPU_GetConnectedDisplayIds(nvGPUHandle[Count], dispIds, &dispIdCount, 0) != NVAPI_OK)
            {
                return NVAPI_ERROR;
            }

            for(NvU32 dispIndex = 0; dispIndex < dispIdCount; dispIndex++)
            {
                if(dispIds[dispIndex].isMultiStreamRootNode)
                {
                    continue;
                }
                displayIds[noDisplay] = dispIds[dispIndex].displayId;
                noDisplay++;  
            }
        }
    }

    *noDisplays = noDisplay;

    return ret;
}

NvAPI_Status ApplyCustomDisplay()
{
    NvAPI_Status ret = NVAPI_OK;
    NvU32 noDisplays = 0;

    NvDisplayHandle hNvDisplay[NVAPI_MAX_DISPLAYS] = {0};

    NvU32 displayIds[NVAPI_MAX_DISPLAYS] = {0};

    ret = GetConnectedDisplays(&displayIds[0],&noDisplays);
    if(ret != NVAPI_OK)
    {
        printf("\nCall to GetConnectedDisplays() failed");
        return ret;
    }
    
    printf("\nNumber of Displays in the system = %2d",noDisplays);

    NV_CUSTOM_DISPLAY cd[NVAPI_MAX_DISPLAYS] = {0};
    
    float rr = 60;

    //timing computation (to get timing that suits the changes made)
    NV_TIMING_FLAG flag    = {0};

    NV_TIMING_INPUT timing = {0};

    timing.version = NV_TIMING_INPUT_VER;

    for (NvU32 count = 0; count < noDisplays; count++)
    {
        //Load the NV_CUSTOM_DISPLAY structure with data from XML file
        loadCustomDisplay(&cd[count]);

        timing.height = cd[count].height;
        timing.width  = cd[count].width;
        timing.rr     = rr;

        timing.flag   = flag;
        timing.type   = NV_TIMING_OVERRIDE_CVT_RB;
        timing.flag.scaling = 1;
        
        ret = NvAPI_DISP_GetTiming( displayIds[0], &timing, &cd[count].timing);
      
        if ( ret != NVAPI_OK)
        {
            printf("NvAPI_DISP_GetTiming() failed = %d\n", ret);		//failed to get custom display timing
            return ret;
        }
    }

    printf("\nCustom Timing to be tried: ");
    printf("%d X %d @ %0.2f hz",cd[0].width,cd[0].height,rr);

    printf("\nNvAPI_DISP_TryCustomDisplay()");

    ret = NvAPI_DISP_TryCustomDisplay(&displayIds[0],noDisplays, &cd[0]); // trying to set custom display
    if ( ret != NVAPI_OK)
    {
        printf("NvAPI_DISP_TryCustomDisplay() failed = %d", ret);		//failed to set custom display
        return ret;
    }
    else
    {
        printf(".....Success!\n");
    }
    Sleep(5000);

    printf("NvAPI_DISP_SaveCustomDisplay()");

    ret = NvAPI_DISP_SaveCustomDisplay(&displayIds[0],noDisplays, true, true);
    if ( ret != NVAPI_OK)
    {
        printf("NvAPI_DISP_SaveCustomDisplay() failed = %d", ret);		//failed to save custom display
        return ret;
    }
    else
    {
        printf(".....Success!\n");
    }

    Sleep(5000);

    printf("NvAPI_DISP_RevertCustomDisplayTrial()");

    // Revert the new custom display settings tried.
    ret = NvAPI_DISP_RevertCustomDisplayTrial(&displayIds[0],1);
    if ( ret != NVAPI_OK)
    {
        printf("NvAPI_DISP_RevertCustomDisplayTrial() failed = %d", ret);		//failed to revert custom display trail
        return ret;
    }
    else
    {
        printf(".....Success!");
        Sleep(5000);

    }

    return ret;	// Custom Display.
}