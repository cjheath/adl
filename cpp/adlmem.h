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

ErrNum	error(ErrNum num, const char* why, StrVal what);	// forward declared: Handle::finish_assign()
								// (defined before it, below) needs to call it too

/*
 * adl.adl re-declares Object's own true built-ins (Parent, Name, Super,
 * Aspect, Is Sterile, Is Complete, Is Array - README "Variables") as real
 * ADL fields, purely so they're visible/inheritable/typed from ADL. An
 * assignment to one of these, found via inheritance on some object other
 * than Object itself (see Handle::builtin_variable_override()), is wired
 * to the engine's own internals instead of becoming an inert Assignment
 * child disconnected from parent()/name()/super()/aspect()/the flags -
 * see Handle::finish_builtin_assign().
 */
enum class BuiltinObjectVariable
{
	None, Parent, Name, Super, Aspect, IsSterile, IsComplete, IsArray
};

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
	StrVal		syntax();		// Syntax that a value assigned to *this* must conform to, i.e.
					// the effective (inherited) Syntax of super() - fetched via store()->Syntax()
	StrVal		effective_syntax();	// This object's own effective (inherited) Syntax - what
					// `Syntax = SomeType;` (README "Copying a Syntax") copies
	bool		is_array();
	void		set_array();		// Mark this object as accepting an array value
	bool		is_assignment();
	bool		is_reference();		// Is this object's type chain rooted at the built-in Reference?
	bool		is_regular_expression(); // Is this object's type chain rooted at the built-in Regular Expression?
	bool		is_alias();		// Is this object's immediate super() the built-in Alias?
	bool		hides(StrVal name);	// Does one of this object's own aliases hide the inherited `name`?

	Handle		lookup(StrVal name);	// Search down one level
	void		each(std::function<void (Handle child)> operation) const;	// Named, non-Assignment children
	void		each_child(std::function<void (Handle child)> operation) const;	// Every child, named or anonymous
	void		each_assignment(std::function<void (Handle assignment)> operation) const;	// Only this object's own local Assignments
	// Shortcut methods:
	ErrNum		assign(Handle variable, Value value, bool is_final);	// Create/refine an Assignment
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
	Handle		for_();			// The object this alias refers to
	void		set_alias(Handle target);	// Point this (already-created) Alias object at its target

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
	ErrNum		check_final_violation(Handle variable, Value value);
					// The half of finish_assign() that decides whether the new
					// value violates an existing final restriction on this variable
	ErrNum		check_reference_finality(Handle variable, Value value);
					// The Reference-specific narrowing rule check_final_violation()
					// dispatches to for a Reference variable

	BuiltinObjectVariable	builtin_variable_override(Handle variable);
					// Is `variable` one of Object's own built-in fields, and is
					// *this* (the assignment's context) something other than
					// the Object that declares it - i.e. a genuine attempt to
					// (re)assign an inherited occurrence, not the field's own
					// default-restriction declaration (adl.adl's own
					// "Parent -> Object;" inside Object's own definition)?
	ErrNum		finish_builtin_assign(BuiltinObjectVariable kind, Value value, bool is_final);
					// Handle an assignment identified by builtin_variable_override()

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
	bool		is_sterile_final() { return (flags & IsSterileFinal) != 0; }
	bool		is_complete_final() { return (flags & IsCompleteFinal) != 0; }
	void		set_sterile(bool value, bool is_final)
			{
				if (value)
					flags |= IsSterile;
				else
					flags &= ~IsSterile;
				if (is_final)
					flags |= IsSterileFinal;
			}
	void		set_complete(bool value, bool is_final)
			{
				if (value)
					flags |= IsComplete;
				else
					flags &= ~IsComplete;
				if (is_final)
					flags |= IsCompleteFinal;
			}

	Handle		lookup(StrVal name);		// Search down one level
	void		each(std::function<void (Handle child)> operation) const;	// Named, non-Assignment children
	void		each_child(std::function<void (Handle child)> operation) const	// Every child, named or anonymous
			{
				for (int i = 0; i < children.length(); i++)
					operation(children[i]);
			}
	void		each_assignment(std::function<void (Handle assignment)> operation) const
			{		// Only this object's own local Assignments
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

	// Only meaningful when this Object is an Alias:
	Handle		for_target() { return _for; }
	void		set_alias(Handle target) { _for = target; }

	/*
	 * Does one of this object's own Alias children hide the inherited `name`?
	 * True whether or not that alias also gives it a new name.  Either way,
	 * `name` is no longer reachable by ordinary lookup from here downward.
	 */
	bool		hides(StrVal name)
			{
				for (int i = 0; i < children.length(); i++)
				{
					Handle	c = children[i];
					if (c.is_alias() && !c.for_().is_null() && c.for_().name() == name)
						return true;
				}
				return false;
			}

protected:
	enum Flags {
		IsSterile = 0x1,
		IsComplete = 0x2,
		IsArray = 0x4,
		IsFinal = 0x8,		// Set on an Assignment: was *it* final?
		IsSterileFinal = 0x10,	// Has Is Sterile been finally assigned on this Object?
		IsCompleteFinal = 0x20	// Has Is Complete been finally assigned on this Object?
	};
	Handle		_parent;
	StrVal		_name;
	Handle		_super;
	Handle		_aspect;
	SyntaxValue	_syntax;
	int		flags;

	/*
	 * Named sub-objects and Assignments made to this object share this array:
	 * - A normal sub-object,
	 * - An Assignment is identified by having the built-in Assignment object
	 *   as its immediate super(), and is anonymous bookkeeping, skipped in
	 *   lookup() and each(),
	 * - An anonymous object-literal value is a third kind of entry, anonymous
	 *   like an Assignment, but not as Assignment itself, so lookup() skips it,
	 *   but it's included by each()/each_child(), parented by the Assignment,
	 * - An Alias is a real child like any other, but steers lookup_child()
	 */
	Array<Handle>	children;

	Handle		_var;		// Set only on an Object that is itself an Assignment
	Value		_val;

	Handle		_for;		// Set only on an Object that is itself an Alias

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
	Handle		Alias()			// aka TOP.Alias, set once by bootstrap()
			{
				if (_alias_type.is_null())
					top();		// Ensure bootstrap() has run
				return _alias_type;
			}
	Handle		RegularExpression()	// aka TOP.Regular Expression, set once by bootstrap()
			{
				if (_regexp_type.is_null())
					top();		// Ensure bootstrap() has run
				return _regexp_type;
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
	Handle		_alias_type;
	Handle		_regexp_type;
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
	_regexp_type = regexp;

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

	Handle	alias = new Object(_top, "Alias", _object, 0);
	_top.children().push(alias);
	_alias_type = alias;
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
	Handle	s = super();
	return s.is_null() ? "" : s.effective_syntax();
}

inline StrVal
Handle::effective_syntax()
{
	Handle	syntax_variable = store()->Syntax();
	if (syntax_variable.is_null())
		return "";

	// Walk this object's own supertype chain (starting at itself) looking
	// for an assignment to Syntax - local, or inherited.
	// Note: Syntax cannot be contextual, because that would invalidate existing assigned values and syntax for new assignments
	for (Handle t = *this; !t.is_null(); t = t.super())
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
Handle::is_regular_expression()
{
	Handle	regexp_type = store()->RegularExpression();
	if (regexp_type.is_null())
		return false;
	for (Handle t = *this; !t.is_null(); t = t.super())
		if (t == regexp_type)
			return true;
	return false;
}

inline bool
Handle::is_alias()
{
	Handle	s = super();
	return !s.is_null() && s == store()->Alias();
}

inline bool
Handle::hides(StrVal name)
{
	return object->hides(name);
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
Handle::each(std::function<void (Handle child)> operation) const
{		// Named, non-Assignment children
	object->each(operation);
}

void
Handle::each_child(std::function<void (Handle child)> operation) const
{		// Every child, named or anonymous
	object->each_child(operation);
}

void
Handle::each_assignment(std::function<void (Handle assignment)> operation) const
{		// Only this object's own local Assignments
	object->each_assignment(operation);
}

// Shortcut methods:
ErrNum
Handle::assign(Handle variable, Value value, bool is_final)
{		// Create/refine an Assignment
	return finish_assign(begin_assign(variable), variable, value, is_final);
}

/*
 * Locate or create the local Assignment for `variable`, without yet setting its value.
 * Split out from assign() so a value that itself contains an anonymous object-literal
 * provides a real parent for that object.
 * finish_assign() completes the job once the value is ready.
 */
Handle
Handle::begin_assign(Handle variable)
{
	if (!variable.is_null())
	{
		if (builtin_variable_override(variable) != BuiltinObjectVariable::None)
			return *this;	// No real Assignment needed - finish_assign() wires it natively instead;
					// *this is just a harmless non-null placeholder (ADLStoreSink::assignment()
					// silently bails out on a null slot, assuming variable/context resolution
					// failed - it never actually gets used to store anything, see finish_assign())

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
 * Complete a slot obtained from begin_assign(): check for a final violation,
 * then set the value.
 */
ErrNum
Handle::finish_assign(Handle slot, Handle variable, Value value, bool is_final)
{
	if (!variable.is_null())
	{
		BuiltinObjectVariable	kind = builtin_variable_override(variable);
		if (kind != BuiltinObjectVariable::None)
			return finish_builtin_assign(kind, value, is_final);

		/*
		 * adl.adl's own comment on Is Complete: "No further contents may
		 * be added." A brand new Assignment is content, same as a brand
		 * new named/anonymous child (see ADLStoreSink::
		 * check_complete_parent(), adlstore.h) - so this also blocks an
		 * object-literal value from being attached to one, since that can
		 * only happen via a new Assignment. Refining an *existing* local
		 * Assignment (assigned(variable) already finds one - begin_assign()
		 * reused it rather than creating a new one) is not new content,
		 * so it's unaffected, whether or not this object is complete.
		 */
		if (is_complete() && assigned(variable).is_null())
			return error(
				ADLERR_COMPLETE_PARENT,
				"Cannot add new content to a complete object",
				pathname() + "." + variable.name()
			);

		ErrNum	err = check_final_violation(variable, value);
		if (err)
			return err;
	}

	slot.object->set_assignment(variable, value, is_final);
	return 0;
}

/*
 * A position-less counterpart to ADLStoreSink::error() (adlstore.h): this
 * layer (the object model, not the parser) has no Source/line-number to
 * report, only the names of the variable/context/value involved in the
 * failure - the Sink's own error() call, back at the site that receives
 * this ErrNum, still prints the parse position that goes with it.
 */
ErrNum
error(ErrNum num, const char* why, StrVal what)
{
	printf("%s: %s\n", why, what.asUTF8());
	return num;
}

/*
 * Would assigning `value` to `variable` in this context violate an
 * existing final assignment (local, or inherited via this context's own
 * supertype chain)? A Reference variable gets the narrowing exception,
 * such that a final restriction may be refined by any subtype of its
 * existing target. Every other variable gets the plain rule: once
 * finalized, it simply cannot be re-assigned at all.
 */
ErrNum
Handle::check_final_violation(Handle variable, Value value)
{
	if (variable.is_reference())
		return check_reference_finality(variable, value);

	Handle	existing;
	for (Handle t = *this; !t.is_null() && existing.is_null(); t = t.super())
		existing = t.assigned(variable);

	if (!existing.is_null() && existing.is_final())
		return error(
			ADLERR_FINAL_VIOLATION,
			"Assignment violates a final restriction",
			pathname() + "." + variable.name() + " was already finalised by " + existing.parent().pathname()
		);
	return 0;
}

/*
 * A Reference variable's assigned value is itself a type restriction: any
 * value it's ever refined to must be that same value, or a subtype of it.
 * An array-typed Reference (e.g. "X => Y") restricts each element
 * individually.
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
	Handle	rejected;
	if (value.elements.length() > 0)
	{
		for (int i = 0; ok && i < value.elements.length(); i++)
			if (!(ok = same_or_subtype(rejected = value.elements[i].handle)))
				break;
	}
	else
		ok = same_or_subtype(rejected = value.handle);

	if (!ok)
		return error(
			ADLERR_FINAL_VIOLATION,
			"Reference assignment violates a final restriction",
			pathname() + "." + variable.name() + " must be " + old_target.pathname()
				+ " or a subtype, not " + rejected.pathname()
		);
	return 0;
}

inline BuiltinObjectVariable
builtin_object_variable_named(StrVal name)
{
	if (name == "Parent")		return BuiltinObjectVariable::Parent;
	if (name == "Name")		return BuiltinObjectVariable::Name;
	if (name == "Super")		return BuiltinObjectVariable::Super;
	if (name == "Aspect")		return BuiltinObjectVariable::Aspect;
	if (name == "Is Sterile")	return BuiltinObjectVariable::IsSterile;
	if (name == "Is Complete")	return BuiltinObjectVariable::IsComplete;
	if (name == "Is Array")		return BuiltinObjectVariable::IsArray;
	return BuiltinObjectVariable::None;
}

BuiltinObjectVariable
Handle::builtin_variable_override(Handle variable)
{
	Handle	field_owner = variable.parent();
	if (field_owner.is_null() || field_owner != store()->object() || field_owner == *this)
		return BuiltinObjectVariable::None;	// Not one of these fields, or it's their own declaration
	return builtin_object_variable_named(variable.name());
}

/*
 * `value` refers to True (README's Boolean/Enumeration built-ins) - looked
 * up fresh each time rather than memoized, since this is a rare code path
 * (only Is Sterile/Is Complete assignments reach it) and True isn't a
 * bootstrap()-set built-in (it comes from adl.adl, like the fields above).
 */
static bool
value_is_true(Handle context, Value value)
{
	if (value.handle.is_null())
		return false;
	Handle	true_object = context.top().lookup("True");
	return !true_object.is_null() && value.handle == true_object;
}

/*
 * An assignment to one of Object's own built-in fields, found via
 * inheritance on some object other than Object itself (see
 * builtin_variable_override()) - wired to the engine's internals instead
 * of becoming a disconnected Assignment child (README "Variables"; was
 * cpp/ToDo (a)(5)). Parent/Name/Super/Aspect/Is Array are each fixed once
 * by the engine itself when this object is created or declared, so any
 * such assignment either restates that same value (accepted, a no-op) or
 * contradicts it (rejected, "like a final assignment" per your framing).
 * Is Sterile/Is Complete instead follow the normal tentative/final
 * assignment rules, but are stored directly as flags on this Object
 * rather than as a generic Assignment child.
 */
ErrNum
Handle::finish_builtin_assign(BuiltinObjectVariable kind, Value value, bool is_final)
{
	auto	same_or_reject = [&](Handle actual, Handle attempted, const char* label) -> ErrNum
			{
				if (attempted == actual)
					return 0;
				return error(
					ADLERR_FINAL_VIOLATION,
					"Assignment violates a final restriction",
					pathname() + "." + label + " is already " + actual.pathname()
						+ ", not " + attempted.pathname()
				);
			};

	switch (kind)
	{
	case BuiltinObjectVariable::Parent:
		return same_or_reject(parent(), value.handle, "Parent");
	case BuiltinObjectVariable::Super:
		return same_or_reject(super(), value.handle, "Super");
	case BuiltinObjectVariable::Aspect:
		return same_or_reject(aspect(), value.handle, "Aspect");

	case BuiltinObjectVariable::Name:
		if (value.string == name())
			return 0;
		return error(
			ADLERR_FINAL_VIOLATION,
			"Assignment violates a final restriction",
			pathname() + ".Name is already '" + name() + "', not '" + value.string + "'"
		);

	case BuiltinObjectVariable::IsArray:
	{
		bool	attempted = value_is_true(*this, value);
		if (attempted == is_array())
			return 0;
		return error(
			ADLERR_FINAL_VIOLATION,
			"Assignment violates a final restriction",
			pathname() + ".Is Array is already " + (is_array() ? "True" : "False")
		);
	}

	case BuiltinObjectVariable::IsSterile:
	case BuiltinObjectVariable::IsComplete:
	{
		bool	is_sterile_kind = kind == BuiltinObjectVariable::IsSterile;
		const char*	label = is_sterile_kind ? "Is Sterile" : "Is Complete";
		for (Handle t = *this; !t.is_null(); t = t.super())
		{
			bool	already_final = is_sterile_kind ? t.object->is_sterile_final() : t.object->is_complete_final();
			if (already_final)
				return error(
					ADLERR_FINAL_VIOLATION,
					"Assignment violates a final restriction",
					pathname() + "." + label + " was already " + (is_sterile_kind ? "sterilised" : "finalised") + " by " + t.pathname()
				);
		}
		bool	attempted = value_is_true(*this, value);
		if (is_sterile_kind)
			object->set_sterile(attempted, is_final);
		else
			object->set_complete(attempted, is_final);
		return 0;
	}

	default:
		return 0;
	}
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
	 * The target as declared right here: this variable's own parent's assignment
	 * to it (the self-assignment made by "X -> Y" or "X: Reference = Y").
	 * This does not resolve a further refinement recorded on some other instance
	 * that inherits this variable. That would need a walk from the instance in
	 * question, not from the variable itself, since the same shared variable
	 * can be refined differently by many different instances.
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
	return is_alias() ? object->for_target() : Handle();
}

void
Handle::set_alias(Handle target)
{
	object->set_alias(target);
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
