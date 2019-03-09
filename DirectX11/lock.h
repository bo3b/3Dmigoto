#pragma once

#include <windows.h>

void enable_lock_dependency_checks();
struct held_lock_info {
	CRITICAL_SECTION *lock;
	uintptr_t ret;
	size_t stack_hash;
};
