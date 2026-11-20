#!/usr/bin/env bash
##
# @file     build-agent.sh
# @brief    Run build, QEMU tests, and clang-tidy static analysis
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Build agent: build, test and static analysis for microkernel. The exit code is
# 0 for PASS, and not 0 for FAIL.
set -uo pipefail
cd "$(dirname "$0")/.."

ENV=./scripts/env.sh

echo "==> build"
BUILD_LOG="$(mktemp)"
if ! "$ENV" build > "$BUILD_LOG" 2>&1; then
    echo "BUILD: FAIL (build step; log: $BUILD_LOG)"
    tail -20 "$BUILD_LOG"
    exit 1
fi

echo "==> test"
TEST_LOG="$(mktemp)"
if ! "$ENV" test --timeout 120 \
    --expect '\[OK\] PMM/VMM/APIC initialized' \
    --expect 'name resolved /dev/echo' \
    --expect 'errno thread-local and preserved across success' \
    --expect 'signal handler entered' > "$TEST_LOG" 2>&1; then
    echo "BUILD: FAIL (test step; log: $TEST_LOG)"
    tail -20 "$TEST_LOG"
    exit 1
fi
TESTS="$(grep -c '^\[OK\]' "$TEST_LOG" || true)"
echo "tests: $TESTS [OK] expectations matched"

echo "==> lint"
LINT_LOG="$(mktemp)"
TIDY_CMD="run-clang-tidy -p build -quiet \$(find kernel libc servers tests"
TIDY_CMD="$TIDY_CMD \( -name '*.c' -o -name '*.h' \))"
"$ENV" bash -c "$TIDY_CMD" > "$LINT_LOG" 2>&1
if grep -qE 'warning:|error:' "$LINT_LOG"; then
    echo "BUILD: FAIL (lint findings; log: $LINT_LOG)"
    grep -E 'warning:|error:' "$LINT_LOG" | head -20
    exit 1
fi

echo "BUILD: PASS"
