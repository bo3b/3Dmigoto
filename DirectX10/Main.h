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

namespace D3D11Base
{
#include <d3d10.h>
#include "../nvapi.h"
}

#include "../PointerSet.h"

namespace D3D11Wrapper
{
#include "d3d10Wrapper.h"
}
