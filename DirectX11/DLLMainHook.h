#pragma once

#include "Nektra/NktHookLib.h"

// We can have only one of these hook libraries for the entire process.
// This is exported so that we can install hooks later during runtime, as
// well as at launch.

extern CNktHookLib cHookMgr;

