#!/usr/bin/env python3
"""
Instrument a variant's sources for the probes.

Three separate edits, each applied only where it is needed, and each recorded
here rather than in a copy someone edited by hand:

  stack   - one call to stackprobe::sample() at the parser's recursion point,
            so the stack probe sees the depth the grammar actually reaches and
            not just wherever an allocation happened to occur.
  bodies  - an include of bodywalk.h, a call registering the input StrVal (so
            fragments can be told from strings the code built), and a call
            reporting the Store once parsing is done.
  leak    - the pre-tranche-2 drivers slurp the file into a raw buffer and
            never free it. That is a property of the test driver, not of the
            library, and on a large corpus it would swamp the comparison, so
            it is normalised here: every variant frees its buffer.

Every edit asserts its anchor, so a variant whose source has moved fails loudly
rather than being measured on a half-applied patch.
"""
import sys


def apply_edits(path, edits):
    src = open(path).read()
    for anchor, addition, what in edits:
        if src.count(anchor) != 1:
            raise SystemExit(f"{path}: {what} anchor found {src.count(anchor)} times, "
                             f"expected 1:\n  {anchor.strip()[:70]}")
        src = src.replace(anchor, addition, 1)
    open(path, 'w').write(src)


def instrument_parser(path):
    """Sample the stack at the parser's recursion point."""
    if 'stackprobe' in open(path).read():
        raise SystemExit(f"{path}: already instrumented")
    apply_edits(path, [(
        "#include\t<pegexp.h>",
        "#include\t<pegexp.h>\n#include\t<stackprobe.h>\t// analysis: stack probe",
        "pegexp include",
    ), (
        "template<typename Source> bool ADLParser<Source>::definition(Source& source)\n{",
        "template<typename Source> bool ADLParser<Source>::definition(Source& source)\n"
        "{\n\tstackprobe::sample(__builtin_frame_address(0));\t// analysis: stack probe",
        "definition()",
    )])


LEAK_EDIT = (
    "\toff_t\t\t\t\tbytes_parsed = source - text;",
    "\toff_t\t\t\t\tbytes_parsed = source - text;\n"
    "\tdelete [] text;\t\t// analysis: this driver leaked its buffer",
    "leak normalisation",
)


def instrument_stack_driver(path):
    """Mark where main() starts, so the stack figure excludes process startup."""
    apply_edits(path, [(
        "#include\t<adlmem.h>",
        "#include\t<adlmem.h>\n#include\t<stackprobe.h>\t// analysis: stack probe",
        "adlmem include",
    ), (
        "int main(int argc, const char** argv)\n{",
        "int main(int argc, const char** argv)\n{\n"
        "\tstackprobe::mark_start();\t// analysis: measure from here, not from the loader\n",
        "main()",
    )])


def instrument_plain(path):
    """
    Normalise buffer handling only, for the builds that measure the parser as
    it is - code size, the tree dump, the suite. Without this the pre-tranche-2
    variants leak their input buffer, which on the large corpus is ~18 KB of
    the measurement and would make the comparison about the test driver.
    The t2 and later drivers already free theirs, so this is a no-op for them.
    """
    src = open(path).read()
    if 'delete [] text;' in src or 'delete [] raw;' in src:
        return False
    apply_edits(path, [LEAK_EDIT])
    return True


def instrument_driver(path, has_input_strval):
    """
    Register the input StrVal with the body walk when the variant has one,
    report the Store before it is torn down, and normalise buffer handling.
    """
    edits = [(
        "#include\t<adlmem.h>",
        "#include\t<adlmem.h>\n#include\t<bodywalk.h>\t// analysis: body walk",
        "adlmem include",
    )]
    if has_input_strval:
        edits.append((
            "\tADLParser<ADLMemStoreSink>\tadl(sink);",
            "\tadl_analysis::note_input(text);\t\t// analysis: the pinned input body\n\n"
            "\tADLParser<ADLMemStoreSink>\tadl(sink);",
            "note_input",
        ))
    elif 'delete [] text;' not in open(path).read():
        # Every fragment is a copy in these variants, so the buffer is
        # finished with once the parse returns.
        edits.append(LEAK_EDIT)
    edits.append((
        "\tp(last);",
        "\tadl_analysis::bodies_report(store.top());\t// analysis: whole Store\n\n\tp(last);",
        "p(last)",
    ))
    apply_edits(path, edits)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit("usage: instrument.py <parser|driver|stack|plain> <path> <flag>")
    what, path, flag = sys.argv[1], sys.argv[2], sys.argv[3] == '1'
    if what == 'parser':
        instrument_parser(path)
    elif what == 'driver':
        instrument_driver(path, flag)
    elif what == 'stack':
        instrument_stack_driver(path)
    elif what == 'plain':
        if not instrument_plain(path):
            print(f"{path}: already frees its buffer, nothing to do")
            raise SystemExit(0)
    else:
        raise SystemExit(f"unknown target {what}")
    print(f"instrumented {path}")
