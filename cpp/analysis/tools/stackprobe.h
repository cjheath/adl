/*
 * Stack high-water probe. NOT part of any library - linked in beside a
 * driver, and its sample() also called from the parser's recursion point
 * (see instrument.py, which inserts that one line into a copy).
 *
 * Samples the stack pointer and reports the deepest (lowest) address seen
 * against the thread's stack base. That is a sampled high-water mark, not a
 * static worst case: real usage could be marginally deeper between samples.
 */
#ifndef STACKPROBE_H
#define STACKPROBE_H
#include	<cstdio>
#include	<cstdlib>
#include	<cstdint>
#include	<pthread.h>

namespace stackprobe
{
inline uintptr_t&	base()		{ static uintptr_t b = 0; return b; }
inline uintptr_t&	low()		{ static uintptr_t m = ~(uintptr_t)0; return m; }
inline uintptr_t&	start()		{ static uintptr_t s = 0; return s; }
inline unsigned long&	samples()	{ static unsigned long n = 0; return n; }

/*
 * Called by the driver as the first thing main() does. Everything the process
 * spent before that - the loader, main's own caller, and the environment block
 * - is not the parser's cost, and varies with how the process was launched, so
 * measuring from the stack base would put a constant offset on every figure
 * that had nothing to do with what is being compared.
 */
inline void	mark_start()
	{ start() = reinterpret_cast<uintptr_t>(__builtin_frame_address(0)); }

inline void	sample(void* p)
{
	uintptr_t	a = reinterpret_cast<uintptr_t>(p);
	samples()++;
	if (a < low())
		low() = a;
}

inline void	report()
{
	uintptr_t	b = start() ? start() : base();
	uintptr_t	l = low();
	printf("STACK used=%llu bytes (samples=%lu)\n",
		(unsigned long long)(b > l ? b - l : 0), samples());
}

struct Installer
{
	Installer()
	{
		base() = reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(pthread_self()));
		atexit(report);
	}
};
static Installer	_installer;
}
#endif
