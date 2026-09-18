Analysis
=========

Tools, scripts and a Makefile to measure the ADL parser and re-run the
comparison behind Memory.md. Everything generated goes under `build/` and
`out/`; nothing outside those two directories is written, so a run cannot
disturb the tree it is measuring.

    make            measure everything and print the tables
    make summary    print the tables from the last run, without re-measuring
    make clean      remove build/ and out/

What it measures, and why each way
------------------------------------

  code size    `__text` per optimisation level from the object, and
               `__TEXT`+`__DATA` of the linked image - the latter is what a
               flash image carries. The executable's *file size* is not used:
               a few extra symbols can push the page-aligned `__LINKEDIT`
               reservation over a boundary and add 16 KB that is not code.

  stack        the stack pointer sampled from `operator new/delete` and from
               `ADLParser::definition()`, the grammar's recursion point,
               against the thread's stack base. A sampled high-water mark, so
               real usage could be marginally deeper between samples. Run
               without `-a`: the debug tree printer recurses too, and would be
               measured instead of the parser.

  memory       `operator new/delete` overridden, tracking live bytes with
               `malloc_size()` so it counts what the allocator handed out
               rather than what was asked for. Peak (what must be available at
               once), live at exit (the retained object graph) and allocation
               activity (the churn a peak cannot show) are reported separately.

  sharing      the finished Store is walked and every StrVal asked which
               StrBody it references. A store holding one body per string and
               one holding a body sliced 70 ways can have the same byte count
               and are not remotely the same thing, so the bytes alone do not
               answer "did the sharing happen".

The variants
-------------

Each is assembled from git into `build/<variant>/`, so nothing is edited by
hand and the comparison is reproducible. A variant is named by the commit it
is - its abbreviated SHA - so a table column says which revision produced the
number without a lookup:

  6c83f6f  before either tranche, with strpp `0bc0394~1`
  baaa6b5  after tranche 1, the Array slice fix, with strpp `0bc0394`
  <sha>    whatever HEAD is, with whatever strpp HEAD is

The head names itself: its abbreviated commit when both trees are clean, or
`head` when either has changes that no commit produced - in which case it is
the working tree that gets measured, and claiming a commit for it would
understate what produced the numbers. Only the strpp headers the ADL sources
actually include decide that (`strpp_headers_used()`), so an edit to an
unrelated strpp file does not rename the head.

Two normalisations are applied to the older variants, both recorded in
`tools/instrument.py` rather than left in a hand-edited copy:

  - the pre-tranche-2 drivers slurp the file into a raw buffer and never free
    it. That is the test driver's business, not the library's, and on a large
    corpus it would swamp the comparison, so every variant frees its buffer.
  - the stack probe needs one line at the parser's recursion point.

The Source switch
------------------

`adlmem.cpp` takes its Source and Sink from a `#define`, so the older
byte-pointer Source can still be built:

    -DADL_SOURCE_UTF8PTR    the byte-pointer Source; every fragment is copied
    (default)               the StrVal Source; every fragment is a slice

The last table reports the head built both ways, which is the check that
"both Sources still work" is true rather than merely intended. The variants
before the switch existed report `n/a`: they are the byte-pointer Source, so
there is nothing to compare them against.

Layout
-------

  tools/peakprobe.cpp    dynamic memory: peak, retained, allocations, bytes
  tools/stackprobe.*     stack high-water, sampled from allocations
  tools/bodywalk.h       the Store walk (a header: adlmem.h defines a
                         non-inline function, so two TUs including it collide)
  tools/shadow.py        generates the strval.h with body accessors added
  tools/instrument.py    applies the three probe edits to a variant's copies
  tools/sections.py      code size from an object or a linked image
  scripts/analyse.py     assembles, builds, runs and tabulates
  out/tables.txt         the tables, as printed
  out/raw.json           every measurement, for re-deriving figures
