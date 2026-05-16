# Mentorship Scheduler — Python Client

A simple, single-page web frontend for the weekly mentor-slot scheduling
problem defined in `Analysis_Project_II___Problem_Statement.pdf`. The
application talks to the existing C++ backends in `Human_Solution/` —
this client does **not** re-implement the scheduling logic in Python.

* **Frontend:** Flask + a small vanilla-JS UI (no build step, no framework).
* **Backend:** three scheduling algorithms, all driven over stdin/stdout
  from Python:
  - `baseline.cpp` — exhaustive backtracking (`Human_Solution/baseline.cpp`).
  - `improved.cpp` — preprocessed backtracking + greedy fallback
    (`Human_Solution/improved.cpp`).
  - **AI** — the `ScarcityAwareGreedySolver` from `AI_Solution/main.cpp`,
    exposed via a tiny adapter (`client/cpp/ai_adapter.cpp`) that pulls
    the original file in unchanged and translates the same stdin format
    the other two backends use.
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
* **Built-in test cases** — one-click load any CSV under `client/tests/`.
  Each load also becomes the saved snapshot so you can immediately Reset
  back to it after experimenting.
* **Import CSV** — upload (or paste) a CSV to replace the hot state
  with a mid-lifecycle snapshot. See `sample.csv` for the layout.
* **Reset to snapshot** — restores the last-imported CSV without
  re-uploading it. Use this instead of "import again, then redo the
  request setup" when you want to iterate on the same initial state.
* **Flush** — wipes the current week state (mentors, slots, fellows,
  pending requests, schedule, run history) and resets the week counter
  to 0, but keeps the saved snapshot so you can still reset to it.

## Solver switch

The pill in the header (`Baseline` / `Improved` / `AI`) is wired to a
real runtime switch — every solve invokes whichever binary is currently
selected, and a fresh selection takes effect on the very next solve.
This makes it easy to demo the performance and quality gap mid-session:
load `large_mixed.csv`, run once with `improved` (~5 s, falls back to
greedy), flip to `ai` (single-digit ms, often higher-quality), then to
`baseline` (will time out — that's the whole point of the comparison).

The AI backend has one important caveat: its internal
`ScarcityAwareGreedySolver` starts each process invocation with `s_f = 0`
for every fellow, so cross-week starvation tracked by the Python store
is not propagated into its decisions. The Python frontend's own `s_f`
counters are still maintained and displayed correctly — the limitation
only affects the AI backend's *internal* tie-breaking.

## Sanity checks on requests

The submit-request form runs a live validation against the current hot
state (no server round-trip per keystroke — it debounces 200 ms and
asks `/api/validate_request`). It returns three classes of feedback:

* **Errors** (red box, blocks submission): missing/duplicate fellow,
  unknown urgency, out-of-range slot ID, empty topic.
* **Warnings** (yellow box, will queue but flagged):
  * Topic is not listed as a specialty by any mentor → the request is
    guaranteed to be classified as infeasibly unserved.
  * None of the listed slots are owned by a mentor that covers the
    requested topic for the given week.
  * Some, but not all, of the listed slots are viable.
* **OK** (green box): at least one of the listed slots can satisfy the
  request.

The topic input is also wired to a `<datalist>` autocomplete populated
from the union of mentor specialties, so the user can pick a known
topic in one click instead of typing it (and possibly mistyping it).

Pending requests already in the queue show a small **status chip**
(`ready` / `infeasible`) in the Submit Request tab so the user can spot
known-bad rows before running the solver.

## Architecture

```
client/
├── app.py                # Flask routes
├── store.py              # HotStore: in-memory state + CSV loader + validation
├── solver.py             # subprocess wrapper for the C++ binaries
├── build_cpp.sh          # compiles baseline + improved + AI adapter into ./build
├── cpp/
│   ├── bits/stdc++.h     # tiny header shim so clang++ can build the GCC sources
│   └── ai_adapter.cpp    # pulls AI_Solution/main.cpp in unchanged + adds stdin I/O
├── templates/index.html
├── static/{style.css, app.js}
├── sample.csv            # mid-lifecycle sample snapshot
├── tests/                # built-in edge-case CSVs (see below)
└── build/                # compiled binaries (created by build_cpp.sh)
```

The Python code never touches the original `Human_Solution/baseline.cpp`,
`Human_Solution/improved.cpp` or `AI_Solution/main.cpp` files — they are
compiled as-is, either with a local header shim (Human backends) or
re-included into a thin adapter that adds a stdin I/O front (AI
backend), then driven by `subprocess.run` with the same stdin format
documented in `Human_Solution/requirements.md`.

## Built-in test cases

The `client/tests/` directory ships ready-to-load edge-case scenarios.
Each one shows up in the **Built-in test cases** card on the CSV/Flush
tab and is loadable with one click.

| File                        | What it exercises                                                                 |
|-----------------------------|------------------------------------------------------------------------------------|
| `all_same_slot.csv`         | 5 fellows, 1 slot — verifies tie-breaking and that ≤1 ends up assigned.            |
| `impossible_topic.csv`      | Mix of feasible + impossible requests; checks per-request infeasibility reporting. |
| `no_overlap.csv`            | One unique slot per request — every solver must find the trivial perfect matching. |
| `topic_bottleneck.csv`      | 6 requests for a single low-coverage topic — verifies urgency-based selection.     |
| `starvation_rescue.csv`     | Spec's Case 3 — starved exploratory fellow vs. fresh blocker.                       |
| `large_mixed.csv`           | 10 mentors / 30 slots / 25 requests — shows the **performance gap** between solvers (baseline times out, improved falls back to greedy, AI returns in single-digit ms). |

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
* The Python wrapper also enforces an outer wall-clock timeout on each
  solver invocation (default 10 s, override with
  `SCHED_SOLVER_TIMEOUT_S`). This keeps the UI responsive when running
  `baseline` against pathologically-large inputs.
* Slot deletion rewrites slot IDs inside pending requests so the input
  to the C++ backend always uses dense 0..S-1 IDs.
* The store enforces "one pending request per fellow per week" as the
  spec requires.
