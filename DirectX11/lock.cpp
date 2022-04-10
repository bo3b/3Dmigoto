#include "lock.h"
#include "overlay.h"

#include <psapi.h>
#include <inttypes.h>
#include <unordered_map>
#include <vector>

// This implements a lock dependency checker to detect possible deadlock
// scenarios even if no deadlock was actually hit. It is inspired by the
// lockdep subsystem of the Linux Kernel, but is not derived from it.
//
// This check more than just 3DMigoto's locks because we are are specifically
// interested in detecting more complicated locking scenarios that might
// involve a two or more way interaction between locking within:
//
// a) 3DMigoto
// b) The game
// c) DirectX
// d) The driver
//
// We can only see the 3DMigoto locks directly, but those are the least
// interesting because by itself it is fairly simple, with little more than one
// big lock protecting a whole bunch of data structures and therefore minimal
// potential for deadlocks (but perhaps some confusion over when exactly we
// need to take the lock and why - we would be good to adopt the kernel's
// way of looking at things to reduce that confusion: that data structures are
// locked, not code).
//
// Things get more interesting when DirectX and a multi-threaded game is in the
// mix, because we have the potential that we might get called with a already
// lock held and take our lock, and at other times we might call into DirectX
// with our lock held, and if it takes it's lock from that call we have a
// potential AB-BA deadlock scenario, even though one of the locks is not in
// our code. This does indeed happen with interactions between the resource
// release tracker, overlay, ShaderRegex and possibly elsewhere.
//
// We also sometimes see hangs in some games that do not involve 3DMigoto at
// all, where one thread is blocked waiting on the DirectX critical section,
// and another is blocked waiting on a semaphore in the driver, each holding
// the other's lock. Even though these aren't our bugs, these would also be
// nice to have some visibility into.
//
// To get visibility into locking outside of our control we are going to hook
// into various locking routines, starting with EnterCriticalSection, and maybe
// adding other locking primitives later. Each thread will track what locks it
// takes in Thread Local Storage, and Whenever a lock is taken we use this to
// construct a lock dependency graph. If we find any cycles in the graph it
// means we have a discovered potential deadlock.
//
// This should also help greatly increase our confidence that we haven't
// introduced any new locking bugs when splitting up 3DMigoto's lock or working
// with code that uses it.


static void(__stdcall *_EnterCriticalSection)(CRITICAL_SECTION *lock) = EnterCriticalSection;
static void(__stdcall *_LeaveCriticalSection)(CRITICAL_SECTION *lock) = LeaveCriticalSection;
static BOOL(__stdcall *_TryEnterCriticalSection)(CRITICAL_SECTION *lock) = TryEnterCriticalSection;
static void(__stdcall *_DeleteCriticalSection)(CRITICAL_SECTION *lock) = DeleteCriticalSection;

// We store a list (set) of all locks taken *after* a given lock
typedef std::unordered_set<CRITICAL_SECTION*> lock_graph_node;
static std::unordered_map<CRITICAL_SECTION*, lock_graph_node> lock_graph;
static CRITICAL_SECTION graph_lock;
static std::unordered_map<size_t, LockStack> cached_stacks;
static std::unordered_set<size_t> reported_stacks;
static std::set<std::pair<CRITICAL_SECTION*,CRITICAL_SECTION*>> overlay_reported;

static bool lock_dependency_checks_enabled;

// TODO: If we get any more of these put them in a list
static uintptr_t ntdll_base, ntdll_end;
static uintptr_t apphelp_base, apphelp_end;

static std::unordered_map<CRITICAL_SECTION*, std::string> lock_names;
static const char* lock_name(CRITICAL_SECTION *lock, char buf[20])
{
	auto i = lock_names.find(lock);
	if (i == lock_names.end()) {
		_snprintf_s(buf, 20, _TRUNCATE, "%p", lock);
		return buf;
	}
	return i->second.c_str();
}

