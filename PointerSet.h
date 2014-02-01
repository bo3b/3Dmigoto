#pragma once

#include <map>

class ThreadSafePointerSet
{
private:
	std::map<void *, void *> mMap;
	CRITICAL_SECTION m_CritSec;

public:
	ThreadSafePointerSet()
	{
		InitializeCriticalSection(&m_CritSec);
	}
	~ThreadSafePointerSet()
	{
		DeleteCriticalSection(&m_CritSec);
	}
	PVOID GetDataPtr(PVOID pKey)
	{
		PVOID p;
		EnterCriticalSection(&m_CritSec);
		std::map<void *, void *>::iterator i = mMap.find(pKey);
		p = i == mMap.end() ? 0 : i->second;
		LeaveCriticalSection(&m_CritSec);
		return p;
	}
	void AddMember(PVOID pKey, PVOID pData)
	{
		EnterCriticalSection(&m_CritSec);
		mMap[pKey] = pData;
		LeaveCriticalSection(&m_CritSec);
	}
	void DeleteMember(PVOID pKey)
	{
		EnterCriticalSection(&m_CritSec);
		mMap.erase(pKey);
		LeaveCriticalSection(&m_CritSec);
	}
};
