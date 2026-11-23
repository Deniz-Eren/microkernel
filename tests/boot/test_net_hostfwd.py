##
# @file     test_net_hostfwd.py
# @brief    Verify host-to-guest TCP over QEMU SLIRP hostfwd and virtio-net
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify host->guest TCP over QEMU SLIRP hostfwd, virtio-net, and /boot/nc."""

# need:: VER_IPC_0012

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
_BUILD = ROOT / "build"


def _pick_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def _build_sh_iso(tmpdir: Path) -> Path:
    iso_dir = tmpdir / "iso-sh"
    grub_dir = iso_dir / "boot" / "grub"
    grub_dir.mkdir(parents=True, exist_ok=True)
    (grub_dir / "grub.cfg").write_text(
        'set timeout=0\n'
        'set default=0\n'
        'menuentry "Microkernel (Hostfwd Test)" {\n'
        '    multiboot2 /boot/kernel.elf init=sh\n'
        '    module2 /boot/boot.tar bootfs\n'
        '    boot\n'
        '}\n',
        encoding="utf-8",
    )
    shutil.copy2(_BUILD / "kernel.elf", iso_dir / "boot" / "kernel.elf")
    shutil.copy2(_BUILD / "boot.tar", iso_dir / "boot" / "boot.tar")
    iso_path = tmpdir / "microkernel-hostfwd.iso"
    res = subprocess.run(
        ["grub-mkrescue", "-o", str(iso_path), str(iso_dir)],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"grub-mkrescue failed:\n{res.stderr}")
    return iso_path


class HostForwardNetTests(unittest.TestCase):
    """End-to-end host -> QEMU SLIRP hostfwd -> virtio-net -> guest TCP test."""

    def test_host_to_guest_tcp_burst_over_virtio_net(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            tmpdir = Path(td)
            iso_path = _build_sh_iso(tmpdir)
            disk_path = tmpdir / "disk.img"
            disk_path.write_bytes(b"\x00" * (2 * 1024 * 1024))
            host_port = _pick_free_port()

            qemu_cmd = [
                "qemu-system-x86_64",
                "-machine",
                "q35",
                "-cpu",
                "max",
                "-m",
                "256M",
                "-smp",
                "2",
                "-cdrom",
                str(iso_path),
                "-drive",
                f"id=hd0,file={disk_path},if=none,format=raw",
                "-device",
                "virtio-blk-pci,drive=hd0,disable-legacy=on",
                "-netdev",
                f"user,id=net0,hostfwd=tcp:127.0.0.1:{host_port}-:8080",
                "-device",
                "virtio-net-pci,netdev=net0,disable-legacy=on",
                "-serial",
                "stdio",
                "-display",
                "none",
                "-no-reboot",
                "-no-shutdown",
                "-monitor",
                "none",
            ]

            proc = subprocess.Popen(
                qemu_cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
            )
            serial_buf = bytearray()
            lock = threading.Lock()

            def _reader() -> None:
                assert proc.stdout is not None
                while True:
                    chunk = proc.stdout.read(256)
                    if not chunk:
                        break
                    with lock:
                        serial_buf.extend(chunk)

            t = threading.Thread(target=_reader, daemon=True)
            t.start()

            def _wait_serial(needle: bytes, timeout: float = 30.0) -> bool:
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    with lock:
                        if needle in serial_buf:
                            return True
                    time.sleep(0.05)
                return False

            def _send_serial(data: bytes) -> None:
                assert proc.stdin is not None
                # Pace writes under the 16-byte 16550 UART RX FIFO depth.
                for i in range(0, len(data), 4):
                    proc.stdin.write(data[i : i + 4])
                    proc.stdin.flush()
                    time.sleep(0.015)

            try:
                self.assertTrue(
                    _wait_serial(b"vfsserv registered /dev/vfs"),
                    "vfsserv did not initialize:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )
                self.assertTrue(
                    _wait_serial(
                        b"netserv registered /dev/net (nic=virtio-net)"
                    ),
                    "netserv did not initialize:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )
                self.assertTrue(
                    _wait_serial(b"$ "),
                    "shell prompt did not appear:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )

                # Start TCP listener translating lowercase->uppercase via nc.
                _send_serial(b"/boot/nc -l -e '/boot/tr a-z A-Z' 8080 &\n")
                time.sleep(0.5)
                _send_serial(b"/boot/echo NET''READY\n")
                self.assertTrue(
                    _wait_serial(b"NETREADY"),
                    "guest nc listener not ready:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )

                # Connect from host to guest port 8080 over SLIRP hostfwd with a
                # > 4 KiB multi-segment burst to exercise RX/TX hardening.
                payload = b"hello microkernel hostfwd!\n" * 160
                expected = payload.upper()

                received = bytearray()
                last_err: Exception | None = None
                for _attempt in range(15):
                    received = bytearray()
                    try:
                        with socket.create_connection(
                            ("127.0.0.1", host_port), timeout=5.0
                        ) as cli:
                            cli.settimeout(10.0)
                            cli.sendall(payload)
                            cli.shutdown(socket.SHUT_WR)
                            while True:
                                data = cli.recv(4096)
                                if not data:
                                    break
                                received.extend(data)
                        last_err = None
                        if bytes(received) == expected:
                            break
                    except OSError as exc:
                        last_err = exc
                    time.sleep(0.3)

                self.assertIsNone(
                    last_err,
                    f"host->guest TCP connect failed: {last_err}\n"
                    f"{serial_buf.decode(errors='replace')}",
                )
                self.assertEqual(
                    bytes(received),
                    expected,
                    f"host->guest TCP echo mismatch: got {len(received)} B, "
                    f"expected {len(expected)} B\n"
                    f"{serial_buf.decode(errors='replace')}",
                )
            finally:
                proc.terminate()
                try:
                    proc.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5.0)


def main() -> int:
    global _BUILD
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=str(_BUILD))
    args, _ = ap.parse_known_args()
    _BUILD = Path(args.build_dir)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        HostForwardNetTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
