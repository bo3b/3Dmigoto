#pragma once

#include <cstdio>
#include <ctime>
#include <string>

// Wrappers to make logging cleaner.

extern FILE* LogFile;
extern bool  gLogDebug;

// Note that for now I've left the definitions of LogFile and LOG_DEBUG as they
// were - either declared locally in a file, as an extern, or from another
// namespace altogether. At some point this needs to be cleaned up, but it's
// probably not worth doing so unless we were switching to use a central
// logging framework.

#define LOG_INFO(fmt, ...)                      \
    do                                          \
    {                                           \
        if (LogFile)                            \
            fprintf(LogFile, fmt, __VA_ARGS__); \
    } while (0)
#define LOG_INFO_V(fmt, va_args)             \
    do                                       \
    {                                        \
        if (LogFile)                         \
            vfprintf(LogFile, fmt, va_args); \
    } while (0)
#define LOG_INFO_W(fmt, ...)                     \
    do                                           \
    {                                            \
        if (LogFile)                             \
            fwprintf(LogFile, fmt, __VA_ARGS__); \
    } while (0)
#define LOG_INFO_W_V(fmt, va_args)            \
    do                                        \
    {                                         \
        if (LogFile)                          \
            vfwprintf(LogFile, fmt, va_args); \
    } while (0)

#define LOG_DEBUG(fmt, ...)             \
    do                                  \
    {                                   \
        if (gLogDebug)                  \
            LOG_INFO(fmt, __VA_ARGS__); \
    } while (0)
#define LOG_DEBUG_V(fmt, va_args)     \
    do                                \
    {                                 \
        if (gLogDebug)                \
            LOG_INFO_V(fmt, va_args); \
    } while (0)
#define LOG_DEBUG_W(fmt, ...)             \
    do                                    \
    {                                     \
        if (gLogDebug)                    \
            LOG_INFO_W(fmt, __VA_ARGS__); \
    } while (0)

// Aliases for the above functions that we use to denote that omitting the
// newline was done intentionally. For now this is just for our reference, but
// later we might actually make the default function insert a newline:
#define LOG_INFO_NO_NL LOG_INFO
#define LOG_INFO_W_NO_NL LOG_INFO_W
#define LOG_DEBUG_NO_NL LOG_DEBUG
#define LOG_DEBUG_W_NO_NL LOG_DEBUG_W

static std::string log_time()
{
    std::string timeStr;
    char        cTime[32];
    tm          timestruct;

    time_t ltime = time(nullptr);
    localtime_s(&timestruct, &ltime);
    asctime_s(cTime, sizeof(cTime), &timestruct);

    timeStr = cTime;
    return timeStr;
}
