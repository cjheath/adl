using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Collections;

namespace ADL
{
    // An assigned value is either a bool, a string, an ADLObject, a Regex or an Array of those:
    class Value
    {
	private	bool?	    m_b_val;
	private	string	    m_s_val;
	private	ADLObject   m_o_val;
	private	Regex	    m_r_val;
	private	List<Value> m_a_val;

	public	bool?	    b_val{get{return m_b_val;} private set{m_b_val = value;}}
	public	string	    s_val{get{return m_s_val;} private set{m_s_val = value;}}
	public	ADLObject   o_val{get{return m_o_val;} private set{m_o_val = value;}}
	public	Regex	    r_val{get{return m_r_val;} private set{m_r_val = value;}}
	public	List<Value> a_val{get{return m_a_val;} private set{m_a_val = value;}}

	public	    Value(string _s_val) { s_val = _s_val; }
	public	    Value(bool _b_val) { b_val = _b_val; }
	public	    Value(ADLObject _o_val) { o_val = _o_val; }
	public	    Value(Regex _r_val) { r_val = _r_val; }
	public	    Value(List<Value> _a_val) { a_val = _a_val; }
	public override string ToString()
	{
	    if (m_r_val != null)
		return m_r_val.ToString();
	    else if (m_o_val != null)
		return m_o_val.inspect();
	    else if (m_s_val != null)
		return m_s_val;
	    else if (m_a_val != null)
		throw new System.ArgumentException("Not Implemented");
	    else
		return m_b_val.ToString();
	}
    }

    class ADLObject
    {
	public	ADLObject	parent{get; private set;}
	public	string		name{get; private set;}
	public	ADLObject	zuper{get; private set;}
	public	ADLObject	aspect{get; private set;}
	public	Regex		syntax{get; private set;}
	public	bool		is_array{get; private set;}
	public	bool		is_sterile{get; private set;}
	public	bool		is_complete{get; private set;}
	public	List<ADLObject>	members{get;}

	public	ADLObject(
		ADLObject	_parent,
		string		_name,
		ADLObject	_zuper,
		ADLObject	_aspect = null
	)
	{
	    parent = _parent;
	    name = _name;
	    zuper = _zuper;
	    aspect = _aspect;
	    if (parent != null)
	    	parent.adopt(this);
	    members = new List<ADLObject>();
	}

	// Adopt this child
	public void adopt(ADLObject child)
	{
	    if (child.name != null && member(child.name) != null)
		throw new System.ArgumentException("Can't have two children called "+child.name);
	    members.Add(child);
	}

	// Get list of ancestors starting with the outermost and ending with this object
	private List<ADLObject>	_ancestry;
	public List<ADLObject>	ancestry{
	    get {
		if (_ancestry == null)
		{	// Build and memoize the result
		    if (parent != null)
			_ancestry = new List<ADLObject>(parent.ancestry);
		    else
			_ancestry = new List<ADLObject>();
		    _ancestry.Add(this);
		}
		return _ancestry;
	    }
	}

	// Get list of supertypes starting with this object and ending with "Object"
	private List<ADLObject>	_supertypes;
	public List<ADLObject> supertypes{
	    get {
		if (_supertypes == null)
		{	// Build and memoize the result
		    _supertypes = new List<ADLObject>();
		    _supertypes.Add(this);
		    if (zuper != null)
			_supertypes.AddRange(zuper.supertypes);
		}
		return _supertypes;
	    }
	}

	public Tuple<Value, ADLObject, bool> assigned(ADLObject variable) {
	    ADLObject outermost = supertypes.Last();	// This will always be "Object"

	    if (variable.parent == outermost)
	    {	    // Check for an assignment to the special built-in variables
		switch (variable.name)
		{
		case "Name":	    return Tuple.Create(new Value(name), outermost, name != null);
		case "Parent":	    return Tuple.Create(new Value(parent), outermost, parent != null);
		case "Super":	    return Tuple.Create(new Value(zuper), outermost, zuper != null);
		case "Aspect":	    return Tuple.Create(new Value(aspect), outermost, aspect != null);
		case "Syntax":	    return Tuple.Create(new Value(syntax), outermost, syntax != null);
		case "Is_Array":    return Tuple.Create(new Value(is_array), outermost, true);
		case "Is_Sterile":  return Tuple.Create(new Value(is_sterile), outermost, true);
		case "Is_Complete": return Tuple.Create(new Value(is_complete), outermost, true);
		}
	    }

	    // Find an existing assignment:
	    Assignment	existing = members.Find(m => m is Assignment && (m as Assignment).variable == variable) as Assignment;
	    if (existing != null)
		return null;
	    return Tuple.Create(existing.value, existing.parent, existing.is_final);
	}

