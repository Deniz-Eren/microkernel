#!/usr/bin/env bash
##
# @file     build-agent.sh
# @brief    Run pre-commit gates: format, text, STE, build, QEMU, lint, CTest
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Unified pre-commit gate for microkernel. Exit code is 0 only when every gate
# passes: clang-format, docs-traceability-setup check_text.py, ASD-STE100
# tools/check_ste.py, 4-stage build + Sphinx -W, QEMU tests, clang-tidy, and
# CTest.
set -uo pipefail
cd "$(dirname "$0")/.."

ENV=./scripts/env.sh
DEFAULT_TRACE_DIR="$HOME/.gemini/skills/docs-traceability-setup"
SKILL_DIR="${DOCS_TRACEABILITY_SKILL:-$DEFAULT_TRACE_DIR}"
CHECK_TEXT="$SKILL_DIR/scripts/check_text.py"

if ! ./scripts/formatter-agent.sh; then
    echo "BUILD: FAIL (format step)"
    exit 1
fi

echo "==> text (docs-traceability-setup)"
if [ ! -f "$CHECK_TEXT" ]; then
    echo "BUILD: FAIL (missing check_text.py at $CHECK_TEXT)"
    exit 1
fi
TEXT_LOG="$(mktemp)"
if ! python3 "$CHECK_TEXT" --root . > "$TEXT_LOG" 2>&1; then
    echo "BUILD: FAIL (check_text step; log: $TEXT_LOG)"
    head -30 "$TEXT_LOG"
    exit 1
fi

echo "==> ste (asd-ste100)"
STE_LOG="$(mktemp)"
if ! python3 tools/check_ste.py --root . > "$STE_LOG" 2>&1; then
    echo "BUILD: FAIL (ste step; log: $STE_LOG)"
    head -40 "$STE_LOG"
    exit 1
fi

echo "==> build"
rm -rf build/docs/doctrees
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
    --expect 'sigaction rejects SIGKILL and SIGSTOP with EINVAL' \
    --expect 'errno thread-local and preserved across success' \
    --expect 'signal handler entered' \
    --expect 'signal mask saved and restored across sigreturn' \
    --expect 'VMMap/mmap W\^X, mprotect, msync, and munmap verified' \
    --expect 'ProcCreate spawned procchild' \
    --expect 'procchild COW read and private write succeeded' \
    --expect 'waitpid reaped procchild .* exit=42' \
    --expect 'VMCloneCow isolated parent and child writes cleanly' \
    --expect 'rtcdrv granted Ring 3 IOPL=3' \
    --expect 'rtcdrv handled 3 hardware IRQ 8 events via IPC' \
    --expect 'unprivileged IOPLSet and AttachIntrEvent rejected with EPERM' \
    --expect 'drvtest ReadMsgV/WriteMsgV /dev/rtc round trip succeeded' \
    --expect 'CLOCK_MONOTONIC, CLOCK_REALTIME, and clock_nanosleep verified' \
    --expect 'POSIX timer_create/timer_settime fired .* SIGUSR1 ticks' \
    --expect 'pthreads FS\.base TLS, mutex, spinlock, barrier, rwlock' \
    --expect 'pthread_cond_clockwait and sem_wait/sem_clockwait verified' \
    --expect 'pipeserv registered /dev/pipe' \
    --expect 'fork COW isolated parent and child \(child=.* exit=33\)' \
    --expect 'pipe2, dup3 O_CLOEXEC, and execve argv/envp/pipe verified' \
    --expect 'ppoll finite timeout and atomic sigmask EINTR verified' \
    --expect 'aio_read, aio_write, aio_suspend, and SIGEV_SIGNAL verified' \
    --expect 'aio_cancel, aio_suspend EAGAIN timeout, and aio_fsync verified' \
    --expect 'lio_listio LIO_WAIT and LIO_NOWAIT batch verified' \
    --expect 'pthread_cancel woke cond_wait and ran cleanup' \
    --expect 'PTHREAD_CANCEL_DISABLE, testcancel, and LIFO cleanup verified' \
    --expect 'cancellation points read, ppoll, sem_wait, and clock_nanosleep' \
    --expect 'flockfile/ftrylockfile/funlockfile serialized concurrent stdio' \
    > "$TEST_LOG" 2>&1; then
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

echo "==> ctest"
CTEST_LOG="$(mktemp)"
if ! "$ENV" ctest --test-dir build --output-on-failure > "$CTEST_LOG" 2>&1; then
    echo "BUILD: FAIL (ctest step; log: $CTEST_LOG)"
    tail -30 "$CTEST_LOG"
    exit 1
fi
grep -E 'tests passed|Total Test time' "$CTEST_LOG" || true

echo "BUILD: PASS"
