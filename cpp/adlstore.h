/*
 * ADL API to an object store.
 *
 * Several kinds of stores are possible:
 * - In-memory full store (See adlmem.h)
 * - In-memory minimal store (only to provide Syntax to a scanner)
 * - Database-backed store
 */
#if	!defined(ADLSTORE_H)
#define ADLSTORE_H

#include	<adlparser.h>
#include	<strval.h>
#include	<cstdlib>

/*
 * Verbose parse-tracing, off by default: gated behind the ADL_DEBUG
 * environment variable so all the tracing added while developing this
 * Sink stays available for future debugging without cluttering normal
 * output. getenv() is checked only once - the result can't change during
 * a run - and cached in a function-local static; `inline` keeps this
 * header-safe while still sharing that one cached value process-wide.
 */
inline bool adl_debug_enabled()
{
	static bool	enabled = getenv("ADL_DEBUG") != nullptr;
	return enabled;
}
#define	ADL_TRACE(...)	do { if (adl_debug_enabled()) printf(__VA_ARGS__); } while (0)


/*
 * An ADLStoreStub relies on a Value and a Handle to an object.
 * These define and stub the required APIs.
 */
class	ADLValueStub
{
};

template<typename Value, typename PegexpValue = StrVal>
class	ADLHandleStub
{
	using	Handle = ADLHandleStub;
public:
	bool		is_null();
	bool		operator==(const Handle& other) const;
	bool		operator!=(const Handle& other) const
			{ return !(*this == other); }

	Handle		parent();
	StrVal		name();
	Handle		super();
	Handle		aspect();
	bool		is_sterile();
	bool		is_complete();
	PegexpValue	syntax();	// Syntax a value assigned to *this* must conform to (super()'s effective Syntax)
	PegexpValue	effective_syntax();	// This object's own effective (inherited) Syntax
	bool		is_array();
	bool		is_reference();	// Is this object's type chain rooted at the built-in Reference?
	bool		is_regular_expression();	// Is it rooted at the built-in Regular Expression?
						// (ADLStoreSink::expected_value_kind() needs this, and the
						// contract declared every other predicate of the same kind.)
	bool		is_alias();	// Is this object's immediate super() the built-in Alias?
	bool		hides(StrVal name);	// Does one of this object's own aliases hide the inherited `name`?

	Handle		lookup(StrVal name);		// Search down one level
	Array<Handle>&	children();			// Every child, named or anonymous; caller filters
	// Shortcut methods:
	ErrNum		assign(Handle variable, Value value, bool is_final);	// Create/refine an Assignment;
					// ADLERR_FINAL_VIOLATION if this violates a final Reference restriction
	Handle		assigned(Handle variable);	// Search for an assignment

	/*
	 * Split form of assign(), used when the value may itself contain an
	 * anonymous object-literal value that needs a home (see ADLStoreSink::
	 * assignment_starts()/object_literal_starts()) before it's known:
	 */
	Handle		begin_assign(Handle variable);		// Locate/create the local Assignment slot, value not yet set
	ErrNum		finish_assign(Handle slot, Handle variable, Value value, bool is_final);	// Finality check, then set the value

	// when Handle is an Assignment:
	Handle		variable();
	Value		value();
	bool		is_final();

	// when Handle is a Reference:
	Handle		to();

	// when Handle is an Alias:
	Handle		for_();			// The object this alias refers to
	void		set_alias(Handle target);	// Point this (already-created) Alias object at its target

	void		set_array();		// Mark this object as accepting an array value

#if defined(ADL_HELPERS)
	Handle		reference(StrVal name, Handle target, bool is_multi);	// Create new Reference child
	Handle		alias(StrVal name, Handle target);			// Create new Alias child
#endif

	// Derived behaviour:
	void		adopt(Handle child);
	bool		is_top()
			{ return parent().is_null(); }
	StrVal		pathname()
			{
				if (is_null())
					return "<NULL>";
				Handle	p = parent();
				StrVal	n = name();
				return (!p.is_null() /*&& !p.is_top()*/ ? p.pathname() + "." : "") +
					(n.isEmpty() ? "<anonymous>" : n);
			}
};

template<typename _Handle = ADLHandleStub<ADLValueStub>, typename _Value = ADLValueStub>
class	ADLStoreStub
{
public:
	using	Handle = _Handle;
	using	Value = _Value;

	// Access built-ins quickly:
	Handle		top() { return Handle(); }
	Handle		object();		// aka TOP.Object; backends should memoize this lookup
	Handle		Syntax();		// aka Object.Syntax; backends should memoize this lookup
	Handle		Assignment();		// aka TOP.Assignment; backends should memoize this lookup

	// Make new objects:
	Handle		object(Handle parent, StrVal name, Handle supertype, Handle aspect = 0);	// New Object

	// Make new Values:
	static	Value	pegexp_literal(StrVal);			// contents of a pegexp excluding the '/'s
	static	Value	reference_literal(Handle);		// the object a pathname resolved to (see Sink::lookup_path)
	static	Value	object_literal(Handle);			// an inline object
	static	Value	matched_literal(StrVal);		// Value matching a Syntax
	static	Value	string_literal(StrVal);			// placeholder in the absence of Syntax
	static	Value	numeric_literal(StrVal);		// placeholder in the absence of Syntax
	static	Value	array_literal(Array<Value>);		// One element Value per array member
};

/*
 * This Sink builds an ADLStoreStub using the events passed from the Parser.
 */
template<typename _Store = ADLStoreStub<>, typename _Source = ADLSourceUTF8Ptr>
class ADLStoreSink
{
public:
	using	Source = _Source;		// Either ADLSourceUTF8Ptr or ADLSourceStrVal: the
						// latter makes every fragment a slice of the input
private:
	using	Store = _Store;
	using	Handle = typename Store::Handle;
	using	Value = typename Store::Value;

