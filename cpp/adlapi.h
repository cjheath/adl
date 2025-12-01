#if	!defined(ADLAPI_H)
#define ADLAPI_H
/*
 * ADL API to an object store
 */
#include	<strval.h>

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

template<typename Handle = ADLHandleStub<ADLValueStub>, typename Value = ADLValueStub>
class	ADLAPI
{
public:
	// Access built-ins quickly:
	static	Handle	top();

	// Make new objects:
	Handle		object(Handle parent, StrVal name, Handle supertype, Handle aspect = 0);	// New Object

	// Make new Values:
	static	Value	pegexp_literal(StrVal);			// contents of a pegexp excluding the '/'s
	static	Value	reference_literal(StrVal);		// just a pathname
	static	Value	object_literal(Handle);			// an inline object
	static	Value	matched_literal(StrVal);		// Value matching a Syntax
	static	Value	string_literal(StrVal);			// placeholder in the absence of Syntax
	static	Value	numeric_literal(StrVal);		// placeholder in the absence of Syntax

#if defined(ADL_HELPERS)			// All these builtins can be found by searching in top()
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
	void		is_array(Handle);			// Set IsArray
#endif

};

#endif /* ADLAPI_H */