static void dump_stack_trace()
{
	uintptr_t trace[62];
	USHORT frames;
	HMODULE module;
	wchar_t path[MAX_PATH];
	MODULEINFO mod_info;

	LogInfo("%04x call stack:\n", GetCurrentThreadId());
	frames = CaptureStackBackTrace(0, 62, (void**)trace, NULL);
	for (USHORT i = 0; i < frames; i++) {
		if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)trace[i], &module)
		 && GetModuleFileName(module, path, MAX_PATH)
		 && GetModuleInformation(GetCurrentProcess(), module, &mod_info, sizeof(MODULEINFO))) {
			LogInfo("%04x: %S+0x%"PRIxPTR"\n",
					GetCurrentThreadId(), path, trace[i] - (uintptr_t)mod_info.lpBaseOfDll);
		} else {
			LogInfo("%04x: 0x%"PRIxPTR"\n",
					GetCurrentThreadId(), trace[i]);
		}
	}
}

static void log_held_locks(LockStack &held_locks, std::vector<LockStack> &other_sides)
{
	HMODULE module;
	wchar_t path[MAX_PATH];
	MODULEINFO mod_info;
	char buf[20];

	LogInfo("%04x held locks (most recent first):\n", GetCurrentThreadId());
	for (auto info = held_locks.rbegin(); info != held_locks.rend(); info++) {
		if (info->function) {
			// 3DMigoto internal locking call with decorated function and line number
			LogInfo("%04x: EnterCriticalSection(%s) %s(%d)\n",
					GetCurrentThreadId(), lock_name(info->lock, buf), info->function, info->line);
		} else {
			if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)info->ret, &module)
			 && GetModuleFileName(module, path, MAX_PATH)
			 && GetModuleInformation(GetCurrentProcess(), module, &mod_info, sizeof(MODULEINFO))) {
				LogInfo("%04x: EnterCriticalSection(%s) %S+0x%"PRIxPTR"\n",
						GetCurrentThreadId(), lock_name(info->lock, buf), path, info->ret - (uintptr_t)mod_info.lpBaseOfDll);
			} else {
				LogInfo("%04x: EnterCriticalSection(%s) 0x%"PRIxPTR"\n",
						GetCurrentThreadId(), lock_name(info->lock, buf), info->ret);
			}
		}
	}

	for (auto &other_stack: other_sides) {
		LogInfo("----- Previously seen locking pattern that could lead to an AB-BA deadlock:\n");
		// Bit of a copy & paste job just to remove the thread IDs
		// since those aren't saved in the stack (yet?)
		for (auto info = other_stack.rbegin(); info != other_stack.rend(); info++) {
			if (info->function) {
				// 3DMigoto internal locking call with decorated function and line number
				LogInfo("      EnterCriticalSection(%s) %s(%d)\n",
						lock_name(info->lock, buf), info->function, info->line);
			} else {
				if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)info->ret, &module)
				 && GetModuleFileName(module, path, MAX_PATH)
				 && GetModuleInformation(GetCurrentProcess(), module, &mod_info, sizeof(MODULEINFO))) {
					LogInfo("      EnterCriticalSection(%s) %S+0x%"PRIxPTR"\n",
							lock_name(info->lock, buf), path, info->ret - (uintptr_t)mod_info.lpBaseOfDll);
				} else {
					LogInfo("      EnterCriticalSection(%s) 0x%"PRIxPTR"\n",
							lock_name(info->lock, buf), info->ret);
				}
			}
		}
	}
	if (other_sides.empty()) {
		LogInfo("----- No previously seen single locking stack could lead to an AB-BA deadlock - this may be a 3+ way deadlock\n");
		// TODO: Search the graph of cached stacks to find all the
		// stacks that can lead to this. We could alternatively store
		// (a second copy of) the locking graph that doesn't have
		// dependencies merged into each lock as they are found
		// (i.e. without the lock_graph[lock].insert(new_lock_after),
		// then recursively searching that graph will reveal the
		// scenario that led to this.
	}

	// Dump the current thread's call stack to help locate the issue if the
	// lock covers a large amount of code. This won't dump the other
	// thread's call stack, so it won't be the complete picture, but it
	// helps. TODO: Consider caching the call stacks along with the lock
	// stacks so we can dump out the other side.
	dump_stack_trace();

	LogInfo("%04x - - - - - - - - - -\n", GetCurrentThreadId());

	// Just in case we are about to deadlock for real, flush the log file
	// to make sure we know what happened:
	if (LogFile)
		fflush(LogFile);
}