	enum ValueType {
		None,
		Number,
		String,
		Reference,
		Object,
		Pegexp,
		Match,
		ArrayValue,	// Not "Array": would shadow the ::Array<> template used throughout this class
		SyntaxCopy	// A path naming another object, for a Regular-Expression variable: copy its own effective Syntax
	};

	struct	PathName
	{
		PathName()
				{ clear(); }

		int		ascent;
		StringArray	names;

		// Next separator to use while building ("", " " or ".")
		const char*	sep;

		void		clear()
				{ ascent = 0; names.clear(); sep = ""; }
		bool		is_empty() const
				{ return ascent == 0 && names.length() == 0; }
		void		consume(PathName& target)
				{ target = *this; clear(); }
		StrVal		display() const
				{
					return (ascent > 0 ? StrVal(".")*ascent : "")
						+ (names.length() == 0 ? "<none>" : names.join("."));
				}
	};

	class Frame
	{
	public:
		Frame(bool literal = false)
		: object_started(false)
		, supertype_present(false)
		, obj_array(false)
		, value_type(None)
		, is_literal_value(literal)
		, saw_block(false)
		{}

		// path name and ascent for current object:
		PathName	object_path;

		// path name and ascent for current object's supertype:
		PathName	supertype_path;
		bool		supertype_present;
		bool		object_started;	// We've seen the name and supertype and can announce those
		bool		obj_array;	// This object accepts an array value
		ValueType	value_type;	// Type of value assigned
		StrVal		value;		// Value assigned (display text; see reference_path for a Reference)

		// If there was an error storing this Frame's value, this code will be set:
		ErrNum		value_error = 0;

		// path name and ascent for a Reference value (only meaningful when value_type == Reference)
		PathName	reference_path;

		// One Store::Value per element, accumulated by array_value_element()
		// as each is parsed; only meaningful when value_type == Array
		Array<Value>	array_elements;

		/*
		 * True for a Frame pushed by object_literal_starts(), i.e. this
		 * Frame is for an anonymous object literal ('": Super {...}"')
		 * used as a value, not for a named definition. Makes start_object()
		 * use start_literal_object() instead of the usual name-search path.
		 */
		bool		is_literal_value;

		/*
		 * Set by block_start() before it calls start_object(): a block
		 * ('{') is about to follow this definition's name. Together with
		 * object_path().sep == "." (a trailing dot - see path_name(),
		 * adlparser.h), this is what start_object() uses to tell a
		 * reopen *attempt* (README "Eponymous Naming": "the object is
		 * not being re-opened (by { or .)") from true eponymous naming,
		 * for a bare name not already a local/inherited child.
		 */
		bool		saw_block;

		/*
		 * Set by start_object() (whether or not saw_block is true) when
		 * this definition's own path resolves to something contextual
		 * (README "Contextual Extension" - reached only by ascending
		 * beyond the enclosing scope, or only through inheritance, or a
		 * trailing dot forced it): the Aspect it's contextual to (null
		 * if not contextual). current_context() consults this so that a
		 * plain *value assignment* through a trailing dot is contextual
		 * too, not just a reopen with a block - README "contextual
		 * definitions (objects, variables, aliases, or assignments)".
		 */
		Handle		contextual_aspect;

		/*
		 * Set by start_object(), alongside `handle`: the enclosing object
		 * this definition's final path component was actually looked up
		 * in (the local `parent` in start_object(), at the point `child`
		 * was resolved) - *not* necessarily `handle.parent()`. Those
		 * differ exactly when `child` was found through inheritance
		 * (e.g. "Surname" accessed via a subtype that doesn't declare it
		 * itself): `handle.parent()` is the *declaring* supertype, but
		 * `scope_parent` is the object this statement is actually working
		 * within. current_context() must redirect a contextual value
		 * assignment's owner to an extension of *this*, not of the
		 * field's unrelated declaring supertype.
		 */
		Handle		scope_parent;

		/*
		 * Set by assignment_starts() on the Frame for the variable whose
		 * value is about to be parsed: the local Assignment that value
		 * belongs to. Read by a nested object_literal via current_assignment()
		 * (one level down the stack) so it knows what to parent itself on.
		 */
		Handle		pending_assignment;

		/*
		 * Resolved handle of an anonymous object literal parsed as this
		 * Frame's value (set by object_literal_ends(), consumed by build_value()).
		 */
		Handle		literal_handle;

		Handle		handle;

		StrVal		display() const
				{
					return	"Frame[" +
						object_path.display() +
						(supertype_present ? " : " + supertype_path.display() : "") +
						(obj_array ? "[]" : "") +
						(value_type != None ? " = \""+value+"\"" : "") +
						"]";
				}
	};

	Store&		store;
	Source		last_source;		// Where were we up to in the ADL input?
	Handle		last_closed;		// Handle to the last definition that finished

	// Current path name being built (with ascent - outer scope levels to rise before searching)
	PathName	current_path;

	Array<Frame>	stack;

	// Owns the pegexp pattern text returned by the most recent lookup_syntax() call,
	// so the Source it returns (which points into this buffer) stays valid.
	StrVal		current_syntax;

	// Access the current ADL Frame:
	Frame&		frame() { return stack.last_mut(); }

	// The parent frame (enclosing the current one). Read-only, so const&
	const Frame&	enclosing_frame() const
			{ return stack.asElements()[stack.length()-2]; }

	// Read/write access to members of the current frame:
	PathName&	object_path()
			{ return frame().object_path; }
	PathName&	supertype_path()
			{ return frame().supertype_path; }
	bool&		supertype_present()
			{ return frame().supertype_present; }
	bool&		object_started()
			{ return frame().object_started; }
	bool&		obj_array()
			{ return frame().obj_array; }
	ValueType&	value_type()
			{ return frame().value_type; }
	StrVal&		value()
			{ return frame().value; }

public:
	Handle		root_object;		// Starting point for top-level declarations

	ADLStoreSink(Store& a)
	: store(a)
	, last_source()
	{
	}

	Handle	last_object() const
	{ return last_closed; }

