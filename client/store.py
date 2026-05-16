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
SUPPORTED_SOLVERS = ("baseline", "improved", "ai")


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
        # The "saved snapshot" lives outside flush() so a reset doesn't wipe it.
        self._saved_snapshot_csv: Optional[str] = None
        self._saved_snapshot_label: Optional[str] = None
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
        if name not in SUPPORTED_SOLVERS:
            raise ValueError(
                f"Unknown solver '{name}'. Expected one of {SUPPORTED_SOLVERS}"
            )
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
    ) -> Tuple[int, List[str]]:
        if urgency not in URGENCY_LEVELS:
            raise ValueError(f"Bad urgency '{urgency}'. Expected one of {URGENCY_LEVELS}")
        topic_norm = topic.strip().lower()
        if not topic_norm:
            raise ValueError("Topic must not be empty")
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
                topic=topic_norm,
                urgency=urgency,
                week=self.current_week if week is None else week,
            )
            self.requests.append(req)
            warnings = self._feasibility_warnings(req)
            return len(self.requests) - 1, warnings

    # ----------------------------------------------------- feasibility helpers
    def _feasibility_warnings(self, req: Request) -> List[str]:
        """Return human-readable warnings about why *req* may be infeasible.

        These are intentionally soft warnings — the C++ backends handle every
        case below gracefully (the request just ends up `Not Assigned`). They
        let the UI flag known-bad inputs early so the user knows what to
        expect before they hit Solve.
        """
        warnings: List[str] = []
        topic = req.topic
        covering_mentors = [mid for mid, spec in self.mentors.items() if topic in spec]
        if not covering_mentors:
            warnings.append(
                f"No mentor lists '{topic}' as a specialty. "
                "Request will be classified as infeasibly unserved."
            )
            return warnings

        compatible_slot_ids: List[int] = []
        for sid in req.available_slot_ids:
            if not (0 <= sid < len(self.slots)):
                continue
            mentor_id = self.slots[sid].mentor_id
            if mentor_id in covering_mentors and self.slots[sid].week >= req.week:
                compatible_slot_ids.append(sid)
        if not compatible_slot_ids:
            warnings.append(
                f"None of the listed slots are owned by a mentor that covers "
                f"'{topic}' for week ≥ {req.week}. The request can never be "
                "satisfied with the current slot/mentor lineup."
            )
        elif len(compatible_slot_ids) < len(req.available_slot_ids):
            dropped = len(req.available_slot_ids) - len(compatible_slot_ids)
            warnings.append(
                f"{dropped} of the {len(req.available_slot_ids)} listed slot(s) "
                f"are not viable for '{topic}' (wrong mentor or earlier week)."
            )
        return warnings

    def validate_request(
        self,
        fellow_id: int,
        available_slot_ids: List[int],
        topic: str,
        urgency: str,
        week: Optional[int] = None,
    ) -> Dict[str, Any]:
        """Dry-run the same validation/feasibility logic as submit_request,
        but without mutating any state. Used by the UI for live previews.
        """
        info: Dict[str, Any] = {"errors": [], "warnings": [], "ok": True}
        topic_norm = (topic or "").strip().lower()
        urgency_norm = (urgency or "").strip().lower()
        if urgency_norm and urgency_norm not in URGENCY_LEVELS:
            info["errors"].append(
                f"Urgency must be one of {URGENCY_LEVELS} (got {urgency_norm!r})."
            )
        if not topic_norm:
            info["errors"].append("Topic must not be empty.")
        with self._lock:
            for sid in available_slot_ids:
                if not (0 <= sid < len(self.slots)):
                    info["errors"].append(f"Slot {sid} does not exist.")
            if any(r.fellow_id == fellow_id for r in self.requests):
                info["errors"].append(
                    f"Fellow {fellow_id} already has a pending request for the "
                    "current week."
                )
            if not info["errors"] and topic_norm and urgency_norm in URGENCY_LEVELS:
                preview = Request(
                    fellow_id=fellow_id,
                    available_slot_ids=list(available_slot_ids),
                    topic=topic_norm,
                    urgency=urgency_norm,
                    week=self.current_week if week is None else week,
                )
                info["warnings"] = self._feasibility_warnings(preview)
            info["known_topics"] = sorted({t for spec in self.mentors.values() for t in spec})
        if info["errors"]:
            info["ok"] = False
        return info

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
            known_topics = sorted({t for spec in self.mentors.values() for t in spec})
            req_dicts: List[Dict[str, Any]] = []
            for rid, r in enumerate(self.requests):
                d = {"request_id": rid, **r.to_dict()}
                d["warnings"] = self._feasibility_warnings(r)
                req_dicts.append(d)
            return {
                "active_solver": self.active_solver,
                "supported_solvers": list(SUPPORTED_SOLVERS),
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
                "requests": req_dicts,
                "schedule": {
                    str(w): rows for w, rows in sorted(self.schedule.items())
                },
                "solver_runs": [r.to_dict() for r in self.solver_runs[-50:]],
                "urgency_levels": list(URGENCY_LEVELS),
                "known_topics": known_topics,
                "saved_snapshot": {
                    "available": self._saved_snapshot_csv is not None,
                    "label": self._saved_snapshot_label,
                },
            }

    # -------------------------------------------------------- snapshot / reset
    def reset_to_snapshot(self) -> Dict[str, int]:
        """Restore the last imported CSV. Solver runs and progress made since
        the import are discarded."""
        with self._lock:
            if self._saved_snapshot_csv is None:
                raise ValueError(
                    "No snapshot is saved yet — import a CSV first to capture one."
                )
            csv_text = self._saved_snapshot_csv
            label = self._saved_snapshot_label
        # load_from_csv re-saves the same CSV; preserve its label.
        summary = self.load_from_csv(csv_text, label=label)
        return summary

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
    def load_from_csv(self, csv_text: str, label: Optional[str] = None) -> Dict[str, int]:
        """Replace the current hot state with the contents of *csv_text*.

        Format is a multi-section CSV where every row's first column is a type
        tag. See sample.csv for the canonical layout.

        Returns a small summary dict for the UI. The CSV is also stored as the
        "saved snapshot" so the Reset button can restore it later.
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
            prior_solver = self.active_solver
            self.flush()
            self.current_week = int(parsed["meta"].get("current_week", 0))
            self.active_solver = parsed["meta"].get("solver", prior_solver).lower()
            if self.active_solver not in SUPPORTED_SOLVERS:
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

            self._saved_snapshot_csv = csv_text
            self._saved_snapshot_label = label

            return {
                "fellows": len(self.fellows),
                "mentors": len(self.mentors),
                "slots": len(self.slots),
                "requests": len(self.requests),
                "current_week": self.current_week,
                "solver": self.active_solver,
                "snapshot_label": label,
            }
