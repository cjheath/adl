/*
 * Store body-count probe. NOT part of any library.
 *
 * Answers "how much sharing actually happened", which the byte figures only
 * imply: walk the finished Store, and for every StrVal found ask which StrBody
 * it references. A store that holds one body per string and a store that holds
 * one body sliced 70 ways can have the same byte count and are not remotely
 * the same thing.
 *
 * This is a HEADER, not a TU, because adlmem.h defines a non-inline function:
 * two TUs including it would collide at link time. instrument.py inserts an
 * include of this file plus two calls into a copy of the driver, so the walk
 * lives in the driver's TU.
 *
 * Needs bodyIdentity(), which only the shadow strval.h has - see shadow.py.
 * The accessor is an addition to the class, not a data member, so the layout
 * is unchanged and the shadow cannot change what it measures.
 */
#ifndef ANALYSIS_BODYWALK_H
#define ANALYSIS_BODYWALK_H
#include	<cstdio>
#include	<cstdlib>

namespace adl_analysis {

const int	MAX_BODIES = 65536;

const void*	bodies[MAX_BODIES];
int		refs[MAX_BODIES];
char		samples[MAX_BODIES][25];
int		n_bodies = 0;
long		n_strvals = 0;
long		n_empty = 0;

const void*	inputs[8];
int		n_inputs = 0;
long		input_slices[8];

/*
 * The StrVal each input file was slurped into, so that fragments can be told
 * from strings the code built. Called once per file by the driver. Only the
 * t2 and later drivers have such a StrVal; earlier ones never made one, and
 * their fragments are copies, so they simply don't call this.
 */
inline void	note_input(StrVal s)
{
	const void*	b = s.bodyIdentity();
	for (int k = 0; k < n_inputs; k++)
		if (inputs[k] == b)
			return;
	if (n_inputs < 8)
		inputs[n_inputs++] = b;
}

inline int	find(const void* b)
{
	for (int i = 0; i < n_bodies; i++)
		if (bodies[i] == b)
			return i;
	return -1;
}

inline void	note(StrVal s)
{
	if (n_bodies >= MAX_BODIES)
		return;
	n_strvals++;
	if (s.length() == 0)
		n_empty++;

	const void*	b = s.bodyIdentity();
	int		i = find(b);
	if (i < 0)
	{
		i = n_bodies++;
		bodies[i] = b;
		refs[i] = 0;
	}
	refs[i]++;

	for (int k = 0; k < n_inputs; k++)
		if (inputs[k] == b)
			input_slices[k]++;

	if (samples[i][0] == '\0' && !s.isEmpty())
	{
		/*
		 * A sample of the first non-empty StrVal seen on this body. Read
		 * through the CONST asUTF8(), which must not unshare: the non-const
		 * one would copy a body whose suffix is elided, inventing a body and
		 * destroying the very sharing being measured.
		 */
		StrValIndex	bytes = 0;
		const char*	cp = s.asUTF8(bytes);
		int		len = (int)(bytes < 24 ? bytes : 24);
		for (int k = 0; k < len; k++)
		{
			char c = cp[k];
			samples[i][k] = (c >= 32 && c < 127) ? c : '.';
		}
		samples[i][len] = '\0';
	}
}

inline void	value(ADL::Value v)
{
	if (v.elements.length() > 0)
	{
		const ADL::Value*	elems = v.elements.asElements();
		for (int i = 0; i < v.elements.length(); i++)
			value(elems[i]);
		return;
	}
	if (v.handle.is_null())
		note(v.string);
}

inline void	object(ADL::Handle h)
{
	note(h.name());
	value(h.value());
	Array<ADL::Handle>&	kids = h.children();
	const ADL::Handle*	elems = kids.asElements();
	for (int i = 0; i < kids.length(); i++)
		object(elems[i]);
}

/*
 * Walk `root` and report. One line for the table, and - only when
 * ADL_BODIES_DETAIL is set - the bodies that hold a single StrVal, which is
 * where any remaining sharing failure shows up.
 */
inline void	bodies_report(ADL::Handle root)
{
	object(root);

	int	shared = 0;
	for (int i = 0; i < n_bodies; i++)
		if (refs[i] > 1)
			shared++;

	printf("BODIES strvals=%ld bodies=%d shared_bodies=%d empty_strvals=%ld",
		n_strvals, n_bodies, shared, n_empty);
	long	on_inputs = 0;
	for (int k = 0; k < n_inputs; k++)
		on_inputs += input_slices[k];
	printf(" on_input=%ld inputs=%d", on_inputs, n_inputs);
	printf("\n");
	for (int k = 0; k < n_inputs; k++)
		printf("BODIES input[%d] slices=%ld\n", k, input_slices[k]);

	if (getenv("ADL_BODIES_DETAIL"))
	{
		int	single_text = 0, single_empty = 0;
		for (int i = 0; i < n_bodies; i++)
			if (refs[i] == 1)
				(samples[i][0] ? single_text : single_empty)++;
		printf("BODIES single-ref bodies: %d with text, %d empty\n",
			single_text, single_empty);
		printf("BODIES single-ref bodies with text:\n");
		for (int i = 0; i < n_bodies; i++)
			if (refs[i] == 1 && samples[i][0])
				printf("   '%s'\n", samples[i]);
	}
}

} // namespace adl_analysis
#endif
