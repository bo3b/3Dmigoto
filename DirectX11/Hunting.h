// Hunting
//  This code is to implement the Hunting mechanism as a separate compilation from the main wrapper code.
//  It implements all the shader management based on user input via key presses from Input.
//
//  RunFrameActions is moved here, because the primary purpose is for hunting, but it also does Input
//  handling for user overrides, so it should probably be moved.
//  It was moved out of d3d11Wrapper so that d3d11Wrapper can be only wrapper code.

#include "Direct3D11Device.h"

void TimeoutHuntingBuffers();
void RegisterHuntingKeyBindings(wchar_t *iniFile);
void RunFrameActions(HackerDevice *device);
