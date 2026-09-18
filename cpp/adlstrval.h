#if	!defined(ADLSTRVAL_H)
#define	ADLSTRVAL_H

#include	<adlparser.h>
#include	<adlstore.h>

/*
 * A Source that views a StrVal, so that every fragment the Sink keeps is a
 * substr() of that one body rather than a fresh allocation.
 *
 * Like ADLSourceUTF8Ptr this is a non-owning view: it is trivially
 * destructible and cheap to copy, which matters because the parser and the
 * Sink pass Sources around by value and error() default-constructs one at
 * every call site. It additionally knows the StrVal it views, which is what
 * lets fragment() slice it; nothing else needs the base.
 *
 * The viewed StrVal must outlive every Source and everything fragment()
 * returns from it. Fragments themselves hold the body alive after that, so
 * the base only has to survive the parse.
 */
class	ADLSourceStrVal
{
	const StrVal*	base;		// The StrVal being viewed; never owned. Null when empty
	const char*	p;		// Byte pointer at the current character
	int		peeked_bytes;
	StrValIndex	char_pos;	// Characters already consumed
	int		_line_number;
	int		_column;

public:
	ADLSourceStrVal()		// An empty Source, viewing nothing
		: base(0), p(""), peeked_bytes(0), char_pos(0)
		, _line_number(1), _column(1) {}
					// View `t` from its beginning. A pointer, so that viewing a
					// temporary StrVal by mistake is a compile error, and non-const
					// because asUTF8() may have to unshare a body that doesn't end
					// where the slice does: the parser reads up to a NUL.
	explicit ADLSourceStrVal(StrVal* t)
		: base(t), peeked_bytes(0), char_pos(0)
		, _line_number(1), _column(1)
		{ p = t->asUTF8(); }
					// A Source seeing the whole of `s`. The caller keeps `s` alive.
	static ADLSourceStrVal	over(StrVal& s) { return ADLSourceStrVal(&s); }
	ADLSourceStrVal(const ADLSourceStrVal& c)
		: base(c.base), p(c.p), peeked_bytes(0), char_pos(c.char_pos)
		, _line_number(c._line_number), _column(c._column) {}

	UCS4	peek_char()
		{
			const UTF8*	tp = (const UTF8*)p;
			UCS4		ch = UTF8Get(tp);
			peeked_bytes = tp-(const UTF8*)p;
			return ch != 0 ? ch : UCS4_NONE;
		}
	void	advance()
		{
			if (peeked_bytes == 0)
				return;
			if (*p == '\n')
				_column = 1, _line_number++;
			else
				_column++;
			p += peeked_bytes;
			char_pos++;
			peeked_bytes = 0;
		}
	StrValIndex	chars_from(const ADLSourceStrVal& start) const	// From `start` to here
		{ return char_pos - start.char_pos; }
	off_t	bytes_from(const ADLSourceStrVal& start) const
		{ return p - start.p; }			// How far we have come, for progress
	int	line_number() const { return _line_number; }
	int	column() const { return _column; }
	const char*	peek() const { return p; }	// Raw and non-copying, for the pegexp path
	StrVal	fragment(const ADLSourceStrVal& end) const	// From here to `end`
		{	// A slice of the base's body, so nothing is copied. It is the
			// CHARACTER distance that indexes a StrVal, never the byte one.
			return base ? base->substr(char_pos, end.chars_from(*this)) : StrVal();
		}
	void	print_from(const ADLSourceStrVal& start) const
		{ StrValIndex b; const char* cp = start.fragment(*this).asUTF8(b); printf("%.*s", (int)b, cp); }
	void	print_ahead() const { printf("`%.*s`...\n", 20, p); }
};

template<typename _Store = ADLStoreStub<>>
using	ADLStrValSink = ADLStoreSink<_Store, ADLSourceStrVal>;

#endif /* ADLSTRVAL_H */
