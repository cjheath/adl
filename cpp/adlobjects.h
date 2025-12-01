#if !defined(ADLOBJECTS_H)
#define ADLOBJECTS_H

#include	<adl.h>
#include	<array.h>
#include	<cowmap.h>
#include	<error.h>
#include	<refcount.h>
#include	<strval.h>

namespace ADL {

class	Object;
class	ObjRef
{
public:
	ObjRef();
	ObjRef(Object* o);
	~ObjRef() {}
	Object*		operator->() const { return object; }
	Object&		operator*() const { return *object; }
			operator bool() const { return (bool)object; }

private:
	Ref<Object>	object;
};

class			Assigned;		// result of searching for a variable assignment
class			Value;			// Value that was assigned
typedef	Ref<Value>	ValueRef;
typedef	StrVal		SyntaxValue;
class			PegexpValue;

typedef	Array<ObjRef>	ObjectArray;

class Object
: public RefCounted
{
public:
	static	ObjRef		new_top()
				{
					ADL::ObjRef	top(new ADL::Object(0, "TOP", 0, 0));
					ADL::ObjRef	object(new ADL::Object(top, "Object", 0, 0));
					top->_super = object;
					ADL::ObjRef	regexp(new ADL::Object(object, "Regular Expression", object, 0));
					ADL::ObjRef	syntax(new ADL::Object(regexp, "Syntax", object, 0));
					ADL::ObjRef	reference(new ADL::Object(top, "Reference", object, 0));
					ADL::ObjRef	assignment(new ADL::Object(top, "Assignment", object, 0));
					// ADL::ObjRef	alias(new ADL::Object(top, "Alias", object, 0));
					// ADL::ObjRef	is_for(new ADL::Object(alias, "For", object, 0));

					return top;
				}
	Object(ObjRef parent, StrVal name, ObjRef super, ObjRef aspect = ObjRef(0))
	: _parent(parent), _name(name), _super(super), _aspect(aspect), flags(0) {}

	// Base attributes:
	ObjRef			parent() const { return _parent; }
	StrVal			name() const { return _name; }
	ObjRef			super() const { return _super; }
	ObjRef			aspect() const { return _aspect; }
	SyntaxValue		syntax() const { return _syntax; }

	// Flags
	bool			is_sterile() const { return 0 != (flags & IsSterile); }
	bool			is_complete() const { return 0 != (flags & IsComplete); }
	bool			is_array() const { return 0 != (flags & IsArray); }
	bool			is_final() const { return 0 != (flags & IsFinal); }		// Applies to Assignment

	// Derived attributes and methods:
	ObjectArray&		ancestry();
	void			adopt(ObjRef child);
	ObjRef			child(StrVal name);
	ObjRef			child_transitive(StrVal name);

	virtual	StrVal		pathname() const;
	StrVal			pathname_relative_to(ObjRef);

	StrVal			super_name();
	ObjectArray&		supertypes();

	SyntaxValue		syntax_transitive() const
				;/*{
					if (syntax() && syntax().regex())
						return syntax().regex();
					if (super())
						return super()->syntax_transitive();
					return 0;
				}*/

	Assigned		assigned(ObjRef variable);
	Assigned		assigned_transitive(ObjRef variable);
	ObjRef			assign(ObjRef variable, Ref<Value> value, bool is_final);

	bool			is_reference() const
				{
					// Furthermost and 2nd-furthermost supertype must be Object and Reference:
					ObjRef	p, r;		// Previous, root
					for (r = _super; r->super(); p = r, r = r->super())
						;
					return p && r->name() == "Object" && p->name() == "Reference";
				}
	bool			is_syntax() const
				{
					// Three furthermost supertypes are Object, Regular Expression, Syntax
					ObjRef	q, p, r;		// Previous, root
					for (r = _super; r->super(); q = p, p = r, r = r->super())
						;
					return p && r->name() == "Object" && p->name() == "Regular Expression" && q->name() == "Syntax";
				}
	bool			is_object_literal() const
				{
					return _parent == 0 && name().isEmpty();
				}
	ObjRef			assignment_supertype();
	bool			is_top();
	virtual	StrVal		as_inline() const;

protected:
	enum Flags {
		IsSterile = 0x1,
		IsComplete = 0x2,
		IsArray = 0x4,
		IsFinal = 0x8
	};
	ObjRef			_parent;
	StrVal			_name;
	ObjRef			_super;
	ObjRef			_aspect;
	SyntaxValue		_syntax;
	int			flags;
};
StrVal		Object::pathname() const { return _name; }
StrVal		Object::as_inline() const { return _name; }

ObjRef::ObjRef(Object* o) : object(o) {}
ObjRef::ObjRef() : object(0) {}

class Assignment
: public Object
{
public:
	Assignment(ObjRef parent, ObjRef variable, ValueRef value, bool is_final)
	: Object(parent, "", ObjRef(0))
	, _variable(variable)
	, _value(value)
	{
		if (is_final)
			flags |= IsFinal;
	}

	virtual	StrVal		pathname();
	virtual	StrVal		as_inline();
public:
	ObjRef			_variable;
	ValueRef		_value;
};

class Value
: public RefCounted
{
protected:
	Value() {};
public:
	virtual	StrVal	representation();
};

class StringValue
: public Value
{
	StrVal		lexical;
	StrVal		_value;
public:
	StringValue(StrVal val);
	virtual	StrVal	representation() { return lexical; }
	virtual	StrVal	value() { if (_value == "") _value = lexical; return _value; }
};

class ObjectValue
: public Value
{
	ObjRef		reference;
public:
	ObjectValue(ObjRef ref) : reference(ref) {}
	virtual StrVal	representation() { return reference->is_object_literal() ? reference->as_inline() : reference->pathname(); }

	ObjRef		obj() const { return reference; }
};

class ArrayValue
: public Value
{
	ObjectArray	array;
public:
	ArrayValue(ObjectArray a) : array(a) {}
	int		length() const { return array.length(); }
	const ObjRef	element(int i) const { return array[i]; }
	ArrayValue&	add(ObjRef obj) { array.push(obj); return *this; }

	// virtual StrVal	representation() { return StrVal("[") + array.map().join() + "]"; }
};

class PegexpValue
: public Value
{
	StrVal		pegexp;
public:
	PegexpValue(StrVal s) : pegexp(s) {}
};

class Assigned
{
	Ref<Value>	v;
	ObjRef		o;
	bool		f;
public:	Assigned(Ref<Value> _v, ObjRef _o, bool _f) : v(_v), o(_o), f(_f) {}
	Ref<Value>	value() { return v; }
	ObjRef		object() { return o; }
	bool		final() { return f; }
};

}
#endif // ADLOBJECTS_H
