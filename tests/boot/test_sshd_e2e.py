##
# @file     test_sshd_e2e.py
# @brief    End-to-end OpenSSH sshd test over QEMU hostfwd with real ssh client
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify real OpenSSH client logins into Microkernel /boot/sshd over QEMU."""

# need:: VER_IPC_0014

from __future__ import annotations

import argparse
import os
from pathlib import Path
import pty
import select
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
        'menuentry "Microkernel (SSHD E2E Test)" {\n'
        '    multiboot2 /boot/kernel.elf init=sh\n'
        '    module2 /boot/boot.tar bootfs\n'
        '    boot\n'
        '}\n',
        encoding="utf-8",
    )
    shutil.copy2(_BUILD / "kernel.elf", iso_dir / "boot" / "kernel.elf")
    shutil.copy2(_BUILD / "boot.tar", iso_dir / "boot" / "boot.tar")
    iso_path = tmpdir / "microkernel-sshd.iso"
    res = subprocess.run(
        ["grub-mkrescue", "-o", str(iso_path), str(iso_dir)],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"grub-mkrescue failed:\n{res.stderr}")
    return iso_path


def _run_ssh_with_pty(
    cmd: list[str],
    interactions: list[tuple[bytes, bytes]],
    timeout: float = 45.0,
) -> tuple[int, bytes]:
    pid, master_fd = pty.fork()
    if pid == 0:
        os.execvp(cmd[0], cmd)
        os._exit(127)
    out = bytearray()
    step = 0
    deadline = time.monotonic() + timeout
    exit_code = -1
    try:
        while time.monotonic() < deadline:
            rlist, _, _ = select.select([master_fd], [], [], 0.2)
            if master_fd in rlist:
                try:
                    chunk = os.read(master_fd, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                out.extend(chunk)
                while step < len(interactions):
                    expect_pat, reply = interactions[step]
                    if expect_pat in out:
                        if reply:
                            os.write(master_fd, reply)
                        step += 1
                    else:
                        break
            wpid, status = os.waitpid(pid, os.WNOHANG)
            if wpid == pid:
                exit_code = (
                    os.waitstatus_to_exitcode(status)
                    if hasattr(os, "waitstatus_to_exitcode")
                    else (status >> 8)
                )
                while True:
                    rlist, _, _ = select.select([master_fd], [], [], 0.05)
                    if master_fd not in rlist:
                        break
                    try:
                        chunk = os.read(master_fd, 4096)
                    except OSError:
                        break
                    if not chunk:
                        break
                    out.extend(chunk)
                return exit_code, bytes(out)
        # Reap briefly if PTY hit EOF, otherwise kill after timeout.
        reap_end = time.monotonic() + 2.0
        while time.monotonic() < reap_end:
            wpid, status = os.waitpid(pid, os.WNOHANG)
            if wpid == pid:
                exit_code = (
                    os.waitstatus_to_exitcode(status)
                    if hasattr(os, "waitstatus_to_exitcode")
                    else (status >> 8)
                )
                return exit_code, bytes(out)
            time.sleep(0.05)
        try:
            os.kill(pid, 9)
        except OSError:
            pass
        try:
            os.waitpid(pid, 0)
        except OSError:
            pass
        return exit_code, bytes(out)
    finally:
        os.close(master_fd)


class SshdEndToEndTests(unittest.TestCase):
    """Real OpenSSH ssh client -> QEMU SLIRP hostfwd -> Microkernel sshd."""

    def test_sshd_pubkey_password_and_interactive_pty(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            tmpdir = Path(td)
            iso_path = _build_sh_iso(tmpdir)
            disk_path = tmpdir / "disk.img"
            disk_path.write_bytes(b"\x00" * (4 * 1024 * 1024))
            host_port = _pick_free_port()

            key_path = tmpdir / "id_ed25519"
            kg = subprocess.run(
                [
                    "ssh-keygen",
                    "-t",
                    "ed25519",
                    "-N",
                    "",
                    "-f",
                    str(key_path),
                    "-q",
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                kg.returncode, 0, f"ssh-keygen failed: {kg.stderr}"
            )
            pubkey = (tmpdir / "id_ed25519.pub").read_text(
                encoding="utf-8"
            ).strip()

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
                f"user,id=net0,hostfwd=tcp:127.0.0.1:{host_port}-:22",
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

            def _wait_serial(needle: bytes, timeout: float = 60.0) -> bool:
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

            base_ssh = [
                "ssh",
                "-p",
                str(host_port),
                "-o",
                "StrictHostKeyChecking=no",
                "-o",
                "UserKnownHostsFile=/dev/null",
                "-o",
                "ConnectTimeout=15",
                "-o",
                "LogLevel=ERROR",
            ]

            try:
                self.assertTrue(
                    _wait_serial(b"[OK] sshd listening on port 22", 60.0),
                    "sshd did not start on boot:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )
                self.assertTrue(
                    _wait_serial(b"$ ", 30.0),
                    "shell prompt did not appear:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )

                # Install temporary client Ed25519 public key into guest.
                install_cmd = (
                    f"/boot/printf '%s\\n' '{pubkey}' "
                    "> /mnt/root/.ssh/authorized_keys && "
                    "/boot/chmod 600 /mnt/root/.ssh/authorized_keys && "
                    "/boot/echo KEY''DONE\n"
                ).encode("utf-8")
                _send_serial(install_cmd)
                self.assertTrue(
                    _wait_serial(b"KEYDONE", 30.0),
                    "failed to install authorized_keys:\n"
                    f"{serial_buf.decode(errors='replace')}",
                )

                # 1. Ed25519 public-key authentication + non-interactive exec.
                r1 = subprocess.run(
                    base_ssh
                    + [
                        "-i",
                        str(key_path),
                        "-o",
                        "BatchMode=yes",
                        "-o",
                        "IdentitiesOnly=yes",
                        "root@127.0.0.1",
                        "/boot/uname -s && /boot/id -un",
                    ],
                    capture_output=True,
                    text=True,
                    timeout=45.0,
                )
                self.assertEqual(
                    r1.returncode,
                    0,
                    f"ssh pubkey exec failed (rc={r1.returncode}):\n"
                    f"stdout={r1.stdout!r}\nstderr={r1.stderr!r}\n"
                    f"serial:\n{serial_buf.decode(errors='replace')}",
                )
                self.assertIn("Microkernel", r1.stdout)
                self.assertIn("root", r1.stdout)

                # 2. Password authentication (dev password: microkernel).
                rc_pw, out_pw = _run_ssh_with_pty(
                    base_ssh
                    + [
                        "-o",
                        "PubkeyAuthentication=no",
                        "-o",
                        "PreferredAuthentications=password",
                        "root@127.0.0.1",
                        "/boot/echo PW_AUTH_OK",
                    ],
                    [(b"password:", b"microkernel\n")],
                    timeout=45.0,
                )
                self.assertEqual(
                    rc_pw,
                    0,
                    f"ssh password auth failed (rc={rc_pw}):\n"
                    f"out={out_pw.decode(errors='replace')}\n"
                    f"serial:\n{serial_buf.decode(errors='replace')}",
                )
                self.assertIn(b"PW_AUTH_OK", out_pw)

                # 3. Interactive PTY session (-tt) checking /dev/pts/ support.
                rc_pty, out_pty = _run_ssh_with_pty(
                    base_ssh
                    + [
                        "-tt",
                        "-i",
                        str(key_path),
                        "-o",
                        "BatchMode=yes",
                        "-o",
                        "IdentitiesOnly=yes",
                        "root@127.0.0.1",
                    ],
                    [
                        (
                            b"$ ",
                            b"/boot/tty && /boot/stty -a >/dev/null && exit\n",
                        )
                    ],
                    timeout=45.0,
                )
                self.assertEqual(
                    rc_pty,
                    0,
                    f"ssh interactive PTY failed (rc={rc_pty}):\n"
                    f"out={out_pty.decode(errors='replace')}\n"
                    f"serial:\n{serial_buf.decode(errors='replace')}",
                )
                self.assertIn(b"/dev/pts/", out_pty)
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
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SshdEndToEndTests)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