	// The Parser's own grammar-level errors come in here. They carry
	// ADLERR_SYNTAX - the input doesn't match the grammar, as opposed to
	// violating the object model - so every error path returns a code the
	// Parser can count rather than only printing.
	ErrNum	error(const char* why, const char* what = 0, const Source& where = Source())
	{
		return error(ADLERR_SYNTAX, why, what, where);
	}

	ErrNum	error(ErrNum num, const char* why, const char* what = 0, const Source& where = Source())
	{
		printf("At line %d:%d, %s", where.line_number(), where.column(), why);
		if (what)
		{
			printf(" looking for %s", what);
			const char*	cp = where.peek();
			if (*cp != '\0')
			{
				printf(": ");
				where.print_ahead();
				return num;
			}
		}
		printf("\n");
		return num;
	}

	void	definition_starts()			// A declaration just started
	{
		ADL_TRACE("-------- Definition Starts\n");
		stack.push(Frame());			// Start with an empty Frame
	}

	ErrNum	definition_ends()
	{
		ErrNum	err = start_object();
		ADL_TRACE("-------- Definition Ends for %s\n", stack.last_mut().handle.pathname().asUTF8());
		last_closed = stack.pull().handle;	// This can be used as a starting point for the next input file
		current_path.clear();
		return err;
	}

	void	ascend()				// Go up one scope level to look for a name
	{
		current_path.ascent++;
	}

	void	name(Source start, Source end)		// A name exists between start and end
	{
		last_source = end;
		StrVal	n(start.fragment(end));

		if (' ' != current_path.sep[0])			// "" or ".", start new name in pathname
			current_path.names.push(n);
		else
		{
			// Materialize pull() from StrRef to StrVal so we can append
			StrVal	prev = current_path.names.pull();
			current_path.names.push(prev + current_path.sep + n);
		}
		current_path.sep = " ";
	}

	void	descend()				// Go down one level from the last name
	{
		current_path.sep = ".";	// Start new multi-word name
	}

	void	pathname(bool ok)			// The sequence *ascend name *(descend name) is complete
	{
		if (!ok)
			current_path.clear();
	}

	void	object_name()				// The last name was for a new object
	{
		// Save the path:
		current_path.consume(object_path());
		// printf("Object PathName '%s'\n", object_path().display().asUTF8());
	}

	ErrNum	supertype()				// Last pathname was a supertype
	{
		current_path.consume(supertype_path());

		supertype_present() = true;

		// printf("Supertype PathName '%s'\n", supertype_path().display().asUTF8());
		return start_object();
	}

	ErrNum	reference_type(bool is_multi)		// Last pathname was a reference
	{
		PathName	reference_path;
		current_path.consume(reference_path);

		/*
		 * README "Reference Shorthand": "Eponymous naming of reference
		 * variables is also allowed" - a bare "-> Y;"/"=> Y;" with no
		 * name of its own (object_path() still empty here, since
		 * object_name() already committed whatever path_name preceded
		 * the arrow) names the new variable after the referenced type's
		 * own last name, the same way a bare type name creates an
		 * eponymous child elsewhere (see start_object()'s eponymous
		 * naming branch).
		 */
		if (object_path().is_empty() && reference_path.names.length() > 0)
			object_path().names.push(reference_path.names.last());

		ADL_TRACE("-------------- new Reference %s %s '%s'\n",
			object_path().display().asUTF8(),
			is_multi ? "=>" : "->",
			reference_path.display().asUTF8());

		/*
		 * "X -> Y" (or "X => Y") is sugar for "X: Reference = Y", so
		 * reuse start_object()'s existing name resolution/reopening
		 * logic by presenting it with a synthetic supertype path of
		 * just "Reference", exactly as if that had been parsed after
		 * a ':'.
		 *
		 * Resolve the reference's target type from the enclosing context,
		 * before start_object() below creates (or finds) the field
		 * itself. For the eponymous case just above, the field and its
		 * target now share a name, so looking this up afterward would
		 * find the just-created field instead of the intended external
		 * type.
		 */
		Handle	context = current_context();
		Handle	target = context.is_null() ? Handle() : lookup_path(context, reference_path);

		PathName&	super = supertype_path();
		super.clear();
		super.names.push("Reference");
		supertype_present() = true;

		ErrNum	err = start_object();
		if (err)
			return err;

		if (is_multi)
			obj_array() = true;

		Handle	variable = frame().handle;
		if (variable.is_null() || context.is_null())
			return 0;

		if (target.is_null())
			return error(ADLERR_REFERENCE_NOT_FOUND, "Reference target not found", reference_path.display().asUTF8());

		// The implicit type restriction is always final; a following
		// explicit assignment ("X -> Y ~= Z") refines it in place (see
		// Handle::assign()), rather than adding a second Assignment.
		ErrNum	final_err = context.assign(variable, store.reference_literal(target), true);
		if (final_err)
			return error(final_err, "Reference restriction violates a final restriction", object_pathname().asUTF8());

		return 0;
	}

	void	reference_done(bool ok)			// Reference completed
	{
		// printf("Reference finished\n");
	}

	ErrNum	alias()					// Last pathname is an alias
	{
		PathName	alias_path;
		current_path.consume(alias_path);

		ADL_TRACE("---------------- new Alias %s to '%s'\n",
			object_path().display().asUTF8(),
			alias_path.display().asUTF8());

		/*
		 * "NewName! OldName;" (or "! OldName;", with no new name) is
		 * sugar for "NewName: Alias" (or an anonymous Alias) - reuse
		 * start_object()'s existing name resolution/creation logic by
		 * presenting it with a synthetic supertype path of "Alias",
		 * exactly as reference_type() does for "Reference".
		 */
		PathName&	super = supertype_path();
		super.clear();
		super.names.push("Alias");
		supertype_present() = true;

		ErrNum	err = start_object();
		if (err)
			return err;

		Handle	context = current_context();
		if (context.is_null())
			return 0;

		Handle	target = lookup_path(context, alias_path);
		if (target.is_null())
			return error(ADLERR_ALIAS_NOT_FOUND, "Alias target not found", alias_path.display().asUTF8());

		frame().handle.set_alias(target);
		return 0;
	}

