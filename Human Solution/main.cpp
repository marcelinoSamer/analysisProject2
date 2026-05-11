// Required libraries and headers
#include "bits/stdc++.h"
#include "./include/config.h"
#include "./include/enums.h"
#include "./include/interfaces.h"

// Optimization flags for faster execution
#pragma GCC optimize("O3,unroll-loops")

/*
\section*{Objective Function}

The algorithm minimizes the total starvation penalty for the current week:
\[
  \text{Penalty}_{\text{week}} = \sum_{f \in U} w(\texttt{urgency}_f)
  \cdot (s_f + 1)^{2}
\]
where $U$ is the set of fellows who submitted a request but were not assigned
this week, and $(s_f + 1)$ is the value $s_f$ will take after the update. The
urgency weights are:
\[
  w(\texttt{blocker}) = 3, \quad
  w(\texttt{normal}) = 2, \quad
  w(\texttt{exploratory}) = 1
\]

The algorithm minimizes $\text{Penalty}_{\text{week}}$ independently each week.
The accumulated total across all weeks:
\[
  \text{Penalty}_{\text{total}} = \sum_{t=1}^{T} \text{Penalty}_{t}
\]
is reported as a performance metric at the end of the program. Each week's
contribution is a fresh charge for that week's assignment decision. It is
not a re-counting of previous weeks, but a new cost reflecting how much it
hurts to skip a fellow given how long they have already been waiting.

\section*{Tie-Breaking Rules}

When multiple feasible assignments yield the same weekly penalty, ties are
broken in the following order:

\begin{enumerate}
    \item Higher urgency tier first
          ($\texttt{blocker} > \texttt{normal} > \texttt{exploratory}$).
    \item Higher current $s_f$ value.
    \item Earlier request timestamp.
\end{enumerate}
*/

// Using standard namespace and defining long long for convenience
using namespace std;
typedef long long ll;

// Function to implement brute-force backtracking solution => O(S^R)
void backtracking_solution(
  vector<Fellow> fellows,
  vector<Mentor> mentors,
  vector<Slot> slots,
  vector<Request> requests, 
  vector<int> &assignments
  ) {

  // Getting the sizes of fellows, mentors, slots, and requests for easier reference
  int f = int(fellows.size());
  int m = int(mentors.size());
  int s = int(slots.size());
  int r = int(requests.size());
  assignments.assign(r, -1);

  // Vector to keep track of which slot is assigned to which request, initialized to -1
  vector<int> assigned_to(s, -1);

  // Minimum cost of an assignment
  ll min_cost = LLONG_MAX;
  int max_assignments = 0;

  auto backtrack = [&](auto &&self, int idx) -> void {
    
    // Base case
    if (idx == r) {

      ll cost = 0;
      vector<int> new_assignments(r, -1);
      
      // Determine assignments and calculate the max week number among the slots
      int mx_week = 0, assigned_count = 0;
      for (int i = 0; i < s; i++) {
        if (assigned_to[i] != -1) {
          new_assignments[assigned_to[i]] = i;
          ++assigned_count;
        }
        mx_week = max(mx_week, slots[i].week);
      }

      // Calculate the cost for each request based on whether it was assigned or not
      for (int i = 0; i < r; i++) {
        int w = requests[i].urgency + 1;

        if (new_assignments[i] != -1) {
          int d = slots[new_assignments[i]].week - requests[i].week;
          cost += 1LL * w * d * (d + 1) * (2 * d + 1) / 6;
        } else {
          int d = mx_week - requests[i].week + 1;
          cost += 1LL * w * d * (d + 1) * (2 * d + 1) / 6;
        }
      }

      if (assigned_count > max_assignments) {
        max_assignments = assigned_count;
        min_cost = cost;
        assignments = new_assignments;
      } else if (assigned_count == max_assignments && cost < min_cost) {
        min_cost = cost;
        assignments = new_assignments;
      }
      return;
    }

    // Recursive case
    // 1. Do not assign that request to any slot
    self(self, idx + 1);
    // 2. Try to assign that request to each of its available slots
    for(int id_slot : requests[idx].available_slot_ids) {
      // Cannot assign to a slot that is in the past
      if (requests[idx].week > slots[id_slot].week) {
        continue;
      }

      // Check if the slot is unassigned and if the mentor of that slot has the required specialty
      if (assigned_to[id_slot] == -1) {
        bool can_assign = false;
        for(int topic_id : mentors[slots[id_slot].mentor_idx].speciality_ids) {
          if (topic_id == requests[idx].requested_topic_id) {
            can_assign = true;
            break;
          }
        }
        if (can_assign) {
          assigned_to[id_slot] = idx;
          self(self, idx + 1);
          assigned_to[id_slot] = -1;
        }
      }
    }
  }; backtrack(backtrack, 0);

}