	public Tuple<Value, ADLObject, bool> assigned_transitive(ADLObject variable)
	{
	    Tuple<Value, ADLObject, bool>	a = assigned(variable);
	    if (a != null)
		return a;
	    if (zuper != null)
		return zuper.assigned_transitive(variable);
	    return null;
	}

	public ADLObject member(string name)
	{
	    if (name == null)
		return null;
	    return members.Find(m => m.name == name);
	}

	public ADLObject member_transitive(string name)
	{
	    ADLObject	m = member(name);
	    if (m != null)
		return m;
	    if (zuper != null)
		return zuper.member_transitive(name);
	    return null;
	}

	// Is this object a subtype of the "Reference" built-in?
	public bool is_reference()
	{
	    List<ADLObject> s = supertypes;
	    return supertypes[supertypes.Count-1].name == "Object" &&
		supertypes[supertypes.Count-2].name == "Reference";
	}

	// Is this object a subtype of the "Syntax" built-in?
	public bool is_syntax()
	{
	    List<ADLObject> s = supertypes;
	    return s.Count >= 3 &&
		s[supertypes.Count-1].name == "Object" &&
		s[supertypes.Count-2].name == "Regular Expression" &&
		s[supertypes.Count-3].name == "Syntax";
	}

	public bool is_object_literal{get{
	    // Object Literals have no parent and no name.
	    return parent == null && name == null;
	}}

	public Regex syntax_transitive()
	{
	    if (syntax != null)
		return syntax;
	    if (zuper != null)
		return zuper.syntax_transitive();
	    return null;
	}

	// Return a string with all ancestor names from the top.
	public string pathname()
	{
	    return (parent != null ? parent.pathname() + "." : "") +
		(name != null ? name : "<anonymous>");
	}

	public string pathname_relative_to(ADLObject them)
	{
	    List<ADLObject> mine = new List<ADLObject>(ancestry);
	    List<ADLObject> theirs = new List<ADLObject>(them.ancestry);
	    int	    common = 0;
	    while (mine.Any() && theirs.Any() && mine.First() == theirs.First()) {
		mine.RemoveAt(0);
		theirs.RemoveAt(0);
		common += 1;
	    }
	    return new String('.', common) + String.Join(".", mine.Select(m => m.name));
	}

	private bool is_top()
	{
	    return parent == null;
	}

	public string	zuper_name()
	{
	    if (zuper != null)
		if (zuper.parent.parent == null && zuper.name == "Object")
		    return ":";
		else
		    return ": "+zuper.name;
	    else
		return null;
	}

	public virtual string inspect()
	{
	    return pathname() + zuper_name();
	}

	public void assign(ADLObject variable, Value value, bool is_final)
	{
	    Tuple<Value, ADLObject, bool> t = assigned(variable);
	    Value	a = t.Item1;	    // Existing value
	    ADLObject	p = t.Item2;	    // Location of existing assignment
	    bool    	f = t.Item3;

	    if (a != null && a != value && variable != this)
		throw new System.ArgumentException("#{inspect} cannot have two assignments to #{variable.inspect}");

	    if (variable.is_syntax())
	    {
		string	s = value.s_val;
		if (s == null)
		    throw new System.ArgumentException("Assignment to Syntax requires a string");
		syntax = new Regex("^" + s.Substring(1, s.Length-2));
	    }
	    else
		new Assignment(this, variable, value, is_final);
	}

	public virtual string as_inline(string level = "")
	{
	    Assignment self_assignment = members.Find(m => m is Assignment && (m as Assignment).variable == this) as Assignment;
	    List<ADLObject> others = members.Where(m => m != self_assignment).ToList();

	    return ":" +
		zuper.name +
		(!others.Any()
		    ? ""
		    :	"{" +
			others.Select(
			    m =>
				m is Assignment
				? (m as Assignment).variable.name + "; " + (m as Assignment).as_inline()
				: ""
			) +
			"}"
		) +
		(self_assignment != null ? self_assignment.as_inline() : "");
	}

