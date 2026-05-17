# Test Case Traceability Matrix

This matrix maps the core capabilities and algorithmic rules of the dynamic programming matcher to the structural behaviors validated in both the core and extended test suites.

## 1. Traceability Matrix Table

| Test ID | Test Name | Target Functionality / Core Mechanism | Key Assertions & Expected Behavior |
| :--- | :--- | :--- | :--- |
| **TEST 1** | All requests can be served | Perfect matching, basic compatibility evaluation (`compatible()`). | Served = 2; `pending` queue drains fully. |
| **TEST 2** | Greedy trap | DP global optimization vs. myopic greedy matching choices. | Served = 2 instead of picking a sub-optimal choice that blocks subsequent items. |
| **TEST 3** | Older request priority | Correct integration of the age delta parameter ($\\delta \\times \\text{age}$) inside `benefit()`. | Old medium-urgency request beats new high-urgency request due to accumulated backlog age. |
| **TEST 4** | No feasible slot (topic) | Missing string intersect detection (`topicMatch() == 0`). | Served = 0; outputs structural unserved reason `"no feasible slot"`. |
| **TEST 5** | Preferred mentor bonus | Preference component value computation logic ($\\gamma \\times p$). | Matches with preferred mentor's slot to maximize total benefit over a generic matching slot. |
| **TEST 6** | Multiple fellow sessions | Handling overlapping independent items under the same structural user context. | Served = 2; same `fellowId` maps successfully to separate slots without artificial restrictions. |
| **TEST 7** | Canceled request | Upstream request status parsing and edge case pruning. | Served = 0; logs target `reason` as `"request canceled"`. |
| **TEST 8** | Canceled slot | Upstream slot state handling. | Served = 0; sets unserved reason to `"no feasible slot"`. |
| **TEST 9** | Higher topicMatch wins | Value optimization maximization via explicit intersection sizing. | Selects slot with higher structural token intersections to score higher `totalBenefit`. |
| **TEST 10**| Combined unserved reasons | Simultaneous generation of complex failure vectors. | Categorizes unserved entries explicitly between `"lower priority"` and `"no feasible slot"`. |
| **TEST 11**| Backlog persists and drains | Multiturn memory mutations across iterative function calls. | Carries state to Week 2, captures backlog time-decay points ($+2$ benefit points per week). |
| **TEST 12**| Persistent infeasibility | Enduring failure state persistence tracking. | Tracks identity in `pending` across 3 continuous iterations with identical failure labels. |
| **TEST 13**| Topic priority over preference| Guardrail hierarchy constraints (`compatible()` relies strictly on topics, not preferences). | Prevents preference score from forcing invalid assignments; falls back to an alternate valid slot. |
| **TEST 14**| Empty required topics | Empty array handling constraints. | Evaluates empty parameter inputs as structurally incompatible (`"no feasible slot"`). |
| **TEST 15**| Topicless mentor | Dictionary profile edge conditions. | Evaluates slot provided by a profile with a 0-count topic map as completely unmatchable. |
| **TEST 16**| Dense bipartite graph | DP bitmask correctness under maximum matching conditions. | Traverses complex graph bottlenecks to pick optimal pairings; serves 4 out of 5 requests. |
| **TEST 17**| Program-level accounting | Aggregated session summary logic across an extended simulation loop. | Retains total simulation variables perfectly across multiple phases (Final Served = 4, Benefit = 57). |

***

## 2. Parameter Configurations & Weight Equations

The dynamic programming assignment logic uses a weighted function to choose the optimal distribution of pairings when matching requests ($R$) to slots ($S$). The system state validates the equation below:

$$\\text{Benefit} = (\\alpha \\times u) + (\\beta \\times tm) + (\\gamma \\times p) + (\\delta \\times \\text{age})$$

### Explicit Weights Configuration
* **$\\alpha$ (ALPHA) = 5**: Urgency factor scaling weight.
  * `high` urgency $\\rightarrow u = 3$ (Value contribution: 15)
  * `medium` urgency $\\rightarrow u = 2$ (Value contribution: 10)
  * `low` urgency $\\rightarrow u = 1$ (Value contribution: 5)
* **$\\beta$ (BETA) = 2**: Multiplier applied directly to the intersecting count of compatible tags between request and mentor (`topicMatch`).
* **$\\gamma$ (GAMMA) = 1**: Boolean satisfaction weight for a preferred mentor match (`1` if match matches preferred, else `0`).
* **$\\delta$ (DELTA) = 2**: Accumulating chronological age modifier weight for every simulation round a request remains in the backlog.