// Should be called with the graph lock held
static bool takes_lock_in_order(LockStack &other_stack,
		CRITICAL_SECTION *first_lock, CRITICAL_SECTION *second_lock)
{
	bool first_lock_seen = false;

	for (auto info = other_stack.begin(); info < other_stack.end(); info++) {
		if (info->lock == first_lock)
			first_lock_seen = true;
		else if (info->lock == second_lock)
			return first_lock_seen;
	}

	return false;
}

static void validate_lock(LockStack &locks_held, CRITICAL_SECTION *new_lock)
{
	std::vector<std::pair<CRITICAL_SECTION*, bool>> issues;
	std::vector<LockStack> other_sides;
	bool reported = false;

	// First lock taken in any thread cannot lead to a deadlock - only
	// starvation if we are already in deadlock:
	if (locks_held.size() < 2)
		return;

	_EnterCriticalSection(&graph_lock);

	// Save time by not re-checking previously checked stacks:
	if (cached_stacks.count(locks_held.back().stack_hash))
		goto out_unlock;

	// Critical section type locks are re-entrant, so we are not worried if
	// we see a currently held lock being taken again. Last locks_held is
	// the current lock, so check all others leading up to it:
	for (auto info = locks_held.begin(); info < locks_held.end() - 1; info++) {
		if (info->lock == new_lock) {
			cached_stacks.insert({locks_held.back().stack_hash, locks_held});
			goto out_unlock;
		}
	}

	reported = !!reported_stacks.count(locks_held.back().stack_hash);

	// Check if any of the currently held locks appear in the new lock's
	// after list, indicating an AB-BA deadlock scenario.
	auto &new_lock_after = lock_graph[new_lock];
	for (auto info = locks_held.begin(); info < locks_held.end() - 1; info++) {
		// Add the newly taken lock to each currently held lock's after
		// list, along with all the other locks in this lock's after
		// list since their order will have been constrained by taking
		// this lock:
		lock_graph[info->lock].insert(new_lock);
		lock_graph[info->lock].insert(new_lock_after.begin(), new_lock_after.end());

		if (!reported && new_lock_after.count(info->lock)) {
			if (overlay_reported.count({new_lock, info->lock})) {
				issues.push_back({info->lock, false});
			} else {
				issues.push_back({info->lock, true});
				overlay_reported.insert({new_lock, info->lock});
			}
		}
	}

	// Take note of a hash identifying the current lock stack so we can
	// avoid checking it again, and so we don't report the same bug
	// multiple times:
	cached_stacks.insert({locks_held.back().stack_hash, locks_held});

	if (issues.empty())
		goto out_unlock;

	reported_stacks.insert(locks_held.back().stack_hash);

	// We're reporting a new issue, but our locking stack is only one side
	// of this. Search through the other locking stacks we have seen so far
	// to find the other side of this issue. This will only locate two way
	// AB-BA deadlocks, while the above code will also have noted three or
	// more way AB-BC-CA deadlocks as well. For now, if this doesn't find
	// anything we just log that it might be a 3+ way deadlock.
	for (auto &other_stack: cached_stacks) {
		for (auto &issue: issues) {
			if (takes_lock_in_order(other_stack.second, new_lock, issue.first)) {
				other_sides.push_back(other_stack.second);
				break;
			}
		}

	}

	// Report issues only after dropping the lock, since the logging code
	// is going to take its own locks and can easily deadlock with us
	_LeaveCriticalSection(&graph_lock);

	for (auto &issue: issues) {
		char buf1[20], buf2[20];
		if (issue.second) {
			LogOverlay(LOG_NOTICE, "%04x: Potential deadlock scenario detected: Lock %s taken after %s\n",
					GetCurrentThreadId(), lock_name(new_lock, buf1), lock_name(issue.first, buf2));
		} else {
			LogInfo("%04x: Potential deadlock scenario detected: Lock %s taken after %s\n",
					GetCurrentThreadId(), lock_name(new_lock, buf1), lock_name(issue.first, buf2));
		}
	}
	log_held_locks(locks_held, other_sides);

	if (IsDebuggerPresent())
		__debugbreak();

	return;
out_unlock:
	_LeaveCriticalSection(&graph_lock);
}

