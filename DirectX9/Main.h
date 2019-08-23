#pragma once

#define INITGUID
#define NOMINMAX

// Windows Header Files
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <tchar.h>
#include <stdio.h>
#include <crtdbg.h>
#include <fstream>
#include <set>
#include <unordered_map>
#include <stdint.h>

using namespace std;

namespace D3D9Base
{
#include <d3d9.h>
}
#include <nvapi.h>
#include "../PointerSet.h"
#include "DrawCallInfo.h"
#include <nvstereo.h>
#include "Globals.h"
#include "Overlay.h"

namespace D3D9Wrapper
{
#include "d3d9Wrapper.h"
}
void NvAPIOverride();
void NvAPIEnableStereoActiveTracking();
void NvAPIEnableConvergenceTracking();
void NvAPIEnableSeparationTracking();
void NvAPIEnableEyeSeparationTracking();
void NvAPIResetStereoActiveTracking();
void NvAPIResetConvergenceTracking();
void NvAPIResetSeparationTracking();
void NvAPIResetEyeSeparationTracking();
