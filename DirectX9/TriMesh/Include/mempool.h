#ifndef MEMPOOL_H
#define MEMPOOL_H
/*
Szymon Rusinkiewicz
Princeton University

mempool.h
Replacement memory management for a class using a memory pool.

Sample usage:
	class MyClass {
	private:
		static PoolAlloc memPool;
	public:
		void *operator new(size_t n) { return memPool.alloc(n); }
		void operator delete(void *p, size_t n) { memPool.free(p,n); }
		// ...
	};

	PoolAlloc MyClass::memPool(sizeof(MyClass));

Does *no* error checking.
Make sure sizeof(MyClass) is larger than sizeof(void *).
Based on the description of the Pool class in _Effective C++_ by Scott Meyers.
*/

#include <vector>
#include <algorithm>


namespace trimesh {

class PoolAlloc {
private:
	enum { BLOCKSIZE = 4088 };
	size_t itemsize;
	void *freelist;
	void grow_freelist()
	{
		size_t n = BLOCKSIZE / itemsize;
		freelist = ::operator new(n * itemsize);
		for (size_t i = 0; i < n-1; i++)
			*(void **)((char *)freelist + itemsize*i) =
					(char *)freelist + itemsize*(i+1);
		*(void **)((char *)freelist + itemsize*(n-1)) = 0;
	}

public:
	PoolAlloc(size_t size) : itemsize(size), freelist(0) {}
	void *alloc(size_t n)
	{
		if (n != itemsize)
			return ::operator new(n);
		if (!freelist)
			grow_freelist();
		void *next = freelist;
		freelist = *(void **)next;
		return next;
	}
	void free(void *p, size_t n)
	{
		if (!p)
			return;
		else if (n != itemsize)
			::operator delete(p);
		else {
			*(void **)p = freelist;
			freelist = p;
		}
	}
	void sort_freelist()
	{
		using namespace ::std;
		if (!freelist)
			return;
		vector<void *> v;
		void *p;
		for (p = freelist; *(void **)p; p = *(void **)p)
			v.push_back(p);
		sort(v.begin(), v.end());
		p = freelist = v[0];
		for (size_t i = 1; i < v.size(); i++) {
			*(void **)p = v[i];
			p = *(void **)p;
		}
		*(void **)p = NULL;
	}
};

}; // namespace trimesh

#endif
