#!/usr/bin/env python3
"""
Measure the ADL parser across the revisions that produced the current tree.

Four variants, each assembled from git rather than edited by hand, so the
whole comparison can be re-run from a clean checkout:

  a commit   named by its abbreviated commit, so a column says which revision
             produced the number without a lookup
  head       the working tree, when either repository has changes that no
             commit produced - named `head` rather than claiming a commit

For each variant and each workload it measures:

  what parses        - every file consumed, and the tree dump byte-identical
                       to every other variant's (the correctness invariant the
                       whole series was held to)
  dynamic memory     - peak live bytes, retained store, allocations, total
                       bytes allocated
  Store sharing      - how many StrBody objects the finished Store holds, and
                       how many of its StrVals are slices of a pinned input
  stack              - high-water mark, and bytes per level of nesting
  code size          - __text per optimisation level, and the flashable
                       __TEXT+__DATA of the linked image

Two workloads: the small one the earlier reports used, and fbmwd08, a real
model loaded as three files in order.

    make            run everything and print the tables
    make summary    print the tables from the last run, without re-measuring
    make clean      remove build/ and out/

Everything it generates goes under build/ and out/; nothing outside those two
directories is written, so a run cannot disturb the tree it measures.
"""
from collections import OrderedDict, namedtuple
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ANALYSIS = os.path.dirname(HERE)
CPP = os.path.dirname(ANALYSIS)                 # adl/cpp
ADL_ROOT = os.path.dirname(CPP)                 # the git repository
STRPP = os.path.normpath(os.path.join(CPP, '..', '..', 'strpp'))
TOOLS = os.path.join(ANALYSIS, 'tools')
BUILD = os.path.join(ANALYSIS, 'build')
OUT = os.path.join(ANALYSIS, 'out')

sys.path.insert(0, TOOLS)
import sections                                                     # noqa: E402

Variant = namedtuple('Variant', 'name adl_rev strpp_rev strval_source desc')

# The revisions the work started from. strpp '0bc0394~1' is the commit before
# the Array slice fix, which is what tranche 1 was about.
HISTORICAL = [
    ('6c83f6f', '0bc0394~1', False, 'before either tranche'),
    ('baaa6b5', '0bc0394',   False, 'after tranche 1 (Array slices)'),
]

# What decides a variant's name, and whether the head is committed. An
# uncommitted head is named `head`, so a table column never claims a commit
# that did not produce it.
MEASURED_FILES = ['cpp/adlparser.h', 'cpp/adlstore.h', 'cpp/adlmem.h',
                  'cpp/adlmem.cpp', 'cpp/adlstrval.h', 'cpp/adl_scan.cpp',
                  'cpp/Makefile']


def git(root, *args):
    p = subprocess.run(['git', '-C', root] + list(args), capture_output=True, text=True)
    if p.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} in {root} failed: {p.stderr.strip()[:200]}")
    return p.stdout.strip()


def strpp_headers_used():
    """
    The strpp headers the ADL sources actually include.

    Only these decide whether the head is uncommitted. An edit to some other
    strpp file cannot change what is measured here, and letting it name the
    head `head` would understate what produced these numbers - which is the
    opposite of the point of naming them at all.
    """
    used = set()
    for f in sorted(os.listdir(CPP)):
        if not f.endswith(('.h', '.cpp')):
            continue
        text = open(os.path.join(CPP, f), errors='replace').read()
        for m in re.finditer(r'#include\s+[<"]([^>"]+)[>"]', text):
            if os.path.exists(os.path.join(STRPP, 'include', m.group(1))):
                used.add(m.group(1))
    return sorted(used)


