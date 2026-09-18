#!/usr/bin/env python3
"""
Code size, from the two places that can be trusted to report it.

  object_text()    __text of a single .o, which is the code the compiler
                   generated for this translation unit.
  image_text_data() the __TEXT and __DATA sections of a linked executable,
                   summed. That is what a flash image actually carries, and it
                   is the figure to quote for a target.

The *file size* of an executable is the weakest of the three and is not used
here: it moves by more than the code does, because a few extra symbols can
push the page-aligned __LINKEDIT reservation over a boundary and add 16 KB
that is not code at all.
"""
import re
import subprocess


def _sections(path, tool):
    out = subprocess.run([tool, '-l', path], capture_output=True, text=True).stdout
    seg = None
    sect = None
    found = {}
    for line in out.splitlines():
        t = line.strip()
        m = re.match(r'segname (\S+)', t)
        if m:
            seg = m.group(1)
            continue
        m = re.match(r'sectname (\S+)', t)
        if m:
            sect = m.group(1)
            continue
        m = re.match(r'size (0x[0-9a-f]+)$', t)
        if m and seg and sect:
            found[(seg, sect)] = int(m.group(1), 16)
            sect = None
    return found


def object_text(path):
    """__text of an object file, via size(1) - reliable on .o files."""
    out = subprocess.run(['size', '-m', path], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if '__text' in line:
            return int(line.split(':')[1])
    return 0


def image_text_data(path):
    """__TEXT + __DATA of a linked executable: the flashable figure."""
    s = _sections(path, 'otool')
    text = sum(v for (seg, _), v in s.items() if seg == '__TEXT')
    data = sum(v for (seg, _), v in s.items() if seg.startswith('__DATA'))
    return text, data, text + data


if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        if p.endswith('.o'):
            print(f"{p}: __text {object_text(p):,}")
        else:
            t, d, tot = image_text_data(p)
            print(f"{p}: __TEXT {t:,} + __DATA {d:,} = {tot:,}")
