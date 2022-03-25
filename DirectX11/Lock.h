#pragma once

#include <vector>
#include <windows.h>

// This version of EnterCriticalSection will use the function and line number
// in any lock stacks dumped when potential deadlock hazards are detected:
#define ENTER_CRITICAL_SECTION(lock) \
    _EnterCriticalSectionPretty(lock, __FUNCTION__, __LINE__)
void _EnterCriticalSectionPretty(CRITICAL_SECTION *lock, const char *function, int line);

// To keep visual consistency, we define this macro to bookend the sections
// that are actually critical. The use of these macros will thus always show
// that we want the pretty version with Function and Line numbers.
#define LEAVE_CRITICAL_SECTION(lock) \
    LeaveCriticalSection(lock)

// Use this when initialising a critical section in 3DMigoto to give it a nice
// name in lock stack dumps rather than using its address.
//
// **AVOID CALLING THIS FROM GLOBAL CONSTRUCTORS**
// https://yosefk.com/c++fqa/ctors.html#fqa-10.12
#define INIT_CRITICAL_SECTION(lock) \
    _InitializeCriticalSectionPretty(lock, #lock)
void _InitializeCriticalSectionPretty(CRITICAL_SECTION *lock, char *lock_name);

void enable_lock_dependency_checks();
struct held_lock_info {
    CRITICAL_SECTION *lock;
    uintptr_t ret;
    size_t stack_hash;
    char *function;
    int line;
};
typedef std::vector<held_lock_info> LockStack;
