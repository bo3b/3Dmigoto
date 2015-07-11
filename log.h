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

#define LogInfo(fmt, ...) \
	do { if (LogFile) fprintf(LogFile, fmt, __VA_ARGS__); } while (0)
#define LogInfoW(fmt, ...) \
	do { if (LogFile) fwprintf(LogFile, fmt, __VA_ARGS__); } while (0)

#define LogDebug(fmt, ...) \
	do { if (gLogDebug) LogInfo(fmt, __VA_ARGS__); } while (0)
#define LogDebugW(fmt, ...) \
	do { if (gLogDebug) LogInfoW(fmt, __VA_ARGS__); } while (0)

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

