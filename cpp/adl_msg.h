#if	!defined(ADL_MSG_H)
#define	ADL_MSG_H
/*
 * ADL's error reporting functions: one per message, gathering the parameters
 * its default text calls for and handing them to the error buffer's Error().
 * This is the private half of the pair, for code that raises a message;
 * adl_err.h holds the numbers, and is for code that recognises a condition.
 *
 * Each function answers the ErrNum it has just reported, so reporting an error
 * and returning it are one act, and the ordinary use is a return:
 *
 *	return ErrorADL_ExpectClosingBrace(probe.source_name(), probe.line_number(), probe.column());
 *
 * Nothing is formatted here, and nothing is decided about language, style or
 * severity: the buffer holds the number, the default text and the parameters
 * until they are read out, which may be in another thread or another process.
 *
 * The parameters are gathered into a VariantArray built in place, so a report
 * cannot be disturbed by another being made while its own parameters are still
 * being gathered. Variant's constructors from StrVal, int, long, long long and
 * const char* are not explicit, so most parameters need no Variant(...) cast.
 *
 * Every message that carries a position takes its source name, line and
 * column last, matching every call site (which already ends `source_name(),
 * line, column` or the equivalent), and its own default text opens with them
 * itself: "{1}:{2}:{3}: ...". The VariantArray push order puts them first,
 * ahead of the content parameters, to match - only that push order, and the
 * text's own numbering, differ from the parameter list a function takes.
 * adl_display.h renders each message in a single pass: nothing outside this
 * file assembles a position prefix any more.
 */
#include	<adl_err.h>
#include	<errbuf.h>
#include	<strval.h>
#include	<variant.h>

// The object model, the Store and the Sink:

inline ErrNum
ErrorADL_TopName(StrVal name, const char* source_name, int line, int column)
{
	return Error(ADLERR_TOP_NAME,
		"{1}:{2}:{3}: the outermost object must be named TOP, not `{4}`",
		VariantArray() << source_name << line << column << name);
}

inline ErrNum
ErrorADL_TopSuper(StrVal supertype, const char* source_name, int line, int column)
{
	return Error(ADLERR_TOP_SUPER,
		"{1}:{2}:{3}: TOP's supertype, if given, must be Object, not `{4}`",
		VariantArray() << source_name << line << column << supertype);
}

inline ErrNum
ErrorADL_NoParent(StrVal child, const char* source_name, int line, int column)
{
	return Error(ADLERR_NO_PARENT,
		"{1}:{2}:{3}: the child `{4}` was skipped because its parent is missing",
		VariantArray() << source_name << line << column << child);
}

inline ErrNum
ErrorADL_ParentNotFound(StrVal name, const char* source_name, int line, int column)
{
	return Error(ADLERR_PARENT_NOT_FOUND,
		"{1}:{2}:{3}: the name `{4}` on the way to the parent was not found",
		VariantArray() << source_name << line << column << name);
}

inline ErrNum
ErrorADL_SupertypeNotFound(StrVal supertype, StrVal object, const char* source_name, int line, int column)
{
	return Error(ADLERR_SUPERTYPE_NOT_FOUND,
		"{1}:{2}:{3}: the supertype `{4}` of `{5}` was not found",
		VariantArray() << source_name << line << column << supertype << object);
}

inline ErrNum
ErrorADL_SupertypeChanged(StrVal object, StrVal existing, StrVal attempted, const char* source_name, int line, int column)
{
	return Error(ADLERR_SUPERTYPE_CHANGED,
		"{1}:{2}:{3}: the object `{4}` already has the supertype `{5}`, which this declaration tried to change to `{6}`",
		VariantArray() << source_name << line << column << object << existing << attempted);
}

inline ErrNum
ErrorADL_ReopenNotFound(StrVal object, const char* source_name, int line, int column)
{
	return Error(ADLERR_REOPEN_NOT_FOUND,
		"{1}:{2}:{3}: there is no supertype and no object `{4}` to reopen",
		VariantArray() << source_name << line << column << object);
}

inline ErrNum
ErrorADL_NameNotFound(StrVal name, const char* source_name, int line, int column)
{
	return Error(ADLERR_NAME_NOT_FOUND,
		"{1}:{2}:{3}: the name `{4}` was not found",
		VariantArray() << source_name << line << column << name);
}

inline ErrNum
ErrorADL_ReferenceNotFound(StrVal reference, StrVal target, const char* source_name, int line, int column)
{
	return Error(ADLERR_REFERENCE_NOT_FOUND,
		"{1}:{2}:{3}: the target `{5}` of the Reference `{4}` was not found",
		VariantArray() << source_name << line << column << reference << target);
}