	ErrNum	block_start()				// enter the block given by the pathname and supertype
	{
		// printf("Enter block\n");
		frame().saw_block = true;	// Before start_object(): see the comment on Frame::saw_block
		return start_object();
	}

	void	block_end()				// exit the block given by the pathname and supertype
	{
		// printf("Exit block\n");
	}

	ErrNum	is_array()				// This definition is an array
	{
		ErrNum	err = start_object();
		if (err)
			return err;
		obj_array() = true;
		frame().handle.set_array();
		ADL_TRACE("-------- %s.Is Array = true;\n",
			object_pathname().asUTF8()
		);
		return 0;
	}

	ErrNum	assignment_starts(bool is_final)	// '=' or '~=' just seen, about to parse its value
	{
		/*
		 * Obtain (creating if necessary) the variable's local Assignment
		 * now, before its value is parsed: a nested object-literal value
		 * (see object_literal_starts()) needs this to already exist, so it
		 * has somewhere of its own to be parented (see current_assignment()).
		 */
		ErrNum	err = start_object();
		if (err)
			return err;

		// The variable being assigned, and the context it's assigned from:
		Handle	variable = frame().handle;
		Handle	context = current_context();

		if (variable.is_null() || context.is_null())
			return 0;

		frame().pending_assignment = context.begin_assign(variable);
		return 0;
	}

	ErrNum	assignment(bool is_final)		// The value(s) are assigned to the current definition
	{
		ErrNum	err = start_object();
		if (err)
			return err;
		ADL_TRACE("-------- new Assignment '%s' %s %s;\n",
			object_pathname().asUTF8(),
			is_final ? "=" : "~=",
			value().asUTF8()
		);

		Handle	variable = frame().handle;
		Handle	context = current_context();
		Handle	slot = frame().pending_assignment;

		if (variable.is_null() || context.is_null() || slot.is_null())
			return 0;

		ErrNum	final_err = context.finish_assign(slot, variable, build_value(context), is_final);
		if (final_err)
			return error(final_err, "Assignment violates a final restriction", object_pathname().asUTF8());

		// A bad/missing value was reported above; return it here.
		return frame().value_error;
	}

	void	string_literal(Source start, Source end)	// Contents of a string between start and end
	{
		last_source = end;
		StrVal	string(start.fragment(end));
		value_type() = ValueType::String;
		value() = string;
	}

	void	numeric_literal(Source start, Source end)	// Contents of a number between start and end
	{
		last_source = end;
		StrVal	number(start.fragment(end));
		value_type() = ValueType::Number;
		value() = number;
	}

	void	matched_literal(Source start, Source end)	// Contents of a matched value between start and end
	{
		last_source = end;
		StrVal	match(start.fragment(end));
		value_type() = ValueType::Match;
		value() = match;
	}

	void	object_literal_starts()			// ':' seen for an object-literal value: start a Frame for it
	{
		/*
		 * Pushed *before* supertype()/block() run, so their sink calls
		 * (supertype(), block_start(), ...) land on this new Frame instead
		 * of corrupting the enclosing value's Frame.
		 */
		stack.push(Frame(true));
	}

	ErrNum	object_literal_ends()			// supertype/?block/?assignment for the literal are complete
	{
		ErrNum	err = start_object();		// In case neither block nor assignment triggered it (e.g. ': d;')
		Handle	handle = frame().handle;
		stack.pull();				// Pop the literal's Frame; the enclosing value's Frame is current again
		if (err)
			return err;
		value_type() = ValueType::Object;
		frame().literal_handle = handle;
		return 0;
	}

	void	reference_literal()			// The last pathname is a value to assign to a reference variable
	{
		value_type() = ValueType::Reference;
		value() = current_path.display();		// Retained for debug display only
		current_path.consume(frame().reference_path);	// The structured path, resolved later in build_value()
	}

	void	pegexp_literal(Source start, Source end)	// Contents of a pegexp between start and end
	{
		last_source = end;
		StrVal	pegexp(start.fragment(end));
		value_type() = ValueType::Pegexp;
		value() = pegexp;		// excludes the delimiting '/'s, per Store::pegexp_literal's contract
	}

	void	syntax_copy()				// The last pathname is a value to assign to a Regular-Expression variable
	{
		value_type() = ValueType::SyntaxCopy;
		value() = current_path.display();		// Retained for debug display only
		current_path.consume(frame().reference_path);	// The structured path, resolved later in build_value()
	}

	void	array_value_start()			// '[' seen; an array of values follows
	{
		frame().array_elements.clear();
	}

	void	array_value_element()			// One element's literal was just reported above (value()/value_type())
	{
		frame().array_elements.push(build_value(current_context()));
	}

	void	array_value_end()			// ']' seen; the reported elements are now the whole value
	{
		value_type() = ValueType::ArrayValue;	// build_value() will use frame().array_elements, not value()
	}

	/*
	 * Which kind of value the variable being assigned expects (README
	 * "Reference Variables"/"Regular Expression": "If the Variable is a
	 * Regular Expression the value is a regexp, or a path naming another
	 * object whose own effective Syntax is copied onto this one (README
	 * "Copying a Syntax"). If the Variable is a Reference, the value is a
	 * path_name or object literal. Otherwise, the value is defined by the
	 * Syntax of the variable"). Resolves
	 * frame().handle now (like lookup_syntax(), below, for the same
	 * reason: a bare "X.Y = value" with no ':' or '{' otherwise wouldn't
	 * resolve it until after the value is parsed, but we need the
	 * variable's type before we know what kind of value to expect).
	 */
	ValueExpectation	expected_value_kind(Source type)
	{
		start_object();
		Handle	var = frame().handle;
		if (var.is_null())
			return ExpectMatch;	// The variable itself failed to resolve (already reported);
						// this fails cleanly too (lookup_syntax() also sees a null
						// var and returns empty), rather than accepting a value no
						// real check was ever run against

		if (var.is_reference())
			return ExpectReference;
		if (var.is_regular_expression())
			return ExpectRegexp;
		// An array variable's value is an array literal, or a single element.
		// This allows the fallback.
		if (var.is_array())
			return ExpectArray;
		return ExpectMatch;
	}

