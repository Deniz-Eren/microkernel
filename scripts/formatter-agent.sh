#!/usr/bin/env bash
##
# @file     formatter-agent.sh
# @brief    Check or fix C formatting across kernel, libc, servers, and tests
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Formatter agent for microkernel. Command: scripts/formatter-agent.sh [--fix].
# Exit code is 0 when clean, non-zero on formatting violations.
set -uo pipefail
cd "$(dirname "$0")/.."

ENV=./scripts/env.sh
FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

MODE="$( [ "$FIX" -eq 1 ] && echo fix || echo check )"
echo "==> format ($MODE): kernel libc servers bin tests"

FIND_CMD="find kernel libc servers bin tests \( -name '*.c' -o -name '*.h' \)"
if [ "$FIX" -eq 1 ]; then
    exec "$ENV" bash -c \
        "$FIND_CMD -print0 | xargs -0 -r clang-format --style=file -i"
fi

LOG="$(mktemp)"
FMT_CMD="$FIND_CMD -print0 | xargs -0 -r"
FMT_CMD="$FMT_CMD clang-format --style=file --dry-run --Werror"
"$ENV" bash -c "$FMT_CMD" > "$LOG" 2>&1
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FORMAT: FAIL (log: $LOG)"
    grep -oP '^\S+\.(c|h)' "$LOG" | sort -u | head -20
    exit 1
fi
echo "FORMAT: PASS"
