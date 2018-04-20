<h3 align="center">Quick Introduction to the Aspect Definition Language</h3>

<center>
(c) Copyright, Clifford Heath, 1983-2018.
</center>

<div style="width: 50%; margin: 4em 25% 2em 25%;">
The Aspect Definition Language (ADL) is a language for
defining the classification, attributes and values of
arbitrary data objects. The language is extensible,
as are objects defined using the language. Contextual
extension (sometimes called Aspects or views) is also
supported. The syntax is free-form, except inside value
literals. Whitespace and newlines are considered as
equivalent to a single space and are ignored.
</div>

<table cellpadding="5">

<tr>
<th valign="top" width="15%"></td>
<th valign="top" width="45%">Description</td>
<th valign="top" width="40%">Examples</td>
</tr>

<!-- REVISIT: Comments? -->

<tr>
<th align="right" valign="top">Object</th>
<td valign="top">
<p>
Everything in ADL is an Object, including the
top supertype which is called "Object".
Any named object may have supertype objects,
so there is no distinction between types (classes)
and instances. Any normal object may also contain
child objects.
</p>
<td valign="top">
<pre>
Object:
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top" width="10%">Parent</th>
<td valign="top">
<p>
Every Object has a value assigned to its Parent variable,
except the Origin object, which serves as the outermost
parent. The Parent of an object may not be re-assigned.
</p>
</tr>

<tr>
<th align="right" valign="top" width="10%">Built-ins</th>
<td valign="top">
<p>
Some Objects are built-in (including Object and Origin).
We'll indicate more as we proceed.
</p>
</td>
</tr>

<tr>
<!-- REVISIT: Consider renaming this Symbol, since it's global.
Or possibly, disallow references to Name as a variable or assignment?
But then, what about other built-ins, like Parent and Super?
Can we use local Context notation to contain these variables to local lookup only?
-->
<th align="right" valign="top" width="10%">Name</th>
<td valign="top">
<p>
An Object may have a Name, which can be one
or more words. The amount of whitespace between
the words may vary and is considered equivalent
to a single space.
A word is an sequence of alphanumeric or
underscore characters, and may even start with
a digit, so <strong>42</strong> is a valid Name.
</p>
<p>
An object may be anonymous, for purposes described
below.
</p>
<p>
A Name is unique within a given Parent, but anonymous
objects are unrestricted.
</p>
<td valign="top">
Declare an object called Person. It has Origin for
a Parent, and Object for a supertype.
<pre>Person:
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">New Objects</th>
<td valign="top">
<p>
An object is declared using colon <b>:</b>
after the name (if any) followed by the name
of its Super (supertype object).  If no supertype
name is given, the supertype is Object.
</p>
</td>
<td valign="top">
Declare an object called Employee with supertype Person:
<pre>
Employee: Person;
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Arrays</th>
<td valign="top">
<p>
A new Object may refer to an array (vector) of
its supertype, using square brackets after the
supertype name.  The objects in the array are
maintained in the sequence they were defined.
</p>
<p>
</p>
<td valign="top">
<pre>
Given Name: String[];
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Children</th>
<td valign="top">
Any Object may have one or more child objects.
</p>
<p>
A single child object may be created by declaring
it following a dot "." after the name of its parent,
or by opening the Aspect (namespace) of the parent
between braces (curly brackets), and declaring
children separated by semi-colons.
</p>
<p>
The parent name before the dot is not required to
have yet been declared. A place-holder object is
created that has an undefined (Origin) supertype.
The type of this parent remains incomplete until
it is defined explicitly, but the object may not
be otherwise referred to until its type is defined.
</p>
</td>
<td valign="top">
Declare two child objects for Person. Each is a
subtype of the built-in variable String (described below).
After the first statement, Person is a placeholder
with a Origin type. The second statement declares
that Person is a subtype of Object.
<pre>
Person.Family Name: String;
Person: {
    Given Name: String[];
}
</pre>
</tr>

