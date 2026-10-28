#!/bin/sh
# Runs every example in examples/ (debug and release builds) and every reject case in g2c2/tests/reject/.
# Usage, from the repository root: sh g2c2/tests/run.sh build/g2c2/g2c2
#
# An example x.go2, or a project folder x/ holding project.go2, expects:
#   x.out   its output (required)
#   x.exit  its exit code, when not 0
#   x.err   what it writes to stderr, when anything
# and for a release build x.release.out, x.release.exit, x.release.err where those differ.
# A reject case x.go2 (or folder x/) expects G2C2 to refuse it, writing x.err to stderr.

g2c2=$1
if [ -z "$g2c2" ]; then echo "usage: sh g2c2/tests/run.sh <g2c2>"; exit 2; fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
passed=0
failed=0

# expected <base> <suffix> <ext> <default>: the expectation file's contents, or the default
pick() {
    if [ -f "$1$2.$3" ]; then echo "$1$2.$3"; elif [ -f "$1.$3" ]; then echo "$1.$3"; else echo ""; fi
}

report_fail() {
    failed=$((failed + 1))
    echo "FAIL $1"
    shift
    for line in "$@"; do echo "     $line"; done
}

check_example() {
    src=$1; base=$2; mode=$3
    flag=""; suffix=""
    if [ "$mode" = release ]; then flag="--release"; suffix=".release"; fi
    out_file=$(pick "$base" "$suffix" out)
    exit_file=$(pick "$base" "$suffix" exit)
    err_file=$(pick "$base" "$suffix" err)
    if [ -z "$out_file" ]; then report_fail "$base ($mode)" "no $base.out"; return; fi
    "$g2c2" run $flag "$src" > "$tmp/out" 2> "$tmp/err" < /dev/null
    code=$?
    want_code=0
    if [ -n "$exit_file" ]; then want_code=$(cat "$exit_file"); fi
    if [ -n "$err_file" ]; then cp "$err_file" "$tmp/want_err"; else : > "$tmp/want_err"; fi
    if ! cmp -s "$tmp/out" "$out_file"; then
        report_fail "$base ($mode)" "stdout differs:" "$(diff "$out_file" "$tmp/out" | head -20)"
    elif [ "$code" != "$want_code" ]; then
        report_fail "$base ($mode)" "exit code $code, expected $want_code" "$(head -5 "$tmp/err")"
    elif ! cmp -s "$tmp/err" "$tmp/want_err"; then
        report_fail "$base ($mode)" "stderr differs:" "$(diff "$tmp/want_err" "$tmp/err" | head -20)"
    else
        passed=$((passed + 1))
    fi
}

check_reject() {
    src=$1; base=$2
    if [ ! -f "$base.err" ]; then report_fail "$base" "no $base.err"; return; fi
    "$g2c2" run "$src" > "$tmp/out" 2> "$tmp/err" < /dev/null
    code=$?
    if [ "$code" != 1 ]; then
        report_fail "$base" "exit code $code, expected 1 (refused)" "$(head -5 "$tmp/err")"
    elif ! cmp -s "$tmp/err" "$base.err"; then
        report_fail "$base" "error differs:" "$(diff "$base.err" "$tmp/err" | head -20)"
    else
        passed=$((passed + 1))
    fi
}

for src in examples/*.go2 examples/*/project.go2; do
    [ -e "$src" ] || continue
    case "$src" in
        */project.go2) dir=${src%/project.go2}; check_example "$dir" "$dir" debug; check_example "$dir" "$dir" release ;;
        *) base=${src%.go2}; check_example "$src" "$base" debug; check_example "$src" "$base" release ;;
    esac
done

for src in g2c2/tests/reject/*.go2 g2c2/tests/reject/*/project.go2; do
    [ -e "$src" ] || continue
    case "$src" in
        */project.go2) dir=${src%/project.go2}; check_reject "$dir" "$dir" ;;
        *) check_reject "$src" "${src%.go2}" ;;
    esac
done

echo "$passed passed, $failed failed"
[ "$failed" = 0 ]
