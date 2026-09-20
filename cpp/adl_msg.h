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
 *	return ErrorADL_ExpectClosingBrace(probe.line_number(), probe.column());
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
 * The source position is the last two parameters, line then column, which the
 * display prints as a prefix.
 */
#include	<adl_err.h>
#include	<errbuf.h>
#include	<strval.h>
#include	<variant.h>

// The object model, the Store and the Sink:

inline ErrNum
ErrorADL_TopName(StrVal name, int line, int column)
{
	return Error(ADLERR_TOP_NAME,
		"The outermost object must be named TOP, not `{1}`",
		VariantArray() << name << line << column);
}

inline ErrNum
ErrorADL_TopSuper(StrVal supertype, int line, int column)
{
	return Error(ADLERR_TOP_SUPER,
		"TOP's supertype, if given, must be Object, not `{1}`",
		VariantArray() << supertype << line << column);
}

inline ErrNum
ErrorADL_NoParent(StrVal child, int line, int column)
{
	return Error(ADLERR_NO_PARENT,
		"The child `{1}` was skipped because its parent is missing",
		VariantArray() << child << line << column);
}

inline ErrNum
ErrorADL_ParentNotFound(StrVal name, int line, int column)
{
	return Error(ADLERR_PARENT_NOT_FOUND,
		"The name `{1}` on the way to the parent was not found",
		VariantArray() << name << line << column);
}

inline ErrNum
ErrorADL_SupertypeNotFound(StrVal supertype, StrVal object, int line, int column)
{
	return Error(ADLERR_SUPERTYPE_NOT_FOUND,
		"The supertype `{1}` of `{2}` was not found",
		VariantArray() << supertype << object << line << column);
}

inline ErrNum
ErrorADL_SupertypeChanged(StrVal object, StrVal existing, StrVal attempted, int line, int column)
{
	return Error(ADLERR_SUPERTYPE_CHANGED,
		"The object `{1}` already has the supertype `{2}`, which this declaration tried to change to `{3}`",
		VariantArray() << object << existing << attempted << line << column);
}

inline ErrNum
ErrorADL_ReopenNotFound(StrVal object, int line, int column)
{
	return Error(ADLERR_REOPEN_NOT_FOUND,
		"There is no supertype and no object `{1}` to reopen",
		VariantArray() << object << line << column);
}

inline ErrNum
ErrorADL_NameNotFound(StrVal name, int line, int column)
{
	return Error(ADLERR_NAME_NOT_FOUND,
		"The name `{1}` was not found",
		VariantArray() << name << line << column);
}

inline ErrNum
ErrorADL_ReferenceNotFound(StrVal reference, StrVal target, int line, int column)
{
	return Error(ADLERR_REFERENCE_NOT_FOUND,
		"The target `{2}` of the Reference `{1}` was not found",
		VariantArray() << reference << target << line << column);
}

inline ErrNum
ErrorADL_FinalViolation(StrVal object, StrVal attribute, StrVal prior, StrVal attempted, int line, int column)
{
	return Error(ADLERR_FINAL_VIOLATION,
		"The `{2}` of `{1}` was already {3}, so it cannot be set to {4}",
		VariantArray() << object << attribute << prior << attempted << line << column);
}

inline ErrNum
ErrorADL_AliasNotFound(StrVal alias, StrVal target, int line, int column)
{
	return Error(ADLERR_ALIAS_NOT_FOUND,
		"The target `{2}` of the Alias `{1}` was not found",
		VariantArray() << alias << target << line << column);
}

inline ErrNum
ErrorADL_SterileSupertype(StrVal object, StrVal subtype, int line, int column)
{
	return Error(ADLERR_STERILE_SUPERTYPE,
		"Is Sterile forbids a new subtype of `{1}`, so `{2}` cannot be created",
		VariantArray() << object << subtype << line << column);
}

inline ErrNum
ErrorADL_CompleteParent(StrVal object, StrVal child, int line, int column)
{
	return Error(ADLERR_COMPLETE_PARENT,
		"Is Complete forbids new content in `{1}`, so `{2}` cannot be added",
		VariantArray() << object << child << line << column);
}

inline ErrNum
ErrorADL_SyntaxCopyNotFound(StrVal object, int line, int column)
{
	return Error(ADLERR_SYNTAX_COPY_NOT_FOUND,
		"The object `{1}`, whose Syntax was to be copied, was not found",
		VariantArray() << object << line << column);
}

inline ErrNum
ErrorADL_ReferenceFinalViolation(StrVal reference, StrVal prior, StrVal attempted, int line, int column)
{
	return Error(ADLERR_REFERENCE_FINAL_VIOLATION,
		"The Reference `{1}` is final, so it may only be `{2}` or a subtype of it, not `{3}`",
		VariantArray() << reference << prior << attempted << line << column);
}

// The Parser's grammar expectations:

inline ErrNum
ErrorADL_ExpectTypename(int line, int column)
{
	return Error(ADLERR_EXPECT_TYPENAME,
		"A typename was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingBrace(int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_BRACE,
		"A closing brace was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingBracket(int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_BRACKET,
		"A closing square bracket was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectValue(int line, int column)
{
	return Error(ADLERR_EXPECT_VALUE,
		"A value was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectTildeAssign(int line, int column)
{
	return Error(ADLERR_EXPECT_TILDE_ASSIGN,
		"An = sign was expected after the ~",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectMatchingLiteral(StrVal syntax, int line, int column)
{
	return Error(ADLERR_EXPECT_MATCHING_LITERAL,
		"A value matching the declared Syntax was expected here: /{1}/",
		VariantArray() << syntax << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingQuote(int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_QUOTE,
		"A closing quote was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingSlash(int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_SLASH,
		"A closing slash was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpAtom(int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_ATOM,
		"A regular-expression atom was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpSequence(int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_SEQUENCE,
		"A regular-expression sequence was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectClosingParen(int line, int column)
{
	return Error(ADLERR_EXPECT_CLOSING_PAREN,
		"A closing parenthesis was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpClassBody(int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_CLASS_BODY,
		"A character valid in a class was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpClassClose(int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_CLASS_CLOSE,
		"A closing bracket to end the class was expected here",
		VariantArray() << line << column);
}

inline ErrNum
ErrorADL_ExpectRegexpClassPart(int line, int column)
{
	return Error(ADLERR_EXPECT_REGEXP_CLASS_PART,
		"A valid class character was expected here",
		VariantArray() << line << column);
}

#endif	// ADL_MSG_H
