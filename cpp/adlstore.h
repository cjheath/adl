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
	PegexpValue	syntax();
	bool		is_array();

	Handle		lookup(StrVal name);		// Search down one level
	void		each(std::function<void (Handle child)> operation) const;	// Children iterator?
	// Shortcut methods:
	void		assign(Handle variable, Value value, bool is_final);	// Create new Assignment
	Handle		assigned(Handle variable);	// Search for an assignment

	// when Handle is an Assignment:
	Handle		variable();
	Value		value();
	bool		is_final();

	// when Handle is a Reference:
	Handle		to();

	// when Handle is an Alias:
	Handle		for_();

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
				return (!p.is_null() && !p.is_top() ? p.pathname() + "." : "") +
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

	// Make new objects:
	Handle		object(Handle parent, StrVal name, Handle supertype, Handle aspect = 0);	// New Object
	Handle		assign(Handle object, Handle variable, Value value, bool is_final);	// New Assignment

	// Make new Values:
	static	Value	pegexp_literal(StrVal);			// contents of a pegexp excluding the '/'s
	static	Value	reference_literal(StrVal);		// just a pathname
	static	Value	object_literal(Handle);			// an inline object
	static	Value	matched_literal(StrVal);		// Value matching a Syntax
	static	Value	string_literal(StrVal);			// placeholder in the absence of Syntax
	static	Value	numeric_literal(StrVal);		// placeholder in the absence of Syntax

#if defined(ADL_HELPERS)
	// All these builtins can be found by searching in top(), these are short-cuts/caches
	static	Handle	Object();		// aka top.Object
	static	Handle	Parent();		// aka Object.Parent
	static	Handle	Name();			// aka Object.Name
	static	Handle	Super();		// aka Object.Super
	static	Handle	IsSterile();		// aka Object.IsSterile
	static	Handle	IsComplete();		// aka Object.IsComplete
	static	Handle	IsArray();		// aka Object.IsArray
	static	Handle	PegularExpression();	// aka Object.PegularExpression
	static	Handle	Syntax();		// aka Object.Syntax

	static	Handle	Reference();		// aka top.Reference
	static	Handle	Enumeration();		// aka top.Enumeration
	static	Handle	Boolean();		// aka top.Boolean
	static	Handle	False();		// aka top.False
	static	Handle	True();			// aka top.True
	static	Handle	String();		// aka top.String
	static	Handle	Number();		// aka top.Number

	static	Handle	Assignment();		// aka top.Assignment
	static	Handle	Variable();		// aka Assignment.Variable
	static	Handle	ValueOf();		// aka Assignment.Value
	static	Handle	IsFinal();		// aka Assignment.IsFinal (either top.True or top.False)

	static	Handle	Alias();		// aka top.Alias
	static	Handle	For();			// aka Alias.For

	Handle		reference(Handle parent, StrVal name, Handle target, bool is_multi);	// New Reference
	Handle		alias(Handle parent, StrVal name, Handle target);			// New Alias
	void		is_array(Handle);	// Set IsArray
#endif

};

/*
 * This Sink builds an ADLStoreStub using the events passed from the Parser.
 */
