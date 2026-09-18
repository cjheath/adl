/*
 * Dynamic-memory probe. NOT part of any library - linked in beside a driver.
 *
 * Overrides global operator new/delete and tracks live heap bytes using
 * malloc_size(), so it counts what the allocator actually handed out rather
 * than what was asked for. Reports the high-water mark (the number that
 * matters on a target with limited RAM), the live bytes at exit (the retained
 * object graph), and the allocation activity, which is what says how much
 * churn the measurement did not see in the peak.
 *
 * Reports via write(2), not printf, so its own output cannot perturb what it
 * is measuring.
 *
 * The link order matters: this object must come before any other definition
 * of operator new, and it must be linked into the *driver* (the TU with main),
 * not into a library, or the default operator new wins.
 */
#include	<new>
#include	<cstdlib>
#include	<cstddef>
#include	<cstdint>
#include	<unistd.h>
#include	<malloc/malloc.h>

namespace {

uintptr_t	live_bytes, peak_bytes, total_allocs, total_frees, total_bytes;

inline void note_alloc(void* p, size_t requested)
{
	size_t	sz = malloc_size(p);	// What the allocator gave, not what we asked
	live_bytes += sz;
	total_bytes += requested;
	total_allocs++;
	if (live_bytes > peak_bytes)
		peak_bytes = live_bytes;
}

inline void note_free(void* p)
{
	if (!p)
		return;
	live_bytes -= malloc_size(p);
	total_frees++;
}

char* putnum(char* p, uintptr_t v)
{
	char	buf[24];
	int	n = 0;
	if (v == 0)
		buf[n++] = '0';
	while (v)
	{
		buf[n++] = static_cast<char>('0' + (v % 10));
		v /= 10;
	}
	while (n)
		*p++ = buf[--n];
	return p;
}

char* putstr(char* p, const char* s)
{
	while (*s)
		*p++ = *s++;
	return p;
}

void report()
{
	char	buf[256];
	char*	p = buf;
	p = putstr(p, "PEAK peak_bytes=");
	p = putnum(p, peak_bytes);
	p = putstr(p, " live_at_exit=");
	p = putnum(p, live_bytes);
	p = putstr(p, " allocs=");
	p = putnum(p, total_allocs);
	p = putstr(p, " frees=");
	p = putnum(p, total_frees);
	p = putstr(p, " bytes_allocated=");
	p = putnum(p, total_bytes);
	*p++ = '\n';
	write(2, buf, static_cast<size_t>(p - buf));
}

struct Reporter { Reporter() { atexit(report); } };
Reporter	reporter;

}

void* operator new(size_t n)
{
	void*	p = malloc(n ? n : 1);
	if (!p)
		throw std::bad_alloc();
	note_alloc(p, n);
	return p;
}

void* operator new[](size_t n) { return operator new(n); }

void operator delete(void* p) noexcept { note_free(p); free(p); }
void operator delete[](void* p) noexcept { note_free(p); free(p); }
void operator delete(void* p, size_t) noexcept { note_free(p); free(p); }
void operator delete[](void* p, size_t) noexcept { note_free(p); free(p); }

void* operator new(size_t n, const std::nothrow_t&) noexcept
{
	void*	p = malloc(n ? n : 1);
	if (p)
		note_alloc(p, n);
	return p;
}

void* operator new[](size_t n, const std::nothrow_t& t) noexcept { return operator new(n, t); }
void operator delete(void* p, const std::nothrow_t&) noexcept { note_free(p); free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { note_free(p); free(p); }
