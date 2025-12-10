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
	using 	Handle = ADLHandleStub;
public:
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

	Handle		assign(Handle object, Handle variable, Value value, bool is_final);	// New Assignment
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
		StringArray	path;
		StrVal		sep;	// Next separator to use while building ("", " " or ".")

		void		clear()
				{ ascent = 0; path.clear(); sep = ""; }
		bool		is_empty()
				{ return ascent == 0 && path.length() == 0; }
		void		consume(PathName& target)
				{ target = *this; clear(); }
		StrVal		display() const
				{ return (ascent > 0 ? StrVal(".")*ascent : "") + path.join("."); }
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
	};

	Store&		store;

	// Current path name being built (with ascent - outer scope levels to rise before searching)
	PathName	current_path;

	Array<Frame>	stack;

	// Access the current ADL Frame:
	Frame&		frame() { return stack.last_mut(); }

	// Read/write access to members of the current frame:
	PathName&	object_path() { return frame().object_path; }
	PathName&	supertype_path() { return frame().supertype_path; }
	bool&		supertype_present() { return frame().supertype_present; }
	bool&		object_started() { return frame().object_started; }
	bool&		obj_array() { return frame().obj_array; }
	ValueType&	value_type() { return frame().value_type; }
	StrVal&		value() { return frame().value; }

public:
	using	Source = ADLSourceUTF8Ptr;

	ADLStoreSink(Store& a)
	: store(a)
	{
	}

	void	error(const char* why, const char* what, const Source& where)
	{
		printf("At line %d:%d, %s MISSING %s: ", where.line_number(), where.column(), why, what);
		where.print_ahead();
	}

	void	definition_starts()			// A declaration just started
	{
		stack.push(Frame());
	}

	void	definition_ends()
	{
		start_object();
		printf("--------------------- Definition Ends\n");
		(void)stack.pull();
		current_path.clear();
	}

	void	ascend()				// Go up one scope level to look for a name
	{
		current_path.ascent++;
	}

	void	name(Source start, Source end)		// A name exists between start and end
	{
		StrVal	n(start.peek(), (int)(end-start));

		if (current_path.sep != " ")			// "" or ".", start new name in pathname
			current_path.path.push(n);
		else if (current_path.sep != ".")
			current_path.path.push(current_path.path.pull() + current_path.sep + n);	// Append the partial name
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
		printf("--------------------- %s.Is Array = true;\n",
			object_pathname().asUTF8()
		);
	}

	void	assignment(bool is_final)		// The value(s) are assigned to the current definition
	{
		start_object();
		printf("--------------------- new Assignment '%s' %s %s;\n",
			object_pathname().asUTF8(),
			is_final ? "=" : "~=",
			value().asUTF8()
		);
	}

	void	string_literal(Source start, Source end)	// Contents of a string between start and end
	{
		StrVal	string(start.peek(), (int)(end-start));
		value_type() = ValueType::String;
		value() = string;
	}

	void	numeric_literal(Source start, Source end)	// Contents of a number between start and end
	{
		StrVal	number(start.peek(), (int)(end-start));
		value_type() = ValueType::Number;
		value() = number;
	}

	void	matched_literal(Source start, Source end)	// Contents of a matched value between start and end
	{
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

		Handle	parent = stack.length() == 1 ? store.top() : stack[stack.length()-1].handle;

		printf("--------------------- %s Object '%s'", supertype_present() ? "new" : "access", object_pathname().asUTF8());
		if (supertype_present())
		{
			printf(" : ");
			if (!supertype_path().is_empty())
				printf("'%s'", supertype_path().display().asUTF8());
		}
		printf(";\n");
		object_started() = true;
	}
};

#endif /* ADLSTORE_H */