	public virtual void emit(string level = null)
	{
	    if (level == null)
	    {
		foreach (ADLObject m in members) {
		    m.emit("");
		}
		return;
	    }

	    Assignment self_assignment = members.Find(m => m is Assignment && (m as Assignment).variable == this) as Assignment;
	    List<ADLObject> others = members.Where(m => m != self_assignment).ToList();
	    bool    has_attrs = others.Any() || syntax != null;

	    Console.Write(level);
	    Console.Write(name);
	    string  zn = zuper_name();
	    Console.Write(zn);
	    if (has_attrs)
		Console.WriteLine(zn != null ? " {" : "{");

	    if (syntax != null)
		Console.WriteLine(level+"\tSyntax = /"+syntax.ToString()+"/;");

	    foreach (ADLObject m in others)
		m.emit(level+"\t");

	    if (has_attrs)
		Console.Write(level+"}");
	    if (self_assignment != null && members.Any())
		Console.Write(self_assignment.as_inline());
	    Console.WriteLine(!has_attrs || self_assignment != null ? ";" : "");
	}

	public ADLObject   assignment_supertype()
	{
	    ADLObject	p = this;
	    while (p.parent != null)
		p = p.parent;
	    return member("Assignment");
	}
    }

    class Assignment : ADLObject
    {
	public	ADLObject   variable{get; private set;}
	public	Value	    value{get; private set;}
	public	bool	    is_final{get; private set;}

	public Assignment(ADLObject _parent, ADLObject _variable, Value _value, bool _is_final)
	    : base(_parent, null, _parent.assignment_supertype())
	{
	    variable = _variable;
	    value = _value;
	    is_final = _is_final;
	}

	public override string inspect()
	{
	    return
		(parent != null ? parent.pathname() : "-") +
		"." +
		variable.name +
		(is_final ? "= " : "~= ") +
		value.ToString();
	}

	public override string as_inline(string level = "")
	{
	    return
		(is_final ? " = " : " ~= ") +
		(value.o_val != null
		?
		    (value.o_val.is_object_literal
		    ?	value.o_val.as_inline()
		    :	value.o_val.pathname_relative_to(parent)
		    )
		:   (value.a_val != null
		    ?	String.Join(
			    ",\n",
			    value.a_val.Select(v =>
				v.o_val != null && v.o_val.is_object_literal
				?   v.o_val.as_inline()
				:   v.o_val.pathname_relative_to(parent)
			    ).ToArray()
			)
		    :	value.ToString()
		    )
		);
	}

	public override void emit(string level = "")
	{
	    Console.WriteLine(level + variable.name + as_inline(level));
	}
    }

    class ADL
    {
	private List<ADLObject> _stack;
	public List<ADLObject> stack{
	    get{
		if (_stack == null)
		    _stack = new List<ADLObject>();
		return _stack;
	    }
	    private set{
		_stack = value;
	    }
	}
	public ADLObject    stacktop{get{
	    return stack[stack.Count-1];
	}}

	public ADL()
	{
	    make_built_ins();
	}

	private	Scanner	scanner;
	public ADLObject    parse(string io, string filename, ADLObject scope = null)
	{
	    scanner = new Scanner(this, io, filename);
	    stack = new List<ADLObject>((scope != null ? scope : top).ancestry);
	    ADLObject o = scanner.parse();
	    return o;
	}

	public void	emit()
	{
	    top.emit();
	}

	public void	error(string message)
	{
	    Console.WriteLine(message + " at " + scanner.location());
	    System.Environment.Exit(1);
	}

	public	ADLObject   resolve_name(string[] path_name, int levels_up = 1)
	{
	    // REVISIT: Implement
	    return null;
	}

	public	ADLObject   top;
	private	ADLObject   _object;
	private	ADLObject   regexp;
	private	ADLObject   syntax;
	private	ADLObject   reference;
	private	ADLObject   assignment;
	private	ADLObject   alias;
	private	ADLObject   alias_for;
	private void	make_built_ins()
	{
	    top = new ADLObject(null, "TOP", null, null);
	    _object = new ADLObject(top, "Object", null, null);
	    regexp = new ADLObject(top, "Regular Expression", _object, null);
	    syntax = new ADLObject(_object, "Syntax", regexp, null);
	    reference = new ADLObject(top, "Reference", _object, null);
	    assignment = new ADLObject(top, "Assignment", _object, null);
	    alias = new ADLObject(top, "Alias", _object, null);
	    alias_for = new ADLObject(alias, "For", reference, null);
	}
    }

