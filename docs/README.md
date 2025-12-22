## Aspect Definition Language (ADL)

The Aspect Definition Language (ADL) is a minimalist language for
defining the classification, attributes and values of hierarchies
of data objects. It simplifies and improves in almost every way
on JSON, XML and YAML, while being fully extensible and type-safe.

### Free-form

The syntax is free-form, except inside value literals.  Whitespace
and newlines are considered as equivalent to a single space and are
ignored. A comment (beginning with //) continues to the end of line
and is considered as white-space.

Read the [Px grammar for ADL](adl-rr.html)

### Object names

An object name consists of one or more words, where a word is a
sequence of alphanumeric or underscore starting with an alphabetic
or underscore character, or is an integer made of a sequence of
decimal ASCII digits. Any white-space between words of an object
name is regarded as equivalent to a single space character.  Any
object may be anonymous, but there is no way to refer to such an
object, except by iterating over its parent's children.

### Inheritance and children

Every object has one parent and one superclass, and is ultimately
a subclass of **Object**. Each object may contain child objects,
variables, and assignments which give a value to any variable it
contains (or inherits). The root **Object** has built-in variables
including Name, Parent, Super and Syntax. There is only one object
with no parent, which is called **TOP**.

All child objects, variables, and value assignments are inherited
by each subclass of the parent.

An object which has True assigned to Is Sterile may not receive
any new children or assignments.

### Variables

A variable is any object which has an assignment to the Syntax
variable. Syntax values use a powerful grammar-definition language.
When an object has a Syntax it becomes a variable, and that object
and any instance (subclass) allows an Assignment. The value literal
to be assigned will be parsed from the input stream using the rules
of the grammar provided.  Defining a Syntax thus extends the ADL
grammar.

### Assignments

An Assignment is a special kind of object, and is also inherited;
this provides effective support for default values. Assignment
objects may also contain child objects, which is surprisingly often
useful.

An Assignment may be either tentative or final; final assignments
may not be overridden.

### References

One special type of variable is called a Reference. The value
assigned to a Reference is a pathname (traversal) to another object.
An inherited assignment to a Reference may only be re-assigned to
a subtype of the inherited assignment, which preserves type safety.
A reference may be single or multiple, which allows an array of
objects to be included in one reference. All members of the array
must conform to the required type. Anonymous instances may be
defined inline and assigned to a reference.

### Aliases

An Alias is a special kind of assignment to the Name variable. It
makes an existing name invisible in its parent, and the object may
now be found under a new name (or may be anonymous). This may be
used to rename or hide inherited child objects.

### Type Safety

All assignments are type-checked using a system of lexical types.
For example, a Date may only be assigned a value which matches the
syntax for a date. A variable which refers to a Person can never
be assigned to any non-Person. And no value can be assigned to a
variable which is not defined for that parent object.

### Contextual Extension

Any named object may be re-opened and extended with new child objects
including variables and assignments.

When an object is extended from outside its natural parent-child
context, these extensions are **contextual**. Normally, an object's
Aspect is the same as its Parent, but in the case of a contextual
extension the Aspect differs and the object is only visible when
the relevent aspect is activated (included in the view).

For example, a new context object called "German" may modify a whole
hierarchy of existing objects (including overriding assignments),
so that when that hierarchy is queried, the contextual extensions
and modifications override the default view.