inline ErrNum
ErrorADL_FinalViolation(StrVal object, StrVal attribute, StrVal prior, StrVal attempted, const char* source_name, int line, int column)
{
	return Error(ADLERR_FINAL_VIOLATION,
		"{1}:{2}:{3}: the `{5}` of `{4}` was already {6}, so it cannot be set to {7}",
		VariantArray() << source_name << line << column << object << attribute << prior << attempted);
}

inline ErrNum
ErrorADL_AliasNotFound(StrVal alias, StrVal target, const char* source_name, int line, int column)
{
	return Error(ADLERR_ALIAS_NOT_FOUND,
		"{1}:{2}:{3}: the target `{5}` of the Alias `{4}` was not found",
		VariantArray() << source_name << line << column << alias << target);
}

inline ErrNum
ErrorADL_SterileSupertype(StrVal object, StrVal subtype, const char* source_name, int line, int column)
{
	return Error(ADLERR_STERILE_SUPERTYPE,
		"{1}:{2}:{3}: Is Sterile forbids a new subtype of `{4}`, so `{5}` cannot be created",
		VariantArray() << source_name << line << column << object << subtype);
}

inline ErrNum
ErrorADL_CompleteParent(StrVal object, StrVal child, const char* source_name, int line, int column)
{
	return Error(ADLERR_COMPLETE_PARENT,
		"{1}:{2}:{3}: Is Complete forbids new content in `{4}`, so `{5}` cannot be added",
		VariantArray() << source_name << line << column << object << child);
}

inline ErrNum
ErrorADL_SyntaxCopyNotFound(StrVal object, const char* source_name, int line, int column)
{
	return Error(ADLERR_SYNTAX_COPY_NOT_FOUND,
		"{1}:{2}:{3}: the object `{4}`, whose Syntax was to be copied, was not found",
		VariantArray() << source_name << line << column << object);
}

inline ErrNum
ErrorADL_ReferenceFinalViolation(StrVal reference, StrVal prior, StrVal attempted, const char* source_name, int line, int column)
{
	return Error(ADLERR_REFERENCE_FINAL_VIOLATION,
		"{1}:{2}:{3}: the Reference `{4}` is final, so it may only be `{5}` or a subtype of it, not `{6}`",
		VariantArray() << source_name << line << column << reference << prior << attempted);
}

inline ErrNum
ErrorADL_AscentExceedsFile(StrVal path, int levels_open, const char* source_name, int line, int column)
{
	return Error(ADLERR_ASCENT_EXCEEDS_FILE,
		"{1}:{2}:{3}: the ascent in `{4}` reaches beyond this file's own scope, which has only {5} level(s) open",
		VariantArray() << source_name << line << column << path << levels_open);
}

inline ErrNum
ErrorADL_NotVariable(StrVal variable, StrVal attempted, const char* source_name, int line, int column)
{
	return Error(ADLERR_NOT_VARIABLE,
		"{1}:{2}:{3}: the object {4} to which you are assigning a {5:<16...} is not a variable because it has no Syntax",
		VariantArray() << source_name << line << column << variable << attempted);
}

// The Parser's grammar expectations:

inline ErrNum
ErrorADL_ExpectTypename(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_TYPENAME,
		"{1}:{2}:{3}: a typename was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingBrace(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_BRACE,
		"{1}:{2}:{3}: a closing brace was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingBracket(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_BRACKET,
		"{1}:{2}:{3}: a closing square bracket was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectValue(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_VALUE,
		"{1}:{2}:{3}: a value was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectTildeAssign(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_TILDE_ASSIGN,
		"{1}:{2}:{3}: an = sign was expected after the ~",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectMatchingLiteral(StrVal syntax, const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_MATCHING_LITERAL,
		"{1}:{2}:{3}: a value matching the declared Syntax was expected: /{4}/",
		VariantArray() << source_name << line << column << syntax);
}

inline ErrNum
ErrorADL_ExpectClosingQuote(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_QUOTE,
		"{1}:{2}:{3}: a closing quote was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingSlash(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_SLASH,
		"{1}:{2}:{3}: a closing slash was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpAtom(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_ATOM,
		"{1}:{2}:{3}: a regular-expression atom was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpSequence(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_SEQUENCE,
		"{1}:{2}:{3}: a regular-expression sequence was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingParen(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_PAREN,
		"{1}:{2}:{3}: a closing parenthesis was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpClassBody(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_CLASS_BODY,
		"{1}:{2}:{3}: a character valid in a class was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpClassClose(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_CLASS_CLOSE,
		"{1}:{2}:{3}: a closing bracket to end the class was expected",
		VariantArray() << source_name << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpClassPart(const char* source_name, int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_CLASS_PART,
		"{1}:{2}:{3}: a valid class character was expected",
		VariantArray() << source_name << line << column);
}

#endif	// ADL_MSG_H
