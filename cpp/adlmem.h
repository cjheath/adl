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
	void		set_array();		// Mark this object as accepting an array value
	bool		is_assignment();
	bool		is_reference();		// Is this object's type chain rooted at the built-in Reference?

	Handle		lookup(StrVal name);		// Search down one level
	void		each(std::function<void (Handle child)> operation) const;	// Named, non-Assignment children
	void		each_child(std::function<void (Handle child)> operation) const;	// Every child, named or anonymous
	void		each_assignment(std::function<void (Handle assignment)> operation) const;	// Only this object's own local Assignments
	// Shortcut methods:
	ErrNum		assign(Handle variable, Value value, bool is_final);	// Create/refine an Assignment;
					// ADLERR_FINAL_VIOLATION if this violates a final Reference restriction
	Handle		assigned(Handle variable);	// Search for an assignment

	/*
	 * Split form of assign(), used when a value must be parsed (and may
	 * itself contain anonymous object-literal values that need a home)
	 * before the Assignment it belongs to can be finalized:
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

protected:
	ErrNum		check_reference_finality(Handle variable, Value value);
					// The Reference-specific half of finish_assign(): does the new
					// value violate an existing final restriction on this variable?

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
	Value(Array<Value> a) : string(""), handle(0), elements(a) {}
// protected:					// REVISIT: Make this visible until I decide an API
	StrVal		string;
	Handle		handle;
	Array<Value>	elements;	// Non-empty iff this Value is an array of element Values
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
	void		set_array() { flags |= IsArray; }

	Handle		lookup(StrVal name);		// Search down one level
	void		each(std::function<void (Handle child)> operation) const;	// Named, non-Assignment children
	void		each_child(std::function<void (Handle child)> operation) const	// Every child, named or anonymous
			{
				for (int i = 0; i < children.length(); i++)
					operation(children[i]);
			}
	void		each_assignment(std::function<void (Handle assignment)> operation) const	// Only this object's own local Assignments
			{
				for (int i = 0; i < children.length(); i++)
					if (children[i].is_assignment())
						operation(children[i]);
			}

	// Only meaningful when this Object is an Assignment:
	Handle		variable() { return _var; }
	Value		value() { return _val; }
	void		set_assignment(Handle var, Value val, bool is_final)
			{
				_var = var;
				_val = val;
				if (is_final)
					flags |= IsFinal;
				else
					flags &= ~IsFinal;
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
	 * assigned()/each_assignment() consider only such entries.
	 * An anonymous object-literal value (see ADLStoreSink::
	 * object_literal_starts()) is a third kind of entry: anonymous like
	 * an Assignment, but not one itself (its super() is its own declared
	 * supertype), so it's skipped by lookup() (empty name never matches)
	 * but included by each()/each_child() - it's a real, if unnamed and
	 * unreachable-by-name, object in the tree, parented on the Assignment
	 * it's the value of.
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
	Handle		Reference()		// aka TOP.Reference, set once by bootstrap()
			{
				if (_reference_type.is_null())
					top();		// Ensure bootstrap() has run
				return _reference_type;
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
	static	Value	array_literal(Array<Value>);		// One element Value per array member

protected:
	void		bootstrap();
	Handle		_top;
	Handle		_object;
	Handle		_syntax_variable;
	Handle		_assignment_type;
	Handle		_reference_type;
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

	Handle	syntax = new Object(_object, "Syntax", regexp, 0);
	_object.children().push(syntax);
	_syntax_variable = syntax;

	Handle	reference = new Object(_top, "Reference", _object, 0);
	_top.children().push(reference);
	_reference_type = reference;

	Handle	assignment = new Object(_top, "Assignment", _object, 0);
	_top.children().push(assignment);
	_assignment_type = assignment;

	// The default restriction for every Reference is Object, and that's
	// final (README "Reference Variables"; adl.adl: "Reference: {Reference = Object}").
	// Must come after _assignment_type is set, since Handle::assign() needs it.
	reference.assign(reference, reference_literal(_object), true);

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

inline void
Handle::set_array()
{
	object->set_array();
}

inline bool
Handle::is_assignment()
{
	Handle	s = super();
	return !s.is_null() && s == store()->Assignment();
}

inline bool
Handle::is_reference()
{
	Handle	reference_type = store()->Reference();
	if (reference_type.is_null())
		return false;
	for (Handle t = *this; !t.is_null(); t = t.super())
		if (t == reference_type)
			return true;
	return false;
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
Handle::each(std::function<void (Handle child)> operation) const	// Named, non-Assignment children
{
	object->each(operation);
}

void
Handle::each_child(std::function<void (Handle child)> operation) const	// Every child, named or anonymous
{
	object->each_child(operation);
}

void
Handle::each_assignment(std::function<void (Handle assignment)> operation) const	// Only this object's own local Assignments
{
	object->each_assignment(operation);
}

// Shortcut methods:
ErrNum
Handle::assign(Handle variable, Value value, bool is_final)	// Create/refine an Assignment
{
	return finish_assign(begin_assign(variable), variable, value, is_final);
}

/*
 * Locate or create the local Assignment for `variable`, without yet setting
 * its value. Split out of assign() so a value that itself contains an
 * anonymous object-literal (which needs a real Assignment to be parented on
 * while it's being parsed - see ADLStoreSink::assignment_starts()) has
 * somewhere to attach before the value as a whole is known. finish_assign()
 * completes the job once the value is ready.
 */
