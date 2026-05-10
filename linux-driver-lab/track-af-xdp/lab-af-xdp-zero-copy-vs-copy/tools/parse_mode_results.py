#!/usr/bin/env python3
import re
import sys
from pathlib import Path

CASES = [
    ("COPY_BASELINE", "COPY_BASELINE.log", "COPY_BASELINE.rc"),
    ("NATIVE_COPY", "NATIVE_COPY_PROBE.log", "NATIVE_COPY_PROBE.rc"),
    ("ZERO_COPY", "ZERO_COPY_PROBE.log", "ZERO_COPY_PROBE.rc"),
]

def read_rc(path: Path):
    if not path.exists():
        return "MISSING"
    try:
        return path.read_text(errors="ignore").strip() or "UNKNOWN"
    except Exception:
        return "UNKNOWN"

def classify(text: str, rc: str):
    if not text:
        return "MISSING"
    ready = all(s in text for s in ["UMEM_READY", "XSK_SOCKET_READY", "FILL_RING_READY"])
    final = "AF_XDP_FINAL_STATS" in text and "bye" in text
    if rc == "0" and ready:
        return "PASS" if final else "PASS_SOCKET_ONLY"
    lower = text.lower()
    if any(x in lower for x in ["operation not supported", "not supported", "invalid argument", "xsk_socket__create", "bpf_xdp_attach failed"]):
        return "UNSUPPORTED_OR_ATTACH_FAIL"
    if rc not in ("0", "MISSING"):
        return "FAIL"
    return "UNKNOWN"

def extract_first_error(text: str):
    for line in text.splitlines():
        low = line.lower()
        if any(k in low for k in ["failed", "error", "not supported", "invalid argument", "operation not"]):
            return line.strip()
    return ""

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} RECORD_DIR", file=sys.stderr)
        return 2
    rec = Path(sys.argv[1])
    print(f"RECORD_DIR={rec}")
    print()
    any_zc_probe = False
    copy_pass = False
    zero_copy_pass = False
    for name, log_name, rc_name in CASES:
        log_path = rec / log_name
        rc = read_rc(rec / rc_name)
        text = log_path.read_text(errors="ignore") if log_path.exists() else ""
        status = classify(text, rc)
        if name == "COPY_BASELINE" and status.startswith("PASS"):
            copy_pass = True
        if name == "ZERO_COPY" and log_path.exists():
            any_zc_probe = True
        if name == "ZERO_COPY" and status.startswith("PASS"):
            zero_copy_pass = True
        print(f"{name}_RC={rc}")
        print(f"{name}_STATUS={status}")
        err = extract_first_error(text)
        if err:
            print(f"{name}_FIRST_ERROR={err}")
        print()
    print(f"PASS_COPY_BASELINE={'YES' if copy_pass else 'NO'}")
    print(f"ZERO_COPY_PROBED={'YES' if any_zc_probe else 'NO'}")
    print(f"PASS_ZERO_COPY={'YES' if zero_copy_pass else 'NO'}")
    if any_zc_probe and not zero_copy_pass:
        print("ZERO_COPY_RESULT=NOT_SUPPORTED_OR_FAILED_ON_THIS_ENV")
        print("FALLBACK=USE_SKB_COPY_OR_NATIVE_COPY_IF_AVAILABLE")
    elif zero_copy_pass:
        print("ZERO_COPY_RESULT=SUPPORTED")
    else:
        print("ZERO_COPY_RESULT=NOT_PROBED")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