def head_variant():
    """
    The working tree, named by the commit it is - or `head` when either repo
    has changes that no commit produced, in which case it is the working tree
    that gets measured rather than a commit.
    """
    adl_dirty = bool(git(ADL_ROOT, 'status', '--porcelain', '--', *MEASURED_FILES))
    strpp_dirty = bool(git(STRPP, 'status', '--porcelain', '--',
                           *[f'include/{h}' for h in strpp_headers_used()]))
    adl_sha = git(ADL_ROOT, 'rev-parse', '--short', 'HEAD')
    strpp_sha = git(STRPP, 'rev-parse', '--short', 'HEAD')
    if adl_dirty or strpp_dirty:
        dirty_in = ' and '.join(x for x, d in (('adl', adl_dirty), ('strpp', strpp_dirty)) if d)
        return Variant('head', None, None, True,
                       f"the working tree, uncommitted changes in {dirty_in} "
                       f"(adl {adl_sha}, strpp {strpp_sha})")
    return Variant(adl_sha, adl_sha, strpp_sha, True,
                   f"the current tree: {adl_sha} with strpp {strpp_sha}")


def variants():
    out = [Variant(name, name, strpp, src, desc)
           for name, strpp, src, desc in HISTORICAL]
    out.append(head_variant())
    return out


VARIANTS = variants()

WORKLOADS = OrderedDict([
    ('readme',  ['adl.adl', '../readme.adl']),
    ('fbmwd08', ['adl.adl', '../fbmwd08/fbmwd08.adl', '../fbmwd08/fbm-datatypes.adl',
                 '../fbmwd08/oilsupply.adl']),
])

OPTS = ['-O2', '-Os', '-Oz']
BASE_OPT = '-O2'
LADDER_DEPTHS = [5, 10, 15, 20, 25, 30]
STD = ['-std=c++17', '-DHAVE_PTHREADS']
SKIP_SUMMARY = re.compile(rb'^(Success|Failed), ')


def log(msg):
    print(f"  {msg}", flush=True)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)


def checked(cmd):
    p = run(cmd)
    if p.returncode != 0 and p.returncode != 0:
        raise SystemExit(f"failed: {' '.join(cmd)}\n{p.stdout.decode()[:400]}"
                         f"{p.stderr.decode()[:400]}")
    return p


def git_show(root, rev, path):
    p = run(['git', '-C', root, 'show', f'{rev}:{path}'])
    if p.returncode != 0:
        raise SystemExit(f"git show {rev}:{path} failed: {p.stderr.decode()[:200]}")
    return p.stdout


def copy_or_extract(root, rev, src_dir, rel, dst_dir):
    """Copy one file from a revision, or from the working tree if rev is None."""
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, os.path.basename(rel))
    if rev is None:
        shutil.copy(os.path.join(src_dir, rel), dst)
    else:
        open(dst, 'wb').write(git_show(root, rev, rel))


# ---------------------------------------------------------------- assembly

def assemble(v):
    """Build a variant's source tree under build/<name>/src."""
    root = os.path.join(BUILD, v.name)
    adl = os.path.join(root, 'src', 'adl')
    strpp = os.path.join(root, 'src', 'strpp')
    for d in (adl, strpp):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)

    adl_rel = os.path.join(CPP)
    prefix = os.path.relpath(CPP, ADL_ROOT)

    # adl: the headers and the drivers
    for f in ('adlparser.h', 'adlstore.h', 'adlmem.h', 'adlstrval.h',
              'adlmem.cpp', 'adl_scan.cpp'):
        src = os.path.join(CPP, f)
        if not os.path.exists(src):
            continue            # adlstrval.h only exists from tranche 2 on
        if v.adl_rev is None:
            shutil.copy(src, os.path.join(adl, f))
        else:
            p = run(['git', '-C', ADL_ROOT, 'show', f'{v.adl_rev}:{prefix}/{f}'])
            if p.returncode != 0:
                continue        # not present at that revision
            open(os.path.join(adl, f), 'wb').write(p.stdout)

    # strpp: every header at the variant's revision, or the working tree's
    if v.strpp_rev is None:
        inc = os.path.join(STRPP, 'include')
        for f in sorted(os.listdir(inc)):
            if f.endswith('.h'):
                shutil.copy(os.path.join(inc, f), os.path.join(strpp, f))
    else:
        listing = run(['git', '-C', STRPP, 'ls-tree', '--name-only', '--full-tree',
                       '-r', v.strpp_rev, 'include']).stdout.decode().split()
        for rel in listing:
            copy_or_extract(STRPP, v.strpp_rev, STRPP, rel, strpp)
    return root, adl, strpp


