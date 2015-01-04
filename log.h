#pragma once

// Wrappers to make logging cleaner.

// Note that for now I've left the definitions of LogFile and LogDebug as they
// were - either declared locally in a file, as an extern, or from another
// namespace altogether. At some point this needs to be cleaned up, but it's
// probably not worth doing so unless we were switching to use a central
// logging framework.

#define LogInfo(fmt, ...) \
	if (LogFile) fprintf(LogFile, fmt, __VA_ARGS__)
#define LogInfoW(fmt, ...) \
	if (LogFile) fwprintf(LogFile, fmt, __VA_ARGS__)

#define LogDebug(fmt, ...) \
	if (LogDebug) LogInfo(fmt, __VA_ARGS__)
#define LogDebugW(fmt, ...) \
	if (LogDebug) LogInfoW(fmt, __VA_ARGS__)
