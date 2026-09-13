/*
 * In-memory (objects) implementation of the ADL Store API
 */
#if !defined(ADLMEM_H)
#define	ADLMEM_H

#include	<adlstore.h>
#include	<array.h>

namespace ADL {

class	Object;
class	Handle;
class	Value;
class	MemStore;

class	Handle
{
public:
	Handle() : object(0) {}
	Handle(Object* o) : object(o) {}
	~Handle() {}

	bool		is_null()
			{ return !(bool)object; }
	bool		operator==(const Handle& other) const
			{ return static_cast<const Object*>(object) == static_cast<const Object*>(other.object); }
	bool		operator!=(const Handle& other) const
			{ return !(*this == other); }

	Handle		parent();
	StrVal		name();
	Handle		super();
	Handle		aspect();
	bool		is_sterile();
	bool		is_complete();
	StrVal		syntax();		// Effective (inherited) Syntax, fetched via store()->Syntax()
	bool		is_array();
	bool		is_assignment();

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

	// Implementation APIs
	Array<Handle>&	children();
	void		adopt(Handle child)
			{ children().push(child); }
	MemStore*	store();	// The MemStore owning this Handle's tree; reached via TOP

	bool		is_top()
			{ return parent().is_null(); }
	Handle		top()				// The ultimate ancestor (TOP), reached by ascent
			{
				Handle	h = *this;
				while (!h.parent().is_null())
					h = h.parent();
				return h;
			}
	StrVal		pathname()
			{
				if (is_null())
					return "<NULL>";
				Handle	p = parent();
				StrVal	n = name();
				return (!p.is_null() /*&& !p.is_top()*/ ? p.pathname() + "." : "") +
					(n.isEmpty() ? "<anonymous>" : n);
			}

private:
	Ref<Object>	object;
	Object&		o() { return *object; }
};

class	Value
{
public:
	Value() : string(""), handle(0) {}
	Value(StrVal s) : string(s), handle(0) {}
	Value(Handle h) : handle(h) {}
// protected:					// REVISIT: Make this visible until I decide an API
	StrVal		string;
	Handle		handle;
};

class	Object
: public RefCounted
{
	using	SyntaxValue = StrVal;
public:
	Object(Handle parent, StrVal name, Handle super, Handle aspect = 0)
	: _parent(parent), _name(name), _super(super), _aspect(aspect), _syntax(""), flags(0), _store(0)
	{}
	~Object() {}

	Handle		parent() { return _parent; }
	StrVal		name() { return _name; }
	Handle		super() { return _super; }
	Handle		aspect() { return _aspect; }
	SyntaxValue	syntax() { return _syntax; }
	bool		is_sterile() { return (flags & IsSterile) != 0; }
	bool		is_complete() { return (flags & IsComplete) != 0; }
	bool		is_array() { return (flags & IsArray) != 0; }
	bool		is_final() { return (flags & IsFinal) != 0; }

	Handle		lookup(StrVal name);		// Search down one level
	void		each(std::function<void (Handle child)> operation) const;	// Children iterator?

	// Only meaningful when this Object is an Assignment:
	Handle		variable() { return _var; }
	Value		value() { return _val; }
	void		set_assignment(Handle var, Value val, bool is_final)
			{
				_var = var;
				_val = val;
				if (is_final)
					flags |= IsFinal;
			}

protected:
	enum Flags {
		IsSterile = 0x1,
		IsComplete = 0x2,
		IsArray = 0x4,
		IsFinal = 0x8
	};
	Handle		_parent;
	StrVal		_name;
	Handle		_super;
	Handle		_aspect;
	SyntaxValue	_syntax;
	int		flags;

	/*
	 * Named sub-objects and Assignments made to this object share this
	 * array. An Assignment is identified by having the built-in
	 * Assignment object as its immediate super() (see
	 * Handle::is_assignment()), and is anonymous bookkeeping, not
	 * reachable by name: lookup() and each() skip such entries, while
	 * assigned() considers only such entries.
	 */
	Array<Handle>	children;

	Handle		_var;		// Set only on an Object that is itself an Assignment
	Value		_val;

	MemStore*	_store;		// Set only on TOP; see Handle::store()

	friend	class	MemStore;	// Required for bootstrap
	friend	class	Handle;
};

inline Array<Handle>&	Handle::children()
{
	return object->children;
}

inline MemStore*
Handle::store()
{
	return top().object->_store;
}

class	MemStore
{
public:
	using	Handle = ADL::Handle;
	using	Value = ADL::Value;

	MemStore() : _top(0) {}
	Handle		top()
			{ if (_top.is_null()) bootstrap(); return _top; }
	Handle		object()
			{ if (_object.is_null()) bootstrap(); return _object; }
	Handle		Syntax()		// aka Object.Syntax, set once by bootstrap()
			{
				if (_syntax_variable.is_null())
					top();		// Ensure bootstrap() has run
				return _syntax_variable;
			}
	Handle		Assignment()		// aka TOP.Assignment, set once by bootstrap()
			{
				if (_assignment_type.is_null())
					top();		// Ensure bootstrap() has run
				return _assignment_type;
			}

