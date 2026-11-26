#!/usr/bin/env bash
##
# @file     env.sh
# @brief    Podman development container wrapper for build, test, and shell runs
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# Microkernel Podman environment wrapper.
#
# Usage: ./scripts/env.sh <configure|build|build-code|test|run|coverage|clean|
#                          next-ids|task|check-text|check-ste|shell|cmd...>
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${MICROKERNEL_DEV_IMAGE:-microkernel-dev}"
ARCH="${MICROKERNEL_ARCH:-x86-64}"

ensure_image() {
    if ! podman image exists "$IMAGE"; then
        echo "[env.sh] building container image $IMAGE ..."
        podman build -t "$IMAGE" "$ROOT"
    fi
}

crun() {
    podman run --rm -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
}

crun_tty() {
    local extra=()
    while [ "$#" -gt 0 ] && [ "$1" = "--podman-arg" ]; do
        extra+=("$2")
        shift 2
    done
    if [ -t 0 ] && [ -t 1 ]; then
        podman run --rm -it "${extra[@]}" \
            -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
    else
        podman run --rm -i "${extra[@]}" \
            -v "$ROOT:/workspace:Z" -w /workspace "$IMAGE" "$@"
    fi
}

case "${1:-shell}" in
    configure)
        shift
        ensure_image
        crun cmake -S . -B build -G Ninja \
            -DMICROKERNEL_ARCH="$ARCH" \
            -DCMAKE_TOOLCHAIN_FILE="cmake/clang-${ARCH}.cmake" \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
        ;;
    build)
        shift
        ensure_image
        crun cmake --build build "$@"
        ;;
    build-code)
        shift
        ensure_image
        crun cmake --build build --target iso "$@"
        ;;
    test)
        shift
        for _arg in "$@"; do
            case "$_arg" in
                /tmp/*|/var/*|/home/*)
                    echo "[env.sh] error: host path '$_arg' is not" \
                        "mounted in the container." >&2
                    echo "[env.sh] hint: use a relative path inside the" \
                        "repo (e.g. build/...), mounted at /workspace." >&2
                    exit 2
                    ;;
            esac
        done
        ensure_image
        crun python3 tests/test_runner.py --arch "$ARCH" "$@"
        ;;
    run)
        shift
        ensure_image
        NET_PORTS="${MICROKERNEL_NET_PORTS:-2222:22,8080:8080}"
        PODMAN_PORT_ARGS=()
        HOSTFWD_SPEC=""
        if [ "$NET_PORTS" != "none" ] && [ -n "$NET_PORTS" ]; then
            IFS=',' read -r -a _pairs <<< "$NET_PORTS"
            for _p in "${_pairs[@]}"; do
                _hp="${_p%%:*}"
                _gp="${_p##*:}"
                PODMAN_PORT_ARGS+=(
                    --podman-arg "-p"
                    --podman-arg "127.0.0.1:${_hp}:${_hp}"
                )
                HOSTFWD_SPEC="${HOSTFWD_SPEC},hostfwd=tcp::${_hp}-:${_gp}"
            done
        fi
        echo "[env.sh] booting interactive Microkernel shell (Ctrl-A X to exit)"
        if [ -n "$HOSTFWD_SPEC" ]; then
            echo "[env.sh] host->guest localhost ports: $NET_PORTS"
        fi
        crun_tty "${PODMAN_PORT_ARGS[@]}" bash -c '
            set -euo pipefail
            HOSTFWD_SPEC="$1"
            shift
            if [ ! -f build/kernel.elf ] || [ ! -f build/boot.tar ]; then
                cmake --build build
            fi
            mkdir -p build/iso-sh/boot/grub
            cat <<EOF > build/iso-sh/boot/grub/grub.cfg
set timeout=0
set default=0
menuentry "Microkernel (Interactive Shell)" {
    multiboot2 /boot/kernel.elf init=sh
    module2 /boot/boot.tar bootfs
    boot
}
EOF
            cp build/kernel.elf build/iso-sh/boot/kernel.elf
            cp build/boot.tar build/iso-sh/boot/boot.tar
            grub-mkrescue -o build/microkernel-shell.iso build/iso-sh \
                >/dev/null 2>&1
            if [ ! -f build/disk.img ]; then
                dd if=/dev/zero of=build/disk.img bs=1M count=4 status=none
            fi
            exec qemu-system-x86_64 -machine q35 -cpu max \
                -m 256M -smp 2 -cdrom build/microkernel-shell.iso \
                -drive id=hd0,file=build/disk.img,if=none,format=raw \
                -device virtio-blk-pci,drive=hd0,disable-legacy=on \
                -netdev "user,id=net0${HOSTFWD_SPEC}" \
                -device virtio-net-pci,netdev=net0,disable-legacy=on \
                -serial mon:stdio -display none -no-reboot "$@"
        ' -- "$HOSTFWD_SPEC" "$@"
        ;;
    coverage)
        shift
        ensure_image
        crun bash -c '
            set -euo pipefail
            ARCH="$1"
            shift
            cmake -S . -B build-cov -G Ninja \
                -DMICROKERNEL_ARCH="$ARCH" \
                -DCMAKE_TOOLCHAIN_FILE="cmake/clang-${ARCH}.cmake" \
                -DMICROKERNEL_COVERAGE=ON "$@"
            cmake --build build-cov
            rm -rf build-cov/coverage
            mkdir -p build-cov/coverage/raw
            python3 tests/test_runner.py \
                --arch "$ARCH" \
                --iso build-cov/microkernel.iso \
                --timeout 420 \
                --expect-file scripts/qemu-expects.txt \
                --cov-out build-cov/coverage/raw \
                --wait-cov
            python3 tools/coverage/generate_report.py \
                --raw-dir build-cov/coverage/raw \
                --build-dir build-cov \
                --out-dir build-cov/coverage
        ' -- "$ARCH" "$@"
        ;;
    clean)
        rm -rf "$ROOT/build" "$ROOT/build-cov" "$ROOT/sysroot"
        ;;
    next-ids)
        python3 - "$ROOT" << 'PY'
import re, sys
from pathlib import Path

root = Path(sys.argv[1]) / "docs"
pats = [
    ("SWR", re.compile(r":id:\s+(SWR_[A-Z]+)_(\d{4})\b")),
    ("COMP", re.compile(r":id:\s+(COMP_[A-Z]+)_(\d{4})\b")),
    ("VER", re.compile(r":id:\s+(VER_[A-Z]+)_(\d{4})\b")),
    ("ICD", re.compile(r":id:\s+(ICD_[A-Z]+)_(\d{4})\b")),
    ("ADR", re.compile(r":id:\s+(ADR)_(\d{4})\b")),
    ("BDR", re.compile(r":id:\s+(BDR)_(\d{4})\b")),
    ("WDR", re.compile(r":id:\s+(WDR)_(\d{4})\b")),
    ("TSK", re.compile(r":id:\s+(TSK)_(\d{4})\b")),
    ("RDM", re.compile(r":id:\s+(RDM)_(\d{4})\b")),
]
high: dict[str, int] = {}
for p in root.rglob("*.md"):
    txt = p.read_text(encoding="utf-8", errors="ignore")
    for _, rx in pats:
        for m in rx.finditer(txt):
            key, num = m.group(1), int(m.group(2))
            if num > high.get(key, 0):
                high[key] = num
for key in sorted(high):
    print(f"{key}_{high[key] + 1:04d} (last: {key}_{high[key]:04d})")
PY
        ;;
    task)
        shift
        if [ "$#" -lt 1 ]; then
            echo "[env.sh] usage: ./scripts/env.sh task TSK_NNNN" >&2
            exit 2
        fi
        python3 - "$ROOT/docs/records/TASKS.md" "$1" << 'PY'
import re, sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
tid = sys.argv[2]
start = None
for idx, line in enumerate(lines):
    if re.match(rf"^###\s+{re.escape(tid)}\b", line):
        start = idx
        break
if start is None:
    print(f"Task {tid} not found in docs/records/TASKS.md", file=sys.stderr)
    sys.exit(1)
end = len(lines)
for idx in range(start + 1, len(lines)):
    if lines[idx].startswith("### "):
        end = idx
        break
for idx in range(start, end):
    print(f"{idx + 1}: {lines[idx]}")
PY
        ;;
    check-text)
        shift
        DEFAULT_TRACE_DIR="$HOME/.agents/skills/docs-traceability-setup"
        SKILL_DIR="${DOCS_TRACEABILITY_SKILL:-$DEFAULT_TRACE_DIR}"
        CHECK_TEXT="$SKILL_DIR/scripts/check_text.py"
        if [ ! -f "$CHECK_TEXT" ]; then
            echo "[env.sh] missing check_text.py at $CHECK_TEXT" >&2
            exit 1
        fi
        exec python3 "$CHECK_TEXT" --root "$ROOT" "$@"
        ;;
    check-ste)
        shift
        exec python3 "$ROOT/tools/check_ste.py" --root "$ROOT" "$@"
        ;;
    shell)
        shift
        if [ "$#" -gt 0 ]; then
            echo "[env.sh] error: 'env.sh shell' opens an interactive" \
                "shell and takes no arguments." >&2
            echo "[env.sh] hint: omit 'shell' to run a command:" \
                "./scripts/env.sh <cmd...>" >&2
            exit 2
        fi
        ensure_image
        crun_tty bash
        ;;
    *)
        ensure_image
        crun "$@"
        ;;
esac