    class Scanner
    {
	private	ADL	    adl;	// The ADL context we're building
	private	string	    input;	// The input string
	private	string	    filename;	// Name of file being processed, for error messages
	private int	    offset;	// Character offset in input
	private	string	    current;    // Name of the current token
	private	string	    value;	// Value of the current token
	private	ADLObject   last;	// The last ADLObject that was defined

	public Scanner(ADL _adl, string _io, string _filename)
	{
	    adl = _adl;
	    input = _io;
	    filename = _filename;
	    offset = 0;
	    current = null;
	    value = null;
	}

	public ADLObject parse()
	{
	    ADLObject	o;
	    ADLObject	last = null;	// REVISIT: Should this be stack.last?

	    opt_white();
	    while ((o = object_definition()) != null)
		last = o;
	    if (peek() != null)
		error("Parse terminated at " + peek());

	    return last;
	}

	private void	 opt_white()
	{
	    white();
	}

	private string	white()
	{
	    return expect("white");
	}

	private ADLObject object_definition()
	{
	    if (peek("close") || peek() == null)
		return null;

	    if (expect("semi") != null)
		return adl.stacktop;  // Empty definition
	    string[] object_name = path_name();
	    return definition(object_name);
	}

	private string[] path_name()
	{
	    List<string> names = new List<string>();
	    while (expect("scope") != null)
	    {
		opt_white();
		names.Add(".");
	    }
	    string  n, n1;
	    while ((n = expect("symbol")) != null || (n = expect("integer")) != null)
	    {
		opt_white();
		while ((n1 = expect("symbol")) != null || (n1 = expect("integer")) != null)
		{
		    opt_white();
		    n = n+" "+n1;
		}
		names.Add(n);
		if (expect("scope") != null)
		    opt_white();
		else
		    break;
	    }
	    return names.Count == 0 ? null : names.ToArray();
	}

