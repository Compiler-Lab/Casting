#!/bin/sh
CC=${1:-./Casting}
tmp=$(mktemp -d /tmp/Casting-test-XXXXXX)
trap 'rm -rf "$tmp"' INT TERM HUP EXIT
echo > "$tmp/empty.c"

check() {
    if [ $? -eq 0 ]; then
        echo "testing $1 ... passed"
    else
        echo "testing $1 ... failed"
        exit 1
    fi
}

# -o <path>
rm -f "$tmp/out"
"$CC" -o "$tmp/out" "$tmp/empty.c" && [ -f "$tmp/out" ]
check -o

# -o<path>
rm -f "$tmp/out"
"$CC" "-o$tmp/out" "$tmp/empty.c" && [ -f "$tmp/out" ]
check -o-attached

# -o - (stdout)
"$CC" -o - "$tmp/empty.c" | grep -q '\.text'
check -o-stdout

# stdin input (-)
printf 'int main() { return 0; }\n' | "$CC" -o "$tmp/out" - && grep -q 'main:' "$tmp/out"
check stdin

# --help
"$CC" --help 2>&1 | grep -q Casting
check --help

# unknown argument
! "$CC" --unknown-flag "$tmp/empty.c" 2>/dev/null
check unknown-arg

# no input files
! "$CC" -o "$tmp/out" 2>/dev/null
check no-input

echo OK