	Source	lookup_syntax(Source type)		// Return Source of a Pegexp string to use in matching
	{
		start_object();		// Resolve frame().handle now: for a bare "X.Y = value" with no
					// ':' or '{', this otherwise wouldn't run until after the value
					// is parsed, but we need the variable's type to look up its Syntax.
		Handle	var = frame().handle;
		if (var.is_null())
			return Source();

		current_syntax = var.syntax();	// Own a copy: syntax() returns a temporary StrVal,
						// and the Source we return below points into it.
		if (current_syntax.isEmpty())
			return Source();
		return Source::over(current_syntax);
	}

	// Methods below here are not a required part of the Sink:

	/*
	 * current_context is the context from which an assignment, Reference
	 * restriction, or array element value is resolved/recorded from.
	 * Usually "the object one level up the stack from the current
	 * definition" - correct for a plain bareword field name, but a
	 * multi-component dotted path ("Three.Ordinal = Dritte;", or any
	 * explicit ascent) resolves its *whole* navigation within this *one*
	 * Frame - there's no separate Frame for "Three" - so "one level up"
	 * would wrongly give whatever lexically encloses this statement,
	 * not Three.
	 *
	 * Most callers call start_object() first, but reference_type()
	 * deliberately calls this *before* start_object() (it needs the
	 * target type resolved before the field itself exists, for the
	 * eponymous case) when frame().handle/scope_parent are still null.
	 * Fall back to the stack-based answer rather than use an unset Handle.
	 * A reference field's own name is never a multi-component path anyway.
	 *
	 * Use scope_parent (the object this path was actually navigated
	 * within), not handle.parent() (the *found* object's own declaring
	 * parent): those differ when the final component was reached through
	 * inheritance (e.g. "New Context.joe smith.Surname." - Surname is
	 * declared on Person, but this statement is working within joe
	 * smith, and that's what a contextual override must extend).
	 */
	Handle	current_context()
	{
		PathName&	path = frame().object_path;
		Handle		raw = (path.names.length() > 1 || path.ascent > 0) && !frame().scope_parent.is_null()
					? frame().scope_parent
					: (stack.length() >= 2 ? enclosing_frame().handle : root_object);

		/*
		 * start_object() already worked out whether *this* definition's own
		 * path is contextual (ascended, * inherited, or trailing-dot-forced)
		 * and left the answer in contextual_aspect. If `raw` is already an
		 * extension (its own aspect() is set - e.g. this Frame is
		 * nested *within* an already-contextually-reopened block, like
		 * "Insert." inside a contextually-reopened "OperationNames
		 * {...}"), it's already the right, aspect-specific owner -
		 * redirecting *again* would create a meaningless extension of
		 * an extension. Otherwise, if this path is itself contextual
		 * (a flat dotted assignment like "Three.Ordinal. = Dritte;",
		 * where `raw` is still the real, global Three), redirect to
		 * German's own extension of it - the same overlay mechanism
		 * used for a block reopen, just for a single assignment.
		 */
		if (!raw.is_null() && raw.aspect().is_null() && !frame().contextual_aspect.is_null())
			return contextual_extension_of(raw, frame().contextual_aspect);
		return raw;
	}

	/*
	 * The pending_assignment of the Frame one level down - the local
	 * Assignment that the value currently being parsed belongs to (set by
	 * assignment_starts()). This is what an object_literal parents itself
	 * on, so an anonymous value always belongs to the Assignment it's the
	 * value of, never to whatever object happens to lexically enclose it
	 * (which may not even be local, if the variable being assigned is
	 * itself inherited).
	 */
	Handle	current_assignment()
	{
		return stack.length() >= 2 ? enclosing_frame().pending_assignment : Handle();
	}

	// Turn the current Frame's parsed literal into a Store Value.
	// 'context' is where the search for a Reference's target begins (the same
	// starting point used for supertype resolution - see README "Resolving Names").
	Value	build_value(Handle context)
	{
		switch (value_type())
		{
		case ValueType::String:	return store.string_literal(value());
		case ValueType::Number:		return store.numeric_literal(value());
		case ValueType::Pegexp:		return store.pegexp_literal(value());
		case ValueType::Reference:
		{
			Handle	target = lookup_path(context, frame().reference_path);
			return store.reference_literal(target);
		}
		case ValueType::Match:		return store.matched_literal(value());
		case ValueType::ArrayValue:	return store.array_literal(frame().array_elements);
		case ValueType::Object:		return store.reference_literal(frame().literal_handle);
		case ValueType::SyntaxCopy:
		{
			Handle	target = lookup_path(context, frame().reference_path);
			if (target.is_null())
			{
				frame().value_error = error(ADLERR_REFERENCE_NOT_FOUND, "Syntax-copy target not found", frame().reference_path.display().asUTF8());
				return store.pegexp_literal("");
			}
			return store.pegexp_literal(target.effective_syntax());
		}
		default:			return store.string_literal(value());
		}
	}

	// Join the display values of the object names in the stack frames:
	StrVal	object_pathname()
	{
		StringArray	parts;
		const Frame*	frames = stack.asElements();	// No per-element copies
		for (int i = 0; i < stack.length(); i++)
			parts.push(frames[i].object_path.display());
		return parts.join(".");
	}

	// These two error recur three times each:
	ErrNum	reopen_not_found()
			{ return error(ADLERR_REOPEN_NOT_FOUND, "Cannot find object to reopen", object_pathname().asUTF8()); }
	ErrNum	supertype_not_found()
			{ return error(ADLERR_SUPERTYPE_NOT_FOUND, "Supertype name not found", supertype_path().display().asUTF8()); }

