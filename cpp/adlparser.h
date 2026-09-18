/*
 * Aspect Definition Language.
 * An optimised parser, agnostic about its Source and Sink.
 */
#if	!defined(ADLPARSER_H)
#define	ADLPARSER_H

#include	<char_encoding.h>
#include	<error.h>
#include	<fcntl.h>
#include	<pegexp.h>
#include	<strval.h>	// Sources hand the Sink a fragment as a StrVal

#include	<stdio.h>	// Only used for the stub Source and Sink

/*
 * Error numbers for ADL, shared by the Parser and the Store/Sink layer. See
 * strval.h's STRERR_* definitions and error.h's ErrNum for the scheme these
 * follow: a 16-bit message-set number (allocated to this subsystem) plus a
 * message code within that set. A default-constructed/zero ErrNum means
 * "no error".
 *
 * The Parser and the Store/Sink layer share them, and adlstore.h includes
 * this header, so every consumer sees the whole set. The Parser's own
 * grammar diagnostics carry ADLERR_SYNTAX.
 */
#define	ADLERR_SET			1024
#define	ADLERR_TOP_NAME			ErrNum(ADLERR_SET, 1)	// The outermost object must be named TOP
#define	ADLERR_TOP_SUPER		ErrNum(ADLERR_SET, 2)	// TOP's supertype, if given, must be Object
#define	ADLERR_NO_PARENT		ErrNum(ADLERR_SET, 3)	// A child was skipped because its parent is missing
#define	ADLERR_PARENT_NOT_FOUND		ErrNum(ADLERR_SET, 4)	// A name on the way to the parent wasn't found
#define	ADLERR_SUPERTYPE_NOT_FOUND	ErrNum(ADLERR_SET, 5)	// The named supertype wasn't found
#define	ADLERR_SUPERTYPE_CHANGED	ErrNum(ADLERR_SET, 6)	// Re-opening an object may not change its supertype
#define	ADLERR_REOPEN_NOT_FOUND		ErrNum(ADLERR_SET, 7)	// No supertype and no existing object to reopen
#define	ADLERR_NAME_NOT_FOUND		ErrNum(ADLERR_SET, 8)	// A name in a path could not be found at all
#define	ADLERR_REFERENCE_NOT_FOUND	ErrNum(ADLERR_SET, 9)	// A Reference's target path could not be found
#define	ADLERR_FINAL_VIOLATION		ErrNum(ADLERR_SET, 10)	// An assignment violates an existing final restriction
#define	ADLERR_ALIAS_NOT_FOUND		ErrNum(ADLERR_SET, 11)	// An Alias's target path could not be found
#define	ADLERR_STERILE_SUPERTYPE	ErrNum(ADLERR_SET, 12)	// Is Sterile forbids a new subtype of this object
#define	ADLERR_COMPLETE_PARENT		ErrNum(ADLERR_SET, 13)	// Is Complete forbids new content in this object
#define	ADLERR_SYNTAX			ErrNum(ADLERR_SET, 14)	// The input doesn't match the ADL grammar

class	ADLSourceUTF8Ptr
{
	const UTF8*	data;
	int		peeked_bytes;
	int		_line_number;
	int		_column;

public:
	ADLSourceUTF8Ptr()
		: data(""), peeked_bytes(0), _line_number(1), _column(1) {}
	ADLSourceUTF8Ptr(const UTF8* _data)
		: data(_data), peeked_bytes(0), _line_number(1), _column(1) {}
	ADLSourceUTF8Ptr(const ADLSourceUTF8Ptr& c)
		: data(c.data), peeked_bytes(0), _line_number(c._line_number), _column(c._column) {}
	UCS4	peek_char()
		{
			const UTF8*	tp = data;
			UCS4		ch = UTF8Get(tp);
			peeked_bytes = tp-data;
			return ch != 0 ? ch : UCS4_NONE;
		}
	void	advance()
		{
			if (peeked_bytes == 0)
				return;
			if (*data == '\n')
				_column = 1, _line_number++;
			else
				_column++;
			data += peeked_bytes;
			peeked_bytes = 0;
		}
	off_t	bytes_from(const ADLSourceUTF8Ptr& start) const
		{ return data - start.data; }		// How far we have come, for progress
							// No chars_from(): this Source does not track
							// characters, and nothing generic needs them.
	int	line_number() const
		{ return _line_number; }
	int	column() const
		{ return _column; }
	const char*	peek() const
		{ return data; }
	StrVal	fragment(const ADLSourceUTF8Ptr& end) const	// From here to `end`
		{	// A fresh string: this Source does not own its bytes, so nothing can be shared
			return StrVal(peek(), (int)(end.bytes_from(*this)));
		}
	static ADLSourceUTF8Ptr	over(StrVal& s)		// A Source seeing the whole of `s`
		{ return ADLSourceUTF8Ptr(s.asUTF8()); }
	void	print_from(const ADLSourceUTF8Ptr& start) const
		{ printf("%.*s", (int)(bytes_from(start)), start.data); }
	void	print_ahead() const
		{ printf("`%.*s`...\n", 20, data); }
};

