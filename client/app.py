"""Flask front end for the weekly mentor-slot scheduler.

All persistent state lives in a single :class:`store.HotStore` instance kept in
process memory ("hot storage"). The scheduler itself is the existing
``Human_Solution`` C++ backend — see :mod:`solver`.

Run with::

    python -m client.app          # from the repo root, or
    python app.py                 # from inside ./client

then open http://127.0.0.1:5000/
"""
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict

from flask import Flask, jsonify, render_template, request

from store import HotStore, URGENCY_LEVELS
import solver

CLIENT_DIR = Path(__file__).resolve().parent

app = Flask(
    __name__,
    template_folder=str(CLIENT_DIR / "templates"),
    static_folder=str(CLIENT_DIR / "static"),
)

store = HotStore()


def _json_error(message: str, status: int = 400):
    return jsonify({"ok": False, "error": message}), status


# ----------------------------------------------------------------------- UI
@app.get("/")
def index():
    return render_template("index.html", urgency_levels=URGENCY_LEVELS)


# ---------------------------------------------------------------------- state
@app.get("/api/state")
def get_state():
    return jsonify({"ok": True, "state": store.snapshot()})


@app.post("/api/flush")
def flush():
    store.flush()
    return jsonify({"ok": True, "state": store.snapshot()})


@app.post("/api/solver")
def switch_solver():
    payload = request.get_json(silent=True) or {}
    name = (payload.get("solver") or "").strip().lower()
    try:
        store.set_solver(name)
    except ValueError as exc:
        return _json_error(str(exc))
    return jsonify({"ok": True, "active_solver": name})


# ------------------------------------------------------------------- mentors
@app.post("/api/mentor")
def add_mentor():
    payload = request.get_json(silent=True) or {}
    try:
        mentor_id = int(payload["mentor_id"])
        raw = payload.get("specialties", "")
        if isinstance(raw, str):
            specialties = [s for s in raw.replace(",", " ").split() if s]
        else:
            specialties = [str(s).strip() for s in raw if str(s).strip()]
        if not specialties:
            return _json_error("Mentor must have at least one specialty")
        store.upsert_mentor(mentor_id, specialties)
    except (KeyError, TypeError, ValueError) as exc:
        return _json_error(f"Bad mentor payload: {exc}")
    return jsonify({"ok": True, "state": store.snapshot()})


@app.delete("/api/mentor/<int:mentor_id>")
def delete_mentor(mentor_id: int):
    store.remove_mentor(mentor_id)
    return jsonify({"ok": True, "state": store.snapshot()})


# --------------------------------------------------------------------- slots
@app.post("/api/slot")
def add_slot():
    payload = request.get_json(silent=True) or {}
    try:
        mentor_id = int(payload["mentor_id"])
        week = int(payload["week"])
        store.add_slot(mentor_id, week)
    except (KeyError, TypeError, ValueError) as exc:
        return _json_error(f"Bad slot payload: {exc}")
    return jsonify({"ok": True, "state": store.snapshot()})


@app.delete("/api/slot/<int:slot_id>")
def delete_slot(slot_id: int):
    try:
        store.remove_slot(slot_id)
    except ValueError as exc:
        return _json_error(str(exc))
    return jsonify({"ok": True, "state": store.snapshot()})


# ------------------------------------------------------------------- fellows
@app.post("/api/fellow")
def add_fellow():
    payload = request.get_json(silent=True) or {}
    try:
        fellow_id = int(payload["fellow_id"])
        store.add_fellow(fellow_id)
    except (KeyError, TypeError, ValueError) as exc:
        return _json_error(f"Bad fellow payload: {exc}")
    return jsonify({"ok": True, "state": store.snapshot()})


@app.delete("/api/fellow/<int:fellow_id>")
def delete_fellow(fellow_id: int):
    store.remove_fellow(fellow_id)
    return jsonify({"ok": True, "state": store.snapshot()})


# ------------------------------------------------------------------ requests
@app.post("/api/request")
def submit_request():
    payload = request.get_json(silent=True) or {}
    try:
        fellow_id = int(payload["fellow_id"])
        raw_slots = payload.get("available_slot_ids", [])
        if isinstance(raw_slots, str):
            slot_ids = [int(s) for s in raw_slots.replace(",", " ").split() if s]
        else:
            slot_ids = [int(s) for s in raw_slots]
        topic = str(payload["topic"]).strip()
        urgency = str(payload["urgency"]).strip().lower()
        if not topic:
            return _json_error("Topic must not be empty")
        week_raw = payload.get("week")
        week = int(week_raw) if week_raw not in (None, "") else None
        store.submit_request(fellow_id, slot_ids, topic, urgency, week)
    except (KeyError, TypeError) as exc:
        return _json_error(f"Bad request payload: {exc}")
    except ValueError as exc:
        return _json_error(str(exc))
    return jsonify({"ok": True, "state": store.snapshot()})


@app.delete("/api/request/<int:request_id>")
def delete_request(request_id: int):
    try:
        store.remove_request(request_id)
    except ValueError as exc:
        return _json_error(str(exc))
    return jsonify({"ok": True, "state": store.snapshot()})


# ---------------------------------------------------------------------- solve
@app.post("/api/solve")
def solve():
    payload = request.get_json(silent=True) or {}
    chosen = (payload.get("solver") or store.active_solver).lower()
    try:
        store.set_solver(chosen)
    except ValueError as exc:
        return _json_error(str(exc))

    payload_text, requests_snapshot, _ = store.build_solver_input()
    if not requests_snapshot:
        return _json_error("No pending requests to schedule for the current week.")

    try:
        result = solver.run(chosen, payload_text, num_requests=len(requests_snapshot))
    except solver.SolverError as exc:
        return _json_error(str(exc), status=500)

    run_record = store.commit_assignments(
        result.assignments, solver=chosen, elapsed_ms=result.elapsed_ms
    )

    return jsonify(
        {
            "ok": True,
            "run": run_record.to_dict(),
            "fallback_used": result.timed_out_fallback,
            "stdout": result.raw_output,
            "stdin": payload_text,
            "state": store.snapshot(),
        }
    )


# ----------------------------------------------------------------- csv import
@app.post("/api/import_csv")
def import_csv():
    csv_text = ""
    if "file" in request.files:
        csv_text = request.files["file"].read().decode("utf-8", errors="replace")
    else:
        payload = request.get_json(silent=True) or {}
        csv_text = payload.get("csv", "")

    if not csv_text.strip():
        return _json_error("CSV body is empty")

    try:
        summary = store.load_from_csv(csv_text)
    except (ValueError, KeyError, IndexError) as exc:
        return _json_error(f"Could not parse CSV: {exc}")

    return jsonify({"ok": True, "summary": summary, "state": store.snapshot()})


# -------------------------------------------------------------- diagnostics
@app.get("/api/healthz")
def healthz():
    info: Dict[str, Any] = {"ok": True, "solvers": {}}
    for name in solver.SUPPORTED_SOLVERS:
        path = solver.binary_path(name)
        info["solvers"][name] = {
            "binary": str(path),
            "exists": path.exists(),
        }
    return jsonify(info)


if __name__ == "__main__":
    # Pre-build the binaries on first launch so the user does not have to do it.
    try:
        solver.ensure_built("baseline")
        solver.ensure_built("improved")
    except solver.SolverError as exc:
        print(f"[warn] could not pre-build C++ backends: {exc}")
    app.run(host="127.0.0.1", port=5000, debug=False)
