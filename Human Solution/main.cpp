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

// Function to implement brute-force backtracking solution
void backtracking_solution(
  vector<Fellow> fellows,
  vector<Mentor> mentors,
  vector<Slot> slots,
  vector<Request> requests, 
  vector<int> assignments
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


  auto backtrack = [&](auto &&self, int idx) -> void {
    
    // Base case
    if (idx == r) {

      // Calculate the cost of the current assignment
      ll cost = 0;
      for(int i = 0; i < s; i++) {
        if (assigned_to[i] != -1) {
          const Request &request = requests[assigned_to[i]];
          int s_f = (slots[i].week - request.week + 1);
          cost += 1LL * s_f * s_f * (request.urgency + 1);
        }
      }

      // Update the minimum cost and the best assignment if the current cost is lower
      if (cost < min_cost) {
        min_cost = cost;
        for(int i = 0; i < s; i++) {
          if (assigned_to[i] != -1) {
            assignments[assigned_to[i]] = i;
          }
        }
      }

      return;
    }

    // Recursive case
    for(int id_slot : requests[idx].available_slot_ids) {
      if (assigned_to[id_slot] == -1) {
        assigned_to[id_slot] = idx;
        self(self, idx + 1);
        assigned_to[id_slot] = -1;
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
    for(auto &specialty : mentor.specialties) {
      specialty = int(lower_bound(
        all_topics.begin(),
        all_topics.end(),
        specialty
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