template<typename _Store = ADLStoreStub<>>
class ADLStoreSink
{
public:
	using	Source = ADLSourceUTF8Ptr;		// REVISIT: This must work with other Sources also
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
		Match
	};

	struct	PathName
	{
		PathName()
				{ clear(); }

		int		ascent;
		StringArray	names;
		StrVal		sep;	// Next separator to use while building ("", " " or ".")

		void		clear()
				{ ascent = 0; names.clear(); sep = ""; }
		bool		is_empty()
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
		using		Handle = typename Store::Handle;
		using		Value = typename Store::Value;

		Frame()
		: object_started(false)
		, supertype_present(false)
		, obj_array(false)
		, value_type(None)
		{}

		// path name and ascent for current object:
		PathName	object_path;

		// path name and ascent for current object's supertype:
		PathName	supertype_path;
		bool		supertype_present;
		bool		object_started;	// We've seen the name and supertype and can announce those
		bool		obj_array;	// This object accepts an array value
		ValueType	value_type;	// Type of value assigned
		StrVal		value;		// Value assigned

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

	// Access the current ADL Frame:
	Frame&		frame() { return stack.last_mut(); }

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

	ADLStoreSink(Store& a)
	: store(a)
	, last_source()
	{
	}

	void	error(const char* why, const char* what = 0, const Source& where = Source())
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
				return;
			}
		}
		printf("\n");
	}

	void	definition_starts()			// A declaration just started
	{
		printf("-------- Definition Starts\n");
		stack.push(Frame());			// Start with an empty Frame
	}

	void	definition_ends()
	{
		start_object();
		printf("-------- Definition Ends\n");
		last_closed = stack.pull().handle;	// This can be used as a starting point for the next input file
		current_path.clear();
	}

	void	ascend()				// Go up one scope level to look for a name
	{
		current_path.ascent++;
	}

	void	name(Source start, Source end)		// A name exists between start and end
	{
		last_source = end;
		StrVal	n(start.peek(), (int)(end-start));

		if (current_path.sep != " ")			// "" or ".", start new name in pathname
			current_path.names.push(n);
		else if (current_path.sep != ".")
			current_path.names.push(current_path.names.pull() + current_path.sep + n);	// Append the partial name
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

	void	supertype()				// Last pathname was a supertype
	{
		current_path.consume(supertype_path());

		supertype_present() = true;

		// printf("Supertype PathName '%s'\n", supertype_path().display().asUTF8());
		start_object();
	}

	void	reference_type(bool is_multi)		// Last pathname was a reference
	{
		PathName	reference_path;
		current_path.consume(reference_path);

		printf("-------------- new Reference %s %s '%s'\n",
			object_path().display().asUTF8(),
			is_multi ? "=>" : "->",
			reference_path.display().asUTF8());

		object_started() = true;
	}

	void	reference_done(bool ok)			// Reference completed
	{
		// printf("Reference finished\n");
	}

	void	alias()					// Last pathname is an alias
	{
		PathName	alias_path;
		current_path.consume(alias_path);

		printf("---------------- new Alias %s to '%s'\n",
			object_path().display().asUTF8(),
			alias_path.display().asUTF8());
		object_started() = true;
	}

	void	block_start()				// enter the block given by the pathname and supertype
	{
		start_object();
		// printf("Enter block\n");
	}

	void	block_end()				// exit the block given by the pathname and supertype
	{
		// printf("Exit block\n");
	}

	void	is_array()				// This definition is an array
	{
		start_object();
		obj_array() = true;
		printf("-------- %s.Is Array = true;\n",
			object_pathname().asUTF8()
		);
	}

	void	assignment(bool is_final)		// The value(s) are assigned to the current definition
	{
		start_object();
		printf("-------- new Assignment '%s' %s %s;\n",
			object_pathname().asUTF8(),
			is_final ? "=" : "~=",
			value().asUTF8()
		);
		// The top object on the stack is the variable.
		// Next top is the object from which it's being assigned (the context)
		// We don't have easy access to the parent for the assignment -
		// it's not the parent of the variable as that is probably a supertype
	}

	void	string_literal(Source start, Source end)	// Contents of a string between start and end
	{
		last_source = end;
		StrVal	string(start.peek(), (int)(end-start));
		value_type() = ValueType::String;
		value() = string;
	}

	void	numeric_literal(Source start, Source end)	// Contents of a number between start and end
	{
		last_source = end;
		StrVal	number(start.peek(), (int)(end-start));
		value_type() = ValueType::Number;
		value() = number;
	}

	void	matched_literal(Source start, Source end)	// Contents of a matched value between start and end
	{
		last_source = end;
		StrVal	match(start.peek(), (int)(end-start));
		value_type() = ValueType::Match;
		value() = match;
	}

	void	object_literal()			// An object_literal (supertype, block, assignment) was pushed
	{
		value_type() = ValueType::Object;
		value() = "<object literal>";		// REVISIT: include object supertype here
	}

	void	reference_literal()			// The last pathname is a value to assign to a reference variable
	{
		value_type() = ValueType::Reference;
		value() = current_path.display();
		PathName	reference_path;
		current_path.consume(reference_path);
	}

	void	pegexp_literal(Source start, Source end)	// Contents of a pegexp between start and end
	{
		last_source = end;
		StrVal	pegexp(start.peek(), (int)(end-start));
		value_type() = ValueType::Pegexp;
		value() = StrVal("/")+pegexp+"/";
	}

	Source	lookup_syntax(Source type)		// Return Source of a Pegexp string to use in matching
	{ return Source(""); }

	// Methods below here are not a required part of the Sink:

	// Join the display values of the object names in the stack frames:
	StrVal	object_pathname()
	{
		return stack.template map<StringArray, StrVal>(
			[&](const Frame& f) -> const StrVal
			{ return f.object_path.display(); }
		).join(".");
	}

	void	start_object()
	{
		if (object_started())
			return;

		PathName&	new_path = object_path();
		PathName&	super_path = supertype_path();

		// REVISIT: Remove diagnostics:
		printf("-------- %s Object '%s'", supertype_present() ? "new" : "access", new_path.display().asUTF8());
		if (supertype_present())
		{
			printf(" : ");
			if (!super_path.is_empty())
				printf("'%s'", super_path.display().asUTF8());
		}
		printf(";\n");

		/*
		 * Search for names in the parent frame, or last_closed, otherwise we must reopen TOP
		 */
		bool		is_outermost = stack.length() == 1;	// We've entered but not initialised this frame
		Handle		parent = is_outermost ? last_closed : stack.elem(stack.length()-2).handle;
		bool		may_ascend = true;	// A path may ascend only once, either implicitly or explicitly
		int		descent = 0;		// Start with the first name

		/*
		 * We're re-opening TOP. Check that's done correctly
		 */
		if (is_outermost && last_closed.is_null())
		{		// names[0] must be "TOP":
			if (new_path.ascent > 0		// Can't ascend to TOP
			 || new_path.names.length() < 1	// Cannot be anonymous
			 || new_path.names[0] != "TOP")	// Must be called "TOP"
			{
				error("Top object must be called TOP");
				return;
			}

			if (new_path.names.length() == 1)
			{
				// If a supertype of TOP is given, it must be just "Object"
				if (supertype_present()
				 && (super_path.ascent != 0 || super_path.names.length() != 1 || super_path.names[0] != "Object"))
				{
					error("TOP must be Object");
					return;
				}

				frame().handle = store.top();
				printf("Re-opening TOP\n");
				object_started() = true;
				return;			// All done here
			}

			descent = 1;			// All good, we re-opened TOP, but can descend from there
			printf("Re-opening top with %d names to descend\n", new_path.names.length()-descent);
			may_ascend = false;
			parent = last_closed;
		}

		if (parent.is_null())
		{
			error("Child skipped because parent is missing");
			return;
		}

		Handle	context = parent;		// We might descend further

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
			printf("Ascended to %s\n", stack[depth].display().asUTF8());
		}

		// Search down from the parent for each name leading to the last one
		StrVal	child_name;
		Handle	child;
		for (; descent+2 < new_path.names.length(); descent++)	// Care: length() is unsigned
		{
			child_name = new_path.names[descent];
			child = lookup_child(parent, child_name);	// Check in all supertypes
			printf("Descending name %d of %d `%s` from %s found %s\n", descent, new_path.names.length(), child_name.asUTF8(), parent.pathname().asUTF8(), child.pathname().asUTF8());
			if (child.is_null())		// Not in this parent and we can't ascend
			{
				if (!may_ascend)
				{
					error("Parent object name not found", child_name.asUTF8());
					return;
				}
				parent = parent.parent();
				may_ascend = false;
				descent--;
				continue;
			}

			parent = child;	// Descend normally
		}
		assert(descent == new_path.names.length()-1);
		child_name = new_path.names[descent];
		child = child_name.isEmpty() ? Handle() : lookup_child(parent, child_name);
		if (!child.is_null())
			printf("Found existing %s\n", new_path.names.last().asUTF8());
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
				printf("Looking up supertype %s in %s\n", super_path.display().asUTF8(), context.pathname().asUTF8());
				supertype = lookup_path(context, super_path);
			}
			else
				supertype = store.object();
			if (supertype.is_null())
			{
				error("Supertype name not found", super_path.display().asUTF8());
				return;
			}

			if (!child.is_null() && child.super() != supertype)
			{
				error("Cannot change supertype", object_pathname().asUTF8());
				return;
			}
		}
		else if (!child.is_null() && parent != context)
		{
#if	defined(CAN_USE_EPONYMOUS_NAME_FROM_PARENTS)
			printf("Found child %s of parent %s from context %s with no supertype\n",
				child_name.isEmpty() ? "<anonymous>" : child_name.asUTF8(),
				parent.is_null() ? "<none>" : parent.pathname().asUTF8(),
				context.is_null() ? "<none>" : parent.pathname().asUTF8()
			);
			printf("REVISIT: Unsure how to proceed, so ignoring it\n");
			return;
#endif
		}
