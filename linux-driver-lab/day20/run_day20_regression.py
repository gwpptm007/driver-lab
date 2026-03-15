#!/usr/bin/env python3
"""Day20 host-side regression runner.

This first version tries to automate the current W3 arm64 + QEMU virt flow:
1. auto-detect Image/rootfs/dtb/demo_regmap.ko from prior days (prefer day18)
2. boot QEMU on a pseudo terminal
3. wait for shell prompt and normalize PS1 to DAY20#
4. upload guest scripts over the serial console
5. run smoke / trace / perf checks
6. pull back pass_fail.env and text artifacts into records/

It is intentionally conservative: failure to boot or missing prompt is reported clearly,
so the user can tell whether the issue is in boot, guest checks, or later collection.
"""
from __future__ import annotations

import argparse
import datetime as dt
import os
import pty
import re
import select
import shlex
import signal
import subprocess
import sys
import textwrap
import time
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[1]
DAY20_DIR = Path(__file__).resolve().parent
GUEST_DIR = DAY20_DIR / "guest"
RECORDS_DIR = DAY20_DIR / "records"

PROMPT = "DAY20# "
MARKER_PREFIX = "__DAY20_MARKER__"
FILE_BEGIN = "__DAY20_FILE_BEGIN__"
FILE_END = "__DAY20_FILE_END__"
ENV_BEGIN = "__DAY20_ENV_BEGIN__"
ENV_END = "__DAY20_ENV_END__"


def now_stamp() -> str:
    return dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def auto_detect_path(candidates: Iterable[Path]) -> Optional[Path]:
    for path in candidates:
        if path.exists():
            return path
    return None


def resolve_artifacts(args: argparse.Namespace, allow_missing: bool = False) -> Dict[str, Optional[Path]]:
    rootfs = Path(args.rootfs) if args.rootfs else auto_detect_path(
        [
            REPO_ROOT / "day18" / "rootfs.img",
            REPO_ROOT / "day17" / "rootfs.img",
            REPO_ROOT / "day15" / "rootfs.img",
        ]
    )
    dtb = Path(args.dtb) if args.dtb else auto_detect_path(
        [
            REPO_ROOT / "day18" / "virt-day18.dtb",
            REPO_ROOT / "day17" / "virt-day17.dtb",
            REPO_ROOT / "day15" / "virt-day15.dtb",
        ]
    )
    module = Path(args.module) if args.module else auto_detect_path(
        [
            REPO_ROOT / "day18" / "demo_regmap.ko",
            REPO_ROOT / "day17" / "demo_regmap.ko",
            REPO_ROOT / "day15" / "demo_regmap.ko",
            REPO_ROOT / "day18" / "records" / "20260315-142432-day18-classified-arm64-virt" / "build_evidence" / "demo_regmap.ko",
            REPO_ROOT / "day17" / "records" / "20260314-231128-day17-round2b-arm64-virt" / "build_evidence" / "demo_regmap.ko",
        ]
    )
    image = Path(args.image) if args.image else auto_detect_path(
        [
            REPO_ROOT.parent / "kernel-src" / "linux-5.15.10" / "output" / "arm64" / "Image",
        ]
    )
    artifacts = {
        "image": image,
        "rootfs": rootfs,
        "dtb": dtb,
        "module": module,
    }
    missing = [name for name, path in artifacts.items() if path is None or not path.exists()]
    if missing and not allow_missing:
        names = ", ".join(missing)
        raise SystemExit(f"[ERROR] missing required artifact(s): {names}. Pass explicit --image/--rootfs/--dtb/--module or build prior days first.")
    resolved: Dict[str, Optional[Path]] = {}
    for name, path in artifacts.items():
        resolved[name] = path.resolve() if path and path.exists() else None
    return resolved


