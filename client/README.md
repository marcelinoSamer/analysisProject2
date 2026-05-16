# Mentorship Scheduler — Python Client

A simple, single-page web frontend for the weekly mentor-slot scheduling
problem defined in `Analysis_Project_II___Problem_Statement.pdf`. The
application talks to the existing C++ backends in `Human_Solution/` —
this client does **not** re-implement the scheduling logic in Python.

* **Frontend:** Flask + a small vanilla-JS UI (no build step, no framework).
* **Backend:** the original `baseline.cpp` and `improved.cpp` from
  `Human_Solution/`, compiled to native binaries and invoked over
  stdin/stdout from Python.
* **Storage:** in-memory ("hot storage") only — no database, no disk
  persistence. Everything lives inside a `HotStore` instance for the
  lifetime of the process.

## Quick start

From the repo root:

```bash
python3 -m pip install -r client/requirements.txt
client/build_cpp.sh               # compiles baseline + improved (macOS / Linux)
python3 client/app.py
```

Then open <http://127.0.0.1:5000/>.

The first run of `app.py` re-builds the C++ binaries automatically if they
are missing, so the explicit `build_cpp.sh` step is optional.

## What you can do

### Submit Request tab
Register fellows, queue weekly requests (`fellow_id`, topic, urgency,
list of acceptable slot IDs, week). Hit **Run solver** to dispatch the
queue to the currently selected C++ backend.

### Mentors & Slots tab
Add or remove mentors (with their specialties) and weekly slots. Slot
IDs are zero-based row indices, matching the C++ input format.

### Weekly Schedule tab
After each solve, the grid shows the full week-by-week schedule per
mentor: which fellow ended up in which slot, on which topic, with what
urgency. Empty cells = the slot was offered but no request was assigned
to it. The starvation table tracks `s_f` (consecutive weeks unserved)
per fellow as described in the problem spec.

### Performance tab
Every solver run is logged with elapsed wall-clock time, assignment count
and total weighted-delay penalty. Flip the solver in the header and
re-run the same week to compare baseline vs. improved on a like-for-like
input.

### CSV / Flush tab
* **Flush** wipes everything (mentors, slots, fellows, pending requests,
  schedule, starvation, run history) and resets the week counter to 0.
* **Import CSV** replaces the hot state with a mid-lifecycle snapshot.
  See `sample.csv` for the layout.

## Solver switch

The pill in the header (`Baseline` / `Improved`) is wired to a real
runtime switch — every solve invokes whichever binary is currently
selected, and a fresh selection takes effect on the very next solve.
This makes it easy to demo the performance gap mid-session: load a
large CSV, run once with `improved`, flip to `baseline`, and run again.

## Architecture

```
client/
├── app.py             # Flask routes
├── store.py           # HotStore: in-memory state + CSV loader
├── solver.py          # subprocess wrapper for the C++ binaries
├── build_cpp.sh       # compiles baseline + improved into ./build
├── cpp/bits/stdc++.h  # tiny header shim so clang++ can build the GCC sources
├── templates/index.html
├── static/{style.css, app.js}
├── sample.csv         # mid-lifecycle sample snapshot
└── build/             # compiled binaries (created by build_cpp.sh)
```

The Python code never touches the original `Human_Solution/baseline.cpp`
or `Human_Solution/improved.cpp` files — they are compiled as-is using a
local header shim, then driven by `subprocess.run` with the same
stdin format documented in `Human_Solution/requirements.md`.

## CSV format (mid-lifecycle import)

Every row is tagged by its first column. Order doesn't matter except
that slot IDs are assigned in the order the `slot,...` rows appear.

```csv
meta,current_week,4
meta,solver,improved
mentor,1,algorithms;ml
slot,1,4
slot,1,5
fellow,10,2                # fellow 10, s_f = 2
request,10,algorithms,blocker,4,0;1
```

* `meta` rows configure the snapshot (`current_week`, `solver`).
* `mentor` rows are upserts; specialties are `;`- or space-separated.
* `slot` rows append slots. The slot ID equals the row index across all
  `slot` rows in this CSV (0-based).
* `fellow` rows register a fellow and optionally seed their starvation.
* `request` rows queue requests for the current week. Available slot
  IDs are `;`- or space-separated.
* Lines starting with `#` and empty lines are ignored.

## Notes

* The backend binaries report `Backtracking timed out. Using greedy
  heuristic solution.` if `improved` falls back. The UI surfaces that
  message inside the solve result panel.
* Slot deletion rewrites slot IDs inside pending requests so the input
  to the C++ backend always uses dense 0..S-1 IDs.
* The store enforces "one pending request per fellow per week" as the
  spec requires.
