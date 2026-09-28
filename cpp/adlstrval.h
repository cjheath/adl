#if	!defined(ADLSTRVAL_H)
#define	ADLSTRVAL_H

#include	<adlparser.h>
#include	<adlstore.h>

/*
 * A Source that views a StrVal, so that every fragment the Sink keeps is a
 * substr() of that one body rather than a fresh allocation.
 *
 * `base` is not the whole of the text being parsed: it is the *unconsumed
 * remainder* of it, from the current position to the very end of whatever
 * StrVal was originally pinned. advance() only ever trims characters off
 * its front (base = base.substr(1)), so it always still ends exactly where
 * that StrVal ends. That is what asUTF8() checks to decide whether it must
 * copy the body to add a terminator - so on this base, it never does: the
 * NUL that terminates the whole is already exactly where base ends too.
 *
 * A mid-slice (a fragment that stops short of the body's end, such as a
 * Pegexp pattern found in the middle of a much longer source text) does not
 * have that property, so the one time we're handed one of those (the
 * converting constructor below), asUTF8() is called on it to unshare it in
 * place onto a new, exactly-sized body of its own - paid once, at that
 * point, rather than on every character read from it afterwards.
 */
class	ADLSourceStrVal
{
	StrVal		base;		// The unconsumed remainder, pinning its body; may be empty
	UCS4		peeked_char;	// The character last returned by peek_char()
	bool		peeked;		// Whether peek_char() has been called since the last advance()
	int		_line_number;
	int		_column;
	const char*	_source_name;	// Where this text came from, for a display to name -
					// a filename today, but agnostic to what a future
					// Source might read from (a socket, say)

public:
	ADLSourceStrVal()		// An empty Source, viewing nothing
		: base(), peeked_char(UCS4_NONE), peeked(false)
		, _line_number(1), _column(1), _source_name("") {}
					// View `t` from its beginning. Taken by value, so that a
					// temporary StrVal is fine to view - the copy is what we keep.
					// asUTF8() unshares `t` in place if it's a mid-slice, onto a
					// body of its own that ends exactly where `t` does; only then
					// is `t` assigned to `base`, so the two are never out of step.
	explicit ADLSourceStrVal(StrVal t, const char* source_name = "")
		: peeked_char(UCS4_NONE), peeked(false)
		, _line_number(1), _column(1), _source_name(source_name)
		{ t.asUTF8(); base = t; }
					// A Source seeing the whole of `s`, which it pins a copy of.
	static ADLSourceStrVal	over(StrVal s) { return ADLSourceStrVal(s); }
	ADLSourceStrVal(const ADLSourceStrVal& c)
		: base(c.base), peeked_char(UCS4_NONE), peeked(false)
		, _line_number(c._line_number), _column(c._column), _source_name(c._source_name) {}

	UCS4	peek_char()
		{	// base[0] is the character here: base always starts at "here", and
			// StrVal already knows how to decode its own first character.
			peeked = true;
			UCS4	ch = base[0];
			return peeked_char = (ch != 0 ? ch : UCS4_NONE);
		}
	void	advance()
		{
			if (!peeked)
				return;
			if (peeked_char == '\n')
				_column = 1, _line_number++;
			else
				_column++;
			base = base.substr(1);		// Drop the character just consumed
			peeked = false;
		}
					// Both compare the *remaining* length/size, which shrinks by
					// exactly the amount consumed since `start` - the two Sources
					// still end at the same place, so their difference is exact.
	StrValIndex	chars_from(const ADLSourceStrVal& start) const	// From `start` to here
		{ return start.base.length() - base.length(); }
	off_t	bytes_from(const ADLSourceStrVal& start) const
		{ return start.base.numBytes() - base.numBytes(); }	// How far we have come, for progress
	int	line_number() const { return _line_number; }
	int	column() const { return _column; }
	const char*	source_name() const { return _source_name; }
	const char*	peek() const			// Raw and non-copying, for the pegexp path
		{ StrValIndex bytes; return base.asUTF8(bytes); }
	StrVal	fragment(const ADLSourceStrVal& end) const	// From here to `end`
		{	// A slice of the base's body, so nothing is copied. It is the
			// CHARACTER distance that indexes a StrVal, never the byte one.
			return base.head(end.chars_from(*this));
		}
	// The text between two positions, and the text ahead of one; both answer
	// a StrVal, since no Source prints anything. from() slices the base, so
	// nothing is copied; ahead() is a short copy, because the base continues
	// past the character it stops at.
	StrVal	from(const ADLSourceStrVal& start) const
		{ return start.fragment(*this); }
	StrVal	ahead(int max_bytes) const
		{
			StrValIndex	bytes;
			const char*	cp = base.asUTF8(bytes);
			int		n = 0;
			while (n < max_bytes && cp[n] != '\0')
				n++;
			return StrVal(cp, (StrValIndex)n);
		}
};

template<typename _Store = ADLStoreStub<>>
using	ADLStrValSink = ADLStoreSink<_Store, ADLSourceStrVal>;

#endif /* ADLSTRVAL_H */