	/*
	 * Making an object Sterile prevents definition of any further subtypes.
	 * We check this wherever a brand new Object is about to be created with
	 * `supertype` as its super(), whether named, anonymous, or an object-literal
	 * value.
	 */
	ErrNum	check_sterile_supertype(Handle supertype)
	{
		if (!supertype.is_null() && supertype.is_sterile())
			return error(ADLERR_STERILE_SUPERTYPE, "Cannot create a new subtype of a sterile object", supertype.pathname().asUTF8());
		return 0;
	}

	/*
	 * adl.adl's own comment on Is Complete: "No further contents may be
	 * added." Checked wherever a brand new named or anonymous child
	 * object (not an object-literal value, and not a mere Assignment) is
	 * about to be added to `parent` - whether `parent` was just reopened
	 * or is only now being created for the first time (always is_complete()
	 * == false then, so the check is harmless there).
	 */
	ErrNum	check_complete_parent(Handle parent)
	{
		if (!parent.is_null() && parent.is_complete())
			return error(ADLERR_COMPLETE_PARENT, "Cannot add new content to a complete object", parent.pathname().asUTF8());
		return 0;
	}

	ErrNum	start_literal_object()		// Create the anonymous Object for an object-literal value
	{
		Handle	slot = current_assignment();	// The Assignment this literal's value belongs to (see assignment_starts())
		if (slot.is_null())
			return 0;	// Can't happen via the grammar: assignment_starts() always runs before a value is parsed

		/*
		 * The Assignment's own context is always local (see begin_assign()),
		 * even when the variable being assigned is itself inherited - so
		 * it's the right scope to search for the supertype name in, per
		 * README "Resolving Names".
		 */
		Handle		context = slot.parent();
		PathName&	super_path = supertype_path();
		Handle		supertype = super_path.is_empty() ? store.object() : lookup_path(context, super_path);
		if (supertype.is_null())
			return supertype_not_found();
		ErrNum	sterile_err = check_sterile_supertype(supertype);
		if (sterile_err)
			return sterile_err;

		frame().handle = store.object(slot, "", supertype);
		object_started() = true;
		return 0;
	}

	/*
	 * README "Contextual Extension": find (or create) `target`'s own
	 * contextual extension for `aspect` - the object that "changes...
	 * only seen from the point of view (the Context) of the place where
	 * the object was re-opened. From other places, the object will
	 * still appear unchanged." Parented *on* target (like an Assignment
	 * or anonymous object-literal value - anonymous, not reachable by
	 * ordinary name lookup), with target as its own supertype too, so
	 * anything not locally overridden here still resolves via the
	 * normal supertype-chain walk (lookup_child()) straight through to
	 * target itself.
	 *
	 * Naive linear scan for now - fine for the small number of aspects
	 * any one object is likely to be extended under, but see cpp/ToDo's
	 * Design note: a real per-Aspect index belongs here once an actual
	 * application-facing query API (context folding / dynamic aspect
	 * toggling) is designed, so it can share the same underlying index.
	 */
	Handle	contextual_extension_of(Handle target, Handle aspect)
	{
		Array<Handle>&	kids = target.children();
		for (int i = 0; i < kids.length(); i++)
		{
			Handle	a = kids[i].aspect();
			if (!a.is_null() && a == aspect)
				return kids[i];
		}
		return store.object(target, "", target, aspect);
	}