#if	defined(CAN_USE_EPONYMOUS_NAME_FROM_PARENTS)
		else if (child.is_null() && may_ascend)	// See if the name appears in a parent context
		{
			// Otherwise it was found elsewhere. Use eponymous naming
			printf("No child %s of parent %s from context %s with no supertype\n",
				child_name.isEmpty() ? "<anonymous>" : child_name.asUTF8(),
				parent.is_null() ? "<none>" : parent.pathname().asUTF8(),
				context.is_null() ? "<none>" : parent.pathname().asUTF8()
			);
			printf("Could be eponymous, or a contextual re-opening of a parent's child\n");
			printf("REVISIT: Unsure how to proceed, so ignoring it\n");
			return;

			// supertype = frame().handle;
		}
#endif

		// At this point, we have set context, parent, and perhaps child and supertype

		printf("%s, ", frame().display().asUTF8());
		if (!context.is_null() && context != parent)
			printf("Context: %s, ", context.pathname().asUTF8());
		printf("Parent: %s, ", parent.is_null() ? "<none>" : parent.pathname().asUTF8());
		printf("Child name: %s, ", child_name.isEmpty() ? "<anonymous>" : child_name.asUTF8());
		if (!child.is_null())
			printf("Found as %s, ", child.pathname().asUTF8());
		printf("Child name: %s, ", child_name.isEmpty() ? "<anonymous>" : child_name.asUTF8());
		printf("Supertype: %s\n", supertype.is_null() ? "<none>" : supertype.pathname().asUTF8());

		// REVISIT: This should be just the same as child_name now:
		StrVal	last_name = new_path.names.length() > 0 ? new_path.names.last() : "";

		if (!child.is_null()
		 && !supertype.is_null()
		 && child.super() != supertype)
		{
			error("Cannot change supertype", super_path.display().asUTF8());
			return;
		}

		frame().handle = child;
		if (frame().handle.is_null())
		{
			frame().handle = store.object(
					parent,
					last_name,
					supertype,
					context		// REVISIT: check that Aspect is correct
				);
		}

		object_started() = true;
	}

	// Search this object and its supertypes for an object of the given name
	Handle	lookup_child(Handle parent, StrVal child_name)
	{
		for (Handle node = parent; !parent.is_null() && !node.is_null(); node = node.super())
		{
			Handle	child = node.lookup(child_name);
			printf("\tLooking up %s in %s %s and found %s\n", child_name.asUTF8(), node.name().asUTF8(), child.is_null() ? "failed" : "succeeded", child.pathname().asUTF8());
			if (child.is_null())
				continue;

			if (!child.for_().is_null())
				return child.for_();	// Traverse the alias
			return child;
		}
		return 0;
	}

	// Lookup the entire path, ascending to the parent where necessary
	Handle	lookup_path(Handle parent, PathName path)
	{
		printf("lookup_path(%s, %s)\n", path.display().asUTF8(), parent.pathname().asUTF8());

		assert(!path.is_empty());
		if (path.is_empty())
			return 0;	// No ascent, no path.

		bool	no_implicit_ascent = path.ascent > 0;
		if (path.ascent)
			while (!parent.is_null() && path.ascent-- > 0)
				parent = parent.parent();
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

			error("Can't find name", child_name.asUTF8());
			return Handle();	// Not found
		}

		return parent;
	}

	friend void p(const ADLStoreSink<Store>::Frame& f);	// Allow a debugger to poke around
};

#endif /* ADLSTORE_H */