class PtyProcess:
    def __init__(self, argv: List[str], log_path: Path):
        self.argv = argv
        self.log_fp = log_path.open("w", encoding="utf-8", errors="replace")
        self.pid: Optional[int] = None
        self.master_fd: Optional[int] = None
        self.alive = False

    def start(self) -> None:
        master_fd, slave_fd = pty.openpty()
        pid = subprocess.Popen(
            self.argv,
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            close_fds=True,
            preexec_fn=os.setsid,
        )
        os.close(slave_fd)
        self.pid = pid.pid
        self._proc = pid
        self.master_fd = master_fd
        self.alive = True

    def write(self, data: str) -> None:
        assert self.master_fd is not None
        os.write(self.master_fd, data.encode("utf-8", errors="ignore"))

    def read_until(self, patterns: Iterable[str], timeout: float) -> Tuple[str, str]:
        assert self.master_fd is not None
        regexes = [(pattern, re.compile(re.escape(pattern))) for pattern in patterns]
        deadline = time.time() + timeout
        buf = ""
        while time.time() < deadline:
            remaining = max(0.0, deadline - time.time())
            r, _, _ = select.select([self.master_fd], [], [], min(0.5, remaining))
            if not r:
                continue
            chunk = os.read(self.master_fd, 4096).decode("utf-8", errors="replace")
            if not chunk:
                break
            self.log_fp.write(chunk)
            self.log_fp.flush()
            buf += chunk
            for literal, rx in regexes:
                if rx.search(buf):
                    return buf, literal
        raise TimeoutError(f"timeout waiting for patterns: {patterns}")

    def terminate(self) -> None:
        if not self.alive:
            return
        try:
            if self.pid is not None:
                os.killpg(self.pid, signal.SIGTERM)
                time.sleep(1)
                try:
                    os.killpg(self.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        except ProcessLookupError:
            pass
        if self.master_fd is not None:
            try:
                os.close(self.master_fd)
            except OSError:
                pass
        self.log_fp.close()
        self.alive = False


def wait_for_boot_prompt(proc: PtyProcess, boot_timeout: float) -> None:
    patterns = ["~ # ", "/ # ", "# "]
    proc.read_until(patterns, boot_timeout)
    proc.write("export PS1='DAY20# '\n")
    proc.read_until([PROMPT], 30)


def run_command(proc: PtyProcess, cmd: str, timeout: float = 60.0) -> str:
    marker = f"{MARKER_PREFIX}:{int(time.time() * 1000)}"
    wrapped = f"{cmd}; rc=$?; echo {marker}:$rc\n"
    proc.write(wrapped)
    out, _ = proc.read_until([marker], timeout)
    proc.read_until([PROMPT], 10)
    return out


def send_heredoc(proc: PtyProcess, remote_path: str, content: str) -> None:
    cmd = f"cat > {shlex.quote(remote_path)} <<'__DAY20_EOF__'\n{content}\n__DAY20_EOF__\nchmod +x {shlex.quote(remote_path)}\n"
    proc.write(cmd)
    proc.read_until([PROMPT], 30)


def fetch_named_blocks(text: str, begin_token: str, end_token: str) -> Dict[str, str]:
    out: Dict[str, List[str]] = {}
    current_name: Optional[str] = None
    collecting: List[str] = []
    for raw_line in text.splitlines():
        line = raw_line.rstrip("\n")
        if line.startswith(begin_token + " "):
            current_name = line.split(" ", 1)[1]
            collecting = []
            continue
        if line.startswith(end_token + " "):
            name = line.split(" ", 1)[1]
            if current_name == name:
                out[name] = collecting[:]
            current_name = None
            collecting = []
            continue
        if current_name is not None:
            collecting.append(raw_line)
    return {k: "\n".join(v).rstrip() + ("\n" if v else "") for k, v in out.items()}


def fetch_env_blocks(text: str) -> Dict[str, str]:
    """Merge all __DAY20_ENV blocks from one or more guest script runs."""
    inside = False
    env: Dict[str, str] = {}
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if line == ENV_BEGIN:
            inside = True
            continue
        if line == ENV_END:
            inside = False
            continue
        if inside and "=" in line:
            key, value = line.split("=", 1)
            env[key] = value
    return env


def expected_keys_for_mode(mode: str) -> List[str]:
    base = [
        "DEBUGFS_OK",
        "DEMO_INSMOD_OK",
        "SNAPSHOT_OK",
        "TRIGGER_OK",
        "RMMOD_OK",
        "DMESG_CLEAN",
    ]
    if mode == "smoke":
        return base
    if mode == "trace":
        return ["DEBUGFS_OK", "TRACING_OK", "FUNCTION_GRAPH_PRESENT", "FGRAPH_OK", "DMESG_CLEAN"]
    if mode == "perf":
        return ["DEBUGFS_OK", "PERF_BIN_OK", "PERF_OK", "DMESG_CLEAN"]
    if mode == "stress":
        return base + ["STRESS_OK"]
    return base + ["TRACING_OK", "FUNCTION_GRAPH_PRESENT", "FGRAPH_OK", "PERF_BIN_OK", "PERF_OK", "STRESS_OK"]


def write_summary(record_dir: Path, statuses: Dict[str, str], mode: str) -> None:
    summary_lines = [
        "Day20 regression summary",
        f"mode={mode}",
        "",
    ]
    if not statuses:
        summary_lines.append("No guest statuses captured; check serial.log and host_runner.log first.")
    else:
        for key in sorted(statuses):
            summary_lines.append(f"{key}={statuses[key]}")

    expected = expected_keys_for_mode(mode)
    failed = [key for key in expected if statuses.get(key) == "0"]
    missing = [key for key in expected if key not in statuses]
    summary_lines.append("")
    if failed or missing:
        summary_lines.append("REGRESSION_PASS=0")
        summary_lines.append("FAIL_KEYS=" + (",".join(failed) if failed else ""))
        summary_lines.append("MISSING_KEYS=" + (",".join(missing) if missing else ""))
    else:
        summary_lines.append("REGRESSION_PASS=1")
        summary_lines.append("FAIL_KEYS=")
        summary_lines.append("MISSING_KEYS=")
    (record_dir / "summary.txt").write_text("\n".join(summary_lines) + "\n", encoding="utf-8")


def write_plan_files(record_dir: Path, args: argparse.Namespace, artifacts: Dict[str, Optional[Path]], argv: List[str]) -> None:
    (record_dir / "host_plan.env").write_text(
        textwrap.dedent(
            f"""\
            mode={args.mode}
            boot_timeout={args.boot_timeout}
            command_timeout={args.command_timeout}
            trace_required={args.trace_required}
            perf_required={args.perf_required}
            qemu_memory_mb={args.memory}
            image={artifacts['image'] or "MISSING"}
            rootfs={artifacts['rootfs'] or "MISSING"}
            dtb={artifacts['dtb'] or "MISSING"}
            module={artifacts['module'] or "MISSING"}
            """
        ),
        encoding="utf-8",
    )
    (record_dir / "host_command.txt").write_text(" ".join(shlex.quote(x) for x in argv) + "\n", encoding="utf-8")


def build_qemu_argv(args: argparse.Namespace, artifacts: Dict[str, Optional[Path]]) -> List[str]:
    return [
        args.qemu_bin,
        "-machine",
        args.machine,
        "-cpu",
        args.cpu,
        "-m",
        str(args.memory),
        "-nographic",
        "-kernel",
        str(artifacts["image"]),
        "-dtb",
        str(artifacts["dtb"]),
        "-initrd",
        str(artifacts["rootfs"]),
        "-append",
        args.kernel_cmdline,
    ]


def prepare_guest_payload() -> Dict[str, str]:
    payload = {}
    for name in [
        "guest_day20_common.sh",
        "guest_day20_smoke.sh",
        "guest_day20_trace.sh",
        "guest_day20_perf.sh",
        "guest_day20_stress.sh",
    ]:
        payload[name] = (GUEST_DIR / name).read_text(encoding="utf-8")
    return payload


def run_mode(proc: PtyProcess, mode: str, args: argparse.Namespace) -> str:
    script_map = {
        "smoke": "/tmp/guest_day20_smoke.sh",
        "trace": "/tmp/guest_day20_trace.sh",
        "perf": "/tmp/guest_day20_perf.sh",
        "stress": "/tmp/guest_day20_stress.sh",
    }
    selected = [mode] if mode in script_map else ["smoke", "trace", "perf", "stress"]
    merged_output = ""
    for item in selected:
        env_prefix = ""
        if item == "trace":
            env_prefix = f"TRACE_REQUIRED={shlex.quote(args.trace_required)} "
        elif item == "perf":
            env_prefix = f"PERF_REQUIRED={shlex.quote(args.perf_required)} "
        cmd = env_prefix + script_map[item]
        merged_output += run_command(proc, cmd, timeout=args.command_timeout)
    return merged_output


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Day20 QEMU regression runner")
    parser.add_argument("--mode", choices=["smoke", "trace", "perf", "stress", "all"], default="all")
    parser.add_argument("--image")
    parser.add_argument("--rootfs")
    parser.add_argument("--dtb")
    parser.add_argument("--module")
    parser.add_argument("--boot-timeout", type=float, default=180.0)
    parser.add_argument("--command-timeout", type=float, default=90.0)
    parser.add_argument("--trace-required", choices=["yes", "no"], default="yes")
    parser.add_argument("--perf-required", choices=["yes", "no"], default="yes")
    parser.add_argument("--qemu-bin", default=os.environ.get("QEMU_BIN", "qemu-system-aarch64"))
    parser.add_argument("--machine", default="virt")
    parser.add_argument("--cpu", default="cortex-a57")
    parser.add_argument("--memory", type=int, default=1024)
    parser.add_argument("--kernel-cmdline", default="console=ttyAMA0 root=/dev/ram0 rw rdinit=/init")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    RECORDS_DIR.mkdir(exist_ok=True)
    artifacts = resolve_artifacts(args, allow_missing=args.dry_run)
    record_dir = RECORDS_DIR / f"{now_stamp()}-day20-{args.mode}-arm64-virt"
    record_dir.mkdir(parents=True, exist_ok=False)

    qemu_argv = build_qemu_argv(args, artifacts)
    write_plan_files(record_dir, args, artifacts, qemu_argv)
    payload = prepare_guest_payload()

    if args.dry_run:
        missing = [k for k, v in artifacts.items() if v is None]
        lines = ["Day20 dry-run only.", "Detected artifacts:"]
        lines.extend(f"- {k}: {v or 'MISSING'}" for k, v in artifacts.items())
        lines.append("")
        lines.append("DRY_RUN_READY=1" if not missing else "DRY_RUN_READY=0")
        if missing:
            lines.append("MISSING_ARTIFACTS=" + ",".join(missing))
        (record_dir / "summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"[INFO] dry-run record created: {record_dir}")
        return 0

    missing = [k for k, v in artifacts.items() if v is None]
    if missing:
        raise SystemExit(f"[ERROR] missing runtime artifact(s): {', '.join(missing)}")

    serial_log = record_dir / "serial.log"
    host_log = record_dir / "host_runner.log"
    proc = PtyProcess(qemu_argv, serial_log)
    host_fp = host_log.open("w", encoding="utf-8", errors="replace")
    statuses: Dict[str, str] = {}
    try:
        host_fp.write("[INFO] starting qemu\n")
        host_fp.flush()
        proc.start()
        wait_for_boot_prompt(proc, args.boot_timeout)
        host_fp.write("[INFO] boot prompt reached\n")
        host_fp.flush()

        run_command(proc, "mkdir -p /tmp/day20 && rm -f /tmp/day20/*")
        for name, content in payload.items():
            remote_path = f"/tmp/{name}"
            send_heredoc(proc, remote_path, content)
            host_fp.write(f"[INFO] uploaded {name} -> {remote_path}\n")

        merged_output = run_mode(proc, args.mode, args)
        env_status = fetch_env_blocks(merged_output)
        statuses.update(env_status)

        blocks = fetch_named_blocks(merged_output, FILE_BEGIN, FILE_END)
        for name, content in blocks.items():
            (record_dir / name).write_text(content, encoding="utf-8")

        if statuses:
            with (record_dir / "pass_fail.env").open("w", encoding="utf-8") as fp:
                for key in sorted(statuses):
                    fp.write(f"{key}={statuses[key]}\n")
        write_summary(record_dir, statuses, args.mode)
        host_fp.write("[INFO] regression finished\n")
        host_fp.flush()
        print(f"[INFO] day20 record: {record_dir}")
        return 0
    except Exception as exc:  # noqa: BLE001
        host_fp.write(f"[ERROR] {exc}\n")
        host_fp.flush()
        write_summary(record_dir, statuses, args.mode)
        print(f"[ERROR] day20 regression failed: {exc}", file=sys.stderr)
        print(f"[INFO] inspect record dir: {record_dir}", file=sys.stderr)
        return 1
    finally:
        proc.terminate()
        host_fp.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
