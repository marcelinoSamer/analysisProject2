# Analysis of Scarcity-Aware Greedy Test Results

## Executive Summary

The self-checking test suite completed successfully:

```text
Tests passed: 57
Tests failed: 0
All self-checking tests passed.
```

This means the implementation behaved exactly as expected for all encoded scenarios. The algorithm correctly handles core constraints such as topic compatibility, fellow availability, slot capacity, one-slot-per-fellow assignment, starvation updates, weekly penalty calculation, infeasible request detection, and tie-breaking.

However, the most important result is not simply that all tests passed. The most important result is that the tests deliberately expose the algorithm's weakness: **scarcity-aware greedy is a heuristic, not an optimal algorithm**.

The test suite proves two things at the same time:

1. The algorithm works correctly according to its intended greedy design.
2. The algorithm can still make globally suboptimal decisions, even when every local decision looks reasonable.

This distinction is crucial. Passing all tests does **not** mean the greedy algorithm is always optimal. It means the implementation is consistent, predictable, and honest about its limitations.

---

## What the Algorithm Is Trying to Do

The algorithm solves a weekly mentorship assignment problem. Fellows submit requests, mentors provide slots, and each slot can serve only one fellow. A fellow can only be assigned to a slot if:

- the slot is listed in the fellow's availability,
- the mentor owning that slot supports the fellow's requested topic,
- the slot is still free,
- the fellow has not already been assigned.

For every unassigned fellow, the weekly penalty is:

```math
w(\text{urgency}_f)(s_f + 1)^2
```

where:

- `blocker` has weight 3,
- `normal` has weight 2,
- `exploratory` has weight 1,
- `s_f` is the number of consecutive previous weeks the fellow has gone without assignment.

The greedy algorithm tries to minimize skipped penalty by assigning fellows with high priority. But instead of using raw penalty alone, it uses a scarcity-aware score:

```math
\text{score}_f = \frac{P_f}{k_f}
```

where:

- `P_f` is the skipped penalty of fellow `f`,
- `k_f` is the number of currently free compatible slots available to fellow `f`.

This makes the algorithm prioritize fellows who are both costly to skip and difficult to place.

---

## Overall Test Outcome

The test suite produced:

```text
Tests passed: 57
Tests failed: 0
```

This confirms that the implemented behavior matches the expected behavior in all tested cases.

The passing result validates the following:

- feasible fellows are assigned when enough compatible slots exist,
- impossible requests are classified correctly,
- bottlenecks are handled without violating constraints,
- starvation can outweigh urgency,
- the least-damaging slot selection logic works,
- timestamp tie-breaking works,
- no-request weeks are handled safely,
- greedy results match the optimal solution on several small natural cases,
- known greedy failure cases are detected correctly.

The final point is the most important one. The test suite is not only checking successful behavior. It is also checking that the known weakness of the algorithm appears exactly where expected.

---

## Basic Feasibility Results

The first test group checks the easiest case: all fellows have valid compatible slots, and there are enough slots for everyone.

The output includes:

```text
[PASS] all feasible: F1 gets S1
[PASS] all feasible: F2 gets S2
[PASS] all feasible: no unserved fellows
[PASS] all feasible: weekly penalty is 0
```

This confirms that the algorithm does not create unnecessary conflicts when the instance is simple. If all fellows can be served, they are served.

The starvation updates also pass:

```text
[PASS] all feasible: F1 starvation resets to 0
[PASS] all feasible: F2 starvation resets to 0
```

This confirms that assigned fellows correctly reset their starvation count.

This is the baseline behavior. If this failed, the algorithm would be structurally broken. Since it passes, we know the basic assignment logic is functioning.

---

## Infeasible Request Handling

The infeasible request tests check cases where assignment is impossible regardless of algorithm quality.

The output includes:

```text
[PASS] infeasible: F1 topic mismatch classified correctly
[PASS] infeasible: F2 missing slot classified correctly
[PASS] infeasible: F3 uncovered topic classified correctly
[PASS] infeasible: penalty is 2 + 3 + 1 = 6
[PASS] infeasible: no algorithm failure is reported
```