static void push_lock(LockStack &locks_held, CRITICAL_SECTION *new_lock, uintptr_t ret,
		char *function = NULL, int line = 0)
{
	size_t stack_hash = 0;

	if (!locks_held.empty())
		stack_hash = locks_held.back().stack_hash;

	stack_hash ^= std::hash<void*>()(new_lock) + 0x9e3779b9 + (stack_hash << 6) + (stack_hash >> 2);
	stack_hash ^= std::hash<uintptr_t>()(ret) + 0x9e3779b9 + (stack_hash << 6) + (stack_hash >> 2);

	locks_held.push_back({new_lock, ret, stack_hash, function, line});
}

static bool ignore_caller(uintptr_t ret)
{
	// ntdll.dll has a lock that seems to be obtained all over the place,
	// and constantly shows up in as being involved in potential deadlock
	// scenarios. I don't know for sure what this lock is - the locking
	// stacks do look concerning when taken at face value, but we have yet
	// to see an actual deadlock involving this lock and I suspect there
	// might be something else going on with this lock that we aren't
	// seeing. For now, consider these (likely) false positives and ignore
	// them, so that the reports that we do get will be more relevant.
	if (ret >= ntdll_base && ret < ntdll_end)
		return true;

	// apphelp.dll shows up in SkyrimVR with similar false positives to
	// the above. It looks like this may use additional locking methods
	// such as LeaveCriticalSectionWhenCallbackReturns and Rtl variants
	// that we aren't hooking yet, so it's possible we just miss some
	// cases when the dll releases its lock.
	return (ret >= apphelp_base && ret < apphelp_end);
}

static void EnterCriticalSectionHook(CRITICAL_SECTION *lock)
{
	if (ignore_caller((uintptr_t)_ReturnAddress()))
		return _EnterCriticalSection(lock);

	// If a TLS structure hasn't been allocated yet for this thread, we
	// might be in the TLS allocation itself!
	if (!TlsGetValue(tls_idx))
		return _EnterCriticalSection(lock);

	// Protect against reentrancy:
	if (get_tls()->hooking_quirk_protection)
		return _EnterCriticalSection(lock);
	get_tls()->hooking_quirk_protection = true;

	LockStack &locks_held = get_tls()->locks_held;
	push_lock(locks_held, lock, (uintptr_t)_ReturnAddress());
	validate_lock(locks_held, lock);

	_EnterCriticalSection(lock);

	get_tls()->hooking_quirk_protection = false;
}

void _EnterCriticalSectionPretty(CRITICAL_SECTION *lock, char *function, int line)
{
	if (!lock_dependency_checks_enabled)
		return EnterCriticalSection(lock);

	// Protect against reentrancy:
	if (get_tls()->hooking_quirk_protection)
		return _EnterCriticalSection(lock);
	get_tls()->hooking_quirk_protection = true;

	LockStack &locks_held = get_tls()->locks_held;
	push_lock(locks_held, lock, (uintptr_t)_ReturnAddress(), function, line);
	validate_lock(locks_held, lock);

	_EnterCriticalSection(lock);

	get_tls()->hooking_quirk_protection = false;
}

static BOOL TryEnterCriticalSectionHook(CRITICAL_SECTION *lock)
{
	BOOL ret = _TryEnterCriticalSection(lock);
	if (!ret)
		return ret;

	if (ignore_caller((uintptr_t)_ReturnAddress()))
		return ret;

	// If a TLS structure hasn't been allocated yet for this thread, we
	// might be in the TLS allocation itself!
	if (!TlsGetValue(tls_idx))
		return ret;

	// Protect against reentrancy:
	if (get_tls()->hooking_quirk_protection)
		return ret;
	get_tls()->hooking_quirk_protection = true;

	// Updating the lock stack, but not the dependency list since the try
	// lock is optional. TODO: Still check for applicable deadlocks
	LockStack &locks_held = get_tls()->locks_held;
	push_lock(locks_held, lock, (uintptr_t)_ReturnAddress());

	get_tls()->hooking_quirk_protection = false;
	return ret;
}

