#pragma once

#define INITGUID
#define NOMINMAX

//
// Windows Header Files
//
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <tchar.h>
#include <stdio.h>
#include <crtdbg.h>

#include <fstream>

using namespace std;
extern FILE *LogFile;
extern bool LogDebug;

namespace D3D11Base
{
#include <d3d11.h>
#include <dxgi1_2.h>
#include "../nvapi.h"
#include "../nvstereo.h"
#include <D3Dcompiler.h>
}

#include "../PointerSet.h"

namespace D3D11Wrapper
{
#include "../IDirect3DUnknown.h"
#include "d3d11Wrapper.h"
}
