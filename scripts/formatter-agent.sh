#!/usr/bin/env bash
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#
# Formatter agent for microkernel. It does a check of the formatting (no lint).
# Command: scripts/formatter-agent.sh [--fix]
# The exit code is 0 when the formatting is clean, and not 0 when there are violations.
set -uo pipefail
cd "$(dirname "$0")/.."

ENV=./scripts/env.sh
FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

echo "==> format ($([ "$FIX" -eq 1 ] && echo fix || echo check)): kernel libc servers tests"

if [ "$FIX" -eq 1 ]; then
    exec "$ENV" bash -c \
        "find kernel libc servers tests \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r clang-format --style=file -i"
fi

LOG="$(mktemp)"
"$ENV" bash -c \
    "find kernel libc servers tests \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r clang-format --style=file --dry-run --Werror" \
    > "$LOG" 2>&1
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FORMAT: FAIL (log: $LOG)"
    grep -oP '^\S+\.(c|h)' "$LOG" | sort -u | head -20
    exit 1
fi
echo "FORMAT: PASS"