static void LeaveCriticalSectionHook(CRITICAL_SECTION *lock)
{
	_LeaveCriticalSection(lock);

	if (ignore_caller((uintptr_t)_ReturnAddress()))
		return;

	// If a TLS structure hasn't been allocated yet for this thread, we
	// might be in the TLS allocation itself!
	if (!TlsGetValue(tls_idx))
		return;

	// Protect against reentrancy:
	if (get_tls()->hooking_quirk_protection)
		return;
	get_tls()->hooking_quirk_protection = true;

	LockStack *locks_held = &(get_tls()->locks_held);
	for (auto i = locks_held->rbegin(); i != locks_held->rend(); i++) {
		if (i->lock == lock) {
			// C++ gotcha: reverse_iterator::base() points to the *next* element
			locks_held->erase(i.base() - 1);
			break;
		}
	}

	get_tls()->hooking_quirk_protection = false;
}

static void DeleteCriticalSectionHook(CRITICAL_SECTION *lock)
{
	// If a TLS structure hasn't been allocated yet for this thread, we
	// might be in the TLS allocation itself!
	if (!TlsGetValue(tls_idx))
		return;

	// Protect against reentrancy:
	if (get_tls()->hooking_quirk_protection)
		return;
	get_tls()->hooking_quirk_protection = true;

	// Purge the lock from our data structures:
	_EnterCriticalSection(&graph_lock);

	lock_graph.erase(lock);

	for (auto &l: lock_graph)
		l.second.erase(lock);

	for (auto stack = cached_stacks.begin(), next=stack; stack != cached_stacks.end(); stack = next) {
		next++;
		for (auto &info: stack->second) {
			if (info.lock == lock) {
				next = cached_stacks.erase(stack);
				reported_stacks.erase(stack->first);
				break;
			}
		}
	}

	_LeaveCriticalSection(&graph_lock);

	get_tls()->hooking_quirk_protection = false;

	_DeleteCriticalSection(lock);
}

void _InitializeCriticalSectionPretty(CRITICAL_SECTION *lock, char *lock_name)
{
	InitializeCriticalSection(lock);
	// NOTE: If we have been called from a global constructor, this may
	// crash due to C++'s undefined initialisation order. Do not use this
	// function from a global constructor, no matter how convenient they
	// seem like they would be to wrap a CRITICAL_SECTION! Even if we
	// switched to dynamically allocating this the first time this function
	// is called we then risk leaking memory in every process whenever
	// 3DMigoto Loader.exe is in use.
	// https://yosefk.com/c++fqa/ctors.html#fqa-10.12
	lock_names[lock] = lock_name;
}

void enable_lock_dependency_checks()
{
	SIZE_T hook_id;
	HMODULE mod_handle;
	MODULEINFO mod_info;

	if (lock_dependency_checks_enabled)
		return;
	lock_dependency_checks_enabled = true;

	InitializeCriticalSectionPretty(&graph_lock);

	if ((mod_handle = GetModuleHandleA("ntdll.dll"))
	  && GetModuleInformation(GetCurrentProcess(), mod_handle, &mod_info, sizeof(MODULEINFO))) {
		ntdll_base = (uintptr_t)mod_info.lpBaseOfDll;
		ntdll_end = ntdll_base + mod_info.SizeOfImage;
	}

	if ((mod_handle = GetModuleHandleA("apphelp.dll"))
		&& GetModuleInformation(GetCurrentProcess(), mod_handle, &mod_info, sizeof(MODULEINFO))) {
		apphelp_base = (uintptr_t)mod_info.lpBaseOfDll;
		apphelp_end = apphelp_base + mod_info.SizeOfImage;
	}

	// Deviare will itself take locks while we are hooking, so protect against re-entrancy:
	get_tls()->hooking_quirk_protection = true;

	cHookMgr.Hook(&hook_id, (void**)&_LeaveCriticalSection, LeaveCriticalSection, LeaveCriticalSectionHook);
	cHookMgr.Hook(&hook_id, (void**)&_EnterCriticalSection, EnterCriticalSection, EnterCriticalSectionHook);
	cHookMgr.Hook(&hook_id, (void**)&_TryEnterCriticalSection, TryEnterCriticalSection, TryEnterCriticalSectionHook);
	cHookMgr.Hook(&hook_id, (void**)&_DeleteCriticalSection, DeleteCriticalSection, DeleteCriticalSectionHook);

	get_tls()->hooking_quirk_protection = false;
}