	private ADLObject definition(string[] object_name)
	{
	    return null;
/*
	    var save = m_adl.stack.dup;

	    if ((defining = reference(object_name)) != null || (defining = alias_from(object_name)) != null)
	    {
		inheriting = peek("inherites");
		supertype_name = type;

		if (inheriting || supertype_name || peek("lbrack"))
		{
		    defining = m_adl.start_object(object_name, supertype_name);
		    has_block = block(object_name);
		    defining.is_array = array_indicator;
		    is_array = array_indicator;
		    m_adl.end_object;
		    is_assignment = Assignment(defining);
		} else if (peek("open"))
		{
		    defining = m_adl.resolve_name(obbject_name, 0);
		    if ((m_adl.stack && defining.ancestry) != m_adl.stack)
		    {

		    }
		    m_adl.stack.replace(defining.anerstry);
		    has_block = block(object_name);
		}
		else if (object_name && peek("equals") || peek("approx"))
		{
		    reopen = m_adl.resolve_name(object_name[0...- 2], 0);
		    m_adl.stack.replace(reopen.ancestry);
		    varialbe = m_adl.resolve_name([object_name[0...- 2]],0);
		    is_assignment = Assignment(variable);
		    defining = Assignment(variable);
		}
		else if (object_name)
		{
		    defining = m_adl.resolve_name(object_name, 0);
		}

		if (!has_block || is_array || is_assignment)
		{
		    peek "close" || require "semi"
		}
	    }
	    m_adl.stack.replace(save);
	    opt_white();

	    return defining || true;
    */
	}

/*
	private void type()
	{
	    if (expect("inherits"))
	    {
		opt_white();
		supertype_name = path_name;
		if (!(supertype_name || peek("semi") || peek("open") || peek("approx") || peek("equals"))) Console.WriteLine("Expected supertype, body or ;");
		return supertype_name || ["Object"]
	    }
	}

	private bool block(object_name)
	{
	    if (has_block == expect("open"))
	    {
		opt_white();
		while ()
		{

		}
		require "close";
		opt_white();
		return true;
	    }
	}

	private bool array_indicator()
	{
	    if (expect("lbrack"))
	    {
		opt_white();
		require "rbrack";
		opt_white();
		return true;
	    }
	}
	private bool reference(object_name)
	{
	    if (m_operator == (expect('arrow') || expect('darrow')))
	    {
		opt_white();
		reference_to = path_name;
		if (reference_to) Console.WriteLine("expected path name for Reference");

		reference_object = m_adl.resolve_name()reference_to,0);

		if (!object_name && reference_object.parent == m_adl.stack.last)
		{
		    Console.WriteLine("Reference to " + reference_to + ' ' + " in " + reference_object.parent.pathname + " cannot have the same name");
		}

		defining = m_adl.start_object(object_name || [reference_to.last],["Reference"]);
		defining.is_array = "=>";
		m_operator = "=>";

		defining.assign(defining, reference_object, true);

		has_block = block(object_name);
		m_adl.end_object;
		has_assignment = Assignment(defining);

		if (!has_block || has_assignment) peek("rblock") || require("semi");

		opt_white();
		defining();
	    }
	}

	private ADLObject alias_from(object_name) {
	    if (expect("rename")) return null;
	    defining = m_adl.start_object(object_name, ['Alias']);
	    defining.assign(defining, m_alias_for, false);
	    m_adl.end_object();
	    defining();
	}

	private ADLObject assignment(variable)
	{
	    if (m_operator == ((is_final = !!expect("equals")) || expect("approx")))
	    {
		opt_white();
		if (local_value) Console.WriteLine("Cannot reassign " + parent.name + "." + variable.name);

		controlling_syntax = variable;

		if (variable.is_syntax)
		{
		    val = expect("regexp");
		}
		else
		{
		    if (variable.is_reference)
		    {
			existing, p, final = parent.assigned_transitive(variable) || variable.assigned(variable);
			refine_from = (final && existing) || parent.supertypes[-1];
			refine_from = Array(refine_from)[0];
		    }
		    else
		    {
			if variable.supertypes.include ? (variable.parent) && parent.supertypes.include ? (variable.parent)
			  {
			    controlling_syntax = parent;
			}
			existing, p, final = parent.assigned_transitive(variable) || variable.assigned(variable);
			if (final) Console.WriteLine("Cannot override final assignment " + parent.name + "." + variable.name + "=" + existing.inspect);
		    }
		    val = value(controlling_syntax, refine_from);
		}
		parent.assign(variable, val, is_final);
	    }
	}

	private void value(variable, refine_from)
	{
	    if (variable.is_array && peek("lbrack"))
	    {
		val = f_array(variable, refine_from)
	    }
	    else
	    {
		a = atomic_value(variable, refine_from);
		if (variable.is_array) a = [a];
		a;
	    }
	}

	private void atomic_value(variable, refine_from)
	{
	    if (refine_from)
	    {
		if (supertype_name == type)
		{
		    defining = m_adl.start_object(null, supertype_name, true);
		    has_block = block nil;
		    assignment(defining);
		    m_adl.end_object;
		    val = defining;
		}
		else
		{
		    p = path_name;
		    if (!p) Console.WriteLine("Assignment to " + variable.name " must name an ADL object");
		    val = m_adl.resolve_name(p);
		}
		if (refine_from && !val.supertypes.contain(refine_from))
		{
		    Console.WriteLine("Assignment of " + val.inspect + " to " + variable.name + " must refine the existing final assignment of " + refine_from.name);
		}
		val;
	    }
	    else {
		syntax = variable.syntax_transitive;
		if (!syntax) Console.WriteLine(variable.inspect + " has no Syntax so cannot be assigned");
		val = next_token(syntax);
		if (!val) Console.WriteLine("Expected value matching syntax for " + variable.name);
		consume();
		opt_white();
		val;
	    }
	}


	private ADLObject f_array(variable, refine_from) {
	    if (!expect('lbrack')) return null;
	    array_value = [];
	    while (val = atomic_value(variable, refine_from))
	    {
		array_value << val;
		if (!expect('comma')) break;
	    }
	    if (!expect('rbrack')) Console.WriteLine("Array elements must separated by , and end in ]");
	    return array_value;
	}

	private int integer()
	{
	    var s = expect("integer") && eval(s);
	    reture s;
	}
	private int f_string()
	{
	    var s = expect("string") && eval(s);
	    reture s;
	}

	*/