def make_flavours(v, root, adl, strpp):
    """Instrumented copies: the stack probe samples the parser's recursion
    point, the body walk needs the shadow strval.h and a patched driver."""
    tools = [os.path.join(TOOLS, 'instrument.py'), os.path.join(TOOLS, 'shadow.py')]

    plain = os.path.join(root, 'plain')
    shutil.rmtree(plain, ignore_errors=True)
    os.makedirs(plain)
    shutil.copy(os.path.join(adl, 'adlmem.cpp'), os.path.join(plain, 'adlmem_plain.cpp'))
    checked([sys.executable, tools[0], 'plain', os.path.join(plain, 'adlmem_plain.cpp'), '0'])

    stack = os.path.join(root, 'stack')
    shutil.rmtree(stack, ignore_errors=True)
    os.makedirs(stack)
    shutil.copy(os.path.join(adl, 'adlparser.h'), os.path.join(stack, 'adlparser.h'))
    checked([sys.executable, tools[0], 'parser', os.path.join(stack, 'adlparser.h'), '0'])
    shutil.copy(os.path.join(plain, 'adlmem_plain.cpp'), os.path.join(stack, 'adlmem_stack.cpp'))
    checked([sys.executable, tools[0], 'stack', os.path.join(stack, 'adlmem_stack.cpp'), '0'])

    bodies = os.path.join(root, 'bodies')
    shutil.rmtree(bodies, ignore_errors=True)
    os.makedirs(bodies)
    checked([sys.executable, tools[1], os.path.join(strpp, 'strval.h'),
             os.path.join(bodies, 'strval.h')])
    drv = os.path.join(bodies, 'adlmem_bodies.cpp')
    shutil.copy(os.path.join(plain, 'adlmem_plain.cpp'), drv)
    checked([sys.executable, tools[0], 'driver', drv, '1' if v.strval_source else '0'])
    return plain, stack, bodies


# ---------------------------------------------------------------- building

def gxx(args, out, opt=BASE_OPT, incs=()):
    cmd = ['g++', opt] + STD + [f'-I{d}' for d in incs] + args + ['-o', out]
    p = run(cmd)
    if p.returncode != 0:
        raise SystemExit(f"compile failed: {' '.join(cmd)}\n{p.stderr.decode()[:1500]}")


