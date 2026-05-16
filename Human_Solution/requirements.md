# TA/Mentor Slot Scheduling Requirements

## 1. Problem Statement

The system schedules academic help requests from fellows into fixed weekly mentor office-hour slots. Each request belongs to one fellow, asks for help on one topic, has a request week, has an urgency level, and lists the slots the fellow can attend. Each slot belongs to one mentor and occurs in one week.

The goal is to produce a feasible partial schedule that serves as many requests as possible. When multiple schedules serve the same number of requests, the preferred schedule is the one with the smallest urgency-weighted waiting cost.

This is a matching and scheduling problem over existing slots. The scheduler does not create new slots, move slots, change mentor specialties, or assign more than one request to the same slot.

## 2. Data Model

The scheduler uses four record types:

- `Fellow`: an integer `id`.
- `Mentor`: an integer `id` and a list of topic specialties.
- `Slot`: a `mentor_id` and a `week`.
- `Request`: a `fellow_id`, a requested topic, an urgency level, a request `week`, and a list of available slot IDs.

Slot IDs are the zero-based positions of slots in the input. Request IDs are the zero-based positions of requests in the input. Fellow and mentor IDs are integer labels supplied by the input.

Urgency has three accepted values:

- `exploratory`: low urgency.
- `normal`: medium urgency.
- `blocker`: high urgency.

For optimization, these urgency levels use weights `1`, `2`, and `3`, respectively.

## 3. Input Format

Input is whitespace-separated. Line breaks are used for readability, but the parser reads tokens in the following order:

```text
F M S R
fellow_id_0 fellow_id_1 ... fellow_id_(F-1)
mentor_id_0 K_0 specialty_0_1 ... specialty_0_K0
...
mentor_id_(M-1) K_(M-1) specialty_(M-1)_1 ... specialty_(M-1)_K
slot_0_mentor_id slot_0_week
...
slot_(S-1)_mentor_id slot_(S-1)_week
request_0_fellow_id A_0 slot_id_0_1 ... slot_id_0_A0 request_0_topic request_0_urgency request_0_week
...
request_(R-1)_fellow_id A_(R-1) slot_id ... request_(R-1)_topic request_(R-1)_urgency request_(R-1)_week
```

Where:

- `F` is the number of fellows.
- `M` is the number of mentors.
- `S` is the number of slots.
- `R` is the number of requests.
- `K_i` is the number of specialties for mentor `i`.
- `A_i` is the number of available slots listed for request `i`.

Input requirements:

- Each slot's `mentor_id` must refer to a mentor in the mentor list.
- Each request's available slot IDs must be valid slot positions from `0` to `S - 1`.
- Topics and specialties are single-token strings.
- Topic and specialty comparison is case-insensitive after normalization to lowercase.
- Urgency strings must be exactly `exploratory`, `normal`, or `blocker`.
- Weeks are integer planning periods. A request can only be assigned to a slot in the same week or a later week.

## 4. Output Format

The scheduler produces one output line per request, in request ID order:

```text
Request i: Assigned to Slot j (Mentor ID: m, Week: w)
```

or:

```text
Request i: Not Assigned
```

Internally, the result is an `assignments` vector of length `R`. `assignments[i]` is the assigned slot ID for request `i`, or `-1` if request `i` is not scheduled.

The improved scheduler may print this notice before the assignment lines if the exact search exceeds the configured time limit:

```text
Backtracking timed out. Using greedy heuristic solution.
```

## 5. Feasibility Constraints

A schedule is feasible only if all of the following hold:

- Each request is assigned to at most one slot.
- Each slot is assigned to at most one request.
- A request can only be assigned to a slot listed in its availability list.
- A request can only be assigned to a slot whose week is greater than or equal to the request week.
- A mentor can serve a request only when the mentor has the requested topic in their specialty list after lowercase normalization.
- Requests that cannot be feasibly assigned may remain unassigned.

The scheduler should not fail merely because demand exceeds supply. It should return the best partial schedule it can find under the algorithm being used.

## 6. Optimization Objective

Let:

- `R` be the set of requests.
- `S` be the set of slots.
- `A_i` be the available slot set for request `i`.
- `topic(i)` be the requested topic for request `i`.
- `week(i)` be the request week for request `i`.
- `mentor(j)` be the mentor assigned to slot `j`.
- `week(j)` be the week of slot `j`.
- `spec(m)` be the specialty set for mentor `m`.
- `u_i` be the urgency weight for request `i`.

Define:

```text
x_ij = 1 if request i is assigned to slot j
x_ij = 0 otherwise
```

Feasibility requires:

```text
sum over j in S of x_ij <= 1                 for every request i
sum over i in R of x_ij <= 1                 for every slot j
x_ij = 0 if j is not in A_i
x_ij = 0 if week(j) < week(i)
x_ij = 0 if topic(i) is not in spec(mentor(j))
```

For an assigned request, delay is:

```text
d_i = week(assigned_slot_i) - week(i)
```

The weighted delay penalty is:

```text
penalty(i) = u_i * d_i(d_i + 1)(2d_i + 1) / 6
```

This is the urgency weight multiplied by the sum of squares from `1` to `d_i`. A delay of `0` therefore has penalty `0`, and longer waits become increasingly expensive.

The objective is lexicographic:

1. Maximize the number of assigned requests.
2. Among schedules with the same number of assigned requests, minimize total weighted delay penalty.

For comparison during exact search, unassigned requests receive an artificial delay based on a week beyond the last available slot. This keeps unassigned requests more expensive than scheduled requests when evaluating otherwise similar schedules.

## 7. Baseline Algorithm

The baseline algorithm is an exact exhaustive search for small and moderate inputs. It establishes the target behavior for the optimization objective.

For each request in input order, the search explores two kinds of choices:

1. Leave the request unassigned.
2. Tentatively assign the request to each slot in its availability list.

After a complete assignment candidate is built, the algorithm validates the candidate against the slot-capacity, request-capacity, week, availability, and mentor-topic constraints. If the candidate is feasible, it computes:

- the number of assigned requests;
- the total urgency-weighted delay cost for assigned requests;
- the artificial delay cost for unassigned requests.

The baseline keeps the candidate with the largest assignment count. Ties are broken by the smallest total cost.

This approach is useful as a correctness reference because it directly searches the assignment space and applies the same objective used by the project. Its expected cost grows exponentially with the number of requests and available slot choices, so it is intended for cases where exhaustive search is practical.

## 8. Improved Algorithm

The improved scheduler uses the same objective and feasibility rules, but adds preprocessing and a timeout-controlled fallback.

Before scheduling, it:

- normalizes topics and specialties to lowercase;
- compresses topic strings to integer IDs for faster comparison;
- maps each slot to its mentor record;
- removes unavailable choices that are already impossible because of week or topic constraints.

It then runs an optimized backtracking search. Instead of validating all constraints only at the end, it tracks which slots are already taken while assigning requests. The search still evaluates complete candidates by assignment count first and weighted delay cost second.

The exact search is limited by `SOLUTION_MAX_TIME`, with periodic checks controlled by `TIME_CHECK_INTERVAL`. If the exact search finishes within the limit, its result is returned. If it times out, the scheduler uses a greedy heuristic.

The greedy fallback:

1. Preprocesses requests using the same feasibility filtering.
2. Builds per-slot request queues grouped by urgency.
3. Sorts each urgency queue by request week so earlier requests are considered first within the same urgency.
4. Processes slots in increasing week order.
5. For each slot, considers the first untaken request from each urgency queue and assigns the request with the largest urgency-weighted delay penalty for that slot.

The fallback is designed to produce a feasible schedule quickly for larger inputs. It follows the same fairness idea as the exact objective, but because it makes local decisions slot by slot, it is a heuristic rather than a guarantee of global optimality.

## Summary

The project specifies a weekly mentor-slot matching system with fixed slot capacity, topic compatibility, request availability, urgency, and weighted waiting-time fairness. The baseline algorithm defines the exact optimization behavior by exhaustive search. The improved algorithm preserves that behavior when exact search is practical and uses a faster greedy fallback when the search exceeds the configured time limit.