This means the algorithm correctly identifies requests where:

- the fellow is available for a slot, but the mentor does not support the requested topic,
- the fellow lists a slot that does not exist,
- the requested topic is not covered by any compatible available slot.

These cases are classified as `infeasibly_unserved`.

This distinction matters because the algorithm should not be blamed for failing to assign someone who had no valid assignment possibility. A strong solution must separate true algorithmic mistakes from impossible cases.

The test also confirms that the algorithm still charges the weekly penalty for these unserved fellows. That is consistent with the penalty objective: the fellow remains unassigned, so the weekly penalty is incurred, even though the algorithm was not at fault.

---

## Bottleneck Case Analysis

The bottleneck tests simulate a situation where more fellows request a topic than there are compatible slots available.

The output includes:

```text
[PASS] bottleneck: highest penalty fellow F1 is assigned
[PASS] bottleneck: second-highest penalty fellow F2 is assigned
[PASS] bottleneck: lowest penalty fellow F3 is unassigned
[PASS] bottleneck: F3 is competitively unserved
[PASS] bottleneck: penalty is only F3's skipped penalty = 1
[PASS] bottleneck: no algorithm failure is reported
```

This is an important scenario because it reflects the realistic case where mentor capacity is limited.

The algorithm correctly assigns the highest-penalty fellows first. Since there are only two slots and three fellows, one fellow must remain unassigned. The unassigned fellow is classified as `competitively_unserved`, not as an algorithm failure.

This is correct because all compatible slots were already consumed by other valid assignments.

The result shows that the algorithm handles competition correctly. It does not label every unassigned feasible fellow as a failure. It only labels a failure when a compatible slot is left unused.

---

## Starvation Rescue Analysis

The starvation rescue case is one of the strongest positive results for the algorithm.

The output includes:

```text
[PASS] starvation rescue setup: A has starvation 3
[PASS] starvation rescue setup: B has starvation 0
[PASS] starvation rescue: starved exploratory A beats blocker B
[PASS] starvation rescue: B is competitively unserved
[PASS] starvation rescue: penalty is B's skipped blocker penalty = 3
[PASS] starvation rescue: A starvation resets
[PASS] starvation rescue: B starvation increments
```

This test proves that the algorithm is not simply urgency-first.

Fellow A is exploratory but has been skipped for three consecutive weeks. Fellow B is a blocker but has not been skipped before.

The penalties are:

```math
A = 1 \cdot (3+1)^2 = 16
```

```math
B = 3 \cdot (0+1)^2 = 3
```

Even though B has higher urgency, skipping A is much more expensive. Therefore, assigning A is the better decision.

This is a very important validation because the problem explicitly wants the algorithm to prevent repeated skipping. If urgency always dominated, starvation would not matter enough. This test confirms that the quadratic starvation term is actually influencing decisions.

---

## Least-Damaging Slot Choice Analysis

The least-damaging slot test checks the second half of the greedy strategy.

Selecting the right fellow is not enough. The algorithm must also choose the assigned slot carefully.

The output includes:

```text
[PASS] least damaging: A avoids B's only slot and takes S2
[PASS] least damaging: B still gets its only slot S1
[PASS] least damaging: both fellows assigned, penalty 0
[PASS] least damaging: no algorithm failure is reported
```

This test confirms that the algorithm does not blindly assign the first compatible slot.

Fellow A can use either `S1` or `S2`. Fellow B can only use `S1`.

A careless greedy algorithm might assign A to `S1`, accidentally blocking B. The implemented algorithm avoids this by estimating the harm caused by taking each slot. It assigns A to `S2`, preserving `S1` for B.

This is a strong result because it shows that the algorithm is not merely scarcity-aware at the fellow-selection level. It is also scarcity-aware at the slot-selection level.

---

## Tie-Breaking Analysis

The tie-breaking test confirms that when fellows are otherwise equal, the earlier request timestamp wins.

The output includes:

```text
[PASS] tie breaker: earlier timestamp wins
[PASS] tie breaker: later timestamp is competitively unserved
[PASS] tie breaker: skipped normal fellow penalty is 2
```

This validates deterministic behavior.

Without tie-breaking, the output might depend on unordered container traversal or input order. The timestamp rule makes the assignment predictable and aligned with the problem's stated tie-breaking policy.

This is especially important in systems involving fairness. If two fellows are equally deserving according to the penalty model, the earlier request should receive priority.

---

## No-Request Week Analysis

The no-request test checks an edge case where no fellow submits a request.

The output includes:

```text
[PASS] no requests: no assignments
[PASS] no requests: no unserved request classifications
[PASS] no requests: weekly penalty is 0
[PASS] no requests: F1 starvation increments because not assigned
[PASS] no requests: F2 starvation increments because not assigned
```

The algorithm safely produces no assignments and no unserved request classifications. The weekly penalty is zero because penalty is charged only for submitted requests.

The starvation behavior deserves special attention. The implementation increments starvation for all fellows who were not assigned, even if they did not submit a request that week.

This matches the current implementation and the literal state-update rule used in the solver. However, in a real deployment, this design decision may need clarification. If starvation is intended to measure only fellows who requested help and were skipped, then no-request weeks should not increase starvation. If starvation is intended to measure consecutive weeks without mentorship regardless of request status, then the current behavior is correct.

So this test passes, but it also highlights a possible product-definition question.

---

# Optimality Testing

The most meaningful part of the test suite is the addition of an exact brute-force oracle.

The oracle is used only for small instances. It enumerates all valid assignments and finds the minimum possible weekly penalty. This gives us a ground-truth optimum for comparison.

This is important because a greedy algorithm can look correct on many examples while still being globally suboptimal.

The optimality tests are divided into two categories:

1. Cases where greedy matches the exact optimum.
2. Cases where greedy fails compared to the exact optimum.

The second category is the most important.

---

## Cases Where Greedy Matches the Optimum

The output includes:

```text
[PASS] optimality easy: greedy penalty matches exact optimum
[PASS] optimality easy: optimal penalty is 0
[PASS] optimality easy: greedy is within 10%

[PASS] optimality bottleneck: greedy matches exact optimum
[PASS] optimality bottleneck: only exploratory fellow is skipped
[PASS] optimality bottleneck: greedy is within 10%

[PASS] optimality starvation rescue: greedy matches exact optimum
[PASS] optimality starvation rescue: A is assigned despite lower urgency
[PASS] optimality starvation rescue: skipping B costs 3
[PASS] optimality starvation rescue: greedy is within 10%
```

These results show that the greedy strategy performs optimally on several natural scenarios.

In simple cases, there is no meaningful conflict, so greedy naturally reaches the optimum.

In bottleneck cases with clearly ranked penalties, greedy also reaches the optimum because assigning the highest-penalty fellows is globally correct.

In the starvation rescue case, greedy again reaches the optimum because the starvation penalty strongly identifies the better assignment.

These tests are useful because they show that the heuristic is not random or unreliable. It often agrees with the exact optimum in structured cases.

But these successes should not be overinterpreted. They do not prove global optimality.

---

# Failure Case Analysis: The Most Important Finding

The most important part of the entire test suite is the known failure case.

The output includes:

```text
[PASS] known failure: greedy penalty is 3
[PASS] known failure: exact optimal penalty is 2
[PASS] known failure: greedy is worse than optimal
[PASS] known failure: greedy is NOT within 10% of optimum
[PASS] known failure: greedy assigns the scarce low-penalty fellow
```

This is not a weakness of the test suite. This is a strength of the test suite.

The test deliberately constructs a case where scarcity-aware greedy makes the wrong global decision.

---

## Structure of the Known Failure Case

There are three fellows:

| Fellow | Urgency | Compatible Slots | Skip Penalty |
|---|---|---:|---:|
| Scarce | normal | S1 only | 2 |
| BlockerA | blocker | S1 or S2 | 3 |
| BlockerB | blocker | S1 or S2 | 3 |

There are only two slots:

```text
S1, S2
```

The greedy scarcity-aware scores are:

```math
\text{Scarce} = \frac{2}{1} = 2
```

```math
\text{BlockerA} = \frac{3}{2} = 1.5
```

```math
\text{BlockerB} = \frac{3}{2} = 1.5
```

Because Scarce has only one possible slot, the greedy algorithm treats Scarce as the most urgent placement.

So greedy assigns:

```text
Scarce -> S1
```

Then only one slot remains:

```text
S2
```

Only one of the two blockers can be assigned. The other blocker is skipped.

Therefore, the greedy penalty is:

```math
3
```

because one blocker is skipped.

---

## Why the Greedy Decision Looks Reasonable Locally

From a local perspective, the greedy decision makes sense.

Scarce has only one option. If Scarce does not get `S1`, Scarce cannot be assigned at all.

The blockers are flexible. Each blocker can use either `S1` or `S2`.

So the greedy algorithm thinks:

> Assign the fellow with fewer options first, because flexible fellows can be handled later.

This is the central philosophy of scarcity-aware greedy.

In many cases, this philosophy works well.

But here it fails.

---

## Why the Greedy Decision Is Globally Wrong

The optimal solution is:

```text
BlockerA -> S1
BlockerB -> S2
Scarce -> unassigned
```

This skips Scarce, whose penalty is only:

```math
2
```

Both blockers are assigned, so no blocker penalty is paid.

Therefore, the optimal penalty is:

```math
2
```

Greedy penalty:

```math
3
```

Optimal penalty:

```math
2
```

So greedy is worse.

More importantly, it is not just slightly worse according to the 10% criterion:

```math
\frac{3 - 2}{2} = 0.5 = 50\%
```

The greedy solution is 50% worse than optimal in this case.

That is why the test correctly reports:

```text
[PASS] known failure: greedy is NOT within 10% of optimum
```

This is the clearest evidence that the algorithm should not be described as optimal.

---

## What This Failure Really Means

The failure happens because the greedy score:

```math
\frac{P_f}{k_f}
```

can overvalue scarcity.

The fellow named Scarce has only one option, so dividing by `k_f = 1` gives them a high score. But their actual skipped penalty is low compared with the combined value of assigning both blockers.

The greedy algorithm focuses on the immediate risk:

> If I do not assign Scarce now, Scarce may lose their only chance.

But the global optimum considers the larger tradeoff:

> Is saving Scarce worth losing one blocker?

In this case, the answer is no.

Saving Scarce avoids penalty 2, but it forces the algorithm to pay penalty 3 later. That is a bad trade.

This is the key limitation of the method.

---

## The Failure Is Not Caused by a Bug

This failure is not caused by:

- wrong topic matching,
- wrong availability checking,
- wrong slot capacity handling,
- wrong tie-breaking,
- wrong penalty formula,
- wrong starvation update,
- wrong classification.

The failure happens because the algorithmic strategy itself is greedy.

The implementation is doing exactly what the scarcity-aware greedy rule tells it to do.

That is why this result is important: it separates **implementation correctness** from **algorithmic optimality**.

The code is correct as a greedy implementation.

The greedy strategy is not always optimal.

---

## Second Non-Optimal Oracle Case

The output also includes:

```text
[PASS] oracle non-optimal: greedy is worse by exactly 1 penalty point
[PASS] oracle non-optimal: greedy penalty is 3
[PASS] oracle non-optimal: optimal penalty is 2
```

This confirms the same issue using another naming structure:

- one low-value fellow has a single option,
- two high-value fellows are flexible,
- greedy assigns the low-value scarce fellow,
- optimal skips the low-value scarce fellow and assigns the two high-value fellows.

This repeated pattern shows that the failure is not accidental. It is a real structural weakness of scarcity-aware greedy.

The weakness can be summarized as:

> Scarcity-aware greedy can protect a low-penalty constrained fellow at the cost of sacrificing a higher-penalty flexible fellow.

That is the central failure mode.

---

# Why Failure Cases Matter More Than Passing Cases