def build(v, root, adl, strpp, plain, stack, bodies):
    bin_dir = os.path.join(root, 'bin')
    os.makedirs(bin_dir, exist_ok=True)
    obj = os.path.join(root, 'obj')
    os.makedirs(obj, exist_ok=True)

    # char_encoding.cpp supplies UCS4ToLower/Upper and includes neither array.h
    # nor strval.h, so it is independent of everything measured here. Linking it
    # rather than libstrpp.a keeps a variant's headers the only source of any
    # header-defined code.
    support = os.path.join(obj, 'char_encoding.o')
    p = run(['g++', '-O2'] + STD + [f'-I{strpp}',
             '-c', os.path.join(STRPP, 'src', 'char_encoding.cpp'), '-o', support])
    if p.returncode != 0:
        raise SystemExit(f"char_encoding failed: {p.stderr.decode()[:500]}")

    for name, tool in (('peak', 'peakprobe.cpp'), ('stack', 'stackprobe.cpp')):
        p = run(['g++', '-O2'] + STD + [f'-I{TOOLS}', '-c', os.path.join(TOOLS, tool),
                '-o', os.path.join(obj, name + '.o')])
        if p.returncode != 0:
            raise SystemExit(f"{tool} failed: {p.stderr.decode()[:500]}")

    outs = {}
    for opt in OPTS:
        tag = f'{v.name}_{opt.lstrip("-")}'
        gxx([os.path.join(plain, 'adlmem_plain.cpp'), support], os.path.join(bin_dir, tag),
            opt, [plain, adl, strpp])
        outs[opt] = os.path.join(bin_dir, tag)

    # peak: no -a in the runs, so the tree printer is not measured
    gxx([os.path.join(plain, 'adlmem_plain.cpp'), os.path.join(obj, 'peak.o'), support],
        os.path.join(bin_dir, v.name + '_peak'), BASE_OPT, [plain, adl, strpp])

    gxx([os.path.join(stack, 'adlmem_stack.cpp'), os.path.join(obj, 'stack.o'), support],
        os.path.join(bin_dir, v.name + '_stack'), BASE_OPT, [stack, adl, strpp, TOOLS])

    gxx([os.path.join(bodies, 'adlmem_bodies.cpp'), support],
        os.path.join(bin_dir, v.name + '_bodies'), BASE_OPT, [bodies, adl, strpp, TOOLS])

    # The same driver built against the other Source, where this revision has
    # the switch at all. Older revisions have only the byte-pointer Source, so
    # for them there is nothing to compare - they *are* it.
    alt = None
    if 'ADL_SOURCE_UTF8PTR' in open(os.path.join(plain, 'adlmem_plain.cpp')).read():
        alt = os.path.join(bin_dir, v.name + '_utf8ptr')
        gxx(['-DADL_SOURCE_UTF8PTR', os.path.join(plain, 'adlmem_plain.cpp'), support],
            alt, BASE_OPT, [plain, adl, strpp])

    # object code, for the per-TU __text figures
    objtext = {}
    for opt in OPTS:
        o = os.path.join(obj, f'adlmem{opt}.o')
        p = run(['g++', opt] + STD + [f'-I{plain}', f'-I{adl}', f'-I{strpp}', '-c',
                 os.path.join(plain, 'adlmem_plain.cpp'), '-o', o])
        if p.returncode != 0:
            raise SystemExit(f"object compile failed: {p.stderr.decode()[:500]}")
        objtext[opt] = sections.object_text(o)
    return outs, objtext, alt


# ---------------------------------------------------------------- running

def load_args(files):
    """-a before each file so the whole tree is printed, as the suite does."""
    args = []
    for f in files:
        args += ['-a', f]
    return args


def parse_peak(out):
    m = re.search(rb'PEAK peak_bytes=(\d+) live_at_exit=(\d+) allocs=(\d+) '
                  rb'frees=(\d+) bytes_allocated=(\d+)', out)
    if not m:
        return None
    return dict(zip(('peak', 'store', 'allocs', 'frees', 'bytes'),
                    (int(x) for x in m.groups())))


def parse_bodies(out):
    m = re.search(rb'BODIES strvals=(\d+) bodies=(\d+) shared_bodies=(\d+) '
                  rb'empty_strvals=(\d+) on_input=(\d+) inputs=(\d+)', out)
    if not m:
        return None
    d = dict(zip(('strvals', 'bodies', 'shared', 'empty', 'on_input', 'inputs'),
                 (int(x) for x in m.groups())))
    d['per_input'] = [int(x) for x in re.findall(rb'BODIES input\[\d+\] slices=(\d+)', out)]
    return d


def parse_stack(out):
    m = re.search(rb'STACK used=(\d+) bytes', out)
    return int(m.group(1)) if m else None


def summary_unit(out):
    """The unit the driver reports progress in - 'lines' for the StrVal Source,
    'bytes' for the byte-pointer one. Reading it back is how the body walk
    knows it patched for the right Source, rather than trusting a flag."""
    m = re.search(rb'^(?:Success|Failed), (?:parsed|processed) \d+ of \d+ (\w+)', out, re.M)
    return m.group(1).decode() if m else None


