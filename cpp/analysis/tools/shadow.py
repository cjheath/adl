#!/usr/bin/env python3
"""
Generate the shadow strval.h that the body-count walk needs.

StrVal does not expose the identity of the body it references, and it should
not have to: that is an implementation detail the library is entitled to keep.
So the probe generates a copy of the real header with a few accessors added,
and puts it earlier on the include path.

What matters is that this cannot change what it measures. The accessors are
*methods*, not data members, so the class layout, the reference counts and
every allocation are exactly as they would be without it. Anything that did
change those would be measuring itself.
"""
import sys

# Anchors in the real header, and what to add next to each.
ANCHORS = [
    (
        "	inline bool	operator>(const StrRefI& comparand) const { return compare(comparand) > 0; }",
        """
	// Added by analysis/tools/shadow.py. Probe only: methods, no data, so the
	// layout and every allocation are unchanged.
	const void*	bodyIdentity() const { return static_cast<const Body*>(body); }
	Index		bodyOffset() const { return offset; }
	const StrBodyI<Index>*	probeBody() const { return body; }""",
    ),
    (
        "	static	StrBodyI nullBody;",
        """
	// Added by analysis/tools/shadow.py. Probe only.
	bool	probeIsStatic() const { return num_alloc == 0 && num_elements > 0; }
	Index	probeNumAlloc() const { return num_alloc; }""",
    ),
]


def shadow(src_path, dst_path):
    src = open(src_path).read()
    if 'bodyIdentity' in src:
        raise SystemExit(f"{src_path}: already has the probe accessors")
    for anchor, addition in ANCHORS:
        if src.count(anchor) != 1:
            raise SystemExit(f"{src_path}: anchor found {src.count(anchor)} times, "
                             f"expected 1 - has the header moved?\n  {anchor.strip()[:70]}")
        src = src.replace(anchor, anchor + addition, 1)
    open(dst_path, 'w').write(src)
    return dst_path


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit("usage: shadow.py <real strval.h> <shadow strval.h>")
    print(shadow(sys.argv[1], sys.argv[2]))