	ErrNum	start_object()
	{
		if (object_started())
			return 0;

		if (frame().is_literal_value)
			return start_literal_object();

		PathName&	new_path = object_path();
		PathName&	super_path = supertype_path();

		// REVISIT: Remove diagnostics:
		ADL_TRACE("-------- %s Object '%s'", supertype_present() ? "new" : "access", new_path.display().asUTF8());
		if (supertype_present())
		{
			ADL_TRACE(" : ");
			if (!super_path.is_empty())
				ADL_TRACE("'%s'", super_path.display().asUTF8());
		}
		ADL_TRACE(";\n");

		/*
		 * Search for names in the parent frame, or root_object, otherwise we must reopen TOP
		 */
		bool		is_outermost = stack.length() == 1;	// We've entered but not initialised this frame
		Handle		parent = is_outermost ? root_object : enclosing_frame().handle;
		bool		may_ascend = true;	// A path may ascend only once, either implicitly or explicitly
		int		descent = 0;		// Start with the first name

		/*
		 * We're re-opening TOP. Check that's done correctly
		 */
		if (is_outermost && parent.is_null())
		{		// names[0] must be "TOP":
			if (new_path.ascent > 0		// Can't ascend to TOP
			 || new_path.names.length() < 1	// Cannot be anonymous
			 || new_path.names[0] != "TOP")	// Must be called "TOP"
				return error(ADLERR_TOP_NAME, "Top object must be called TOP");

			if (new_path.names.length() == 1)
			{
				// If a supertype of TOP is given, it must be just "Object"
				if (supertype_present()
				 && (super_path.ascent != 0 || super_path.names.length() != 1 || super_path.names[0] != "Object"))
					return error(ADLERR_TOP_SUPER, "TOP must be Object");

				frame().handle = store.top();
				ADL_TRACE("Re-opening TOP\n");
				object_started() = true;
				return 0;		// All done here
			}

			descent = 1;			// All good, we re-opened TOP, but can descend from there
			ADL_TRACE("Re-opening TOP with %d names to descend\n", new_path.names.length()-descent);
			may_ascend = false;
			parent = store.top();
		}

		if (parent.is_null())
			return error(ADLERR_NO_PARENT, "Child skipped because parent is missing");

		if (new_path.is_empty())
		{
			/*
			 * A standalone anonymous object (README "Anonymity", e.g.
			 * `: Person {...}` used directly in a block, not as a value -
			 * for the object-literal-as-value case see
			 * start_literal_object() above). Always a fresh object, never
			 * a reopening: lookup_child()/lookup() match by name, and an
			 * anonymous object has none, so there's no name to search for
			 * here (it stays reachable via each()/each_child(), just not
			 * nameable or reopenable, per README).
			 */
			Handle	supertype = supertype_present() && !super_path.is_empty()
						? lookup_path(parent, super_path)
						: store.object();
			if (supertype.is_null())
				return supertype_not_found();
			ErrNum	sterile_err = check_sterile_supertype(supertype);
			if (sterile_err)
				return sterile_err;
			ErrNum	complete_err = check_complete_parent(parent);
			if (complete_err)
				return complete_err;

			frame().handle = store.object(parent, "", supertype);
			object_started() = true;
			return 0;
		}

		Handle	context = parent;		// We might descend further

		/*
		 * Did we actually have to leave `context` (the enclosing scope
		 * this whole definition starts from) to find the object we end
		 * up reopening? *Not* the same question as "did `parent` change
		 * during descent" - it always does, for a multi-component path,
		 * even with no ascent at all (e.g. ".a.b {...}" written directly
		 * at TOP level: the leading dot's explicit ascent lands right
		 * back on TOP, since that's already the enclosing scope - no
		 * real movement). Used below to decide contextuality (README
		 * "Contextual Extension").
		 */
		bool	truly_ascended = false;

		/*
		 * Handle explicit ascent (up the lexical scopes) to find a parent if requested
		 */
		if (new_path.ascent > 0)		// 1 means use the current parent scope (2nd top on stack)
		{
			may_ascend = false;
			int	depth = stack.length()-new_path.ascent-1;
			if (depth < 0)
				depth = 0;
			parent = stack[depth].handle;
			truly_ascended = parent != context;
			ADL_TRACE("Ascended to %s\n", stack[depth].display().asUTF8());
		}

		// Search down from the parent for each name leading to the last one
		StrVal	child_name;
		Handle	child;
		if (new_path.names.length() == 0)
		{
			// Pure ascent, no name (e.g. a lone "."): reopen the
			// ascended-to object itself - there's no name here to
			// search it for a child of.
			child = parent;
		}
		else
		{
			for (; descent+1 < new_path.names.length(); descent++)	// Care: length() is unsigned
			{
				child_name = new_path.names[descent];
				child = lookup_child(parent, child_name);	// Check in all supertypes
				ADL_TRACE("Descending name %d of %d `%s` from %s found %s\n", descent, new_path.names.length(), child_name.asUTF8(), parent.pathname().asUTF8(), child.pathname().asUTF8());
				if (child.is_null())		// Not in this parent and we can't ascend
				{
					if (!may_ascend)
						return error(ADLERR_PARENT_NOT_FOUND, "Parent object name not found", child_name.asUTF8());
					parent = parent.parent();
					truly_ascended = true;
					may_ascend = false;
					descent--;
					continue;
				}

				parent = child;	// Descend normally
			}
			assert(descent == new_path.names.length()-1);
			child_name = new_path.names[descent];
			if (is_outermost && parent == root_object && child_name == parent.name())
			{
				/*
				 * Declarations in a new file start with the last-finalised
				 * object of the previous file. The exception is when that
				 * last object is explicitly named (re-opened) at the top of
				 * the new file
				 */
				child = parent;
			}
			else
				child = child_name.isEmpty() ? Handle() : lookup_child(parent, child_name);
			if (!child.is_null())
				ADL_TRACE("Found existing %s\n", new_path.names.last().asUTF8());
		}
		frame().handle = child;

		/*
		 * At this point we have a context, a parent, a final name, and perhaps a supertype pathname.
		 * The supertype must be searched from the context.
		 */
		Handle		supertype;
		if (supertype_present())
		{
			auto	empty_super = super_path.is_empty();
			if (!empty_super)
			{
				ADL_TRACE("Looking up supertype %s in %s\n", super_path.display().asUTF8(), context.pathname().asUTF8());
				supertype = lookup_path(context, super_path);
			}
			else
				supertype = store.object();
			if (supertype.is_null())
				return supertype_not_found();

			if (!child.is_null() && child.super() != supertype)
				return error(ADLERR_SUPERTYPE_CHANGED, "Cannot change supertype", object_pathname().asUTF8());
		}
		else
		{
			/*
			 * No explicit supertype given, so this isn't a fresh
			 * creation - one of:
			 *   (a) `child` was already found above by the plain,
			 *       non-ascending lookup for the last path component;
			 *   (b) an unambiguous reopen *attempt* not yet resolved -
			 *       a trailing dot, or a block about to follow (README
			 *       "Eponymous Naming": eponymous naming only applies
			 *       when the object "is not being re-opened (by { or
			 *       .)") - findable only by ascending, the same way a
			 *       supertype name is;
			 *   (c) neither of those: eponymous naming (a bare name, no
			 *       block, no trailing dot), or a genuine failure.
			 */
			bool	is_reopen_attempt = ('.' == object_path().sep[0]) || frame().saw_block;

			if (child.is_null() && is_reopen_attempt)
			{
				PathName	target_path;
				target_path.names.push(child_name);
				child = lookup_path(context, target_path);
				if (child.is_null())
					return reopen_not_found();
				truly_ascended = true;	// Only reached because a plain, local
							// lookup already failed - lookup_path()
							// must have ascended to find it at all
			}

			if (!child.is_null())
			{
				/*
				 * Reopening something is always contextual when it's
				 * not really "yours", but is reached only by ascending
				 * beyond `context` (`truly_ascended`), or reached only
				 * through inheritance. If it could be local, a trailing
				 * dot forces it to be contextual anyway
				 *
				 * It's recorded on the Frame either way (contextual_aspect),
				 * for current_context() below to redirect a plain *value
				 * assignment* to this same Aspect's own extension too. A
				 * trailing dot on an assignment ("Three.Ordinal. = Dritte;")
				 * is contextual the same way a reopen is.
				 */
				bool	ascended = truly_ascended;
				/*
				 * child == parent happens only via the is_outermost
				 * same-name continuation special case above. There,
				 * "child" is not a descendant of "parent" at all, it
				 * *is* parent. Don't start a contextual extension
				 * of the continued object if we reopened it by name.
				 */
				bool	inherited = child != parent && child.parent() != parent;
				bool	forced = '.' == object_path().sep[0];
				frame().contextual_aspect = (ascended || forced) ? context : Handle();
				if (frame().saw_block && (ascended || inherited || forced))
				{
					ADL_TRACE("Contextually extending %s from %s (ascended %d, inherited %d, forced %d)\n",
						child.pathname().asUTF8(), context.pathname().asUTF8(), ascended, inherited, forced);
					child = contextual_extension_of(child, context);
				}
			}
			else if (may_ascend)
			{
				/*
				 * Eponymous naming.
				 *
				 * A bare name (no ':', no block, no trailing dot) not
				 * already a child of this object (which would be
				 * re-opening) but *is* the name of some existing
				 * type - found the same way as a supertype name -
				 * creates a new child of that type, named the same as
				 * the type itself (it's eponymous). E.g. inside
				 * "Event: { Date }", "Date" becomes a new Event child
				 * named "Date" with supertype Date, as if we wrote
				 * "Date: Date;"
				 */
				PathName	eponymous_path;
				eponymous_path.names.push(child_name);
				supertype = lookup_path(context, eponymous_path);
				if (supertype.is_null())
					return reopen_not_found();
			}
			else
			{
				// No supertype, no matching child, this is a failure.
				return reopen_not_found();
			}
		}

		// At this point, we have set context, parent, and perhaps child and supertype

		ADL_TRACE("%s, ", frame().display().asUTF8());
		if (!context.is_null() && context != parent)
			ADL_TRACE("Context: %s, ", context.pathname().asUTF8());
		ADL_TRACE("Parent: %s, ", parent.is_null() ? "<none>" : parent.pathname().asUTF8());
		ADL_TRACE("Child name: %s, ", child_name.isEmpty() ? "<anonymous>" : child_name.asUTF8());
		if (!child.is_null())
			ADL_TRACE("Found as %s, ", child.pathname().asUTF8());
		ADL_TRACE("Supertype: %s\n", supertype.is_null() ? "<none>" : supertype.pathname().asUTF8());

		// REVISIT: This should be just the same as child_name now:
		StrVal	last_name = new_path.names.length() > 0 ? new_path.names.last() : "";

		if (!child.is_null()
		 && !supertype.is_null()
		 && child.super() != supertype)
			return error(ADLERR_SUPERTYPE_CHANGED, "Cannot change supertype", super_path.display().asUTF8());

		frame().handle = child;
		frame().scope_parent = parent;
		if (frame().handle.is_null())
		{
			ErrNum	sterile_err = check_sterile_supertype(supertype);
			if (sterile_err)
				return sterile_err;
			ErrNum	complete_err = check_complete_parent(parent);
			if (complete_err)
				return complete_err;

			frame().handle = store.object(
					parent,
					last_name,
					supertype
					// No Aspect: an ordinary object's Aspect defaults to
					// its Parent (README "Contextual Extension": "All
					// objects actually have a Context [Aspect], which
					// for most, it's the same as the Parent") - only a
					// genuine contextual extension gets a real, distinct
					// one, via contextual_extension_of() above.
				);
		}

		object_started() = true;
		return 0;
	}

