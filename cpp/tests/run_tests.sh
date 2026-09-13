#!/usr/bin/env bash
#
# Run each tests/*.adl file through adlmem and check the result against
# its declared expectation: a "// EXPECT: PASS" or "// EXPECT: FAIL"
# comment anywhere in the file (ADL's own // comment syntax). A file with
# neither marker is skipped, not counted as a pass or a failure.
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

echo
echo "$pass passed, $fail failed, $skip skipped"

[ "$fail" -eq 0 ]
