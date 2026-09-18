/*
 * The allocation-side half of the stack probe: every allocation samples the
 * stack pointer. See stackprobe.h. The parser's recursion point is sampled
 * separately, by the line instrument.py inserts.
 */
#include	<new>
#include	<cstdlib>
#include	<cstddef>
#include	<stackprobe.h>

void* operator new(size_t n)
{
	void*	p = malloc(n ? n : 1);
	if (!p) throw std::bad_alloc();
	stackprobe::sample(__builtin_frame_address(0));
	return p;
}
void* operator new[](size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }
void* operator new(size_t n, const std::nothrow_t&) noexcept
{ void* p = malloc(n ? n : 1); if (p) stackprobe::sample(__builtin_frame_address(0)); return p; }
void* operator new[](size_t n, const std::nothrow_t& t) noexcept { return operator new(n, t); }
void operator delete(void* p, const std::nothrow_t&) noexcept { free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { free(p); }
