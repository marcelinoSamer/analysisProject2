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

from store import HotStore, URGENCY_LEVELS, SUPPORTED_SOLVERS
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


@app.post("/api/reset")
def reset_to_snapshot():
    """Restore the last-imported CSV. Solver runs and edits made since the
    import are discarded so the user can re-test from a clean mid-lifecycle
    baseline without re-uploading the CSV every time."""
    try:
        summary = store.reset_to_snapshot()
    except ValueError as exc:
        return _json_error(str(exc))
    return jsonify({"ok": True, "summary": summary, "state": store.snapshot()})


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
def _parse_request_payload(payload):
    fellow_id = int(payload["fellow_id"])
    raw_slots = payload.get("available_slot_ids", [])
    if isinstance(raw_slots, str):
        slot_ids = [int(s) for s in raw_slots.replace(",", " ").split() if s]
    else:
        slot_ids = [int(s) for s in raw_slots]
    topic = str(payload.get("topic", "")).strip()
    urgency = str(payload.get("urgency", "")).strip().lower()
    week_raw = payload.get("week")
    week = int(week_raw) if week_raw not in (None, "") else None
    return fellow_id, slot_ids, topic, urgency, week


@app.post("/api/request")
def submit_request():
    payload = request.get_json(silent=True) or {}
    try:
        fellow_id, slot_ids, topic, urgency, week = _parse_request_payload(payload)
        if not topic:
            return _json_error("Topic must not be empty")
        _, warnings = store.submit_request(fellow_id, slot_ids, topic, urgency, week)
    except (KeyError, TypeError) as exc:
        return _json_error(f"Bad request payload: {exc}")
    except ValueError as exc:
        return _json_error(str(exc))
    return jsonify({"ok": True, "warnings": warnings, "state": store.snapshot()})


@app.post("/api/validate_request")
def validate_request():
    """Dry-run the same validation+feasibility checks as submit_request without
    mutating state. The UI uses this to flag known-infeasible requests live as
    the user types."""
    payload = request.get_json(silent=True) or {}
    try:
        fellow_id, slot_ids, topic, urgency, week = _parse_request_payload(payload)
    except (KeyError, TypeError, ValueError) as exc:
        return jsonify({
            "ok": False,
            "errors": [f"Could not parse fields: {exc}"],
            "warnings": [],
            "known_topics": sorted({
                t for m in store.snapshot()["mentors"] for t in m["specialties"]
            }),
        })
    info = store.validate_request(fellow_id, slot_ids, topic, urgency, week)
    return jsonify({"ok": info["ok"], **info})


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
TESTS_DIR = CLIENT_DIR / "tests"


def _safe_test_path(name: str) -> Path:
    candidate = (TESTS_DIR / name).resolve()
    if TESTS_DIR.resolve() not in candidate.parents and candidate != TESTS_DIR.resolve():
        raise ValueError("Refusing to read outside the tests directory")
    return candidate


@app.post("/api/import_csv")
def import_csv():
    csv_text = ""
    label = None
    if "file" in request.files:
        uploaded = request.files["file"]
        csv_text = uploaded.read().decode("utf-8", errors="replace")
        label = uploaded.filename or None
    else:
        payload = request.get_json(silent=True) or {}
        csv_text = payload.get("csv", "")
        label = payload.get("label") or None

    if not csv_text.strip():
        return _json_error("CSV body is empty")

    try:
        summary = store.load_from_csv(csv_text, label=label)
    except (ValueError, KeyError, IndexError) as exc:
        return _json_error(f"Could not parse CSV: {exc}")

    return jsonify({"ok": True, "summary": summary, "state": store.snapshot()})


@app.get("/api/test_cases")
def list_test_cases():
    """Return the catalog of built-in edge-case scenarios."""
    if not TESTS_DIR.exists():
        return jsonify({"ok": True, "cases": []})
    cases = []
    for entry in sorted(TESTS_DIR.iterdir()):
        if entry.suffix.lower() != ".csv":
            continue
        title = entry.stem.replace("_", " ").title()
        description = ""
        with entry.open() as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                if line.startswith("#"):
                    description = line.lstrip("# ").strip()
                break
        cases.append({"name": entry.name, "title": title, "description": description})
    return jsonify({"ok": True, "cases": cases})


@app.post("/api/load_test_case")
def load_test_case():
    payload = request.get_json(silent=True) or {}
    name = (payload.get("name") or "").strip()
    if not name or not name.endswith(".csv"):
        return _json_error("Provide a CSV name like 'all_same_slot.csv'")
    try:
        path = _safe_test_path(name)
    except ValueError as exc:
        return _json_error(str(exc), status=400)
    if not path.exists():
        return _json_error(f"Test case '{name}' not found")
    csv_text = path.read_text()
    try:
        summary = store.load_from_csv(csv_text, label=name)
    except (ValueError, KeyError, IndexError) as exc:
        return _json_error(f"Could not parse CSV '{name}': {exc}")
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
    import os

    # Pre-build the binaries on first launch so the user does not have to do it.
    for name in SUPPORTED_SOLVERS:
        try:
            solver.ensure_built(name)
        except solver.SolverError as exc:
            print(f"[warn] could not pre-build '{name}' backend: {exc}")
    port = int(os.environ.get("PORT", "5000"))
    print(f"Open http://127.0.0.1:{port}/")
    app.run(host="127.0.0.1", port=port, debug=False)