Handle
Handle::begin_assign(Handle variable)
{
	if (!variable.is_null() && variable.is_reference())
	{
		for (Handle t = *this; !t.is_null(); t = t.super())
		{
			Handle	existing = t.assigned(variable);
			if (existing.is_null())
				continue;
			if (existing.parent() == *this)
				return existing;	// Refine this local Assignment in place
			break;				// Found, but inherited: a new local Assignment is needed
		}
	}

	Object*	a = new Object(*this, "", store()->Assignment(), Handle());
	children().push(a);
	return Handle(a);
}

/*
 * Complete a slot obtained from begin_assign(): for a Reference variable,
 * check the finality-narrowing rule, then set the value.
 */
ErrNum
Handle::finish_assign(Handle slot, Handle variable, Value value, bool is_final)
{
	if (!variable.is_null() && variable.is_reference())
	{
		ErrNum	err = check_reference_finality(variable, value);
		if (err)
			return err;
	}

	slot.object->set_assignment(variable, value, is_final);
	return 0;
}

/*
 * A Reference variable's assigned value is itself a type restriction: any
 * value it's ever refined to (by the same variable, seen from this context
 * or an inherited one) must be that same value, or a subtype of it. An
 * array-typed Reference (e.g. "X => Y") restricts each element
 * individually; value.handle is meaningless for an ArrayValue (it's only
 * ever set on a single Reference value).
 */
ErrNum
Handle::check_reference_finality(Handle variable, Value value)
{
	Handle	existing;
	for (Handle t = *this; !t.is_null() && existing.is_null(); t = t.super())
		existing = t.assigned(variable);

	if (existing.is_null() || !existing.is_final())
		return 0;

	Handle	old_target = existing.value().handle;
	auto	same_or_subtype = [&](Handle new_target) -> bool
			{
				if (old_target == new_target)
					return true;
				for (Handle t = new_target; !t.is_null(); t = t.super())
					if (t == old_target)
						return true;
				return false;
			};

	bool	ok = true;
	if (value.elements.length() > 0)
	{
		for (int i = 0; ok && i < value.elements.length(); i++)
			ok = same_or_subtype(value.elements[i].handle);
	}
	else
		ok = same_or_subtype(value.handle);

	if (!ok)
		return ADLERR_FINAL_VIOLATION;
	return 0;
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
	/*
	 * The target as declared right here: this variable's own parent's
	 * assignment to it (the self-assignment made by "X -> Y" or
	 * "X: Reference = Y"). This does not resolve a further refinement
	 * recorded on some other instance that inherits this variable (see
	 * Handle::assign()) - that needs a walk from the instance in
	 * question, not from the variable itself, since the same shared
	 * variable can be refined differently by many different instances.
	 */
	Handle	p = parent();
	if (p.is_null())
		return 0;
	Handle	a = p.assigned(*this);
	return a.is_null() ? Handle() : a.value().handle;
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

inline MemStore::Value
MemStore::array_literal(Array<Value> elements)
{
	return Value(elements);
}

}

#endif	// ADLMEM_H
