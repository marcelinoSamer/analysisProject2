"""Subprocess wrapper around the compiled C++ scheduler backends.

We do not re-implement scheduling in Python — the existing ``Human_Solution``
binaries are reused as-is. This module is responsible only for:

* locating the compiled binaries (and rebuilding them on demand);
* serialising the hot store into the backend's stdin format;
* running the binary with a wall-clock timer;
* parsing the textual ``Request i: ...`` output back into a vector of
  assignments.
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple


CLIENT_DIR = Path(__file__).resolve().parent
BUILD_DIR = CLIENT_DIR / "build"
SOURCE_DIR = CLIENT_DIR.parent / "Human_Solution"

SUPPORTED_SOLVERS = ("baseline", "improved")

_ASSIGNED_RE = re.compile(r"Request (\d+):\s+Assigned to Slot (\d+)")
_UNASSIGNED_RE = re.compile(r"Request (\d+):\s+Not Assigned")


class SolverError(RuntimeError):
    pass


@dataclass
class SolverResult:
    solver: str
    elapsed_ms: float
    assignments: List[int]
    raw_output: str
    timed_out_fallback: bool


def binary_path(solver: str) -> Path:
    if solver not in SUPPORTED_SOLVERS:
        raise SolverError(f"Unknown solver '{solver}'")
    suffix = ".exe" if os.name == "nt" else ""
    return BUILD_DIR / f"{solver}{suffix}"


def ensure_built(solver: str) -> Path:
    path = binary_path(solver)
    if path.exists() and os.access(path, os.X_OK):
        return path
    build_all()
    if not path.exists():
        raise SolverError(
            f"Could not locate compiled '{solver}' binary at {path}. "
            "Run client/build_cpp.sh manually."
        )
    return path


def build_all() -> None:
    script = CLIENT_DIR / "build_cpp.sh"
    if not script.exists():
        raise SolverError(f"Build script missing at {script}")
    if shutil.which("bash") is None:
        raise SolverError("bash is required to run the build script")
    proc = subprocess.run(
        ["bash", str(script)],
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise SolverError(
            "Compilation failed.\n"
            f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )


def run(solver: str, stdin_payload: str, num_requests: int, timeout_s: float = 60.0) -> SolverResult:
    """Run the selected backend against *stdin_payload* and parse its output."""
    bin_path = ensure_built(solver)

    start = time.perf_counter()
    try:
        proc = subprocess.run(
            [str(bin_path)],
            input=stdin_payload,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired as exc:
        raise SolverError(
            f"Solver '{solver}' exceeded the {timeout_s:.1f}s timeout."
        ) from exc
    elapsed_ms = (time.perf_counter() - start) * 1000.0

    if proc.returncode != 0:
        raise SolverError(
            f"Solver '{solver}' exited with code {proc.returncode}.\n"
            f"stderr:\n{proc.stderr}"
        )

    assignments = [-1] * num_requests
    fallback_used = False

    for line in proc.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        if "Backtracking timed out" in line:
            fallback_used = True
            continue
        m = _ASSIGNED_RE.match(line)
        if m:
            req_idx = int(m.group(1))
            slot_id = int(m.group(2))
            if 0 <= req_idx < num_requests:
                assignments[req_idx] = slot_id
            continue
        m = _UNASSIGNED_RE.match(line)
        if m:
            continue

    return SolverResult(
        solver=solver,
        elapsed_ms=elapsed_ms,
        assignments=assignments,
        raw_output=proc.stdout,
        timed_out_fallback=fallback_used,
    )
