#!/usr/bin/env bash
# tests/test_cli.sh — CLI smoke tests for md2, md4, md5 binaries
# Run from the repository root after 'make all'.
# Exit code: 0 = all passed, 1 = any failure.

set -euo pipefail

BIN=./bin
PASS=0; FAIL=0

check(){
    local label="$1" expected="$2"; shift 2
    local got; got=$("$@" 2>&1) || true
    if [ "$got" = "$expected" ]; then
        echo "PASS  $label"; PASS=$((PASS+1))
    else
        echo "FAIL  $label"
        echo "      expected: $expected"
        echo "      got     : $got"
        FAIL=$((FAIL+1))
    fi
}

check_rc(){
    local label="$1" expected_rc="$2"; shift 2
    set +e; "$@" >/dev/null 2>&1; local rc=$?; set -e
    if [ "$rc" = "$expected_rc" ]; then
        echo "PASS  $label (exit $rc)"; PASS=$((PASS+1))
    else
        echo "FAIL  $label (exit $rc, want $expected_rc)"; FAIL=$((FAIL+1))
    fi
}

# ── MD5 message hashing ──────────────────────────────────────────────
# ── MD5 message hashing ──────────────────────────────────────────────
# Empty string: output is " -> hash" (only 2 awk fields), use grep instead
if "$BIN/md5" "" | grep -q "d41d8cd98f00b204e9800998ecf8427e"; then
    echo "PASS  md5(\"\") = d41d8cd98f00b204e9800998ecf8427e"; PASS=$((PASS+1))
else
    echo "FAIL  md5(\"\"): $("$BIN/md5" "" 2>&1)"; FAIL=$((FAIL+1))
fi

# The CLI prints "msg -> hash" format
MD5_ABC=$("$BIN/md5" "abc" | awk '{print $3}')
if [ "$MD5_ABC" = "900150983cd24fb0d6963f7d28e17f72" ]; then
    echo "PASS  md5(abc) = $MD5_ABC"; PASS=$((PASS+1))
else
    echo "FAIL  md5(abc): got $MD5_ABC"; FAIL=$((FAIL+1))
fi

# ── --format upper ───────────────────────────────────────────────────
MD5_UP=$("$BIN/md5" --format upper "abc" | awk '{print $3}')
if [ "$MD5_UP" = "900150983CD24FB0D6963F7D28E17F72" ]; then
    echo "PASS  md5(abc) upper"; PASS=$((PASS+1))
else
    echo "FAIL  md5 upper: $MD5_UP"; FAIL=$((FAIL+1))
fi

# ── file hashing ─────────────────────────────────────────────────────
TMPF=$(mktemp); printf -v _ '%s' "" > "$TMPF"
echo -n "abc" > "$TMPF"
HASH_F=$("$BIN/md5" -f "$TMPF" | awk '{print $3}')
if [ "$HASH_F" = "900150983cd24fb0d6963f7d28e17f72" ]; then
    echo "PASS  md5 -f file(abc)"; PASS=$((PASS+1))
else
    echo "FAIL  md5 -f: $HASH_F"; FAIL=$((FAIL+1))
fi
rm -f "$TMPF"

# ── --compare match (exit 0) ─────────────────────────────────────────
check_rc "md5 --compare match"    0 "$BIN/md5" --compare "900150983cd24fb0d6963f7d28e17f72" "abc"

# ── --compare mismatch (exit 1) ──────────────────────────────────────
check_rc "md5 --compare mismatch" 1 "$BIN/md5" --compare "000000000000000000000000000000ff" "abc"

# ── --compare-file ───────────────────────────────────────────────────
HASHF=$(mktemp); echo "900150983cd24fb0d6963f7d28e17f72" > "$HASHF"
check_rc "md5 --compare-file match" 0 "$BIN/md5" --compare-file "$HASHF" "abc"
rm -f "$HASHF"

# ── MD4 and MD2 smoke ────────────────────────────────────────────────
MD4_A=$("$BIN/md4" "a" | awk '{print $3}')
if [ "$MD4_A" = "bde52cb31de33e46245e05fbdbd6fb24" ]; then
    echo "PASS  md4(a)"; PASS=$((PASS+1))
else
    echo "FAIL  md4(a): $MD4_A"; FAIL=$((FAIL+1))
fi

MD2_A=$("$BIN/md2" "a" | awk '{print $3}')
if [ "$MD2_A" = "32ec01ec4a6dac72c0ab96fb34c0b5d1" ]; then
    echo "PASS  md2(a)"; PASS=$((PASS+1))
else
    echo "FAIL  md2(a): $MD2_A"; FAIL=$((FAIL+1))
fi

# ── summary ──────────────────────────────────────────────────────────
echo ""
echo "CLI tests: $PASS passed, $FAIL failed."
[ "$FAIL" -eq 0 ]