// Function to implement greedy huristic solution
void greedy_heuristic_solution() {
  
}

int main() {

  // Fast input/output optimization
  ios::sync_with_stdio(0); cin.tie(0);
  
  // Read the number of fellows, mentors, slots, and requests
  int F, M, S, R;
  cin >> F >> M >> S >> R;

  // Read fellows
  vector<Fellow> fellows(F);
  for(int i = 0; i < F; i++) {
    cin >> fellows[i].id;
  }

  // Read mentors and their specialties
  vector<Mentor> mentors(M);
  for(int i = 0; i < M; i++) {
    // Read mentor ID
    cin >> mentors[i].id;
    int num_specialties;

    // Read the number of specialties and the specialties themselves
    cin >> num_specialties;
    mentors[i].specialties.resize(num_specialties);
    mentors[i].speciality_ids.resize(num_specialties, -1);
    for(int j = 0; j < num_specialties; j++) {
      cin >> mentors[i].specialties[j];
    }
  }

  // Read slots
  vector<Slot> slots(S);
  for(int i = 0; i < S; i++) {
    cin >> slots[i].mentor_id >> slots[i].week;
    auto it = find_if(mentors.begin(), mentors.end(), [&](const Mentor &mentor) {
      return mentor.id == slots[i].mentor_id;
    });
    if (it == mentors.end()) {
      throw runtime_error("Mentor ID not found for slot " + to_string(i));
    }
    slots[i].mentor_idx = int(it - mentors.begin());
  }

  // Read requests
  vector<Request> requests(R);
  for(int i = 0; i < R; i++) {
    // Read fellow ID for the request
    cin >> requests[i].fellow_id;

    // Read the number of available slots and the slot IDs
    int num_available_slots;
    cin >> num_available_slots;
    requests[i].available_slot_ids.resize(num_available_slots);
    for(int j = 0; j < num_available_slots; j++) {
      cin >> requests[i].available_slot_ids[j];
    }

    // Read the requested topic
    cin >> requests[i].requested_topic_str;
    
    // Read the urgency level and convert it to the corresponding enum value
    string urgency_str;
    cin >> urgency_str;
    if (urgency_str == "blocker") {
      requests[i].urgency = BLOCKER;
    } else if (urgency_str == "normal") {
      requests[i].urgency = NORMAL;
    } else {
      requests[i].urgency = EXPLORATORY;
    }

    // Read the week when the request was submitted
    cin >> requests[i].week;
  }

  // Normalizing the topic names
  for(auto &mentor : mentors) {
    for(auto &specialty : mentor.specialties) {
      for(auto &c : specialty) {
        c = tolower(c);
      }
    }
  }

  for(auto &request : requests) {
    for(auto &c : request.requested_topic_str) {
      c = tolower(c);
    }
  }

  // Compressing the requested topics
  vector<string> all_topics;
  for(const auto &mentor : mentors) {
    for(const auto &specialty : mentor.specialties) {
      all_topics.push_back(specialty);
    }
  }

  for(const auto &request : requests) {
    all_topics.push_back(request.requested_topic_str);
  }

  sort(all_topics.begin(), all_topics.end());
  all_topics.erase(unique(all_topics.begin(), all_topics.end()), all_topics.end());

  for(auto &mentor : mentors) {
    int size = int(mentor.specialties.size());
    for(int i = 0; i < size; i++) {
      mentor.speciality_ids[i] = int(lower_bound(
        all_topics.begin(),
        all_topics.end(),
        mentor.specialties[i]
      ) - all_topics.begin());
    }
  }

  for(auto &request : requests) {
    request.requested_topic_id = int(lower_bound(
      all_topics.begin(),
      all_topics.end(),
      request.requested_topic_str
    ) - all_topics.begin());
  }
}