"""Solver runners for the scheduler backends.

The production backends are the existing C++ binaries. This module also
contains one intentionally naive Python greedy backend for comparison.
It is responsible for:

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
from typing import Dict, List


CLIENT_DIR = Path(__file__).resolve().parent
BUILD_DIR = CLIENT_DIR / "build"
SOURCE_DIR = CLIENT_DIR.parent / "Human_Solution"

SUPPORTED_SOLVERS = ("baseline", "improved", "ai", "greedy")

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
    if solver == "greedy":
        raise SolverError("The greedy solver is built into Python and has no binary.")
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


DEFAULT_TIMEOUT_S = float(os.environ.get("SCHED_SOLVER_TIMEOUT_S", "10"))


def run(solver: str, stdin_payload: str, num_requests: int, timeout_s: float = DEFAULT_TIMEOUT_S) -> SolverResult:
    """Run the selected backend against *stdin_payload* and parse its output."""
    if solver == "greedy":
        return _run_bad_greedy(stdin_payload, num_requests)

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


def _run_bad_greedy(stdin_payload: str, num_requests: int) -> SolverResult:
    """A deliberately simple business-baseline greedy.

    It processes requests in input order and takes the first free feasible slot.
    This is intentionally not globally optimal; it is useful for showing that a
    weak algorithm can look acceptable on easy business cases.
    """
    start = time.perf_counter()
    tokens = stdin_payload.split()
    pos = 0

    def take() -> str:
        nonlocal pos
        if pos >= len(tokens):
            raise SolverError("Malformed solver input for greedy backend.")
        value = tokens[pos]
        pos += 1
        return value

    try:
        f_count = int(take())
        m_count = int(take())
        s_count = int(take())
        r_count = int(take())

        for _ in range(f_count):
            take()

        mentors: Dict[int, set[str]] = {}
        for _ in range(m_count):
            mentor_id = int(take())
            n_spec = int(take())
            mentors[mentor_id] = {take().lower() for _ in range(n_spec)}

        slots: List[tuple[int, int]] = []
        for _ in range(s_count):
            slots.append((int(take()), int(take())))

        requests = []
        for _ in range(r_count):
            fellow_id = int(take())
            n_slots = int(take())
            available = [int(take()) for _ in range(n_slots)]
            topic = take().lower()
            urgency = take()
            week = int(take())
            requests.append((fellow_id, available, topic, urgency, week))
    except (ValueError, IndexError) as exc:
        raise SolverError(f"Malformed solver input for greedy backend: {exc}") from exc

    assignments = [-1] * num_requests
    used_slots: set[int] = set()
    lines: List[str] = []

    for req_idx, (_, available, topic, _, req_week) in enumerate(requests[:num_requests]):
        chosen = -1
        for slot_id in available:
            if slot_id in used_slots or not (0 <= slot_id < len(slots)):
                continue
            mentor_id, slot_week = slots[slot_id]
            if slot_week < req_week:
                continue
            if topic not in mentors.get(mentor_id, set()):
                continue
            chosen = slot_id
            break

        if chosen == -1:
            lines.append(f"Request {req_idx}: Not Assigned")
        else:
            used_slots.add(chosen)
            assignments[req_idx] = chosen
            mentor_id, slot_week = slots[chosen]
            lines.append(
                f"Request {req_idx}: Assigned to Slot {chosen} "
                f"(Mentor ID: {mentor_id}, Week: {slot_week})"
            )

    elapsed_ms = (time.perf_counter() - start) * 1000.0
    return SolverResult(
        solver="greedy",
        elapsed_ms=elapsed_ms,
        assignments=assignments,
        raw_output="\n".join(lines) + "\n",
        timed_out_fallback=False,
    )
