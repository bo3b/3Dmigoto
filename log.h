#pragma once

#include <time.h>
#include <stdarg.h>

// Wrappers to make logging cleaner.

extern FILE *gLogFile;
extern bool gLogDebug;


// Note that for now I've left the definitions of LogFile and LogDebug as they
// were - either declared locally in a file, as an extern, or from another
// namespace altogether. At some point this needs to be cleaned up, but it's
// probably not worth doing so unless we were switching to use a central
// logging framework.

#define LogInfo(fmt, ...) \
	if (gLogFile) fprintf(gLogFile, fmt, __VA_ARGS__)
#define LogInfoW(fmt, ...) \
	if (gLogFile) fwprintf(gLogFile, fmt, __VA_ARGS__)

#define LogDebug(fmt, ...) \
	if (gLogDebug) LogInfo(fmt, __VA_ARGS__)
#define LogDebugW(fmt, ...) \
	if (gLogDebug) LogInfoW(fmt, __VA_ARGS__)

