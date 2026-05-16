"""In-memory ("hot storage") state for the mentorship-scheduling client.

Everything lives in plain Python data structures — no database is instantiated.
A single :class:`HotStore` instance is created at startup and shared across the
Flask request handlers behind a re-entrant lock so concurrent UI actions don't
corrupt state.

The store tracks:

* fellows         – set of integer fellow IDs
* mentors         – {mentor_id: [specialty, ...]}  (specialties lowercased)
* slots           – list of dicts {mentor_id, week}  (slot ID == list index)
* requests        – queue of pending requests for the current week
* starvation      – {fellow_id: s_f} consecutive-weeks-unserved counter
* schedule        – {week: [{slot_id, mentor_id, fellow_id|None, ...}, ...]}
* solver_runs     – chronological log of solve invocations with timing data
"""
from __future__ import annotations

import threading
from dataclasses import dataclass, field, asdict
from typing import Any, Dict, List, Optional, Set, Tuple


URGENCY_LEVELS = ("exploratory", "normal", "blocker")
URGENCY_WEIGHT = {"exploratory": 1, "normal": 2, "blocker": 3}


@dataclass
class Request:
    fellow_id: int
    available_slot_ids: List[int]
    topic: str
    urgency: str
    week: int

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class Slot:
    mentor_id: int
    week: int

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class SolverRun:
    week: int
    solver: str
    elapsed_ms: float
    assignments: List[int]
    penalty: int
    num_requests: int
    num_assigned: int

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


def _penalty(urgency: str, delay: int) -> int:
    """Urgency-weighted delay penalty: w * (d*(d+1)*(2d+1))/6 (matches C++ backend)."""
    if delay <= 0:
        return 0
    w = URGENCY_WEIGHT[urgency]
    return w * (delay * (delay + 1) * (2 * delay + 1)) // 6


