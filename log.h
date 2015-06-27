#pragma once

using namespace std;

#include <string>
#include <ctime>

// Wrappers to make logging cleaner.

extern FILE *LogFile;
extern bool gLogDebug;

// Note that for now I've left the definitions of LogFile and LogDebug as they
// were - either declared locally in a file, as an extern, or from another
// namespace altogether. At some point this needs to be cleaned up, but it's
// probably not worth doing so unless we were switching to use a central
// logging framework.

// DISABLE_LOGGING is a stopgap measure to get D3D_Shaders from Project Flugan
// to compile, which is not using this logging infrastracture, but now includes
// some of our HLSL decompiler, which does want these defined.
#ifndef DISABLE_LOGGING

#define LogInfo(fmt, ...) \
	do { if (LogFile) fprintf(LogFile, fmt, __VA_ARGS__); } while (0)
#define LogInfoW(fmt, ...) \
	do { if (LogFile) fwprintf(LogFile, fmt, __VA_ARGS__); } while (0)

#define LogDebug(fmt, ...) \
	do { if (gLogDebug) LogInfo(fmt, __VA_ARGS__); } while (0)
#define LogDebugW(fmt, ...) \
	do { if (gLogDebug) LogInfoW(fmt, __VA_ARGS__); } while (0)

#else // DISABLE_LOGGING

#define LogInfo(...) do {} while (0)
#define LogInfoW(...) do {} while (0)
#define LogDebug(...) do {} while (0)
#define LogDebugW(...) do {} while (0)

#endif // DISABLE_LOGGING

static string LogTime()
{
	string timeStr;
	char cTime[32];
	tm timestruct;

	time_t ltime = time(0);
	localtime_s(&timestruct, &ltime);
	asctime_s(cTime, sizeof(cTime), &timestruct);

	timeStr = cTime;
	return timeStr;
}