def consumed_all(out):
    """Every file reached its end: the line-based success criterion, or the
    byte-based one for the variants that predate it."""
    lines = out.splitlines()
    summaries = [l for l in lines if l.startswith(b'Success, ') or l.startswith(b'Failed, ')]
    ok = [l for l in summaries if re.match(rb'Success, (parsed|processed) (\d+) of \2', l)]
    return len(summaries), len(ok)


def tree_dump(out):
    """Fingerprint of the printed tree, with the summary lines dropped: their
    format legitimately changed from bytes to lines in tranche 2."""
    body = b'\n'.join(l for l in out.splitlines() if not SKIP_SUMMARY.match(l))
    return hashlib.sha256(body).hexdigest()[:16], body


def measure(v, root, files, want):
    bin_dir = os.path.join(root, 'bin')
    r = {}

    if 'plain' in want:
        p = run([os.path.join(bin_dir, f'{v.name}_{BASE_OPT.lstrip("-")}')] + load_args(files),
                cwd=CPP)
        r['exit'] = p.returncode
        r['summaries'], r['consumed'] = consumed_all(p.stdout + p.stderr)
        r['unit'] = summary_unit(p.stdout + p.stderr)
        r['hash'], r['dump'] = tree_dump(p.stdout + p.stderr)

    if 'peak' in want:
        p = run([os.path.join(bin_dir, v.name + '_peak')] + files, cwd=CPP)
        r['peak'] = parse_peak(p.stdout + p.stderr)
        r['exit_peak'] = p.returncode

    if 'bodies' in want:
        p = run([os.path.join(bin_dir, v.name + '_bodies')] + files, cwd=CPP)
        r['bodies'] = parse_bodies(p.stdout + p.stderr)

    if 'stack' in want:
        p = run([os.path.join(bin_dir, v.name + '_stack')] + files, cwd=CPP)
        r['stack'] = parse_stack(p.stdout + p.stderr)

    return r


# ---------------------------------------------------------------- the suite

def discover_suite():
    """Every tests/*.adl, plus each tests/<name>/ directory as one sequence."""
    tests = os.path.join(CPP, 'tests')
    cases = []
    for name in sorted(os.listdir(tests)):
        p = os.path.join(tests, name)
        if name.endswith('.adl'):
            cases.append((os.path.join('tests', name), [os.path.join('tests', name)]))
        elif os.path.isdir(p):
            files = sorted(f for f in os.listdir(p) if f.endswith('.adl'))
            if files:
                cases.append((os.path.join('tests', name),
                              [os.path.join('tests', name, f) for f in files]))
    out = []
    for label, files in cases:
        marker = None
        for f in files:
            text = open(os.path.join(CPP, f), errors='replace').read()
            if 'EXPECT: FAIL' in text:
                marker = 'fail'
                break
            if 'EXPECT: PASS' in text:
                marker = 'pass'
                break
        out.append((label, files, marker))
    return out


def run_suite(binary, cases):
    passed = failed = skipped = 0
    breaks = []
    for label, files, marker in cases:
        if marker is None:
            skipped += 1
            continue
        p = run([binary] + load_args(files), cwd=CPP)
        actual = 'pass' if p.returncode == 0 else 'fail'
        if actual == marker:
            passed += 1
        else:
            failed += 1
            breaks.append(f"{label}: expected {marker}, got {actual}")
    return passed, failed, skipped, breaks


# ---------------------------------------------------------------- the ladder

def make_ladder(depth):
    """One file nesting `depth` levels of blocks, for the per-level cost."""
    lines = ["// EXPECT: PASS", "TOP {"]
    for i in range(1, depth + 1):
        lines.append("\t" * i + f"level{i:02d}: {{")
    lines.append("\t" * (depth + 1) + "leaf: String;")
    for i in range(depth, 0, -1):
        lines.append("\t" * i + "}")
    lines.append("}")
    return "\n".join(lines) + "\n"