/*
 * What kind of value is expected here, per the variable being assigned
 * (grammar comment on atomic_value(), below): a Regular Expression
 * variable's value is a pegexp; a Reference variable's value is a
 * path_name or object literal; any other variable's value must match its
 * Syntax, unless it's an array variable, whose value is an array literal -
 * or a single element, which value() wraps. There's no "don't know, try
 * anything" case: a Sink that can't
 * determine the variable's type (e.g. because it failed to resolve at
 * all) has nothing valid to fall back to either, so it should report
 * whichever of these is closest to correct and let that kind's own
 * parsing fail normally, rather than accepting a value no real check was
 * ever run against.
 */
enum ValueExpectation { ExpectMatch, ExpectReference, ExpectRegexp, ExpectArray };

/*
 * This is an API stub. During parsing, these methods get called.
 * If Syntax lookup is required, you need to save enough data to implement it.
 */
template<typename _Source = ADLSourceUTF8Ptr>
class ADLSinkStub
{
public:
	using	Source = _Source;

	ADLSinkStub() {}

	ErrNum	error(const char* why, const char* what = 0, const Source& where = Source())
		{
			printf("At line %d:%d, %s MISSING %s: ", where.line_number(), where.column(), why, what);
			where.print_ahead();
			return ADLERR_SYNTAX;
		}

	void	definition_starts() {}			// A declaration just started
	ErrNum	definition_ends() { return ErrNum(); }	// This declaration just ended
	void	ascend() {}				// Go up one scope level to look for a name
	void	name(Source start, Source end) {}	// A name exists between start and end
	void	descend() {}				// Go down one level from the last name
	void	pathname(bool ok) {}			// The sequence *ascend name *(descend name) is complete
	void	object_name() {}			// The last pathname was for a new object
	ErrNum	supertype() { return ErrNum(); }	// Last pathname was a supertype
	ErrNum	reference_type(bool is_multi) { return ErrNum(); }	// Last pathname was a reference
	void	reference_done(bool ok) {}		// Reference completed
	ErrNum	alias() { return ErrNum(); }		// Last pathname is an alias
	ErrNum	block_start() { return ErrNum(); }	// enter the block given by the pathname and supertype
	void	block_end() {}				// exit the block given by the pathname and supertype
	ErrNum	is_array() { return ErrNum(); }		// This definition is an array
	ErrNum	assignment_starts(bool is_final) { return ErrNum(); }	// '=' or '~=' just seen; about to parse its value
	ErrNum	assignment(bool is_final) { return ErrNum(); }	// The value(s) are assigned to the current definition
	void	string_literal(Source start, Source end) {}	// Contents of a string between start and end
	void	numeric_literal(Source start, Source end) {}	// Contents of a number between start and end
	void	matched_literal(Source start, Source end) {}	// Contents of a matched value between start and end
	void	object_literal_starts() {}		// ':' seen for an object-literal value
	ErrNum	object_literal_ends() { return ErrNum(); }	// supertype/?block/?assignment for the literal are complete
	void	reference_literal() {}			// The last pathname is a value to assign to a reference variable
	void	syntax_copy() {}			// The last pathname is a value to assign to a Regular-Expression variable
	void	pegexp_literal(Source start, Source end) {}	// Contents of a pegexp between start and end
	void	array_value_start() {}			// '[' seen; an array of values follows
	void	array_value_element() {}		// One element's literal was just reported above
	void	array_value_end() {}			// ']' seen; the reported elements are now the whole value

	Source	lookup_syntax(Source type)		// Return Source of a Pegexp string to use in matching
		{ return Source(); }
	ValueExpectation	expected_value_kind(Source type)	// What kind of value does the variable being assigned expect?
		{ return ExpectMatch; }			// No object model here to consult; this always fails
						// cleanly (lookup_syntax() above always returns empty), which is
						// the right outcome with no real check to run - see adl_scan.cpp
};

template<
	typename _Sink = ADLSinkStub<>
>
class	ADLParser
{
public:
	using		Sink = _Sink;
	using		Source = typename Sink::Source;
	typedef	Source	Type;

	~ADLParser() {}
	ADLParser(Sink& s): sink(s) {}

	bool	parse(Source&);			// ?BOM *definition

	// Count how many errors this Parser has seen
	unsigned	total_errors() const		{ return error_count; }