The successful tests prove that the implementation works on expected scenarios.

The failure tests prove that we understand the boundary of the algorithm.

This is more valuable than pretending the greedy algorithm is always correct.

A weak evaluation would only include easy cases where greedy passes.

A stronger evaluation includes adversarial cases where greedy is forced to reveal its limitations.

The known failure case gives a precise counterexample. It shows exactly when and why the heuristic breaks:

1. A low-penalty fellow has one slot.
2. Two or more higher-penalty fellows share flexible access to the available slots.
3. Greedy sees the low-penalty fellow as scarce.
4. Greedy assigns that fellow first.
5. This consumes a slot needed to assign the higher total penalty group.
6. The final penalty becomes higher than optimal.

This is a fundamental limitation of local decision-making.

---

# Implications for the Project

The results support the following conclusion:

> The scarcity-aware greedy algorithm is a reasonable fast heuristic, but it should not be presented as an exact optimizer.

It is suitable when:

- the input size is large,
- speed and simplicity matter,
- approximate quality is acceptable,
- the system can tolerate occasional suboptimal decisions,
- exact optimization is not required.

It is not suitable when:

- the output must be guaranteed optimal,
- the 10% near-optimal requirement must hold for all cases,
- adversarial or tightly constrained cases are common,
- fairness decisions must be globally justified,
- the system must minimize penalty exactly.

The known failure case is especially important for the project criteria because the problem statement says near-optimal penalty should be checked against exhaustive enumeration on small instances. The failure case proves that this greedy algorithm may violate the 10% near-optimal requirement.

In the known failure case:

```math
\text{Greedy penalty} = 3
```

```math
\text{Optimal penalty} = 2
```

The gap is:

```math
50\%
```

This is far beyond the allowed 10% threshold.

Therefore, if the grading or evaluation strictly enforces near-optimality on adversarial small cases, this greedy algorithm is risky.

---

# Recommended Interpretation

The correct way to describe the result is:

> The implementation successfully realizes the scarcity-aware greedy heuristic and passes all behavioral tests written for it. It handles feasibility, bottlenecks, starvation rescue, tie-breaking, and classification correctly. However, exact-oracle tests reveal that the greedy strategy is not globally optimal and can fail the 10% near-optimality condition on adversarial cases. The failure is structural, not a coding bug.

This is the most honest and technically accurate interpretation.

---

# Recommended Next Step

If the goal is to keep the greedy algorithm, then the report should explicitly state:

> This is a heuristic solution. It is fast and often effective, but it does not guarantee optimality.

If the goal is to satisfy the near-optimal or optimal requirement more reliably, then the algorithm should be upgraded to one of the following:

1. **Maximum-weight bipartite matching**
   - Best natural fit for the current problem.
   - Maximizes saved penalty.
   - Equivalent to minimizing skipped penalty.

2. **Min-cost max-flow**
   - More flexible.
   - Better if additional constraints are added later.
   - Useful for mentor capacities, repeated mentor avoidance, fairness constraints, or multi-slot capacities.

3. **Greedy + local improvement**
   - Keeps the greedy structure.
   - Adds augmenting swaps after the initial assignment.
   - May fix some failure cases but still needs careful proof or oracle testing.

Among these, maximum-weight bipartite matching is the strongest direct replacement for this problem.

---

# Final Conclusion

The test results are excellent from an engineering perspective because they validate both correctness and limitations.

The algorithm successfully passes all 57 checks, meaning the implementation is internally consistent and handles the intended scenarios correctly.

But the failure cases are the most important finding. They show that scarcity-aware greedy can make locally reasonable decisions that are globally wrong. In particular, it can overprotect a low-penalty fellow with only one option and sacrifice a higher-penalty fellow as a result.

Therefore, the final conclusion is:

> The scarcity-aware greedy algorithm is correct as a heuristic implementation, but it is not a guaranteed optimal solution. The test suite proves this honestly by showing both successful cases and deliberate counterexamples. For strict optimality or guaranteed near-optimality, the solution should be replaced or supplemented with an exact matching-based algorithm.