def ladder_measure(binary):
    """bytes = c + k * depth, fitted from the depths where it is straight."""
    dir_ = os.path.join(BUILD, 'ladder')
    os.makedirs(dir_, exist_ok=True)
    pts = []
    for d in LADDER_DEPTHS:
        f = os.path.join(dir_, f'd{d:02d}.adl')
        open(f, 'w').write(make_ladder(d))
        p = run([binary, 'adl.adl', f], cwd=CPP)
        used = parse_stack(p.stdout + p.stderr)
        if used:
            pts.append((d, used))
    if len(pts) < 2:
        return None
    d0, u0 = pts[0]
    d1, u1 = pts[-1]
    k = (u1 - u0) / (d1 - d0)
    c = u0 - k * d0
    residual = max(abs(u - (c + k * d)) for d, u in pts)
    return dict(per_level=int(round(k)), intercept=int(round(c)), residual=int(round(residual)),
                points=pts)


# ---------------------------------------------------------------- tables

def table(rows, headers, title=None):
    widths = [max(len(str(r[i])) for r in [headers] + rows) for i in range(len(headers))]
    out = []
    if title:
        out.append(title)
        out.append('-' * len(title))
    out.append('  '.join(str(h).ljust(w) for h, w in zip(headers, widths)).rstrip())
    for r in rows:
        out.append('  '.join(str(c).ljust(w) for c, w in zip(r, widths)).rstrip())
    return '\n'.join(out)


def delta(new, old):
    if not old or old == 0:
        return ''
    d = (new - old) / old * 100.0
    return f"{d:+.1f}%"


