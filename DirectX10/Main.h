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

namespace D3D10Base
{
#include <d3d10_1.h>
#include "../nvapi.h"
#include "../nvstereo.h"
#include <D3Dcompiler.h>
}

#include "../PointerSet.h"

namespace D3D10Wrapper
{
#include "d3d10Wrapper.h"
}
