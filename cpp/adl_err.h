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
 * first, because a translation may use them in another order or leave one
 * out. Every message that carries a position names its source, line and
 * column itself, as {1}, {2} and {3}: "{1}:{2}:{3}: ...". Content parameters
 * follow, from {4}. A message with nothing to say about *where* (there are
 * none of those yet) would simply not carry these three.
 */
#include	<error.h>

#define	ADLERR_SET			1024	// The message set allocated to ADL

// The object model, the Store and the Sink:

#define	ADLERR_TOP_NAME			ErrNum(ADLERR_SET, 1)	// {1}:{2}:{3}: the outermost object must be named TOP, not `{4}`
#define	ADLERR_TOP_SUPER		ErrNum(ADLERR_SET, 2)	// {1}:{2}:{3}: TOP's supertype, if given, must be Object, not `{4}`
#define	ADLERR_NO_PARENT		ErrNum(ADLERR_SET, 3)	// {1}:{2}:{3}: the child `{4}` was skipped because its parent is missing
#define	ADLERR_PARENT_NOT_FOUND		ErrNum(ADLERR_SET, 4)	// {1}:{2}:{3}: the name `{4}` on the way to the parent was not found
#define	ADLERR_SUPERTYPE_NOT_FOUND	ErrNum(ADLERR_SET, 5)	// {1}:{2}:{3}: the supertype `{4}` of `{5}` was not found
#define	ADLERR_SUPERTYPE_CHANGED	ErrNum(ADLERR_SET, 6)	// {1}:{2}:{3}: the object `{4}` already has the supertype `{5}`, which this declaration tried to change to `{6}`
#define	ADLERR_REOPEN_NOT_FOUND		ErrNum(ADLERR_SET, 7)	// {1}:{2}:{3}: there is no supertype and no object `{4}` to reopen
#define	ADLERR_NAME_NOT_FOUND		ErrNum(ADLERR_SET, 8)	// {1}:{2}:{3}: the name `{4}` was not found
#define	ADLERR_REFERENCE_NOT_FOUND	ErrNum(ADLERR_SET, 9)	// {1}:{2}:{3}: the target `{5}` of the Reference `{4}` was not found
#define	ADLERR_FINAL_VIOLATION		ErrNum(ADLERR_SET, 10)	// {1}:{2}:{3}: the `{5}` of `{4}` was already {6}, so it cannot be set to {7}
#define	ADLERR_ALIAS_NOT_FOUND		ErrNum(ADLERR_SET, 11)	// {1}:{2}:{3}: the target `{5}` of the Alias `{4}` was not found
#define	ADLERR_STERILE_SUPERTYPE	ErrNum(ADLERR_SET, 12)	// {1}:{2}:{3}: Is Sterile forbids a new subtype of `{4}`, so `{5}` cannot be created
#define	ADLERR_COMPLETE_PARENT		ErrNum(ADLERR_SET, 13)	// {1}:{2}:{3}: Is Complete forbids new content in `{4}`, so `{5}` cannot be added
#define	ADLERR_SYNTAX_COPY_NOT_FOUND	ErrNum(ADLERR_SET, 14)	// {1}:{2}:{3}: the object `{4}`, whose Syntax was to be copied, was not found
#define	ADLERR_REFERENCE_FINAL_VIOLATION ErrNum(ADLERR_SET, 15)	// {1}:{2}:{3}: the Reference `{4}` is final, so it may only be `{5}` or a subtype of it, not `{6}`

// The Parser's grammar expectations. Each names what was expected in its text,
// after the position every message opens with - "was expected" needs no
// trailing "here" now that the position itself says where:

#define	ADLERR_EXPECT_TYPENAME		ErrNum(ADLERR_SET, 16)	// {1}:{2}:{3}: a typename was expected
#define	ADLERR_EXPECT_CLOSING_BRACE	ErrNum(ADLERR_SET, 17)	// {1}:{2}:{3}: a closing brace was expected
#define	ADLERR_EXPECT_CLOSING_BRACKET	ErrNum(ADLERR_SET, 18)	// {1}:{2}:{3}: a closing square bracket was expected
#define	ADLERR_EXPECT_VALUE		ErrNum(ADLERR_SET, 19)	// {1}:{2}:{3}: a value was expected
#define	ADLERR_EXPECT_TILDE_ASSIGN	ErrNum(ADLERR_SET, 20)	// {1}:{2}:{3}: an = sign was expected after the ~
#define	ADLERR_EXPECT_MATCHING_LITERAL	ErrNum(ADLERR_SET, 21)	// {1}:{2}:{3}: a value matching the declared Syntax was expected: /{4}/
#define	ADLERR_EXPECT_CLOSING_QUOTE	ErrNum(ADLERR_SET, 22)	// {1}:{2}:{3}: a closing quote was expected
#define	ADLERR_EXPECT_CLOSING_SLASH	ErrNum(ADLERR_SET, 23)	// {1}:{2}:{3}: a closing slash was expected
#define	ADLERR_EXPECT_REGEXP_ATOM	ErrNum(ADLERR_SET, 24)	// {1}:{2}:{3}: a regular-expression atom was expected
#define	ADLERR_EXPECT_REGEXP_SEQUENCE	ErrNum(ADLERR_SET, 25)	// {1}:{2}:{3}: a regular-expression sequence was expected
#define	ADLERR_EXPECT_CLOSING_PAREN	ErrNum(ADLERR_SET, 26)	// {1}:{2}:{3}: a closing parenthesis was expected
#define	ADLERR_EXPECT_REGEXP_CLASS_BODY	ErrNum(ADLERR_SET, 27)	// {1}:{2}:{3}: a character valid in a class was expected
#define	ADLERR_EXPECT_REGEXP_CLASS_CLOSE ErrNum(ADLERR_SET, 28)	// {1}:{2}:{3}: a closing bracket to end the class was expected
#define	ADLERR_EXPECT_REGEXP_CLASS_PART	ErrNum(ADLERR_SET, 29)	// {1}:{2}:{3}: a valid class character was expected

// Back with the object model, the Store and the Sink (see the comment there):

#define	ADLERR_ASCENT_EXCEEDS_FILE	ErrNum(ADLERR_SET, 30)	// {1}:{2}:{3}: the ascent in `{4}` reaches beyond this file's own scope, which has only {5} level(s) open
#define	ADLERR_NOT_VARIABLE		ErrNum(ADLERR_SET, 31)	// {1}:{2}:{3}: the object {4} to which you are assigning a {5:<16...} is not a variable because it has no Syntax

#endif	// ADL_ERR_H
