#!/usr/bin/env bash
#
# Run each tests/*.adl file through adlmem and check the result against
# its declared expectation: a "// EXPECT: PASS" or "// EXPECT: FAIL"
# comment anywhere in the file (ADL's own // comment syntax). A file with
# neither marker is skipped, not counted as a pass or a failure.
#
# A run that dies from a signal is a failure whatever the marker says. The
# shell reports 128+signal for that, so a test whose expectation is FAIL
# (a crash regression test, say) cannot be satisfied by crashing - which is
# the one thing its exit code alone could not distinguish.
#
# A tests/<name>/ subdirectory of *.adl files is instead loaded together,
# in filename order, as one multi-file test - see the second loop below.
#
# Exit status: 0 if every non-skipped test matched its expectation, 1 otherwise.
#
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cpp_dir="$(dirname "$script_dir")"
cd "$cpp_dir" || exit 1

make -s adlmem || { echo "Build failed" >&2; exit 1; }

pass=0
fail=0
skip=0

for adl in tests/*.adl; do
	if grep -q 'EXPECT: FAIL' "$adl"; then
		expect=fail
	elif grep -q 'EXPECT: PASS' "$adl"; then
		expect=pass
	else
		printf 'SKIP  %s (no EXPECT: PASS/FAIL marker)\n' "$adl"
		skip=$((skip + 1))
		continue
	fi

	output=$(./adlmem -a "$adl" 2>&1)
	rc=$?
	if [ "$rc" -ge 128 ]; then
		printf 'CRASH %s (killed by signal %d)\n' "$adl" $((rc - 128))
		printf '%s\n' "$output" | tail -8 | sed 's/^/      /'
		fail=$((fail + 1))
		continue
	fi
	actual=$([ "$rc" -eq 0 ] && echo pass || echo fail)

	if [ "$actual" = "$expect" ]; then
		printf 'ok    %s (expected %s)\n' "$adl" "$expect"
		pass=$((pass + 1))
	else
		printf 'FAIL  %s (expected %s, got %s)\n' "$adl" "$expect" "$actual"
		printf '%s\n' "$output" | tail -8 | sed 's/^/      /'
		fail=$((fail + 1))
	fi
done

#
# Multi-file sequences: a tests/<name>/ directory of *.adl files, loaded
# together in filename order as one combined test (e.g. adl.adl followed
# by a file that continues inside the TOP it defines, with no "TOP {" of
# its own - see adlmem.cpp's main(): `sink.root_object =
# sink.last_object();`, set before each file after the first). The
# EXPECT marker is looked for across the whole group, not one file.
#
for dir in tests/*/; do
	dir=${dir%/}
	shopt -s nullglob
	files=("$dir"/*.adl)
	shopt -u nullglob
	[ ${#files[@]} -eq 0 ] && continue

	IFS=$'\n' sorted=($(printf '%s\n' "${files[@]}" | sort)); unset IFS

	if grep -q 'EXPECT: FAIL' "${sorted[@]}"; then
		expect=fail
	elif grep -q 'EXPECT: PASS' "${sorted[@]}"; then
		expect=pass
	else
		printf 'SKIP  %s (no EXPECT: PASS/FAIL marker)\n' "$dir"
		skip=$((skip + 1))
		continue
	fi

	args=()
	for f in "${sorted[@]}"; do
		args+=(-a "$f")
	done

	output=$(./adlmem "${args[@]}" 2>&1)
	rc=$?
	if [ "$rc" -ge 128 ]; then
		printf 'CRASH %s (killed by signal %d)\n' "$dir" $((rc - 128))
		printf '%s\n' "$output" | tail -8 | sed 's/^/      /'
		fail=$((fail + 1))
		continue
	fi
	actual=$([ "$rc" -eq 0 ] && echo pass || echo fail)

	if [ "$actual" = "$expect" ]; then
		printf 'ok    %s (expected %s)\n' "$dir" "$expect"
		pass=$((pass + 1))
	else
		printf 'FAIL  %s (expected %s, got %s)\n' "$dir" "$expect" "$actual"
		printf '%s\n' "$output" | tail -8 | sed 's/^/      /'
		fail=$((fail + 1))
	fi
done

echo
echo "$pass passed, $fail failed, $skip skipped"

[ "$fail" -eq 0 ]
