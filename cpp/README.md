## Aspect Definition Language C++ parser

<div align="center">
(c) Copyright, Clifford Heath, 1983-2025.
</div>

### Version and compatibility

Not ready to use.

This library implements Syntax using Peg expressions with prefix operators.
This is incompatible with earlier Regexp implementations.

It depends on Unicode (UTF-8) support from <a href="../../strpp">STRPP</a>,
and can use StrVal and container classes from that to build an object store.

### Components

This library includes components at different levels of abstraction and integration.

* adl.h

Contains a C++ template class for a parser with minimal dependencies and no
dynamic memory allocation, just the stack space required for recursive descent.
In order to match value types according to a Syntax, the template Sink must save
and be able to provide the required Syntax pattern, since nothing is stored here.
In any case, actually matching a Syntax is not yet implemented, just the canonical
value types Integer, String, Reference and Pegexp.

The parser has one template parameter, an ADLSink. The default ADLSink provided
documents the event-based API required of this sink, and also has a template
parameter, the ADLSource. An ADLSourcePtr class (which wraps a pointer to a UTF8
character string) is provided to exemplify the ADLSource API.

An ADLSource can peek_char() and advance() past the peeked Unicode character.
The ADLSource must be copyable (copies are unaffected by advance in another copy)
and the number of bytes between two ADLSource pointers must be available. The
default ADLSource also counts lines and column positions, but this is not mandatory.
The default ADLSink does require it however.

* adlapi.h

ADLAPI uses StrVal and Array from STRPP to define an object store. The API can make
a new TOP object with some builtins, and those objects can build and traverse the
object store. The API is a template which takes two parameters: The Handle to an
ADL object, and a type which can contain any ADL Value.

* adlobjects.h

ADLObjects uses reference-counted objects to provide a suitable Handle for ADLAPI,
all defined in an ADL namespace.

* adl_scan.cpp

A shell program around the ADL parser template, which prints nothing but the final character count and file size.

* adl.cpp

A shell program around the ADL parser template, which prints top-level parser events to stdout.

* adlapi.cpp

Partially-implemented ADL API.

* adlobjects.cpp

Minimal test stub for expanding the adlobjects templates.