	// Make new objects:
	Handle		object(Handle parent, StrVal name, Handle supertype, Handle aspect = 0)	// New Object
			{
				Object*	o = new Object(parent, name, supertype, aspect);
				if (!parent.is_null())		// Add this object to the parent
					parent.adopt(o);
				return o;
			}

	// Make new Values:
	static	Value	pegexp_literal(StrVal);			// contents of a pegexp excluding the '/'s
	static	Value	reference_literal(Handle);		// the object a pathname resolved to (see Sink::lookup_path)
	static	Value	object_literal(Handle);			// an inline object
	static	Value	matched_literal(StrVal);		// Value matching a Syntax
	static	Value	string_literal(StrVal);			// placeholder in the absence of Syntax
	static	Value	numeric_literal(StrVal);		// placeholder in the absence of Syntax

protected:
	void		bootstrap();
	Handle		_top;
	Handle		_object;
	Handle		_syntax_variable;
	Handle		_assignment_type;
};

void
MemStore::bootstrap()
{
	Object*	top = new Object(0, "TOP", 0);
	_top = top;
	top->_store = this;		// So any Handle can reach this MemStore via Handle::store()
	_object = new Object(_top, "Object", 0);
	top->_super = _object;
	_top.children().push(_object);

	Handle	regexp = new Object(_top, "Regular Expression", _object, 0);
	_top.children().push(regexp);

	Handle	syntax = new Object(regexp, "Syntax", regexp, 0);
	_object.children().push(syntax);
	_syntax_variable = syntax;

	Handle	reference = new Object(_top, "Reference", _object, 0);
	_top.children().push(reference);

	Handle	assignment = new Object(_top, "Assignment", _object, 0);
	_top.children().push(assignment);
	_assignment_type = assignment;
	// _alias = new Object(_top, "Alias", _object, 0);
	// _is_for = new ADL::Object(_alias, "For", _object, 0);
}

inline Handle
Handle::parent()
{
	return object->parent();
}

inline StrVal
Handle::name()
{
	return object->name();
}

inline Handle
Handle::super()
{
	return object->super();
}

inline Handle
Handle::aspect()
{
	return object->aspect();
}

inline bool
Handle::is_sterile()
{
	return object->is_sterile();
}

inline bool
Handle::is_complete()
{
	return object->is_complete();
}

inline StrVal
Handle::syntax()
{
	Handle	syntax_variable = store()->Syntax();
	if (syntax_variable.is_null())
		return "";

	// Walk the supertype chain looking for an inherited assignment to Syntax
	// Note: Syntax cannot be contextual, because that would invalidate existing assigned values and syntax for new assignments
	for (Handle t = super(); !t.is_null(); t = t.super())
	{
		Handle	a = t.assigned(syntax_variable);
		if (!a.is_null())
			return a.value().string;
	}
	return "";
}

inline bool
Handle::is_array()
{
	return object->is_array();
}

inline bool
Handle::is_assignment()
{
	Handle	s = super();
	return !s.is_null() && s == store()->Assignment();
}

inline bool
Handle::is_final()
{
	return object->is_final();
}

Handle
Handle::lookup(StrVal name)		// Search down one level
{
	return object->lookup(name);
}

void
Handle::each(std::function<void (Handle child)> operation) const	// Children iterator?
{
	object->each(operation);
}

// Shortcut methods:
void
Handle::assign(Handle variable, Value value, bool is_final)	// Create new Assignment
{
	Object*	a = new Object(*this, "", store()->Assignment(), Handle());
	a->set_assignment(variable, value, is_final);
	children().push(a);
}

Handle
Handle::assigned(Handle variable)	// Search for an assignment
{
	Array<Handle>&	c = children();
	for (int i = 0; i < c.length(); i++)
		if (c[i].is_assignment() && c[i].variable() == variable)
			return c[i];
	return 0;
}

// when Handle is an Assignment:
Handle
Handle::variable()
{
	return object->variable();
}

Value
Handle::value()
{
	return object->value();
}

// when Handle is a Reference:
Handle	
Handle::to()
{
	return 0;		// REVISIT: Not Implemented
}

// when Handle is an Alias:
Handle	
Handle::for_()
{
	return 0;		// REVISIT: Not Implemented
}

Handle
Object::lookup(StrVal name)		// Search down one level
{
	for (int i = 0; i < children.length(); i++)
		if (!children[i].is_assignment() && name == children[i].name())
			return children[i];
	return 0;
}

void
Object::each(std::function<void (Handle child)> operation) const	// Children iterator?
{
	for (int i = 0; i < children.length(); i++)
		if (!children[i].is_assignment())
			operation(children[i]);
}

// Make new Values:
// REVISIT: These are thin wrappers with no type-checking or reference resolution yet
inline MemStore::Value
MemStore::pegexp_literal(StrVal s)
{
	return Value(s);
}

inline MemStore::Value
MemStore::reference_literal(Handle h)
{
	return Value(h);
}

inline MemStore::Value
MemStore::object_literal(Handle h)
{
	return Value(h);
}

inline MemStore::Value
MemStore::matched_literal(StrVal s)
{
	return Value(s);
}

inline MemStore::Value
MemStore::string_literal(StrVal s)
{
	return Value(s);
}

inline MemStore::Value
MemStore::numeric_literal(StrVal s)
{
	return Value(s);
}

}

#endif	// ADLMEM_H