class HotStore:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self.flush()

    # ------------------------------------------------------------------ basics
    def flush(self) -> None:
        with self._lock:
            self.fellows: Set[int] = set()
            self.mentors: Dict[int, List[str]] = {}
            self.slots: List[Slot] = []
            self.requests: List[Request] = []
            self.starvation: Dict[int, int] = {}
            self.schedule: Dict[int, List[Dict[str, Any]]] = {}
            self.solver_runs: List[SolverRun] = []
            self.current_week: int = 0
            self.active_solver: str = "improved"

    # ----------------------------------------------------------------- mutators
    def set_solver(self, name: str) -> None:
        if name not in ("baseline", "improved"):
            raise ValueError(f"Unknown solver '{name}'")
        with self._lock:
            self.active_solver = name

    def add_fellow(self, fellow_id: int) -> None:
        with self._lock:
            self.fellows.add(fellow_id)
            self.starvation.setdefault(fellow_id, 0)

    def remove_fellow(self, fellow_id: int) -> None:
        with self._lock:
            self.fellows.discard(fellow_id)
            self.starvation.pop(fellow_id, None)
            self.requests = [r for r in self.requests if r.fellow_id != fellow_id]

    def upsert_mentor(self, mentor_id: int, specialties: List[str]) -> None:
        clean = sorted({s.strip().lower() for s in specialties if s.strip()})
        with self._lock:
            self.mentors[mentor_id] = clean

    def remove_mentor(self, mentor_id: int) -> None:
        with self._lock:
            self.mentors.pop(mentor_id, None)
            self.slots = [s for s in self.slots if s.mentor_id != mentor_id]

    def add_slot(self, mentor_id: int, week: int) -> int:
        with self._lock:
            if mentor_id not in self.mentors:
                raise ValueError(f"Mentor {mentor_id} does not exist")
            self.slots.append(Slot(mentor_id=mentor_id, week=week))
            return len(self.slots) - 1

    def remove_slot(self, slot_id: int) -> None:
        with self._lock:
            if not (0 <= slot_id < len(self.slots)):
                raise ValueError(f"Slot {slot_id} does not exist")
            # We rebuild the slot list and rewrite slot IDs inside requests.
            del self.slots[slot_id]
            for req in self.requests:
                req.available_slot_ids = [
                    (s if s < slot_id else s - 1)
                    for s in req.available_slot_ids
                    if s != slot_id
                ]

    def submit_request(
        self,
        fellow_id: int,
        available_slot_ids: List[int],
        topic: str,
        urgency: str,
        week: Optional[int] = None,
    ) -> int:
        if urgency not in URGENCY_LEVELS:
            raise ValueError(f"Bad urgency '{urgency}'. Expected one of {URGENCY_LEVELS}")
        with self._lock:
            if fellow_id not in self.fellows:
                self.add_fellow(fellow_id)
            if any(r.fellow_id == fellow_id for r in self.requests):
                raise ValueError(
                    f"Fellow {fellow_id} already has a pending request for the current week"
                )
            for sid in available_slot_ids:
                if not (0 <= sid < len(self.slots)):
                    raise ValueError(f"Slot {sid} does not exist")
            req = Request(
                fellow_id=fellow_id,
                available_slot_ids=list(available_slot_ids),
                topic=topic.strip().lower(),
                urgency=urgency,
                week=self.current_week if week is None else week,
            )
            self.requests.append(req)
            return len(self.requests) - 1

    def remove_request(self, request_idx: int) -> None:
        with self._lock:
            if not (0 <= request_idx < len(self.requests)):
                raise ValueError(f"Request {request_idx} does not exist")
            del self.requests[request_idx]

    # ------------------------------------------------- weekly-result book-keeping
    def commit_assignments(
        self, assignments: List[int], solver: str, elapsed_ms: float
    ) -> SolverRun:
        """Update starvation counters and the weekly schedule from a solver run."""
        with self._lock:
            week = self.current_week
            week_entry: List[Dict[str, Any]] = []

            # Build per-slot view first (for the schedule grid)
            assigned_lookup: Dict[int, int] = {}
            for req_idx, slot_id in enumerate(assignments):
                if slot_id != -1:
                    assigned_lookup[slot_id] = req_idx

            for slot_id, slot in enumerate(self.slots):
                if slot.week != week:
                    continue
                row = {
                    "slot_id": slot_id,
                    "mentor_id": slot.mentor_id,
                    "week": slot.week,
                    "fellow_id": None,
                    "topic": None,
                    "urgency": None,
                }
                if slot_id in assigned_lookup:
                    req = self.requests[assigned_lookup[slot_id]]
                    row["fellow_id"] = req.fellow_id
                    row["topic"] = req.topic
                    row["urgency"] = req.urgency
                week_entry.append(row)

            self.schedule[week] = week_entry

            # Starvation update + penalty calculation
            penalty_total = 0
            assigned_fellows: Set[int] = set()
            for req_idx, slot_id in enumerate(assignments):
                req = self.requests[req_idx]
                if slot_id == -1:
                    self.starvation[req.fellow_id] = self.starvation.get(req.fellow_id, 0) + 1
                else:
                    assigned_fellows.add(req.fellow_id)
                    slot = self.slots[slot_id]
                    delay = max(0, slot.week - req.week)
                    penalty_total += _penalty(req.urgency, delay)
                    self.starvation[req.fellow_id] = 0

            run = SolverRun(
                week=week,
                solver=solver,
                elapsed_ms=elapsed_ms,
                assignments=list(assignments),
                penalty=penalty_total,
                num_requests=len(self.requests),
                num_assigned=sum(1 for a in assignments if a != -1),
            )
            self.solver_runs.append(run)

            # Drain the request queue and advance the week
            self.requests.clear()
            self.current_week += 1
            return run

    # ----------------------------------------------------------------- snapshot
    def snapshot(self) -> Dict[str, Any]:
        with self._lock:
            return {
                "active_solver": self.active_solver,
                "current_week": self.current_week,
                "fellows": sorted(self.fellows),
                "starvation": dict(self.starvation),
                "mentors": [
                    {"id": mid, "specialties": list(spec)}
                    for mid, spec in sorted(self.mentors.items())
                ],
                "slots": [
                    {"slot_id": sid, **s.to_dict()} for sid, s in enumerate(self.slots)
                ],
                "requests": [
                    {"request_id": rid, **r.to_dict()} for rid, r in enumerate(self.requests)
                ],
                "schedule": {
                    str(w): rows for w, rows in sorted(self.schedule.items())
                },
                "solver_runs": [r.to_dict() for r in self.solver_runs[-50:]],
                "urgency_levels": list(URGENCY_LEVELS),
            }

    # ----------------------------------------------------------- backend payload
    def build_solver_input(self) -> Tuple[str, List[Request], List[Slot]]:
        """Serialise the current state into the C++ backend's stdin format.

        Returns the input string plus snapshots of the requests/slots lists
        consumed by the solver so the caller can compute output cleanly.
        """
        with self._lock:
            fellows = sorted(self.fellows)
            mentor_ids = sorted(self.mentors.keys())
            slots = list(self.slots)
            requests = list(self.requests)

            lines: List[str] = []
            lines.append(f"{len(fellows)} {len(mentor_ids)} {len(slots)} {len(requests)}")
            lines.append(" ".join(str(f) for f in fellows))
            for mid in mentor_ids:
                spec = self.mentors[mid]
                if not spec:
                    raise ValueError(
                        f"Mentor {mid} has no specialties; cannot run the solver."
                    )
                lines.append(f"{mid} {len(spec)} " + " ".join(spec))
            for slot in slots:
                lines.append(f"{slot.mentor_id} {slot.week}")
            for req in requests:
                a = req.available_slot_ids
                lines.append(
                    f"{req.fellow_id} {len(a)} "
                    + (" ".join(str(s) for s in a) + " " if a else "")
                    + f"{req.topic} {req.urgency} {req.week}"
                )

            return "\n".join(lines) + "\n", requests, slots

    # --------------------------------------------------------------- CSV import
    def load_from_csv(self, csv_text: str) -> Dict[str, int]:
        """Replace the current hot state with the contents of *csv_text*.

        Format is a multi-section CSV where every row's first column is a type
        tag. See sample.csv for the canonical layout.

        Returns a small summary dict for the UI.
        """
        import csv
        import io

        parsed = {
            "meta": {},
            "mentors": [],
            "slots": [],
            "fellows": [],
            "requests": [],
        }

        reader = csv.reader(io.StringIO(csv_text))
        for raw in reader:
            row = [cell.strip() for cell in raw if cell is not None]
            if not row or not row[0] or row[0].startswith("#"):
                continue
            kind = row[0].lower()
            if kind == "type":  # header row, skip
                continue
            if kind == "meta":
                if len(row) < 3:
                    raise ValueError(f"Bad meta row: {row}")
                parsed["meta"][row[1].lower()] = row[2]
            elif kind == "mentor":
                if len(row) < 3:
                    raise ValueError(f"Bad mentor row: {row}")
                mid = int(row[1])
                specialties = [s for s in row[2].replace(";", " ").split() if s]
                parsed["mentors"].append((mid, specialties))
            elif kind == "slot":
                if len(row) < 3:
                    raise ValueError(f"Bad slot row: {row}")
                parsed["slots"].append((int(row[1]), int(row[2])))
            elif kind == "fellow":
                fid = int(row[1])
                sf = int(row[2]) if len(row) > 2 and row[2] != "" else 0
                parsed["fellows"].append((fid, sf))
            elif kind == "request":
                if len(row) < 6:
                    raise ValueError(f"Bad request row: {row}")
                fid = int(row[1])
                topic = row[2].strip().lower()
                urgency = row[3].strip().lower()
                week = int(row[4])
                slot_field = row[5]
                slot_ids = [int(s) for s in slot_field.replace(";", " ").split() if s != ""]
                parsed["requests"].append((fid, slot_ids, topic, urgency, week))
            else:
                raise ValueError(f"Unknown row type '{kind}' in CSV")

        with self._lock:
            self.flush()
            self.current_week = int(parsed["meta"].get("current_week", 0))
            self.active_solver = parsed["meta"].get("solver", self.active_solver).lower()
            if self.active_solver not in ("baseline", "improved"):
                self.active_solver = "improved"

            for mid, spec in parsed["mentors"]:
                self.upsert_mentor(mid, spec)
            for mentor_id, week in parsed["slots"]:
                self.add_slot(mentor_id, week)
            for fid, sf in parsed["fellows"]:
                self.add_fellow(fid)
                self.starvation[fid] = sf
            for fid, slot_ids, topic, urgency, week in parsed["requests"]:
                self.submit_request(fid, slot_ids, topic, urgency, week)

            return {
                "fellows": len(self.fellows),
                "mentors": len(self.mentors),
                "slots": len(self.slots),
                "requests": len(self.requests),
                "current_week": self.current_week,
                "solver": self.active_solver,
            }