	void	error(const char* why, const char* what, const Source& where)
		{ record_error(sink.error(why, what, where)); }

protected:
	bool	definition(Source&);		// &. !'}' ?path_name body ?';'
	bool	path_name(Source&);		// *'.' ?(name *('.' name))
	bool	name(Source&);			// | symbol | integer
	bool	body(Source&);			// | reference | alias_from | ?supertype block |?supertype ?block ?post_body EOB
	bool	EOB(Source&);			// |&';' |&'}' |EOF
	bool	reference(Source&);		// (| '->' | '=>') path_name ?block ?assignment EOB
	bool	alias_from(Source&);		// '!' path_name EOB
	bool	supertype(Source&);		// ':' ?path_name
	bool	block(Source&);			// '{' *definition '}'
	bool	post_body(Source&, Type&);	// | '[]' ?assignment | assignment
	bool	assignment(Source&, Type&);	// | final_assignment | tentative_assignment
	bool	final_assignment(Source&, Type&);	// '=' value
	bool	tentative_assignment(Source&, Type&);	// '~=' value
	bool	value(Source&, Type&);		// | atomic_value | '[' atomic_value *(',' atomic_value) ']'
	bool	array_value(Source& source, Type&); // '[' atomic_value *(',' atomic_value) ']'
	bool	atomic_value(Source&, Type&);	// | '/' pegexp_sequence '/' | path_name | object_literal | matched_literal
	bool	reference_literal(Source&, Type&);	// pathname
	bool	syntax_copy(Source&, Type&);	// pathname, for a Regular-Expression variable: copy another object's Syntax
	bool	object_literal(Source&);	// supertype ?block ?assignment
	bool	matched_literal(Source&, Type&);
	void	recover_to_boundary(Source&);	// Skip to next ';'/'}' after a bad value, honoring quoting/escaping
	bool	space(Source&);			// Optional white-space
	// White-space is free above here, explicit below
	bool	symbol(Source&);		// [_\a] *[_\w]
	bool	integer(Source&);		// [1-9] *[0-9]
	bool	pegexp_literal(Source& source);		// '/' pegexp_sequence '/'
	bool	pegexp_sequence(Source&);	// | pegexp_atom | +('|' pegexp_atom)
	bool	pegexp_atom(Source&);		// ?[*+?] (| pegexp_char | pegexp_class | pegexp_group)
	bool	pegexp_group(Source&);		// '(' pegexp_sequence ')'
	bool	pegexp_lookahead(Source&);	// [&!] pegexp_atom
	bool	pegexp_char(Source&);		// | '\\[adhsw]' | '\\' ?[0-3] [0-7] ?[0-7] | '\\x' \h ?\h | '\\u' ?[0-1] \h ?\h ?\h ?\h
						// | '\\' [pP] '{' +[A-Za-z_] '}' | '\\' [0befntr\\*+?()|/\[] | [^*+?()|/\[ ]
	bool	pegexp_class(Source&);		// '[' ?'^' ?'-' +pegexp_class_part ']'
	bool	pegexp_class_part(Source&);	// !']' pegexp_class_char ?('-' !']' pegexp_class_char)
	bool	pegexp_class_char(Source&);	// | !'-' pegexp_char | [*+?()|/]

	// Bootstrap for values:
	bool	string_literal(Source&);
	bool	numeric_literal(Source&);

	// If ErrNum is non-zero, increment the error count
	void	record_error(ErrNum err)	{ if (err) error_count++; }	// ErrNum's
						// int conversion makes `if (err)` the only
						// unambiguous test: err != 0 matches both it and
						// error.h's operator!=(int)

	Sink&		sink;
	unsigned	error_count = 0;
};

// ?BOM *definition
template<typename Source> bool ADLParser<Source>::parse(Source& source)
{
	Source	probe = source;

	if (probe.peek_char() == 0xFEFF)	// ignore a Byte Order Mark
		probe.advance();
	space(probe);
	while (definition(probe))
		;
	// printf("PARSE ends at '%d': ", probe.peek_char()); probe.print_ahead();
	source = probe;
	return true;
}

// &. !'}' ?path_name body ?';'
template<typename Source> bool ADLParser<Source>::definition(Source& source)
{
	Source	probe = source;

	UCS4	ch = probe.peek_char();
	if (UCS4_NONE == ch || '}' == ch)	// EOF or closing }
		return false;			// I see no definition here

	sink.definition_starts();
	Source	name_start(probe);
	bool	has_path = path_name(probe);	// Accept a path_name
	sink.object_name();

	// printf("DEFINING `"); probe.print_from(name_start); printf("`\n");

	if (!body(probe))
		return false;
	// printf("DEFINITION ends `"); probe.print_from(p); printf("`\n");

	ch = probe.peek_char();
	if (';' == ch)
	{
		probe.advance();
		space(probe);
	}

	record_error(sink.definition_ends());

	// There was a body, so advance:
	source = probe;
	return true;
}