<tr>
<th align="right" valign="top">Re-opening</th>
<td valign="top">
<p>
An object that is already defined cannot be redefined.
Using the same name again with the same parent will
re-open the existing object. If the supertype is
provided it must match the existing definition; the
supertype cannot be changed.
An object with no name cannot be re-opened.
</p>
<td valign="top">
We saw this in the previous example. Person was re-opened
to add Given Name.
<pre>
Employee {
    Employee Number: Integer;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Extension</th>
<td valign="top">
Any existing named object may be re-opened to allow
adding more children - just use its name without
re-declaring its supertype, and add the new children
in a brace-list, or after the "." operator.
</td>
<td valign="top">
<pre>
Person: {
    Given Name: String[] {
	Max Length = 48;
    }
}
Person {
    Family Name: String;
}
Person.Family Name.Max Length = 24;
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top" width="10%">Anonymity</th>
<td valign="top">
<p>
The object name is optional, which allows anonymous objects.
An anonymous object cannot subsequently be re-opened
or used as a supertype, so it is complete at the time
of its declaration.
</td>
<td valign="top">
<p>
Declare an anonymous object which is a supertype of
Person. Given Names is an array, so the assigned value
must also be an Array, declared using square brackets.
<pre>
: Person {
    Given Name = ['John'];
    Family Name = 'Smith';
}
</pre>
</p>
</td>
</tr>

<tr>
<th align="right" valign="top">Eponymous Naming</th>
<td valign="top">
<p>
As a special case, if just a name (no colon) is
used, and the name is of a Object that's not
already a child of this object, and the Object
is not being re-opened (by { or .), then a new
child is created having the same name as the
supertype. This is called eponymous naming.
</p>
</td>
<td valign="top">
<p>
Date is a predefined Variable type, so the
following declares that an Event has a Date
variable. This has the same effect as declaring
<br><strong>Date: Date;</strong><br>
Note that the supertype reference works in
this case because it has not yet been defined
locally, but if it had, a preceding dot would
force an explicit up traversal.
</p>
<pre>
Event: {
    Date
}
: Event { Date = 2001-03-19 }
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Variables</th>
<td valign="top">
<p>
There is a special built-in object called Variable.
Variable has a number of built-in supertypes.
</td>
<td valign="top">
<pre>
Variable: Object;
String: Variable;
Date: Variable;
Number: Variable;
    Integer: Number;
    Real: Number;
    Decimal: Number;
Enumeration: Variable;
Boolean: Enumeration;
Reference: Variable;
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Assignments</th>
<td valign="top">
<p>
Any object that contains a Variable (or that <strong>is</strong>
a Variable), or whose supertypes contain a Variable that
it inherits, may contain one Assignment to that variable.
An Assignment is a special anonymous child object
that associates a Value with that Variable for that Parent
object. Assignment has its own syntax, and cannot be
declared using the usual object definition syntax.
</p>
<p>
If an assignment is inherited, a local assignment overrides it.
If a variable has neither a local nor inherited assignment,
the value of that variable is undefined for that object.
</p>
<td valign="top">
Here's an anonymous Employee object, with assignments to
variables, including inherited Person variables. Each
element of an Array value conforms to the syntax for
that type of value.
<pre>
: Employee {
    Family Name = 'Smith';
    Given Name = ['John'];
    Employee Number = 42317;
}
</pre>
Here's a variable with an assignment to itself:
<pre>
Forty Two: Integer = 42;
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Tentative Assignment</th>
<td valign="top">
<p>
The Assignments in the previous example were Final.
If this object was named and had a subtype, the subtype
inherits these assignments, but it cannot re-assign
those variables. Assignment to Reference variables has
a special version of this rule.
</p>
<p>However, it's also possible that an assignment is
tentative by defining it using the tentative assignment
operator <strong>~=</strong>.  A tentative assignment
may be re-assigned in a subtype or in another Context.
(Contexts are described below.)
</td>
<td valign="top">
<pre>
Baking Product: Product {
    Discount ~= 8.5;
}
Bread Slicer: Baking Product {
    Discount = 10;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Values</th>
<td valign="top">
<p>
Each Variable has an associated Syntax for literal
values that may be assigned. Syntax is defined as
a restricted regular expression, supporting
alternatives using <strong>|</strong>, grouping
using <strong>(...)</strong>, a number of character
escapes using <strong>\</strong>, repetitions and
optionality using <strong>*</strong>, <strong>+</strong>
and <strong>?</strong>, and character classes.
</p>
<p>
Each of the built-in Variables each has its own
Syntax.
</p>
<td valign="top">
Here are some of the built-in definitions.
Note that the syntax for a regular expression
cannot be encoded using the allowed syntax
for regular expressions, since it is not
itself regular. It's built-in.
<pre>
Regular Expression: String;
Variable: Object {
    Syntax: Regular Expression;
}
Integer: Variable {
    Syntax = /-?[1-9][0-9]+|0/;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Reference Variables</th>
<td valign="top">
<p>
The built-in variable Reference allows assigning
a reference to another Object. The Syntax for the
value of a Reference is the syntax defined for
object names.
</p>
<p>
There is a special rule for assigning to a Reference
that already has a Final assignment. Any subtype of
the type in the existing assignment may be assigned.
This creates a further restriction that does not
violate the finality of the previous assignment.
The default assignment for all References is Object,
and that assignment is final. However, any subtype
of Reference may assign a more restricted type.
</p>
<td>
<p>
Employment is between a Company and a Person,
and that's final. A Company cannot employ a Dog.
Every value in the Project array must be a Project.
</p>
<pre>
Company:;
Person:;
Project:;
Employment: {
    Company: Reference = Company;
    Person: Reference = Person;
    Project: Reference[] = Project;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Reference Shorthand</th>
<td valign="top">
<p>
There is a short-hand syntax for defining Reference
subtypes and their arrays, using <b>-&gt;</b> and
<b>=&gt;</b>. Eponymous naming of reference variables
is also allowed, see the definition below.
</p>
<td>
<p>
Here, each Employment has a reference to one
company (as Employer), to one Person (as Employee)
and to an array of zero or more Projects (this
variable uses eponymous naming). Any assignment
to these variables must conform to the type of
the implied Reference.
<pre>
Employment: {
    Employer -> Company;
    Employee -> Person;
    => Project;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Resolving Names</th>
<td valign="top">
<p>
When an object name is used as a reference or as a
supertype, and is not present within the current
context, the supertype context is searched first.
This search is recursive. If the name is still not
found, the same search proceeds in the parent object,
which is implicit traversal.
If the object is found, a following dot may be used
to search inside it, but after traversing down like
this, following searches will not ever traverse to
the object's parent, even though that name might
be usable from that object.
</p>
</td>
<td valign="top">
Here Product is defined in the top scope (the Origin
object). Then in the Retail object, we define
Toaster to be a Product - referring to Product as
it was defined in the Parent's scope. Finally after
returning to the top scope, we traverse into the
Retail space again to assign the Toaster a Price.
<pre>
Product: {
    Name: String;
    Price: Decimal {
	Syntax = /$[1-9]+\.[0-9][0-9]/
    }
}
Retail: {
    Toaster: Product {
	Name = "Toastmaster 9000";
    }
}
Retail.Toaster.Price = $45.96;
</pre>
</td>
</tr>

<!--tr>
<th align="right" valign="top">Referencing Arrays</th>
<td valign="top">
<p>
The name of an Array may be followed a non-negative
integer in square brackets, which refers to the
respective member of the value of that array. This
syntax is only available when assigning a Reference
value.
</p>
</td>
<td valign="top">
<pre>
</pre>
</td>
</tr-->

<tr>
<th align="right" valign="top">Traversing up</th>
<td valign="top">
<p>
If a Name is present in an object, but also in a
parent object, a reference to the parent object
is still possible. Prefix the name by a dot, and
the local object (and inherited objects) will be
ignored, and the search will start at the parent.
Multiple leading dots may traverse to further
ancestors.
</p>
</td>
<td valign="top">
Without the ability to traverse up, the definition
of Alternate would create a second reference to the
eponymous Product variable inside Sale Item,
instead of the global Product.
<pre>
Product:;
Sale Item: {
    -> Product;
    Alternate -> .Product;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">New kinds of Variable</th>
<td valign="top">
<p>
You can define new subtypes of Variable,
with their own syntax, which is used when
parsing an assignment. A new variable does
not have to be a subtype of any of the
built-in types; it can be a direct subtype
of Variable.
</p>
<td valign="top">
Define a new kind of String with a specified Syntax.
In this example, Employee.Employee ID is an eponymous variable.
<pre>
Employee ID: String {
    Syntax = /[A-Z]-[1-9]+/
}

Employee: Person {
    Employee ID;
}

: Employee {
    Name = 'John Smith';
    Employee ID = A-47912
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Parameter Variables</th>
<td valign="top">
<p>
Some Variables contain their own parameter variables.
For example String (which has Max Length) and Number
(Minimum, Maximum). These parameter variables do not
appear special; you can assign to them anywhere you
might expect to. Refer to the definition of the
built-ins for full details.
</p>
</td>
<td valign="top">
Note the combined definition, assignment and child
assignments for the Discount data type here:
<pre>
Given Name: String[] {
    Max Length = 48;
}
Discount: Integer = 0 {
    Minimum = 0;
    Maximum = 99;
}
</pre>
</td>
</tr>

<tr>
<th align="right" valign="top">Aliasing</th>
<td valign="top">
An inherited object may be aliased (renamed or hidden)
using the alias operator "!".  The right-hand side
names the existing object, and the left-hand side
provides the new name (or if it's missing, there is no
name and the object is hidden).
Inherited assignments to hidden variables are also hidden.
Aliasing changes what is seen in the subtype, it
doesn't change the supertype's child.
</td>
<td valign="top">
<pre>
Person: {
    Surname: String;
    Given Name: String[];
}
Employee: Person {
    Family Name! Surname;
}
</pre>

</td>
</tr>

<tr>
<th align="right" valign="top">Contextual Extension</th>
<td valign="top">
<p>
We've seen what happens when an object is re-opened
from the same parent where it was defined. However
when a namespace traversal is used to re-open it,
there are two possibilities for any changes:
<ul>
<li> Make the changes to the original object,
<li> Make changes that are only seen from the
point of view (the Context) of the place where
the object was re-opened. From other places,
the object will still appear unchanged.
</ul>
This latter case is called Contextual Extension.
The context object provides a point-of-view,
which is the Aspect. The objects visible from
that Aspect (whether base objects or contextual
extensions) are collectively the Facets of that
Aspect.  This is the reason that this language
is called Aspect Definition Language.
</p>

<p>
As discussed, there are two kinds of namespace
traversal.  When a name is found in a parent
or some further ancestor, that's a traversal.
If the object is reopened by <strong>{...}</strong>
at this point, any changes will be made to its
original definition. This also works when
traversing down to a child using a dot.
</p>

<p>
However if the traversal <strong>ends</strong>
with a dot, then any changes will be contextual -
only visible from the context where the traversal
starts.	 The contextual definitions (objects,
variables, aliases, or assignments) have a Context
value which indicates this starting point. All
objects actually have a Context, but for most,
it's the same as the Parent.
</p>
</td>
<td valign="top">
<p>
A new variable called Ordinal is added to Integer.
This is a normal extension, not contextual.
The object called 3 (an Integer assigned the value
3 - the object is distinct from the value here)
is given the Ordinal 'third'.  Now German opens
a new context. The assignment to 3.Ordinal has a
dot at the end of the name, so in the German
context, 3 uses the verbalisation "dritte".
In all other contexts, 3 still uses "third".
</p>
<pre>
Integer {
    Ordinal: String;
}
3: Integer {
    Ordinal ~= "third";
} = 3;
German: {
    3.Ordinal. = "dritte";
}
</pre>

<p>
Here's another case. Two variables are defined to
carry the names of two operations. In the German
context, the two names are different.
</p>
<pre>
OperationNames:
{
    Insert : String ~= "Insert";
    Delete : String ~= "Delete";
}

German:
{
    OperationNames {
        Insert. ~= "Einfügen";
        Delete. ~= "Löschen";
    }
}
</pre>

<tr>
<th align="right" valign="top">Contextual Aliasing</th>
<td valign="top">
This is an extended example for contextual extension,
showing how it works with aliasing.
</td>
<td valign="top">
<p>
In New Context, Person.Surname is called Family Name,
but this alias is contextual, it doesn't affect the
widespread view of Surname. We can use Family Name
to define a Person's Surname, and outside this
context, that will be seen as a Surname.
Note that joe smith's Given Name is assigned Final.
</p>

<p>
Back in global context (or rather, Origin context),
New Context is re-opened and the Family Name 'Smith'
(seen in the global context as Surname) is contextually
re-assigned. Note that from the point of view of
New Context, joe smith is still has a Family name of
Smith (this assignment applies) but from the
global context looking in, his Surname is Schmidt.
The Given Name cannot be re-assigned even contextually,
since it had a final assignment already.
</p>
<pre>
Person {
    Surname: String;
}
New Context: {
    Person. {
	Family Name! Surname;
    }
    joe smith: Person {
	Family Name ~= "Smith";
	Given Name = ["Joseph"];
    }
}
New Context.joe smith.Surname.
    = 'Schmidt';
</pre>
</td>
</tr>