def build_tables(m):
    doc = []
    names = [v.name for v in VARIANTS]
    desc = {v.name: v.desc for v in VARIANTS}

    doc.append("ADL parser analysis")
    doc.append("===================")
    doc.append("")
    doc.append("Generated by analysis/scripts/analyse.py. Variants, assembled from git:")
    for v in VARIANTS:
        doc.append(f"  {v.name:<8}{v.desc}")
    doc.append("")
    doc.append("Code size and stack are per variant; memory and sharing are per variant and")
    doc.append("workload. 'peak' is the high-water mark of live bytes, 'store' the live bytes")
    doc.append("when parsing finished, so what must be available at once and what must be")
    doc.append("retained are separate numbers. Measurements run with the tree printer off,")
    doc.append("so they measure the parser and not the test driver.")
    doc.append("")

    # -- corpus
    doc.append(table(
        [[w, sum(os.path.getsize(os.path.join(CPP, f)) for f in files),
          sum(sum(1 for _ in open(os.path.join(CPP, f), errors='replace')) for f in files)]
         for w, files in WORKLOADS.items()],
        ['workload', 'bytes', 'lines'], "Workloads"))

    # -- correctness
    rows = []
    for v in VARIANTS:
        s = m['suite'][v.name]
        r = []
        for w in WORKLOADS:
            d = m['runs'][v.name][w]
            r.append('yes' if d.get('consumed') == d.get('summaries') and d.get('exit') == 0
                     else 'NO')
        rows.append([v.name, f"{s[0]}/{s[1]}/{s[2]}", *r])
    doc.append(table(rows, ['variant', 'suite p/f/s'] + [f'{w} parses' for w in WORKLOADS],
                     "Correctness"))

    first = VARIANTS[0].name
    rows = []
    for w in WORKLOADS:
        base = m['runs'][first][w]['hash']
        row = [w, base]
        for v in VARIANTS[1:]:
            h = m['runs'][v.name][w]['hash']
            row.append('identical' if h == base else 'DIFFERS')
        rows.append(row)
    doc.append(table(rows, ['workload', f'{first} tree hash'] +
                     [f'{v.name} vs {first}' for v in VARIANTS[1:]],
                     "Tree dumps byte-identical (the correctness invariant the series held to)"))

    # -- memory
    for w in WORKLOADS:
        rows = []
        for v in VARIANTS:
            p = m['runs'][v.name][w]['peak']
            rows.append([v.name, f"{p['peak']:,}", f"{p['store']:,}",
                         f"{p['allocs']:,}", f"{p['bytes']:,}"])
        b = m['runs'][first][w]['peak']
        n = m['runs'][VARIANTS[-1].name][w]['peak']
        rows.append([f'{VARIANTS[-1].name} vs {first}',
                     delta(n['peak'], b['peak']), delta(n['store'], b['store']),
                     delta(n['allocs'], b['allocs']), delta(n['bytes'], b['bytes'])])
        doc.append(table(rows, ['variant', 'peak', 'store', 'allocs', 'bytes allocated'],
                         f"Dynamic memory - adl.adl + {w} ({sum(os.path.getsize(os.path.join(CPP,f)) for f in WORKLOADS[w]):,} bytes)"))

    # -- store sharing
    rows = []
    for w in WORKLOADS:
        for v in VARIANTS:
            b = m['runs'][v.name][w].get('bodies')
            if not b:
                continue
            rows.append([w, v.name, f"{b['strvals']:,}", f"{b['bodies']:,}",
                         f"{b['shared']:,}", f"{b['empty']:,}", f"{b['on_input']:,}"])
    doc.append(table(rows, ['workload', 'variant', 'StrVals', 'StrBodies', 'shared bodies',
                            'empty StrVals', 'slices of an input'], "Store sharing"))

    # -- stack
    rows = []
    for v in VARIANTS:
        row = [v.name]
        for w in WORKLOADS:
            s = m['runs'][v.name][w].get('stack')
            row.append(f"{s:,}" if s else '-')
        row.append(f"{m['suite_stack'][v.name]:,}" if m['suite_stack'][v.name] else '-')
        l = m['ladder'][v.name]
        row.append(f"{l['per_level']}" if l else '-')
        rows.append(row)
    doc.append(table(rows, ['variant'] + [f'{w}' for w in WORKLOADS] +
                     ['deepest suite test', 'per nesting level'], "Maximum stack, bytes"))

    # -- code size
    rows = []
    for v in VARIANTS:
        row = [v.name]
        for opt in OPTS:
            row.append(f"{m['objtext'][v.name][opt]:,}")
        t, d, tot = m['flash'][v.name]
        row += [f"{tot:,}"]
        rows.append(row)
    last = VARIANTS[-1].name
    rows.append([f'{last} vs {first}'] +
                [delta(m['objtext'][last][o], m['objtext'][first][o]) for o in OPTS] +
                [delta(m['flash'][last][2], m['flash'][first][2])])
    doc.append(table(rows, ['variant'] + [f'{o} __text' for o in OPTS] + [f'{BASE_OPT} __TEXT+__DATA'],
                     "Code size, bytes"))

    doc.append(table([[v.name, m['flash'][v.name][0], m['flash'][v.name][1]]
                      for v in VARIANTS],
                     ['variant', '__TEXT', '__DATA'], f"Linked image at {BASE_OPT}"))

    # -- both Sources still work
    rows = []
    for v in VARIANTS:
        alt = m['utf8ptr'].get(v.name)
        if not alt:
            # No switch at this revision: the only Source it has IS the
            # byte-pointer one, so there is nothing to compare it against.
            rows.append([v.name, 'n/a'] + ['-'] * len(WORKLOADS))
            continue
        rows.append([v.name, 'builds'] +
                    [('identical' if alt[w] == m['runs'][v.name][w]['hash'] else 'DIFFERS')
                     for w in WORKLOADS])
    doc.append(table(rows, ['variant', '-DADL_SOURCE_UTF8PTR'] +
                     [f'{w} tree vs default' for w in WORKLOADS],
                     "Both Sources still work (adlmem.cpp's default vs the switch)"))

    return '\n'.join(doc) + '\n'


# ---------------------------------------------------------------- main