// &(|'.' |name) *'.' ?(name *('.' name))
template<typename Source> bool ADLParser<Source>::path_name(Source& source)
{
	Source	probe = source;
	bool	ok = false;

	// Ascend one scope level for each .
	while ('.' == probe.peek_char())
	{
		ok = true;
		probe.advance();
		sink.ascend();
		space(probe);
	}

	if (name(probe))
	{
		ok = true;
		space(probe);
		source = probe;			// Already succeeded, get more if we can

		while ('.' == probe.peek_char()) // See if we can descend
		{
			probe.advance();
			sink.descend();
			space(probe);
			source = probe;		// The dot is consumed either way - even a
						// trailing dot (README "Contextual Extension":
						// a traversal *ending* with a dot) is real syntax,
						// not leftover input for body() to trip over
			if (!name(probe))
			{
				sink.pathname(true);
				return true;	// Trailing dot: sink.descend() already fired
						// with no name() to follow it - current_path.sep
						// is left as "." for the Sink to notice
			}
			space(probe);
			source = probe;		// Descent succeeded, try for more
		}
	}
	if (ok)
		source = probe;
	sink.pathname(ok);
	return ok;
}

// | symbol | integer
template<typename Source> bool ADLParser<Source>::name(Source& source)
{
	Source	probe = source;

	bool	ok = false;
	while (true)
	{
		Source	start = probe;
		if (!symbol(probe))
		{
			if (!integer(probe))
				return ok;
		}
		ok = true;
		sink.name(start, probe);
		space(probe);
		source = probe;
	}
}

// | reference | alias_from | ?supertype block |?supertype ?block ?post_body EOB
template<typename Source> bool ADLParser<Source>::body(Source& source)
{
	if (reference(source))
		return true;
	if (alias_from(source))
		return true;

	Source	probe(source);
	Type	type(probe);		// Use Syntax for this type
	bool	has_supertype = supertype(probe);
	bool	has_block = block(probe);
	bool	has_post_body = post_body(probe, type);

	if (!has_block && !EOB(probe))	// If there's no block, the body must be properly terminated
		return false;

	source = probe;
	return true;
}

// |';' |'}' |EOF
template<typename Source> bool ADLParser<Source>::EOB(Source& source)
{
	UCS4	ch = source.peek_char();

	return ';' == ch || '}' == ch || UCS4_NONE == ch;
}

