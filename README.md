# Analysis and Design of Algorithms
## Project II — Tutoring Scheduler
---

## Real-World Narrative

A university mentorship program provides weekly one-on-one support sessions between fellows and mentors. Fellows use these sessions to address technical, academic, or project-related challenges.

Each week, fellows submit support requests describing their needs and availability. Mentors have limited availability and specialized topics of expertise. The program must assign sessions in a way that is fair, explainable, and sensitive to both the **urgency** of the request and the **long-term waiting time (age)** of pending requests.

The system is dynamic: information changes throughout the week. Mentor slots may be canceled or rescheduled, and requests may be withdrawn or canceled.

---

## Notation

- **F**: Number of fellows enrolled.
- **M**: Number of mentors.
- **S**: Number of new mentor slots offered in a given week.
- **R**: Number of new requests submitted in a given week.
- **w**: Current week number (starting from 1).

---

## Input Definition

The system reads all input from a single file. IDs are unique integers.

### Global Setup
1. **Fellows**: A list of unique fellow IDs.
2. **Mentors**: A list of unique mentor IDs.
3. **Mentor Specialties**: For each mentor, a list of strings representing their topics of expertise.

### Weekly Data
For each week `w`, the following are provided:
- **New Slots**: A list of `slot_id mentor_id time_block`.
- **New Requests**: A list of `request_id fellow_id required_topics acceptable_times urgency preferred_mentor`.
- **Slot Cancellations**: A list of `slot_id:time_block` to be removed.
- **Slot Rescheduling**: A list of `slot_id old_time_block new_time_block`.
- **Request Cancellations**: A list of `request_id:time_block` to be withdrawn.

---

## System State

The system maintains a set of **pending requests** across weeks.
- **Age**: For a request $r$ submitted in week $r.week$, its age at week $w$ is $w - r.week$.
- **Backlog Waiting Time**: For a fellow $f$, $W_f(w) = \sum_{r \in pending(f)} (w - r.week)$.

---

## Constraints

1. **Topic Match**: A request and slot are compatible only if their topic sets intersect (at least one common topic).
2. **Availability**: The slot's `time_block` must be in the request's `acceptable_times` list.
3. **Capacity**: Each slot is assigned to at most one request.
4. **No Limits**: A fellow may have multiple requests and receive multiple assignments in the same week if their schedule allows.

---

## Objective Function

The algorithm follows a **lexicographic** optimization strategy each week:

1. **Primary Goal**: Maximize the total number of **requests** served.
2. **Secondary Goal**: Maximize the total **benefit** of served requests:
   $$Benefit_{week} = \sum_{r \in Served} (\alpha \cdot u(urgency_r) + \beta \cdot topicMatch(r, slot_r) + \gamma \cdot p_r + \delta \cdot age_r)$$

**Weights and Values**:
- **Urgency ($u$):** High = 3, Medium = 2, Low = 1.
- **Topic Match:** Number of overlapping topics between request and mentor.
- **Preferred Mentor ($p_r$):** 1 if the assigned mentor matches the request's preference, else 0.
- **Age ($age_r$):** Current week - Submission week.
- **Suggested Constants:** $\alpha=5, \beta=2, \gamma=1, \delta=2$.

---

## Output Definition

At the end of each week, the system produces:
1. **Assignments**: A set of `request_id → slot_id` mappings.
2. **Unserved Reasons**: For every unserved request, one of the following:
   - **no feasible slot**: No compatible slot exists (topic/time mismatch).
   - **lower priority**: A compatible slot existed but was given to a higher-benefit request.
   - **request canceled**: The request was canceled before assignment.

---

## Repo's Link
https://github.com/marcelinoSamer/analysisProject2
