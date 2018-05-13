/*
 * Aspect Definition Language.
 *
 * Instantiate the ADL::Context and ask it to parse some ADL text.
 * The Context will use an ADL::Scanner to parse the input.
 *
 * The ADL text will be compiled into a tree of Objects, including Assignments,
 * which your code can then traverse to produce output. Start traversing the
 * tree at context.top(), or at the last open namespace returned from parse().
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Collections;

namespace ADL
{
    // A builtin value is either a string, an Object, a Regex or an Array of those.
    // All value subclasses retain the original text of the assignment.
    // Implementations may define additional subclasses.
    // Some value subclasses may also create a semantic implementation.
    abstract class Value
    {
	protected		    Value() {}
	public abstract string	    representation();
    }

    class StringValue : Value
    {
	private	string	    lexical;
	public		    StringValue(string _lexical) { lexical = _lexical; }
	public override string	    representation() { return lexical; }

	string		    _value;
	public string	    value{get{
	    if (_value == null)
		_value = lexical;    // REVISIT: store and return the composed form after processing escapes
	    return lexical;
	}}
    };

    class ObjectValue : Value
    {
	private Object	    reference;
	public		    ObjectValue(Object _reference) { reference = _reference; }
	public override string	    representation()
	{
	    if (reference.is_object_literal)
		return reference.as_inline();
	    else
		return reference.pathname();
	}

	public	Object	    obj{get{ return reference; }}
    };

    class RegexValue : Value
    {
	private string	    lexical;
	public		    RegexValue(string _lexical) { lexical = _lexical; }
	public override string	    representation() { return lexical; }

	// Semantics:
	private	Regex	    _regex;
	public Regex	    regex{
	    get{
		if (_regex == null)
		    _regex = new Regex(@"\G(?:"+lexical+")");
		return _regex;
	    }
	}
    };

    class ArrayValue : Value
    {
	private List<Value> array;
	public		    ArrayValue(List<Value> _array) { array = _array; }
	public override string	    representation() {
	    // REVISIT: Include newlines and indentation here, and let the caller further indent it.
	    return "["+
		String.Join(", ", array.Select(v => v.representation()).ToArray()) +
		"]";
	}

	// Semantics:
	public	int	    size() { return array.Count; }
	public	Value	    element(int i) { return array[i]; }
	public	void	    Add(Value element) { array.Add(element); }
    }

    class   Assigned
    {
	Value		v;
	Object		o;
	bool		f;
	public		Assigned(Value _v, Object _o, bool _f)
	{
	    v = _v; o = _o; f = _f;
	}
	public	Value	value{get{return v;}}
	public	Object	obj{get{return o;}}
	public	bool	final{get{return f;}}
    }

    class Object
    {
	public	Object		parent{get; private set;}
	public	string		name{get; private set;}
	public	Object		zuper{get; private set;}
	public	Object		aspect{get; private set;}
	public	RegexValue	syntax{get; private set;}
	public	bool		is_array{get; set;}
	public	bool		is_sterile{get; private set;}
	public	bool		is_complete{get; private set;}
	public	List<Object>	children{get;}

	public	Object(
		Object		_parent,
		string		_name,
		Object		_zuper,
		Object		_aspect = null
	)
	{
	    parent = _parent;
	    name = _name;
	    zuper = _zuper;
	    aspect = _aspect;
	    if (parent != null)
	    	parent.adopt(this);
	    children = new List<Object>();
	}

	// Return a string with all ancestor names from the top.
	public virtual string pathname()
	{
	    return (parent != null && !parent.is_top ? parent.pathname() + "." : "") +
		(name != null ? name : "<anonymous>");
	}

	public string pathname_relative_to(Object them)
	{
	    return pathname();
	/*
        // REVISIT: Implement emitting relative names properly
	    // Trim a common prefix off the pathnames and concatenate the residual
	    List<Object> mine = new List<Object>(ancestry);
	    List<Object> theirs = new List<Object>(them.ancestry);
	    int	    common = 0;
	    while (mine.Any() && theirs.Any() && mine.First() == theirs.First()) {
		mine.RemoveAt(0);
		theirs.RemoveAt(0);
		common += 1;
	    }
	    // REVISIT: If any residual object is anonymous, we need to use an ascending scope to get to the start point
	    return new String('.', common) + String.Join(".", mine.Select(m => m.name));
	*/
	}

	// Adopt this child
	public void adopt(Object _child)
	{
	    if (_child.name != null && child(_child.name) != null)
		throw new System.ArgumentException("Can't have two children called "+_child.name);

	    children.Add(_child);
	}

	public Object child(string name)
	{
	    return name == null ? null : children.Find(m => m.name == name);
	}

	// Search this object and its supertypes for an object of the given name
	// REVISIT: This is where we need to implement aliases
	public Object child_transitive(string name)
	{
	    Object	m = child(name);
	    if (m != null)
		return m;
	    if (zuper != null)
		return zuper.child_transitive(name);
	    return null;
	}

	// Get list of ancestors starting with the outermost and ending with this object
	private List<Object>	_ancestry;
	public List<Object>	ancestry{
	    get {
		if (_ancestry == null)
		{	// Build and memoize the result
		    if (parent != null)
			_ancestry = new List<Object>(parent.ancestry);
		    else
			_ancestry = new List<Object>();
		    _ancestry.Add(this);
		}
		return _ancestry;
	    }
	}

	// Get list of supertypes starting with this object and ending with "Object"
	private List<Object>	_supertypes;
	public List<Object> supertypes{
	    get {
		if (_supertypes == null)
		{	// Build and memoize the result
		    _supertypes = new List<Object>{this};
		    if (zuper != null)
			_supertypes.AddRange(zuper.supertypes);
		}
		return _supertypes;
	    }
	}

	// Search assignments
	public Assigned	assigned(Object variable)
	{
	    Object  outermost = supertypes.Last();	// This will always be "Object"

	    if (variable.parent == outermost)
	    {	    // Check for an assignment to the special built-in variables
		switch (variable.name)
		{
		case "Name":	    return name == null ? null : new Assigned(new StringValue(name), outermost, true);
		case "Parent":	    return parent == null ? null : new Assigned(new ObjectValue(parent), outermost, true);
		case "Super":	    return zuper == null ? null : new Assigned(new ObjectValue(zuper), outermost, true);
		case "Aspect":	    return aspect == null ? null : new Assigned(new ObjectValue(aspect), outermost, aspect != null);
		case "Syntax":	    return syntax == null ? null : new Assigned(syntax, outermost, true);
		// REVISIT: These should be True or False (subtype of Boolean)
		case "Is Array":    return null; // return new Assigned(new Value(is_array), outermost, true);
		case "Is Sterile":  return null; // return new Assigned(new Value(is_sterile), outermost, true);
		case "Is Complete": return null; // return new Assigned(new Value(is_complete), outermost, true);
		}
	    }

	    // Find an existing assignment:
	    Assignment	existing = children.Find(m => m is Assignment && (m as Assignment).variable == variable) as Assignment;
	    if (existing == null)
		return null;
	    return new Assigned(existing.value, existing.parent, existing.is_final);
	}

	public Assigned assigned_transitive(Object variable)
	{
	    Assigned	a = assigned(variable);
	    if (a != null)
		return a;
	    if (zuper != null)
		return zuper.assigned_transitive(variable);
	    return null;
	}

	public Object    assign(Object variable, Value value, bool is_final)
	{
	    Assigned	t = assigned(variable);
	    Value	a = t != null ? t.value : null;	    // Existing value

	    if (a != null && a != value && variable != this)
		throw new System.ArgumentException(pathname()+zuper_name()+" cannot have two assignments to "+variable.pathname());

	    if (variable.is_syntax)
	    {
		syntax = value as RegexValue;
		return variable;    // Not used, but flags that we succeeded
	    }
	    else
		return new Assignment(this, variable, value, is_final);
	}

	// Is this object a subtype of the "Reference" built-in?
	public bool is_reference{get{
	    List<Object> s = supertypes;
	    return supertypes[supertypes.Count-1].name == "Object" &&
		supertypes[supertypes.Count-2].name == "Reference";
	}}

	// Is this object a subtype of the "Syntax" built-in?
	public bool is_syntax{get{
	    List<Object> s = supertypes;
	    return s.Count >= 3 &&
		s[supertypes.Count-1].name == "Object" &&
		s[supertypes.Count-2].name == "Regular Expression" &&
		s[supertypes.Count-3].name == "Syntax";
	}}

	public bool is_object_literal{get{
	    // Object Literals have no parent and no name.
	    return parent == null && name == null;
	}}

	public Regex syntax_transitive()
	{
	    if (syntax != null)
		return syntax.regex;
	    if (zuper != null)
		return zuper.syntax_transitive();
	    return null;
	}

	// Get the built-in supertype of all Assignments
	public Object   assignment_supertype()
	{
	    Object	p = this;
	    while (p.parent != null)
		p = p.parent;
	    return child("Assignment");
	}

	private bool is_top{get{
	    return parent == null;
	}}

	// Manage generation of textual output
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

	public virtual string as_inline(string level = "")
	{
	    Assignment self_assignment = children.Find(m => m is Assignment && (m as Assignment).variable == this) as Assignment;
	    List<Object> others = children.Where(m => m != self_assignment).ToList();

	    return ":" +
		zuper.name +
		(!others.Any()
		    ? ""
		    :	"{" +
			String.Join(
			    "; ",
			    others.Select(
				m =>
				    m is Assignment
				    ? (m as Assignment).variable.name + (m as Assignment).as_inline()
				    : ""    // An object literal may not have new variables
			    )
			) +
			"}"
		) +
		(self_assignment != null ? self_assignment.as_inline() : "");
	}

	public virtual void emit(string level = null)
	{
	    if (level == null)
	    {
		foreach (Object m in children)
		    m.emit("");
		return;
	    }

	    Assignment self_assignment = children.Find(m => m is Assignment && (m as Assignment).variable == this) as Assignment;
	    List<Object> others = children.Where(m => m != self_assignment).ToList();
	    bool    has_attrs = others.Any() || syntax != null;

	    Console.Write(level);
	    Console.Write(name);
	    string  zn = zuper_name();
	    Console.Write(zn);
	    if (has_attrs)
		Console.WriteLine(zn != null ? " {" : "{");

	    if (syntax != null)
		Console.WriteLine(level+"\tSyntax = /"+syntax.representation()+"/;");

	    foreach (Object m in others)
		m.emit(level+"\t");

	    if (has_attrs)
		Console.Write(level+"}");
	    if (self_assignment != null && children.Any())
		Console.Write(self_assignment.as_inline());
	    Console.WriteLine(!has_attrs || self_assignment != null ? ";" : "");
	}
    }

    class Assignment : Object
    {
	public	Object	    variable{get; private set;}
	public	Value	    value{get; private set;}
	public	bool	    is_final{get; private set;}

	public Assignment(Object _parent, Object _variable, Value _value, bool _is_final)
	    : base(_parent, null, _parent.assignment_supertype())
	{
	    variable = _variable;
	    if (_value == null) throw new System.NotImplementedException("REVISIT: Null value assigned");
	    value = _value;
	    is_final = _is_final;
	}

	// Return a string with all ancestor names from the top.
	public override string pathname()
	{
	    return
		(parent != null ? parent.pathname() : "-") +
		"." +
		(variable != null ? variable.name : "<no var>") +
		(is_final ? "= " : "~= ") +
		(value != null ? value.ToString() : "<no value>");
	}

	public override string as_inline(string level = "")
	{
	    return (is_final ? " = " : " ~= ") + value.representation();
	}

	public override void emit(string level = "")
	{
	    Console.WriteLine(level + variable.name + as_inline(level) + ";");
	}
    }

    class Context
    {
	public Context()
	{
	    make_built_ins();
	}

	public	Object	    top;

	private List<Object> _stack;
	public List<Object> stack{
	    get{
		if (_stack == null)
		    _stack = new List<Object>();
		return _stack;
	    }
	    set{
		_stack = new List<Object>(value);
	    }
	}
	public Object	    stacktop{get{
	    return stack[stack.Count-1];
	}}

	private	Scanner	    scanner;
	public Object	    parse(string io, string filename, Object scope = null)
	{
	    scanner = new Scanner(this, io, filename);
	    stack = new List<Object>((scope != null ? scope : top).ancestry);
	    Object  o = scanner.parse();
	    return o;
	}

	public void	    error(string message)
	{
	    Console.WriteLine(message + " at " + scanner.location());
	    // throw new System.ArgumentException("Failed here");
	    System.Environment.Exit(1);
	}

	public	Object	    resolve_name(List<string> path_name, int levels_up = 1)
	{
	    Object	    o = stacktop;
	    for (int i = 0; i < levels_up; i++)
		o = o.parent;
	    if (path_name.Count == 0)
		return o;
	    List<string>	remaining = new List<string>(path_name);
	    bool	no_ascend = false;
	    if (remaining[0] == ".")	    // Do not implicitly ascend
	    {
	        no_ascend = true;
		remaining.RemoveAt(0);
		while (remaining.Count > 0 && remaining[0] == "." && o != null)    // just ascend explicitly
		{
		    remaining.RemoveAt(0);
		    o = o.parent;
		}
	    }
	    if (remaining.Count == 0)
		return o;

	    Object	start_parent = o;
	    Object	m = null;

	    // Ascend the parent chain until we fail or find our first name:
	    if (!no_ascend)
	    {
	        while (remaining.Count > 0 && remaining[0] != (m = o).name)
		{
		    if ((m = o.child_transitive(remaining[0])) != null)
			break;	    // Found!
		    if (o.parent == null)
		        error("Failed to find "+remaining[0]+" from "+stacktop.pathname());
		    o = o.parent;    // Ascend
		}

	        o = m;
		remaining.RemoveAt(0);
	    }
	    if (o == null)
		error("Failed to find "+remaining[0]+" from "+start_parent.pathname());
	    if (remaining.Count == 0)
		return o;

	    // Now descend from the current position down the named children
	    // REVISIT: If we descend a supertype's child, this becomes contextual!
	    foreach (var n in remaining)
	    {
	        m = o.child(n);
		if (m == null)
		    error("Failed to find "+n+" in "+o.pathname());
		o = m;   // Descend
	    }

	    return o;
	}

	// Called when the name and supertype have been parsed
	public virtual Object	start_object(List<string> object_name, List<string> supertype_name, bool orphan = false)
	{
	    Object	parent;
	    if (object_name != null)
	    {
		List<string>	parent_name = new List<string>(object_name);
		parent_name.RemoveAt(parent_name.Count-1);
		parent = resolve_name(parent_name, 0);
	    }
	    else
		parent = stacktop;

	    // Resolve the supertype_name to find the zuper:
	    Object	zuper = supertype_name != null ? resolve_name(supertype_name, 0) : _object;

	    string	local_name = object_name != null ? object_name[object_name.Count-1] : null;
	    Object	o;
	    if (local_name != null && (o = parent.child(local_name)) != null)
	    {
		if (supertype_name != null && o.zuper.name != joined_name(supertype_name))
		    error("Cannot change supertype of "+local_name+" from "+o.zuper.name+" to "+joined_name(supertype_name));
	    }
	    else
	    {
	        o = new Object(orphan ? null : parent, local_name, zuper);
	    }
	    stack.Add(o);
	    return o;
	}

	public virtual void	end_object()
	{
	    stack.RemoveAt(stack.Count-1);
	}

	private	Object   _object;
	private	Object   regexp;
	// private	Object   syntax;
	// private	Object   reference;
	// private	Object   assignment;	// We find this by name
	// private	Object   alias;
	// private	Object   alias_for;
	private void	make_built_ins()
	{
	    top = new Object(null, "TOP", null, null);
	    _object = new Object(top, "Object", null, null);
	    regexp = new Object(top, "Regular Expression", _object, null);
	    // syntax =
		new Object(_object, "Syntax", regexp, null);
	    // reference =
		new Object(top, "Reference", _object, null);
	    // assignment =
		new Object(top, "Assignment", _object, null);
	    // alias = new Object(top, "Alias", _object, null);
	    // alias_for = new Object(alias, "For", reference, null);
	}

	public string joined_name(List<string> parts)
	{
	    return String.Join(" ", parts.ToArray());
	}
    }

    class Scanner
    {
	private	Context	    context;	// The ADL context we're building
	private	string	    input;	// The input string
	private	string	    filename;	// Name of file being processed, for error messages
	private int	    offset;	// Character offset in input
	private	string	    current;    // Name of the current token
	private	string	    value;	// Value of the current token

	public Scanner(Context _context, string _io, string _filename)
	{
	    context = _context;
	    input = _io;
	    filename = _filename;
	    offset = 0;
	    current = null;
	    value = null;
	}

	public string location()
	{
	    string  to_here = input.Remove(offset);
	    int line_number = to_here.Count(c => c == '\n') + 1;
	    int column_number = to_here.LastIndexOf('\n');
	    column_number = column_number == -1 ? to_here.Length-1 : offset-column_number-1;
	    string line_text = input.Substring((offset - column_number));
	    if (line_text.IndexOf("\n") > 0)
		line_text = line_text.Remove(line_text.IndexOf("\n"));
	    return "line "+line_number+" column "+column_number+" of "+filename+":\n"+line_text+"\n";
	}

	public Object	parse()
	{
	    Object	o;
	    Object	last = null;	// REVISIT: Should this be stack.last?

	    try {
		opt_white();
		while ((o = definition()) != null)
		    last = o;
		if (peek() != null)
		    error("Parse terminated at " + peek());
	    } catch (Exception e) {
		error(
		    e.Message+" at "+location()
		    + e.StackTrace
		);
	    }

	    return last;
	}

	private void	opt_white()
	{
	    white();
	}

	private string	white()
	{
	    return expect("white");
	}

	private Object	definition()
	{
	    if (peek("close") || peek() == null)
		return null;

	    if (expect("semi") != null)
		return context.stacktop;  // Empty definition
	    List<string>    object_name = path_name();
	    return body(object_name);
	}

	private List<string> path_name()
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
	    return names.Count == 0 ? null : names;
	}

	private Object	body(List<string> object_name)
	{
	    List<Object>    save = new List<Object>(context.stack);  // Save the stack
	    Object	    defining = null;

	    if ((defining = reference(object_name)) != null)
	    {
		// Done inside reference
	    }
	    else if ((defining = alias_from(object_name)) != null)
	    {
		// Done inside alias_from
	    }
	    else
	    {
		bool		inheriting = peek("inherits");
		List<string>	supertype_name = supertype();
		bool		is_array = false;
		bool		has_block = false;
		bool		is_assignment = false;

		if (inheriting || supertype_name != null || peek("lbrack"))
		{
		    defining = context.start_object(object_name, supertype_name);
		    has_block = block(object_name);
		    is_array = array_indicator();
		    defining.is_array = is_array;
		    context.end_object();
		    is_assignment = assignment(defining) != null;
		}
		else if (peek("open"))
		{
		    defining = context.resolve_name(object_name, 0);
		    // if (defining.ancestry is not a prefix of context.stack)
			// REVISIT: This is contextual extension
		    context.stack = defining.ancestry;
		    has_block = block(object_name);
		}
		else if (object_name != null && (peek("equals") || peek("approx")))
		{
		    List<string>	name_prefix = null;
		    name_prefix = new List<string>(object_name);
		    name_prefix.RemoveAt(object_name.Count-1);

		    Object	reopen = context.resolve_name(name_prefix, 0);
		    context.stack = reopen.ancestry;
		    Object	variable;
		    variable = context.resolve_name(new List<string>{object_name[object_name.Count-1]}, 0);
		    defining = assignment(variable);
		    is_assignment = defining != null;
		}
		else if (object_name != null)
		    defining = context.resolve_name(object_name, 0);

		if (!has_block || is_array || is_assignment)
		    if (!peek("close"))
			require("semi");
	    }
	    context.stack = save;
	    opt_white();

	    return defining;
	}

	private List<string> supertype()
	{
	    if (expect("inherits") != null)
	    {
		opt_white();
		List<string>	supertype_name = path_name();
		if (supertype_name == null && !peek("semi") && !peek("open") && !peek("approx") && !peek("equals"))
		    error("Expected supertype, body or ;");
		return supertype_name != null ? supertype_name : new List<string>{"Object"};
	    }
	    return null;
	}

	private bool block(List<string> object_name)
	{
	    bool    has_block = expect("open") != null;
	    if (has_block)
	    {
		opt_white();
		while (definition() != null)
		    ;
		require("close");
		opt_white();
		return true;
	    }
	    return false;
	}

	private bool array_indicator()
	{
	    if (expect("lbrack") != null)
	    {
		opt_white();
		require("rbrack");
		opt_white();
		return true;
	    }
	    return false;
	}

	private Object	reference(List<string> object_name)
	{
	    Object	defining = null;

	    string  op;
	    if ((op = expect("arrow")) != null || (op = expect("darrow")) != null)
	    {
		opt_white();
		List<string>	reference_to = path_name();
		if (reference_to == null)
		    error("expected path name for Reference");

		// Find what this is a reference to
		Object		reference_object = context.resolve_name(reference_to, 0);

		// An eponymous reference uses reference_to for object_name. It better not be local.
		if (object_name == null && reference_object.parent == context.stacktop)
		    error("Reference to " + context.joined_name(reference_to) + ' ' + " in " + reference_object.parent.pathname() + " cannot have the same name");

		List<string>	def_name = object_name;
		if (def_name == null)
		    def_name = new List<string>{reference_to[reference_to.Count-1]};
		defining = context.start_object(def_name, new List<string>{"Reference"});

		defining.is_array = op == "=>";

		defining.assign(defining, new ObjectValue(reference_object), true);

		bool	has_block = block(object_name);
		context.end_object();
		bool	has_assignment = assignment(defining) != null;

		if (!has_block || has_assignment)
		    if (!peek("rblock"))
			require("semi");

		opt_white();
		return defining;
	    }
	    return null;
	}

	private Object	alias_from(List<string> object_name)
	{
	    if (expect("rename") == null)
		return null;
	    throw new System.NotImplementedException("REVISIT: Aliases");
/*
	    Object	defining = null;
	    defining = context.start_object(object_name, ['Alias']);
	    defining.assign(defining, m_alias_for, false);
	    context.end_object();
	    return defining;
*/
	}

	private Object	assignment(Object variable)
	{
	    string  op;
	    if ((op = expect("equals")) != null || (op = expect("approx")) != null)
	    {
		bool	is_final = op == "=";
		opt_white();

		Object	parent = context.stacktop;

		Assigned	existing = parent.assigned(variable);
		if (existing != null)
		    error("Cannot reassign " + parent.name + "." + variable.name+" from "+existing.ToString());

		Object	    controlling_syntax = variable;
		Object	    refine_from = null;
		Value	    val;

		if (variable.is_syntax)
		{	    // Parse a Regular Expression
		    val = new RegexValue(require("regexp"));
		}
		else
		{
		    if (variable.is_reference)
		    {
			Assigned	overriding = parent.assigned_transitive(variable);
			if (overriding == null)
			    overriding = variable.assigned(variable);
			if (overriding != null && overriding.final)	// Final
			    if (overriding.value is ArrayValue)	// the Value is an array
				refine_from = ((overriding.value as ArrayValue).element(0) as ObjectValue).obj;
			    else
				refine_from = (overriding.value as ObjectValue).obj;
			// Otherwise just refine_from Object
			if (refine_from == null)
			{
			    List<Object> st = parent.supertypes;
			    refine_from = st[st.Count-1];
			}
		    }
		    else
		    {
			if (variable.supertypes.Contains(variable.parent) && parent.supertypes.Contains(variable.parent))
			    controlling_syntax = parent;
			existing = parent.assigned_transitive(variable);
			if (existing == null)
			    existing = variable.assigned(variable);
			if (existing != null && existing.final)	// Is Final
			    error("Cannot override final assignment " + parent.name + "." + variable.name + "=" + existing.ToString());
		    }
		    val = parse_value(controlling_syntax, refine_from);
		}
		return parent.assign(variable, val, is_final);
	    }
	    return null;
	}

	private Value	parse_value(Object variable, Object refine_from)
	{
	    if (variable.is_array && peek("lbrack"))
		return parse_array(variable, refine_from);
	    Value   val = atomic_value(variable, refine_from);
	    if (variable.is_array)
		return new ArrayValue(new List<Value>{val});
	    return val;
	}

	private Value atomic_value(Object variable, Object refine_from)
	{
	    Value	val;
	    if (refine_from != null)
	    {
		List<string>	supertype_name = supertype();
		Object		defining;
		if (supertype_name != null)
		{	// Literal object
		    defining = context.start_object(null, supertype_name, true);
		    block(null);
		    assignment(defining);
		    context.end_object();
		    val = new ObjectValue(defining);
		}
		else
		{
		    List<string>	p = path_name();
		    if (p == null)
			error("Assignment to " + variable.name + " must name an ADL object");
		    val = new ObjectValue(defining = context.resolve_name(p));
		    // If the variable is a reference and refine_from is set, the object must be a subtype
		}
		if (refine_from != null && !defining.supertypes.Contains(refine_from))
		{
		    error("Assignment of " + val.ToString() + " to " + variable.name + " must refine the existing final assignment of " + refine_from.name);
		}
		return val;
	    }
	    else
	    {
		Regex   syntax = variable.syntax_transitive();
		if (syntax == null)
		    error(variable.pathname()+variable.zuper_name() + " has no Syntax so cannot be assigned");
		string	s = next_token(syntax);
		if (s == null)
		    error("Expected a value matching the syntax for an " + variable.name);
		consume();
		opt_white();
		// REVISIT: Use the Value subtype defined for this variable
		return new StringValue(s);
	    }
	}

	private Value parse_array(Object variable, Object refine_from)
	{
	    if (expect("lbrack") == null)
		return null;

	    List<Value>	array_value = new List<Value>();
	    Value	val;
	    while ((val = atomic_value(variable, refine_from)) != null)
	    {
		array_value.Add(val);
		if (expect("comma") == null)
		    break;
	    }
	    if (expect("rbrack") == null)
		error("Array elements must separated by , and end in ]");
	    return new ArrayValue(array_value);
	}

	private Value integer()
	{
	    string  s = expect("integer");
	    return new StringValue(s);
	}

	private static string Tokens =
	    "(?<" + "white" + ">" +	@"(?:\s|//.*)+" + ")|" +
	    "(?<" + "symbol" + ">" +	@"[_\p{L}][_\p{L}\p{N}\p{Mn}]*" + ")|" +
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

	// Match a token to use as our lookahead token, disregarding any existing one.
	// Use the token rule if provided, otherwise use the standard one
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
	    // Console.WriteLine("next_token returning "+current+" '"+value+"'");
	    return value;
	}

	// Report a parse failure
	public void error(string message)
	{
	    context.error(message);
	}

	// Consume the current token, returning the value
	public string consume()
	{
	    if (current == null)
		next_token();

	    current = null;
	    string v = value;
	    value = null;
	    offset += v.Length;
	    return v;
	}

	// If token is next, consume it and return the value, else return nil
	private string expect(string token)
	{
	    if (token == "regexp")
		return expect_regexp();

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

	private bool expect(Regex rule)
	{
	    value = next_token(rule);
	    if (value == null)
		return false;
	    offset += value.Length;
	    return true;
	}

	private void	require(Regex rule, string context)
	{
	    if (!expect(rule))
		  error("In "+context+", expected "+rule.ToString()+" looking at "+input.Substring(offset, 10));
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
	    // N.B. You cannot peek for a regexp
	    return peek() == token;
	}

	private string	expect_regexp()
	{
	    Regex   slash = new Regex(@"\G/");
	    int	    start = offset;

	    opt_white();
	    if (!expect(slash))
		return null;
	    string  regex = regexp_sequence();
	    if (regex != null)
		if (!expect(slash))
		    regex = null;
	    current = null;	    // Don't leave a lookahead token
	    if (regex == null)
	    {
		offset = start;	    // Don't consume any of the text
		return null;
	    }
	    return regex;
	}

	private string regexp_sequence()
	{
	    int start = offset;
	    regexp_alternate();
	    while (expect(new Regex(@"\G\|")))
		regexp_alternate();
	    return input.Substring(start, offset-start);
	}

	private void	regexp_alternate()
	{
	    while (regexp_atom())
		;
	}

	private bool	regexp_atom()
	{
	    if (!regexp_char() && !regexp_class() && !regexp_group())
		return false;
	    expect(new Regex(@"\G[*+?]"));	    // Optional multiplicity
	    return true;
	}

	private	string	regexp_char_regex =
		"("
		+ @"\\s"				// Whitespace. Explicit space not allowed!
		+ @"|\\[0-3][0-7][0-7]"			// Octal character
		+ @"|\\x[0-9A-F][0-9A-F]"		// Hexadecimal character
		+ @"|\\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F]"// Unicode codepoint
                + @"|\\[pP]{[A-Za-z_]+}"		// Unicode category or block
		+ @"|\\[0befntr\\*+?()|/\[]"		// Various control characters or escaped specials
		+ @"|[^*+?()|/\[ ]"			// Any other non-special character
		+ ")";

	private bool	regexp_char()
	{
	    return expect(new Regex(@"\G"+regexp_char_regex));
	}

	private bool	regexp_class()
	{
	    if (!expect(new Regex(@"\G\[")))
		return false;
	    expect(new Regex(@"\G\^"));			// Negate the class
	    expect(new Regex(@"\G-"));			// A hyphen must be first
	    Regex   regexp_class_char = new Regex(
		    @"\G"
		    + @"(?![-\]])"			// Not a closing bracket or a hyphen
		    + "("
		    + regexp_char_regex			// But any other regexp char
		    + "|[+*?()/|]"			// including some special chars
		    + ")"
		);
	    do {
		require(regexp_class_char, "regexp class");
		if (expect(new Regex(@"\G-")))
		    require(regexp_class_char, "regexp class range end");
	    } while (!expect(new Regex(@"\G\]")));
	    return true;
	}

	private bool	regexp_group()
	{
	    if (!expect(new Regex(@"\G\(")))		// parenthesis opens the group
		return false;
	    expect(new Regex(@"\G\?<[_\p{L}\p{N}]+>|\?!")); // regexp_group_type (capture or negative lookahead)
	    regexp_sequence();
	    require(new Regex(@"\G\)"), "regexp group");	// The group must be closed
	    return true;
	}
    }

    class Test {
	static  void Main(string[] args)
	{
	    Context	context = new Context();
	    Object      scope = null;
	    bool	show_all = false;
	    foreach (string arg in args)
	    {
		if (arg == "-a")
		    show_all = true;
		else
		    scope = context.parse(File.ReadAllText(arg), arg, scope);
	    }
	    if (show_all)
		context.top.emit();
	    else if (scope != null)
	     	scope.emit();
	}
    }
}