	private static string Tokens =
	    "(?<" + "white" + ">" +	@"(?:\s|//.*)+" + ")|" +
	    "(?<" + "symbol" + ">" +	@"[_\p{L}][_\p{Nl}\p{L}]*" + ")|" +
	    "(?<" + "integer" + ">" +	@"-?(?:[1-9][0-9]*|0)" + ")|" +
	    "(?<" + "string" + ">" +	@"'(?:\\[0befntr\\']|\\[0-3][0-7][0-7]|\\x[0-9A-F][0-9A-F]|\\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F]|[^\\'])*'" + ")|" +
	    "(?<" + "open" + ">" +	@"{" + ")|" +
	    "(?<" + "close" + ">" +	@"}" + ")|" +
	    "(?<" + "lbrack" + ">" +	@"\[" + ")|" +
	    "(?<" + "rbrack" + ">" +	@"\]" + ")|" +
	    "(?<" + "inherits" + ">" +	@":" + ")|" +
	    "(?<" + "semi" + ">" +	@";" + ")|" +
	    "(?<" + "arrow" + ">" +	@"->" + ")|" +
	    "(?<" + "darrow" + ">" +	@"=>" + ")|" +
	    "(?<" + "rename" + ">" +	@"!" + ")|" +
	    "(?<" + "comma" + ">" +	@"," + ")|" +
	    "(?<" + "equals" + ">" +	@"=" + ")|" +
	    "(?<" + "approx" + ">" +	@"~=" + ")|" +
	    "(?<" + "scope" + ">" +	@"\." + ")|" +
	    "(?<" + "waste" + ">" +	@"." + ")";
	protected Regex	    _ParseRE;

	private Regex	parse_re()
	{
	    if (_ParseRE == null)
		_ParseRE = new Regex(Tokens, RegexOptions.Compiled | RegexOptions.IgnoreCase);
	    return _ParseRE;
	}

	public string location()
	{
	    string  to_here = input.Remove(offset);
	    int line_number = to_here.Count(c => c == '\n') + 1;
	    int column_number = to_here.LastIndexOf('\n');
	    column_number = column_number == -1 ? to_here.Length-1 : offset-column_number-1;
	    string line_text = input.Substring((offset - column_number), 100); // REVISIT: Remove from first \n to end: .sub(/\n.* / m, '');
	    return "line "+line_number+" column "+column_number+" of "+filename+":\n"+line_text+"\n";
	}

	private string next_token(Regex rule = null)
	{
	    Regex	re = rule != null ? rule : parse_re();
	    Match	match = re.Match(input, offset);

	    if (!match.Success)
	    {
		current = null;
		value = null;
	    }
	    else if (rule == null)
	    {
		foreach (Group g in match.Groups)
		{
		    if (g.Name == "0" || g.Captures.Count == 0)
			continue;
		    Capture	capture = g.Captures[0];
		    current = g.Name;
		    value = capture.Value;
		    break;
		}
	    }
	    else
	    {
		current = rule.ToString();
		value = match.ToString();
	    }
	    Console.WriteLine("next_token returning "+current+" '"+value+"'");
	    return value;
	}

	// Report a parse failure
	public void error(string message)
	{
	    adl.error(message);
	}

	// Consume the current token, returning the value
	public string consume()
	{
	    if (current == null)
		next_token();

	    // puts "consuming #{@current.inspect}" unless @current == 'white'

	    current = null;
	    string v = value;
	    value = null;
	    offset += v.Length;
	    return v;
	}

	// If token is next, consume it and return the value, else return nil
	private string expect(string token)
	{
	    if (current == null)
		next_token();
	    if (current == null || @current != token)	// EOF or wrong token
		return null;
	    string v = value;
	    consume();
	    opt_white();
	    return v;
	}

	private string require(string token)
	{
	    string    t = expect(token);
	    if (t == null)
		  error("Expected "+token);
	    return t;
	}

	// Without consuming it, return the name of the next token
	private string	 peek()
	{
	    if (current == null)
		next_token();
	    return current;
	}

	// Return true if the next token is 'token', without consuming it.
	private bool	 peek(string token)
	{
	    if (current == null)
		next_token();
	    return current != null ? current == token : false;
	}
    }

    class Test {
	static  void Main(string[] args)
	{
	    ADL		adl = new ADL();
	    ADLObject   scope = null;
	    foreach (string arg in args)
		scope = adl.parse(File.ReadAllText(arg), arg, scope);
	    if (scope != null)
	     	scope.emit();
		adl.top.emit();
	}
    }
}
