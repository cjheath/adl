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
			operator bool() const 
			{ return (bool)object; }

	Handle		parent();
	StrVal		name();
	Handle		super();
	Handle		aspect();
	bool		is_sterile();
	bool		is_complete();
	StrVal		syntax();
	bool		is_array();
	Array<Handle>&	children();

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

private:
	Ref<Object>	object;
};

class	Value
{
public:
	Value(StrVal s) : string(s), handle(0) {}
	Value(Handle h) : handle(h) {}
protected:
	StrVal		string;
	Handle		handle;
};

class	Object
: public RefCounted
{
	using	SyntaxValue = StrVal;
public:
	Object(Handle parent, StrVal name, Handle super, Handle aspect = 0)
	: _parent(parent), _name(name), _super(super), _aspect(aspect), _syntax(""), flags(0)
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
	Array<Handle>	children;

	friend	class	MemStore;	// Required for bootstrap
	friend	class	Handle;
};

inline Array<Handle>&	Handle::children()
{
	return object->children;
}

class	MemStore
{
public:
	using	Handle = Handle;
	using	Value = Value;

	MemStore() : _top(0) {}
	Handle		top()
			{ if (!_top) bootstrap(); return _top; }
	Handle		object()
			{ if (!_object) bootstrap(); return _object; }

	// Make new objects:
	Handle		object(Handle parent, StrVal name, Handle supertype, Handle aspect = 0)	// New Object
			{
				Object*	o = new Object(parent, name, supertype, aspect);
				if (parent)		// Add this object to the parent
					parent.children().push(o);
				return o;
			}

	// Make new Values:
	static	Value	pegexp_literal(StrVal);			// contents of a pegexp excluding the '/'s
	static	Value	reference_literal(StrVal);		// just a pathname
	static	Value	object_literal(Handle);			// an inline object
	static	Value	matched_literal(StrVal);		// Value matching a Syntax
	static	Value	string_literal(StrVal);			// placeholder in the absence of Syntax
	static	Value	numeric_literal(StrVal);		// placeholder in the absence of Syntax

protected:
	void		bootstrap();
	Handle		_top;
	Handle		_object;
};

void
MemStore::bootstrap()
{
	Object*	top = new Object(0, "TOP", 0);
	_top = top;
	_object = new Object(0, "Object", 0);
	top->_super = _object;

	Handle regexp = new Object(_object, "Regular Expression", _object, 0);
	_top.children().push(regexp);
	Handle syntax = new Object(regexp, "Syntax", _object, 0);
	_object.children().push(regexp);
	Handle reference = new Object(_top, "Reference", _object, 0);
	_top.children().push(reference);
	Handle assignment = new Object(_top, "Assignment", _object, 0);
	_top.children().push(assignment);
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
	return "REVISIT: Not Implemented";
}

inline bool
Handle::is_array()
{
	return object->is_array();
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
}

Handle	
Handle::assigned(Handle variable)	// Search for an assignment
{
	return 0;		// REVISIT: Not Implemented
}

// when Handle is an Assignment:
Handle	
Handle::variable()
{
	return 0;		// REVISIT: Not Implemented
}

Value	
Handle::value()
{
	return StrVal("");	// REVISIT: Not Implemented
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
		if (name == children[i].name())
			return children[i];
	return 0;
}

void
Object::each(std::function<void (Handle child)> operation) const	// Children iterator?
{
	// children.each(operation);
	for (int i = 0; i < children.length(); i++)
		operation(children[i]);
}

}

#endif	// ADLMEM_H