def main():
    shutil.rmtree(BUILD, ignore_errors=True)
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    t0 = time.time()

    cases = discover_suite()
    m = dict(runs={v.name: {} for v in VARIANTS}, suite={}, objtext={}, flash={},
             suite_stack={}, ladder={}, utf8ptr={})

    # The body walk is patched differently for the two Sources, so check the
    # binary agrees with the flag rather than discovering it in the numbers.
    want_unit = {v.name: ('lines' if v.strval_source else 'bytes') for v in VARIANTS}

    for v in VARIANTS:
        print(f"[{v.name}] {v.desc}", flush=True)
        root, adl, strpp = assemble(v)
        log("assembled")
        plain, stack, bodies = make_flavours(v, root, adl, strpp)
        log("instrumented")
        outs, objtext, alt = build(v, root, adl, strpp, plain, stack, bodies)
        m['objtext'][v.name] = objtext
        log("built")

        bin_dir = os.path.join(root, 'bin')
        if True:
            m['flash'][v.name] = sections.image_text_data(
                os.path.join(bin_dir, f'{v.name}_{BASE_OPT.lstrip("-")}'))
            p, f, s, breaks = run_suite(
                os.path.join(bin_dir, f'{v.name}_{BASE_OPT.lstrip("-")}'), cases)
            m['suite'][v.name] = (p, f, s)
            for b in breaks:
                log(f"suite break: {b}")
            log(f"suite: {p} passed, {f} failed, {s} skipped")
            # the deepest single-file test, for the stack table
            deepest = 0
            for label, files, marker in cases:
                if len(files) != 1:
                    continue
                # no -a: the tree printer recurses too, and would be measured
                r = run([os.path.join(bin_dir, v.name + '_stack')] + files, cwd=CPP)
                u = parse_stack(r.stdout + r.stderr)
                deepest = max(deepest, u or 0)
            m['suite_stack'][v.name] = deepest
            m['ladder'][v.name] = ladder_measure(os.path.join(bin_dir, v.name + '_stack'))
            log(f"stack: deepest test {deepest:,}, per level "
                f"{m['ladder'][v.name]['per_level'] if m['ladder'][v.name] else '-'}")

        for w, files in WORKLOADS.items():
            m['runs'][v.name][w] = measure(v, root, files, ('plain', 'peak', 'bodies', 'stack'))
            r = m['runs'][v.name][w]
            got = r.get('unit')
            if got and got != want_unit[v.name]:
                raise SystemExit(
                    f"{v.name}/{w}: the driver reports {got}, but the body walk was "
                    f"patched for {want_unit[v.name]} - fix strval_source for this "
                    f"variant before trusting any of these numbers")
            pk = r['peak']
            bd = r['bodies']
            log(f"{w}: peak {pk['peak']:,} store {pk['store']:,} allocs {pk['allocs']:,} "
                f"bodies {bd['bodies']:,} on_input {bd['on_input']:,}")

        if alt:
            m['utf8ptr'][v.name] = {}
            for w, files in WORKLOADS.items():
                p = run([alt] + load_args(files), cwd=CPP)
                h, _ = tree_dump(p.stdout + p.stderr)
                m['utf8ptr'][v.name][w] = h
            log("byte-pointer Source: builds, tree " +
                ('identical to the default build'
                 if all(m['utf8ptr'][v.name][w] == m['runs'][v.name][w]['hash']
                        for w in WORKLOADS) else 'DIFFERS from the default build'))


    with open(os.path.join(OUT, 'raw.json'), 'w') as fh:
        json.dump({k: (v if not isinstance(v, dict) or 'dump' not in v else
                       {kk: vv for kk, vv in v.items() if kk != 'dump'})
                   for k, v in m.items()}, fh, indent=1, default=str)

    text = build_tables(m)
    with open(os.path.join(OUT, 'tables.txt'), 'w') as fh:
        fh.write(text)
    print()
    print(text)
    print(f"({time.time() - t0:.1f}s; raw measurements in out/raw.json)")


if __name__ == '__main__':
    main()
