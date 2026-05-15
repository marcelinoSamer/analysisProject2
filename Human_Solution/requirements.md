# TA/Mentor Slot Scheduling Requirements

## 1. Real-World Problem

The system schedules academic help requests from students/fellows into a limited set of fixed weekly TA or mentor office-hour slots. Each request asks for help on a specific topic, has a submitted week, an urgency level, and a list of slots the student can attend. Each slot belongs to one mentor and happens in a specific week.

The concrete problem is: given many student help requests over a multi-week planning line and a fixed set of mentor slots, assign as many requests as possible to feasible slots while reducing unfair waiting time, especially for urgent requests.

This is not a flexible calendar-building problem. The system does not create new slots or move existing ones. It chooses among already-existing weekly slots.

## 2. Inputs and Outputs

### Inputs

The system receives four main groups of data:

- Fellows/students: a list of fellow IDs.
- Mentors/TAs: a list of mentor IDs and each mentor's topic specialties.
- Slots: a list of fixed office-hour slots, where each slot has a mentor ID and a week number.
- Requests: a list of help requests. Each request includes the fellow ID, available slot IDs, requested topic, urgency level, and request week.

The urgency levels are:

- `exploratory`: low priority.
- `normal`: medium priority.
- `blocker`: high priority.

The first line contains $4$ integers:

```text
F M S R
```

where `F` is the number of fellows, `M` is the number of mentors, `S` is the number of slots, and `R` is the number of requests. The remaining input lists fellows, mentors and specialties, slots, and requests in that order.

### Output

The system produces an assignment for each request:

- If assigned: the request ID, assigned slot ID, mentor ID, and slot week.
- If not assigned: the request is marked `Not Assigned`.

Structurally, the output is a vector `assignments` of length `R`, where `assignments[i]` is either the assigned slot ID for request `i` or `-1` if request `i` cannot be scheduled.

A useful extension would be a ranked waitlist for unassigned requests, ordered by urgency and accumulated waiting penalty. The current implementation does not print a separate ranked waitlist, but its penalty model gives the information needed to create one.

## 3. Assumptions and Constraints

The implementation makes the following assumptions:

- Slots are fixed. The scheduler cannot create, delete, extend, or move slots.
- Planning is weekly. Each request and each slot has a week number.
- A student request can be assigned to at most one slot.
- A slot can serve at most one request.
- A request can only use slots listed in its availability list.
- A request can only be assigned to a slot in the same week or a later week.
- A mentor can only serve a request if the mentor has the requested topic as a specialty.
- Topic names are normalized to lowercase before comparison.
- Partial schedules are acceptable. If demand exceeds supply or no feasible slot exists, some requests may remain unassigned.
- The primary fairness goal is to maximize the number of assigned requests.
- The secondary fairness goal is to minimize weighted waiting delay, with higher urgency requests receiving larger penalties for delay.

When demand exceeds supply, the system should not fail. It should assign the largest feasible set of requests and leave the remaining requests unassigned. Unassigned requests receive an artificial delay penalty beyond the last available week, making them worse than scheduled requests when comparing otherwise similar solutions.

Fairness in this project is therefore defined as:

1. Serve as many requests as possible.
2. Among schedules serving the same number of requests, prefer the schedule with lower total urgency-weighted waiting cost.
3. Within the same urgency level, earlier requests should generally be favored over later ones.

## 4. Formal Problem Definition

This can be modeled as a constrained matching and scheduling optimization problem.

Let:

- `R` be the set of requests.
- `S` be the set of slots.
- `A_i` be the set of available slots for request `i`.
- `topic(i)` be the requested topic for request `i`.
- `week(i)` be the request week for request `i`.
- `mentor(j)` be the mentor assigned to slot `j`.
- `week(j)` be the week of slot `j`.
- `spec(m)` be the set of specialties for mentor `m`.
- `u_i` be the urgency weight for request `i`.

The code uses urgency weights based on the enum value plus one:

- `exploratory`: 1
- `normal`: 2
- `blocker`: 3

Define decision variable:

```text
x_ij = 1 if request i is assigned to slot j
x_ij = 0 otherwise
```

Feasibility constraints:

```text
sum over j in S of x_ij <= 1                 for every request i
sum over i in R of x_ij <= 1                 for every slot j
x_ij = 0 if j is not in A_i
x_ij = 0 if week(j) < week(i)
x_ij = 0 if topic(i) is not in spec(mentor(j))
```

Delay for an assigned request is:

```text
d_i = week(assigned_slot_i) - week(i)
```

The implemented delay penalty is:

```text
penalty(i) = u_i * d_i(d_i + 1)(2d_i + 1) / 6
```

This is the urgency weight multiplied by the sum of squares from `1` to `d_i`, so longer waits become increasingly expensive.

The optimization goal is lexicographic:

```text
1. Maximize the number of assigned requests.
2. Subject to that, minimize total weighted delay penalty.
```

In a single expression, this can be understood as maximizing assignment count first, then minimizing:

```text
sum over i in R of penalty(i)
```

with a large artificial delay assigned to unassigned requests.

## 5. Baseline Solution

A simple baseline solution is first-come-first-served greedy assignment:

1. Read requests in input order.
2. For each request, scan its available slots.
3. Assign the request to the first unused slot that is feasible by week and mentor topic.
4. If no feasible unused slot exists, mark the request as unassigned.

This baseline is easy to understand and fast. It works well in easy cases where there are enough compatible slots, availability lists do not overlap much, and request order already matches priority.

However, it can fail when requests compete for the same slots. A low-urgency or flexible request may take a slot that a later high-urgency request needs. It also may not minimize waiting time, because choosing the first available feasible slot can block a better global assignment.

The colleague's `baseline.cpp` is stronger than a pure first-come-first-served greedy algorithm: it exhaustively enumerates possible assignments, checks all feasibility constraints, maximizes the assignment count, and breaks ties by total delay penalty. This makes it a correctness baseline for small inputs, but it can be slow because the search grows exponentially with the number of requests and available slots.

The improved implementation keeps the exact backtracking approach when it finishes within the configured time limit, but falls back to a greedy heuristic when exhaustive search takes too long. The heuristic processes slots by week and selects a request using urgency-weighted delay, which is faster for larger cases but can still miss the globally optimal schedule.

## Summary

The project is best described as a weekly mentor-slot matching problem with fixed slot capacity, topic compatibility, request availability, urgency, and waiting-time fairness. The required output is an assignment from requests to slots, allowing partial schedules when capacity or compatibility prevents every request from being served.
