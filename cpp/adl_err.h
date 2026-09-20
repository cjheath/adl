#if	!defined(ADL_ERR_H)
#define	ADL_ERR_H
/*
 * ADL's error numbers: one per message in the set, with the message's default
 * text in a comment. This is the public half of the pair, for code that has to
 * recognise a condition; adl_msg.h holds the function that reports each of
 * these, and is what code that raises one includes.
 *
 * Set 1024 is the number allocated to ADL. An error number holds a set number
 * and a message number within that set. A number, once used, is never re-used
 * or re-numbered: it appears in logs, in the product manual and in a
 * customer's report, and it has to mean the same thing years later. A retired
 * message keeps its number and is never raised again. This set has not been
 * released yet, so it may still be renumbered; once it is released, no number
 * here changes again.
 *
 * A message's default text names its parameters by position, {1} being the
 * first, because a translation may use them in another order or leave one out.
 * The source position follows as the last two parameters, line then column;
 * the texts do not name it, leaving the display to print it as a prefix.
 */
#include	<error.h>

#define	ADLERR_SET			1024	// The message set allocated to ADL

// The object model, the Store and the Sink:

#define	ADLERR_TOP_NAME			ErrNum(ADLERR_SET, 1)	// The outermost object must be named TOP, not `{1}`
#define	ADLERR_TOP_SUPER		ErrNum(ADLERR_SET, 2)	// TOP's supertype, if given, must be Object, not `{1}`
#define	ADLERR_NO_PARENT		ErrNum(ADLERR_SET, 3)	// The child `{1}` was skipped because its parent is missing
#define	ADLERR_PARENT_NOT_FOUND		ErrNum(ADLERR_SET, 4)	// The name `{1}` on the way to the parent was not found
#define	ADLERR_SUPERTYPE_NOT_FOUND	ErrNum(ADLERR_SET, 5)	// The supertype `{1}` of `{2}` was not found
#define	ADLERR_SUPERTYPE_CHANGED	ErrNum(ADLERR_SET, 6)	// The object `{1}` already has the supertype `{2}`, which this declaration tried to change to `{3}`
#define	ADLERR_REOPEN_NOT_FOUND		ErrNum(ADLERR_SET, 7)	// There is no supertype and no object `{1}` to reopen
#define	ADLERR_NAME_NOT_FOUND		ErrNum(ADLERR_SET, 8)	// The name `{1}` was not found
#define	ADLERR_REFERENCE_NOT_FOUND	ErrNum(ADLERR_SET, 9)	// The target `{2}` of the Reference `{1}` was not found
#define	ADLERR_FINAL_VIOLATION		ErrNum(ADLERR_SET, 10)	// The `{2}` of `{1}` was already {3}, so it cannot be set to {4}
#define	ADLERR_ALIAS_NOT_FOUND		ErrNum(ADLERR_SET, 11)	// The target `{2}` of the Alias `{1}` was not found
#define	ADLERR_STERILE_SUPERTYPE	ErrNum(ADLERR_SET, 12)	// Is Sterile forbids a new subtype of `{1}`, so `{2}` cannot be created
#define	ADLERR_COMPLETE_PARENT		ErrNum(ADLERR_SET, 13)	// Is Complete forbids new content in `{1}`, so `{2}` cannot be added
#define	ADLERR_SYNTAX_COPY_NOT_FOUND	ErrNum(ADLERR_SET, 14)	// The object `{1}`, whose Syntax was to be copied, was not found
#define	ADLERR_REFERENCE_FINAL_VIOLATION ErrNum(ADLERR_SET, 15)	// The Reference `{1}` is final, so it may only be `{2}` or a subtype of it, not `{3}`

// The Parser's grammar expectations. Each names what was expected in its text
// and carries only the position, which the display prints as a prefix:

#define	ADLERR_EXPECT_TYPENAME		ErrNum(ADLERR_SET, 16)	// A typename was expected here
#define	ADLERR_EXPECT_CLOSING_BRACE	ErrNum(ADLERR_SET, 17)	// A closing brace was expected here
#define	ADLERR_EXPECT_CLOSING_BRACKET	ErrNum(ADLERR_SET, 18)	// A closing square bracket was expected here
#define	ADLERR_EXPECT_VALUE		ErrNum(ADLERR_SET, 19)	// A value was expected here
#define	ADLERR_EXPECT_TILDE_ASSIGN	ErrNum(ADLERR_SET, 20)	// An = sign was expected after the ~
#define	ADLERR_EXPECT_MATCHING_LITERAL	ErrNum(ADLERR_SET, 21)	// A value matching the declared Syntax was expected here: /{1}/
#define	ADLERR_EXPECT_CLOSING_QUOTE	ErrNum(ADLERR_SET, 22)	// A closing quote was expected here
#define	ADLERR_EXPECT_CLOSING_SLASH	ErrNum(ADLERR_SET, 23)	// A closing slash was expected here
#define	ADLERR_EXPECT_REGEXP_ATOM	ErrNum(ADLERR_SET, 24)	// A regular-expression atom was expected here
#define	ADLERR_EXPECT_REGEXP_SEQUENCE	ErrNum(ADLERR_SET, 25)	// A regular-expression sequence was expected here
#define	ADLERR_EXPECT_CLOSING_PAREN	ErrNum(ADLERR_SET, 26)	// A closing parenthesis was expected here
#define	ADLERR_EXPECT_REGEXP_CLASS_BODY	ErrNum(ADLERR_SET, 27)	// A character valid in a class was expected here
#define	ADLERR_EXPECT_REGEXP_CLASS_CLOSE ErrNum(ADLERR_SET, 28)	// A closing bracket to end the class was expected here
#define	ADLERR_EXPECT_REGEXP_CLASS_PART	ErrNum(ADLERR_SET, 29)	// A valid class character was expected here

#endif	// ADL_ERR_H
