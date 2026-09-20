#if	!defined(ADL_DISPLAY_H)
#define	ADL_DISPLAY_H
/*
 * Showing what a parse reported: the top-level program's job, and the only
 * place in ADL that prints anything. Nothing below the drivers writes to
 * stdout or stderr - a library reports into the thread's error buffer and
 * leaves every decision about presentation to whoever owns the display.
 *
 * This file is NOT generated, and no generated file replaces it: the two
 * generated halves are adl_err.h (the numbers) and adl_msg.h (the reporting
 * functions). This is ADL's own display, and it is deliberately plain -
 * formatting the parameters into the text is not designed yet (see strpp's
 * doc/error.md, "Not implemented yet"), so the default text is printed with
 * its {n} markers and the parameters follow it.
 *
 * By convention a message's last two parameters are the source line and
 * column, and 0 means the reporter did not know where it was; those are
 * printed as a prefix, and each message is retired once it has been shown.
 */
#include	<char_encoding.h>
#include	<errbuf.h>
#include	<stdio.h>
#include	<strval.h>
#include	<variant.h>

inline void
adl_print_parameter(const Variant& p)
{
	switch (p.type())
	{
	case Variant::Integer:	printf("%d", p.as_int()); return;
	case Variant::Long:	printf("%ld", p.as_long()); return;
	case Variant::LongLong:	printf("%lld", p.as_longlong()); return;
	case Variant::String:
		{
			// The const overload: asUTF8() without a length may unshare and copy,
			// and a parameter can be a slice whose body continues past its end.
			StrValIndex	bytes = 0;
			const char*	cp = p.as_strval().asUTF8(bytes);
			printf("%.*s", (int)bytes, cp);
			return;
		}
	default:	printf("<%s>", p.type_name()); return;
	}
}

/*
 * The position a message carries, if it carries one: the trailing pair of
 * parameters, both integers, with a line of 0 meaning "not known" - which is
 * what a reporter that has no Source passes.
 */
inline bool
adl_message_position(const ErrBuf::Message& message, int& line, int& column)
{
	unsigned	n = message.parameters.length();
	if (n < 2)
		return false;

	const Variant&	line_param = message.parameters[n-2];
	const Variant&	column_param = message.parameters[n-1];
	if (line_param.type() != Variant::Integer || column_param.type() != Variant::Integer)
		return false;

	line = line_param.as_int();
	column = column_param.as_int();
	return line > 0;
}

inline void
adl_display_message(const char* filename, const ErrBuf::Message& message)
{
	int		line = 0;
	int		column = 0;
	unsigned	params = message.parameters.length();
	bool		located = adl_message_position(message, line, column);

	if (located)
	{
		printf("%s:%d:%d: %s", filename, line, column, message.default_text);
		params -= 2;
	}
	else
		printf("%s: %s", filename, message.default_text);

	for (unsigned i = 0; i < params; i++)
	{
		printf(i == 0 ? ": " : ", ");
		adl_print_parameter(message.parameters[i]);
	}
	printf("\n");
}

/*
 * How many bytes of `text` lie before a source position. The Sources count
 * lines one to a '\n' and columns in characters (see
 * ADLSourceUTF8Ptr::advance), so this walk counts characters the same way and
 * answers bytes - which is the unit a byte-pointer Source reports progress in,
 * and cannot be counted in lines.
 */
inline long long
adl_bytes_before(const UTF8* text, int line, int column)
{
	const UTF8*	cp = text;
	int		l = 1;
	int		c = 1;

	while (*cp != '\0' && (l < line || (l == line && c < column)))
	{
		if (*cp == '\n')
			l++, c = 1;
		else
			c++;
		(void)UTF8Get(cp);		// One character, however many bytes it takes
	}
	return cp - text;
}

/*
 * Where the first message in the buffer was reported, if it carries a
 * position: the messages are in the order they were reported, so the first of
 * them names the place parsing stopped being clean, and the text successfully
 * parsed is what lies before it. Answers false when nothing has been reported
 * yet, or when the first message carries no position. Read it before the
 * drain, which empties the buffer.
 */
inline bool
adl_first_error_position(int& line, int& column)
{
	ErrBuf*	buffer = error_buffer().peek();		// Never makes a buffer for a thread that reported nothing
	if (!buffer || buffer->count() == 0)
		return false;

	// Its own scope: a Message's parameters are a slice of the buffer's array
	ErrBuf::Message	first = buffer->message(0);
	return adl_message_position(first, line, column);
}

/*
 * Show what has been reported, oldest first, and answer how many messages
 * there were - which is also the number the buffer held, and so the count an
 * error report is judged by.
 */
inline unsigned
adl_display_errors(const char* filename)
{
	ErrBuf*	buffer = error_buffer().peek();		// Never makes a buffer for a thread that reported nothing
	if (!buffer)
		return 0;

	unsigned	shown = 0;
	while (buffer->count() > 0)
	{
		{
			// Its own scope: a Message's parameters are a slice of the buffer's
			// parameter array, and delivered() asserts that none is outstanding.
			ErrBuf::Message	message = buffer->message(0);
			adl_display_message(filename, message);
		}
		buffer->delivered();
		shown++;
	}
	return shown;
}

#endif	// ADL_DISPLAY_H
