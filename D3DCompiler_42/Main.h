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

using namespace std;

#define COMPILER_DLL_VERSION "42"
#define COMPILER_DLL_VERSIONL L"42"

namespace D3DBase
{
#include <d3dcompiler.h>
#include <d3dcommon.h>
}

#include "../PointerSet.h"

namespace D3DWrapper
{
#include "d3dcWrapper.h"
}