// (| '->' | '=>') path_name ?block ?assignment EOB
template<typename Source> bool ADLParser<Source>::reference(Source& source)
{
	Source	probe(source);
	UCS4	ch;

	// Look for the reference symbol (-> or =>):
	ch = probe.peek_char();
	if ('-' != ch && '=' != ch)
		return false;
	probe.advance();
	if ('>' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	// We must find a path_name:
	Type	type(probe);
	if (!path_name(probe))
	{
		error("reference", "typename", probe);
		return false;
	}
	record_error(sink.reference_type(ch == '='));

	bool	has_block = block(probe);
	bool	has_assignment = assignment(probe, type);

	bool	ok = EOB(probe);
	sink.reference_done(ok);
	if (ok)
		source = probe;
	return ok;
}

// '!' path_name EOB
template<typename Source> bool ADLParser<Source>::alias_from(Source& source)
{
	Source	probe(source);

	if ('!' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	if (!path_name(probe))
		return false;

	bool	ok = EOB(probe);
	if (ok)
	{
		record_error(sink.alias());
		source = probe;
	}
	return ok;
}

// ':' ?path_name
template<typename Source> bool ADLParser<Source>::supertype(Source& source)
{
	Source	probe(source);

	if (':' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	Source	start(probe);
	bool	has_path_name = path_name(probe);
	record_error(sink.supertype());
	// printf("Found supertype path_name `"); probe.print_from(start); printf("`\n");
	space(probe);

	source = probe;
	return true;
}

// '{' *definition '}'
template<typename Source> bool ADLParser<Source>::block(Source& source)
{
	Source	probe(source);

	// Must start with {
	if ('{' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	record_error(sink.block_start());

	// Zero or more definitions:
	while (definition(probe))
		;

	// Must end with }
	if ('}' != probe.peek_char())
	{
		error("block", "closing }", probe);
		return false;
	}
	probe.advance();
	sink.block_end();
	space(probe);

	source = probe;
	return true;
}

// | '[]' ?assignment | assignment
template<typename Source> bool ADLParser<Source>::post_body(Source& source, Type& type)
{
	Source	probe(source);

	bool	is_array = false;
	bool	has_assignment;
	if ('[' == probe.peek_char())
	{
		probe.advance();

		if (']' != probe.peek_char())
		{
			error("array_indicator", "closing ]", probe);
			return false;	// '[' with no ']'
		}
		probe.advance();
		record_error(sink.is_array());
		space(probe);
		is_array = true;
	}

	has_assignment = assignment(probe, type);
	if (!is_array && !has_assignment)
		return false;

accept:	source = probe;
	return true;
}

// | final_assignment | tentative_assignment
template<typename Source> bool ADLParser<Source>::assignment(Source& source, Type& type)
{
	return final_assignment(source, type)
	    || tentative_assignment(source, type);
}

// '=' value
template<typename Source> bool ADLParser<Source>::final_assignment(Source& source, Type& type)
{
	Source	probe(source);
	if ('=' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	record_error(sink.assignment_starts(true));
	bool	has_value = value(probe, type);
	if (!has_value)
	{
		error("final_assignment", "value", probe);
		return false;	// Assignment must have a value
	}

	record_error(sink.assignment(true));
	space(probe);
	source = probe; 
	return true;
}

// '~=' value
template<typename Source> bool ADLParser<Source>::tentative_assignment(Source& source, Type& type)
{
	Source	probe(source);
	if ('~' != probe.peek_char())
		return false;
	probe.advance();

	if ('=' != probe.peek_char())
	{
		error("tentative_assignment", "= after ~", probe);
		return false;	// '~' with no '='
	}
	probe.advance();
	space(probe);

	record_error(sink.assignment_starts(false));
	bool	has_value = value(probe, type);
	if (!has_value)
		return false;	// Assignment must have a value

	record_error(sink.assignment(false));
	space(probe);
	source = probe; 
	return true;
}

// | array_value | atomic_value
template<typename Source> bool ADLParser<Source>::value(Source& source, Type& type)
{
	return atomic_value(source, type)
	    || array_value(source, type);
}

// '[' atomic_value *(',' atomic_value) ']'
template<typename Source> bool ADLParser<Source>::array_value(Source& source, Type& type)
{
	Source	probe(source);

	if ('[' != probe.peek_char())
		return false;
	probe.advance();

	space(probe);
	sink.array_value_start();

	UCS4	ch;
	while (true)
	{
		if (!atomic_value(probe, type))
			return false;
		sink.array_value_element();

		ch = probe.peek_char();		// Save ch to avoid peeking again for ']'
		if (',' != ch)
			break;
		probe.advance();
		space(probe);
	}
	if (']' != ch)
		return false;
	probe.advance();
	sink.array_value_end();
	space(probe);
	source = probe;
	return true;
}

/*
 * | '/' pegexp_sequence '/' | path_name | object_literal | matched_literal
 *
 * Which of these is acceptable is determined by the variable being
 * assigned, not tried in some fixed order regardless of type: a Regular
 * Expression variable's value is a pegexp, or a path_name naming another
 * object whose own effective Syntax is copied onto this one (README
 * "Copying a Syntax"); a Reference variable's value is a path_name or
 * object literal; any other variable's value must match its Syntax
 * (matched_literal) - see ValueExpectation. Each variable kind still has
 * only its own fixed pair (or singleton) of acceptable forms, tried in a
 * fixed order for that kind alone: there's no "try every kind in turn"
 * fallback, so an unrecognized or wrongly-typed value is still rejected
 * outright, never silently reinterpreted as some other kind.
 */
template<typename Source> bool ADLParser<Source>::atomic_value(Source& source, Type& type)
{
	Source	probe(source);

	ValueExpectation	kind = sink.expected_value_kind(type);

	switch (kind)
	{
	case ExpectRegexp:
		if (!pegexp_literal(probe) && !syntax_copy(probe, type))
			return false;
		break;

	case ExpectReference:
		if (!reference_literal(probe, type) && !object_literal(probe))
			return false;
		break;

	case ExpectArray:		// An array variable: its Syntax is the element type's,
					// so a lone element is matched in the case below
		// If we aren't looking at [, this is not an array literal,
		// but we might accept a single value of the correct type.
		if (probe.peek_char() == '[')
			return false;
		// fall through
	case ExpectMatch:
	default:
	{
		Source	syntax = sink.lookup_syntax(type);
		if (!matched_literal(probe, syntax))
			return false;
		break;
	}
	}

	source = probe;
	return true;
}

template<typename Source> bool ADLParser<Source>::reference_literal(Source& source, Type& type)
{
	bool	ok = path_name(source);
	if (ok)
		sink.reference_literal();
	return ok;
}

// pathname, for a Regular-Expression variable: copy another object's own Syntax
template<typename Source> bool ADLParser<Source>::syntax_copy(Source& source, Type& type)
{
	bool	ok = path_name(source);
	if (ok)
		sink.syntax_copy();
	return ok;
}

// supertype ?block ?assignment
template<typename Source> bool ADLParser<Source>::object_literal(Source& source)
{
	if (':' != source.peek_char())		// Not a literal; let atomic_value() try something else
		return false;

	/*
	 * Start a new Frame for the anonymous object before supertype()/block()
	 * run, so their sink calls land on it instead of the enclosing value's
	 * Frame (which they used to corrupt - an object literal is not itself
	 * a named definition, but supertype()/block_start() are shared with
	 * that code path and need a Frame of their own to operate on).
	 */
	sink.object_literal_starts();

	Type	type(source);
	supertype(source);			// Always succeeds now that we've seen ':'
	bool	has_block = block(source);
	bool	has_assignment = assignment(source, type);
	record_error(sink.object_literal_ends());	// Create the object (if not already), and pop its Frame
	return true;
}

// Value matches the Type syntax of the variable being assigned
template<typename Source> bool ADLParser<Source>::matched_literal(Source& source, Type& type)
{
	if (type.peek_char() != UCS4_NONE)	// A Syntax was resolved for this variable; use it to match
	{
		using	MatchContext = PegexpDefaultContext<>;

		MatchContext		context;
		Pegexp<MatchContext>	pegexp(type.peek());		// 8-bit pegexp pattern text, no delimiters
		PegexpDefaultSource	psource(source.peek());	// Wraps the same underlying bytes as source

		auto	match = pegexp.match_here(psource, &context);
		if (match.is_failure())
		{
			/*
			 * The Syntax is known and the value doesn't match it at all. This
			 * used to be silently treated as an empty value (assignment_starts()
			 * already created the Assignment slot via begin_assign() before
			 * the value was even attempted, and there was no recovery, so a
			 * bare `return false` here either left that slot empty with no
			 * error - see cpp/ToDo's Bugs section item 8 - or, if reached
			 * from inside a reopened block, corrupted that block's own
			 * closing-brace search and cascaded outward through every
			 * enclosing block). Report a real error, then recover by
			 * skipping to the next statement/block boundary so a single bad
			 * value doesn't take the rest of the file down with it.
			 */
			error("Value doesn't match its declared Syntax", "a valid literal", source);
			Source	start(source);
			recover_to_boundary(source);
			sink.matched_literal(start, start);	// Record an empty value, having reported why
			return true;				// Recovered - let the caller finish this assignment normally
		}

		off_t	consumed = match.to.source.bytes_from(match.from.source);
		Source	start(source);
		while (source.bytes_from(start) < consumed)
		{
			source.peek_char();
			source.advance();
		}
		sink.matched_literal(start, source);
		return true;
	}

	/*
	 * No Syntax could be determined (e.g. while bootstrapping String/Integer/etc themselves,
	 * before any Syntax exists to describe them). Fall back to hard-coded literal forms:
	 */
	UCS4	ch = source.peek_char();
	if ('\'' == ch)
		return string_literal(source);
	if ('0' <= ch && ch <= '9' || '-' == ch || '+' == ch)
		return numeric_literal(source);

	return false;
}

// Skip to the next ';' or '}' (or EOF) after a bad value, honoring backslash-escaping and quote-nesting
// so a stray ';'/'}' inside the bad value's own text doesn't end the skip prematurely.
template<typename Source> void ADLParser<Source>::recover_to_boundary(Source& source)
{
	bool	in_quote = false;
	UCS4	ch;
	while ((ch = source.peek_char()) != UCS4_NONE
	       && (in_quote || (ch != ';' && ch != '}')))
	{
		source.advance();
		if (ch == '\\')
		{
			if (source.peek_char() != UCS4_NONE)
				source.advance();
		}
		else if (ch == '\'')
			in_quote = !in_quote;
	}
}

// Value matches the Type syntax for a string
template<typename Source> bool ADLParser<Source>::string_literal(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('\'' != ch)
		return false;
	probe.advance();

	Source	start(probe);
	while ((ch = probe.peek_char()) != UCS4_NONE && '\'' != ch)
	{
		probe.advance();
		if (ch == '\\')
			probe.advance(), ch = probe.peek_char();
	}
	if (UCS4_NONE == ch)
	{
		error("string_literal", "closing '", probe);
		return false;
	}
	sink.string_literal(start, probe);
	probe.advance();
	source = probe;
	return true;
}

// Value matches the Type syntax for a number
template<typename Source> bool ADLParser<Source>::numeric_literal(Source& source)
{
	Source	probe(source);
	UCS4	ch;

	while ((ch = probe.peek_char()) && ('0' <= ch && ch <= '9' || '-' == ch || '+' == ch || ch == '.'))
		probe.advance();
	bool	ok = probe.bytes_from(source) > 0;	// Anything consumed? A numeric
							// token is ASCII, so that is its length too.
	if (ok)
		sink.numeric_literal(source, probe);
	source = probe;
	return ok;
}

// Optional white-space: *(| +[ \t\n\r] | '//' *(!'\n' .))
template<typename Source> bool ADLParser<Source>::space(Source& source)
{
	Source	probe(source);
	UCS4	ch;

	while ((ch = probe.peek_char()) != UCS4_NONE)	// EOF
	{
		source = probe;	// Accept the current situation
		if (' ' == ch || '\t' == ch || '\n' == ch || '\r' == ch)
		{
			probe.advance();
			source = probe;
			continue;
		}
		if ('/' == ch)
		{
			probe.advance();
			if ('/' != probe.peek_char())
				break;
			probe.advance();
			while ((ch = probe.peek_char()) != UCS4_NONE)	// EOF
			{
				probe.advance();
				source = probe;
				if ('\n' == ch)
					break;
			}
			continue;
		}
		break;	// Not white-space
	}
	return true;
}

// White-space is free above here (not handled yet)

// [_\a] *[_\w]
template<typename Source> bool ADLParser<Source>::symbol(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('_' != ch && !UCS4IsAlphabetic(ch))
		return false;
	probe.advance();

	while ('_' == (ch = probe.peek_char())
	    || (UCS4IsAlphabetic(ch) || UCS4IsDecimal(ch)))
	    	probe.advance();
	source = probe;
	return true;
}

// [1-9] *[0-9]
template<typename Source> bool ADLParser<Source>::integer(Source& source)
{
	Source	probe(source);

	if (ASCIIDigit(probe.peek_char()) < 1)
		return false;
	probe.advance();
	while (ASCIIDigit(probe.peek_char()) >= 0)
		probe.advance();
	source = probe;
	return true;
}

// '/' pegexp_sequence '/'
template<typename Source> bool ADLParser<Source>::pegexp_literal(Source& source)
{
	Source	probe(source);

	if ('/' != probe.peek_char())
		return false;
	probe.advance();
	Source	start(probe);

	if (!pegexp_sequence(probe))
		return false;

	if ('/' != probe.peek_char())
	{
		error("Pegexp", "closing /", probe);
		return false;
	}
	sink.pegexp_literal(start, probe);
	probe.advance();

	source = probe;
	return true;
}

// | +('|' +pegexp_atom) | *pegexp_atom
template<typename Source> bool ADLParser<Source>::pegexp_sequence(Source& source)
{
	Source	probe(source);
	if ('|' == probe.peek_char())
	{
		while ('|' == probe.peek_char())
		{
			probe.advance();
			bool	ok = false;

			while (pegexp_atom(probe))
				ok = true;
			if (!ok)
			{
				error("pegexp_sequence", "atom", probe);
				return false;
			}
		}
accept:		source = probe;
		return true;
	}

	while (pegexp_atom(probe))
		;
	goto accept;
}

// ?[*+?] (| pegexp_lookahead | pegexp_char | pegexp_class | pegexp_group)
template<typename Source> bool ADLParser<Source>::pegexp_atom(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	// Check for repetition operator
	if ('*' == ch || '+' == ch || '?' == ch)
		probe.advance();

	if (pegexp_lookahead(probe))
	{
accept:
		source = probe;
		return true;
	}

	if (pegexp_char(probe))
		goto accept;
	if (pegexp_class(probe))
		goto accept;
	if (pegexp_group(probe))
		goto accept;

	return false;
}

// '(' pegexp_sequence ')'
template<typename Source> bool ADLParser<Source>::pegexp_group(Source& source)
{
	Source	probe(source);

	if ('(' != probe.peek_char())
		return false;
	probe.advance();

	if (!pegexp_sequence(probe))
	{
		error("pegexp_group", "sequence", probe);
		return false;
	}
	
	if (')' != probe.peek_char())
	{
		error("pegexp_group", "closing )", probe);
		return false;
	}
	probe.advance();
	source = probe;
	return true;
}

// [&!] pegexp_atom
template<typename Source> bool ADLParser<Source>::pegexp_lookahead(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('&' != ch && '!' != ch)
		return false;
	probe.advance();
	if (!pegexp_atom(probe))
		return false;
	source = probe;
	return true;
}

// | '\\[adhswLU]'			// alpha, digit, hexadecimal, whitespace, word (alpha or digit), Lowercase, Uppercase
// | '\\' ?[0-3] [0-7] ?[0-7]		// Octal character
// | '\\x' \h ?\h			// hex character, 1 or 2 digits
// | '\\x{' +\h '}'			// hex character \x{...}, arbitrary precision
// | '\\u' \h ?\h ?\h ?\h		// Unicode character 1..4 digits
// | '\\u{' +\h '}'			// Unicode character \u{...}, arbitrary precision
// | '\\' [pP] '{' +[A-Za-z_] '}'	// Unicode named property
// | '\\' [.0befntr\\*+?()|/\[]		// Special escapes
// | [^*+?()|/\[\0- ]	  		// Other non-ctl chars except pegexp operator initiators (but allow . operator)
template<typename Source> bool ADLParser<Source>::pegexp_char(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('\\' == ch)
	{
		probe.advance();
		ch = probe.peek_char();
		if (UCS4IsASCII(ch))
		{
			if (0 != strchr("adhswLU", (char)ch))
			{			// Character property is ok
				probe.advance();
		accept:		source = probe;
				return true;
			}
			if (ch >= '0' && ch <= '7')
			{			// Start of octal char
				bool	zero_to_three = ch <= '3';
				probe.advance();
				ch = probe.peek_char();
				if (ch >= '0' && ch <= '7')
				{
					probe.advance();
					if (!zero_to_three)
						goto accept;
					// 3rd digit is accepted only if the first was 0..3
					ch = probe.peek_char();
					if (ch >= '0' && ch <= '7')
						probe.advance();
				}
				goto accept;
			}
			if ('x' == ch || 'u' == ch)
			{
				bool	is_hex = 'x' == ch;
				probe.advance();
				ch = probe.peek_char();
				bool	has_curly = '{' == ch;
				if (has_curly)
					probe.advance(), ch = probe.peek_char();
				if (UCS4HexDigit(ch) < 0)
					return false;
				const	int	max = is_hex ? (has_curly ? 8 : 2) : (has_curly ? 8 : 4);
				for (int i = 1; i < max; i++)
				{
					probe.advance(), ch = probe.peek_char();
					if (UCS4HexDigit(ch) < 0)
						break;
				}
				if (has_curly && '}' != ch)
					return false;	// Missing closing curly
				goto accept;
			}
			if ('p' == ch || 'P' == ch)
			{			// char property name '\\' [pP] '{' +[A-Za-z_] '}'
				probe.advance();
				if ('{' != probe.peek_char())
					return false;
				probe.advance();
				ch = probe.peek_char();
				bool	got_one = false;
				while (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || '_' == ch)
				{
					got_one = true;
					probe.advance();
					ch = probe.peek_char();
				}
				if (!got_one || '}' != ch)
					return false;
				probe.advance();
				goto accept;
			}
			if (0 != strchr(".0befntr\\*+?()|/[", (char)ch))
			{			// Special escape
				probe.advance();
				goto accept;
			}
		}
		// Unrecognised escape after backslash
		return false;
	}

	// No EOF, control characters, whitespace, or other unescaped special characters:
	if (ch == UCS4_NONE || ch <= ' ' || (UCS4IsASCII(ch) && 0 != strchr("*+?()|/\\[", ch)))
		return false;	// These chars are not allowed unescaped
	probe.advance();
	source = probe;
	return true;
}

// '[' ?'^' ?'-' +pegexp_class_part ']'
template<typename Source> bool ADLParser<Source>::pegexp_class(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('[' != ch)
		return false;
	probe.advance(), ch = probe.peek_char();

	if ('^' == ch)
		probe.advance(), ch = probe.peek_char();
	if ('-' == ch)	// a hyphen must be first
		probe.advance(), ch = probe.peek_char();
	if (!pegexp_class_part(probe))
	{
		error("pegexp_class", "valid class", probe);
		return false;
	}
	while (pegexp_class_part(probe))
		;
	if (']' != probe.peek_char())
	{
		error("pegexp_class", "]", probe);
		return false;
	}
	probe.advance();
	source = probe;
	return true;
}

// !']' pegexp_class_char ?('-' !']' pegexp_class_char)
template<typename Source> bool ADLParser<Source>::pegexp_class_part(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if (']' == ch)
		return false;
	if (!pegexp_class_char(probe))
	{
		error("pegexp_class_part", "valid class character", probe);
		return false;
	}
	ch = probe.peek_char();
	if ('-' == ch)
	{		// Character range
		probe.advance(), ch = probe.peek_char();
		if (']' == ch)
			return false;
		if (!pegexp_class_char(probe))
			return false;
	}
	source = probe;
	return true;
}

// | !'-' pegexp_char | [*+?()|/]
template<typename Source> bool ADLParser<Source>::pegexp_class_char(Source& source)
{
	Source	probe(source);

	UCS4	ch = probe.peek_char();

	if ('-' != ch && pegexp_char(probe))
	{
		source = probe;
		return true;
	}
	if (!UCS4IsASCII(ch) || 0 == strchr("*+?()|/", (char)ch))	// These chars are allowed in classes but not pegexps
		return false;
	probe.advance();
	source = probe;
	return true;
}

#endif	// ADLPARSER_H