	// Search this object and its supertypes for an object of the given name
	Handle	lookup_child(Handle parent, StrVal child_name)
	{
		for (Handle node = parent; !parent.is_null() && !node.is_null(); node = node.super())
		{
			/*
			 * If this level's own Aliases hide child_name, the search
			 * stops here. The object may still be visible from elsewhere.
			 */
			if (node.hides(child_name))
			{
				ADL_TRACE("\t%s hides %s\n", node.name().asUTF8(), child_name.asUTF8());
				return Handle();
			}

			Handle	child = node.lookup(child_name);
			ADL_TRACE("\tLooking up %s in %s %s and found %s\n", child_name.asUTF8(), node.name().asUTF8(), child.is_null() ? "failed" : "succeeded", child.pathname().asUTF8());
			if (child.is_null())
				continue;

			if (!child.for_().is_null())
				return child.for_();	// Traverse the alias
			return child;
		}
		if (child_name == "TOP")
			return store.top();
		return 0;
	}

	// Lookup the entire path, ascending to the parent where necessary
	// Takes PathName by const reference: PathName holds a StringArray and a
	// StrVal, both refcounted, so passing by value cost a copy per call.
	Handle	lookup_path(Handle parent, const PathName& path)
	{
		ADL_TRACE("lookup_path(%s, %s)\n", path.display().asUTF8(), parent.pathname().asUTF8());
		assert(!path.is_empty());
		if (path.is_empty())
			return 0;	// No ascent, no path.

		int	ascent = path.ascent;		// Consumed here; `path` stays read-only
		bool	no_implicit_ascent = ascent > 0;
		if (ascent)
		{
			ascent--;	// First . indicates just parent
			while (!parent.is_null() && ascent-- > 0)
				parent = parent.parent();
		}
		if (parent.is_null())
			return parent;

		if (path.names.length() == 0)
			return parent;

		Handle	start_parent = parent;
		for (int i = 0; !parent.is_null() && i < path.names.length(); i++)
		{
			StrVal	child_name = path.names[i];

			// Search all children lists in the supertype chain
			Handle	child = lookup_child(parent, child_name);
			if (!child.is_null())
			{
				parent = child;
				continue;	// Found, descend again?
			}

			if (!no_implicit_ascent && i == 0)
			{		// We can ascend to look in a parent
				parent = parent.parent();
				i--;	// Look for the same name again
				continue;
			}

			error(ADLERR_NAME_NOT_FOUND, "Can't find name", child_name.asUTF8());
			return Handle();	// Not found
		}

		return parent;
	}

	friend void p(const ADLStoreSink<_Store, _Source>::Frame& f);	// Allow a debugger to poke around
};

#endif /* ADLSTORE_H */
