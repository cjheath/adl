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
 * functions). This is ADL's own display: each message is shown as its default
 * text with its parameters substituted into it, which is what StrVal::format
 * is for, and nothing about presentation is decided below here.
 *
 * By convention a message that carries a position takes its source name,
 * line and column as its first three parameters (0 for line means the
 * reporter did not know where it was), and its own default text names them
 * itself, at its own start: "{1}:{2}:{3}: ...". Nothing here builds that
 * prefix separately - every message is rendered in a single pass, its own
 * text against its own parameters, which is what makes this consistent with
 * an MCS catalog translation (which only ever reorders parameters within
 * the one text it translates). Each message is retired once it has been
 * shown.
 *
 * A Strpp program has no stdio, so what is written here goes out with write(2).
 * A platform with somewhere better to write - a UART, a log - defines
 * ADL_WRITE(data, length) before including this, as it does for strpp's own
 * panic dump.
 */
#include	<char_encoding.h>
#include	<errbuf.h>
#include	<strval.h>
#include	<variant.h>

#if	!defined(ADL_WRITE)
#include	<unistd.h>
#define	ADL_WRITE(data, length)	(void)!write(1, (data), (length))
#endif

// One line of ADL's own: what a driver has to say for itself
inline void
adl_display_line(StrVal line)
{
	ADL_WRITE(line.asUTF8(), (int)line.numBytes());
	ADL_WRITE("\n", 1);
}

/*
 * The position a message carries, if it carries one: parameters 2 and 3 (1
 * is the source name), both integers, with a line of 0 meaning "not known" -
 * which is what a reporter that has no Source passes. Used only to answer
 * *where*, as plain numbers - adl_display_message() below needs none of
 * this, since the text itself already names the position when it has one.
 */
inline bool
adl_message_position(const ErrBuf::Message& message, int& line, int& column)
{
	unsigned	n = message.parameters.length();
	if (n < 3)
		return false;

	const Variant&	line_param = message.parameters[1];
	const Variant&	column_param = message.parameters[2];
	if (line_param.type() != Variant::Integer || column_param.type() != Variant::Integer)
		return false;

	line = line_param.as_int();
	column = column_param.as_int();
	return line > 0;
}

inline void
adl_display_message(const ErrBuf::Message& message)
{
	adl_display_line(StrVal::format(message.default_text, message.parameters));
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
	ErrBuf*	buffer = ErrBuffer();
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
adl_display_errors()
{
	ErrBuf*	buffer = ErrBuffer();
	if (!buffer || buffer->count() == 0)
		return 0;

	unsigned	shown = 0;
	while (buffer->count() > 0)
	{
		{
			// Its own scope: a Message's parameters are a slice of the buffer's
			// parameter array, and delivered() asserts that none is outstanding.
			ErrBuf::Message	message = buffer->message(0);
			adl_display_message(message);
		}
		buffer->delivered();
		shown++;
	}
	return shown;
}

#endif	// ADL_DISPLAY_H
