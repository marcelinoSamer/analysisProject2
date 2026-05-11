// Required libraries and headers
#include "bits/stdc++.h"
#include "./include/config.h"
#include "./include/enums.h"
#include "./include/interfaces.h"

// Optimization flags for faster execution
#pragma GCC optimize("O3,unroll-loops")

// Using standard namespace and defining long long for convenience
using namespace std;
typedef long long ll;

// Function to calculate the penalty based on urgency and delay
ll penalty(int w, int d) {
  return 1LL * d * (d + 1) * (2 * d + 1) / 6 * w;
}

// Function to compress topics and mentor specialties into integer IDs
void compress(
  vector<Mentor> mentors,
  vector<Slot> slots,
  vector<Request> requests,
  vector<vector<int>> &mentor_speciality_ids,
  vector<int> &request_topic_ids,
  vector<int> &slot_mentor_idx
) {

  // Getting the sizes of fellows, mentors, slots, and requests
  int m = int(mentors.size());
  int s = int(slots.size());
  int r = int(requests.size());

  // Compressing the requested topics and mentor specialties into integer IDs for faster comparisons
  vector<string> all_topics;
  for(const auto &mentor : mentors) {
    for(const auto &specialty : mentor.specialties) {
      all_topics.push_back(specialty);
    }
  }
  for(const auto &request : requests) {
    all_topics.push_back(request.requested_topic);
  }
  
  sort(all_topics.begin(), all_topics.end());
  all_topics.erase(unique(all_topics.begin(), all_topics.end()), all_topics.end());
  
  mentor_speciality_ids.resize(m);
  for(int i = 0; i < m; i++) {
    for(const auto &specialty : mentors[i].specialties) {
      int topic_id = int(lower_bound(
        all_topics.begin(),
        all_topics.end(),
        specialty
      ) - all_topics.begin());
      mentor_speciality_ids[i].push_back(topic_id);
    }
  }
  
  request_topic_ids.resize(r);
  for(int i = 0; i < r; i++) {
    request_topic_ids[i] = int(lower_bound(
      all_topics.begin(),
      all_topics.end(),
      requests[i].requested_topic
    ) - all_topics.begin());
  }
  
  // Getting the index of the mentor for each slot for faster access during backtracking
  slot_mentor_idx.resize(s);
  for(int i = 0; i < s; i++) {
    auto it = find_if (mentors.begin(), mentors.end(), [&](const Mentor &mentor) {
      return mentor.id == slots[i].mentor_id;
    });
    if (it == mentors.end()) {
      throw runtime_error("Mentor ID not found for slot " + to_string(i));
    }
    slot_mentor_idx[i] = int(it - mentors.begin());
  }
}

// Function to remove invalid slots from requests
void preprocess_requests(
  vector<Slot> slots,
  vector<vector<int>> mentor_speciality_ids,
  vector<int> request_topic_ids,
  vector<int> slot_mentor_idx,
  vector<Request> &requests
) {
  // Getting the sizes of requests
  int r = int(requests.size());

  for(int i = 0; i < r; i++) {
    vector<int> valid_slots;
    for(int slot_id : requests[i].available_slot_ids) {
      
      // Time feasibility
      if (requests[i].week > slots[slot_id].week) {
        continue;
      }

      // Topic feasibility
      bool can_assign = false;
      for(int topic_id : mentor_speciality_ids[slot_mentor_idx[slot_id]]) {
        if (topic_id == request_topic_ids[i]) {
          can_assign = true;
          break;
        }
      }
      if (can_assign) {
        valid_slots.push_back(slot_id);
      }
    }
    requests[i].available_slot_ids = valid_slots;
  }
}

// Function to implement brute-force backtracking solution => O(S^R)
void backtracking_solution(
  vector<Mentor> mentors,
  vector<Slot> slots,
  vector<Request> requests, 
  vector<int> &assignments,
  bool &timed_out
  ) {

  // Getting the sizes of slots and requests
  int s = int(slots.size());
  int r = int(requests.size());

  // COMPRESSION
  vector<vector<int>> mentor_speciality_ids;
  vector<int> request_topic_ids;
  vector<int> slot_mentor_idx;
  compress(mentors, slots, requests, mentor_speciality_ids, request_topic_ids, slot_mentor_idx);

  // PRECOMPUTATION
  int max_week = 0;
  for(const auto &slot : slots) {
    max_week = max(max_week, slot.week);
  }
  max_week += 3;

  // PREPROCESSING
  preprocess_requests(slots, mentor_speciality_ids, request_topic_ids, slot_mentor_idx, requests);

  assignments.assign(r, -1);

  // Vector to keep track of which slot is assigned to which request, initialized to -1
  vector<int> assigned_to(s, -1);

  // Minimum cost of an assignment
  ll min_cost = LLONG_MAX;
  int max_assignments = 0;

  // Start the backtracking process and measure the time taken
  int call_count = 0;
  auto start = chrono::steady_clock::now();
  auto backtrack = [&](auto &&self, int idx) -> void {
    
    // Check for time limit exceeded
    if (timed_out) {
      return;
    }
    if (++call_count % TIME_CHECK_INTERVAL == 0) {
      auto elapsed = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - start
      ).count();
      if (elapsed > SOLUTION_MAX_TIME) {
        timed_out = true;
        return;
      }
    }

    // Base case
    if (idx == r) {

      ll cost = 0;
      vector<int> new_assignments(r, -1);
      
      // Determine assignments and calculate the max week number among the slots
      int assigned_count = 0;
      for (int i = 0; i < s; i++) {
        if (assigned_to[i] != -1) {
          new_assignments[assigned_to[i]] = i;
          ++assigned_count;
        }
      }

      // Calculate the cost for each request based on whether it was assigned or not
      for (int i = 0; i < r; i++) {
        int w = requests[i].urgency + 1;

        if (new_assignments[i] != -1) {
          int d = slots[new_assignments[i]].week - requests[i].week;
          cost += penalty(w, d);
        } else {
          int d = max_week - requests[i].week + 1;
          cost += penalty(w, d);
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

      // Check if the slot is unassigned and if the mentor of that slot has the required specialty
      if (assigned_to[id_slot] == -1) {
        assigned_to[id_slot] = idx;
        self(self, idx + 1);
        assigned_to[id_slot] = -1;
      }
    }
  }; backtrack(backtrack, 0);
}

// Function to implement greedy huristic solution
void greedy_heuristic_solution(
  vector<Mentor> mentors,
  vector<Slot> slots,
  vector<Request> requests,
  vector<int> &assignments
) {

  // Getting the sizes of slots and requests
  int s = int(slots.size());
  int r = int(requests.size());

  // COMPRESSION
  vector<vector<int>> mentor_speciality_ids;
  vector<int> request_topic_ids;
  vector<int> slot_mentor_idx;
  compress(mentors, slots, requests, mentor_speciality_ids, request_topic_ids, slot_mentor_idx);

  // PRECOMPUTATION
  int max_week = 0;
  for(const auto &slot : slots) {
    max_week = max(max_week, slot.week);
  }
  max_week += 3;

  // PREPROCESSING
  preprocess_requests(slots, mentor_speciality_ids, request_topic_ids, slot_mentor_idx, requests);

  assignments.assign(r, -1);
  vector<bool> request_taken(r, false);

  // For each slot, build 3 queues (one per urgency level)
  // Each queue holds (request_week, request_idx) sorted by request_week
  // so that earlier requests are served first within the same urgency
  vector<array<vector<pair<int,int>>, 3>> slot_queues(s);
  for(int i = 0; i < r; i++) {
    for(int slot_id : requests[i].available_slot_ids) {
      int urgency = requests[i].urgency;
      slot_queues[slot_id][urgency].push_back({requests[i].week, i});
    }
  }

  // Sort each queue by request week (earlier requests first)
  for(int i = 0; i < s; i++) {
    for(int j = 0; j < 3; j++) {
      sort(slot_queues[i][j].begin(), slot_queues[i][j].end());
    }
  }

  // Sort slots by week
  vector<int> slot_order(s);
  iota(slot_order.begin(), slot_order.end(), 0);
  sort(slot_order.begin(), slot_order.end(), [&](int a, int b) {
    return slots[a].week < slots[b].week;
  });

  // Pointers into each queue per slot to avoid erasing elements
  vector<array<int, 3>> slot_queue_ptrs(s, {0, 0, 0});

  // Process slots in order
  for(int slot_id : slot_order) {
    int slot_week = slots[slot_id].week;

    // From each urgency queue, find the top valid (not yet taken) request
    // Then among the 3 candidates, pick the one with maximum penalty at this slot
    int best_req = -1;
    ll max_penalty = -1;

    for(int u = 0; u < 3; u++) {
      auto &q = slot_queues[slot_id][u];
      auto &ptr = slot_queue_ptrs[slot_id][u];

      while(ptr < int(q.size()) && request_taken[q[ptr].second]) ptr++;

      // If all taken
      if (ptr >= int(q.size())) {
        continue;
      }

      int req_idx = q[ptr].second;
      int w = requests[req_idx].urgency + 1;
      int d = slot_week - requests[req_idx].week;
      ll p = penalty(w, d);

      if (p > max_penalty) {
        max_penalty = p;
        best_req = req_idx;
      }
    }

    // If no valid request found
    if (best_req == -1) {
      continue;
    }

    // Assign best request to this slot
    assignments[best_req] = slot_id;
    request_taken[best_req] = true;
  }
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
    cin >> requests[i].requested_topic;
    
    // Read the urgency level and convert it to the corresponding enum value
    string urgency_str;
    cin >> urgency_str;
    if (urgency_str == "blocker") {
      requests[i].urgency = BLOCKER;
    } else if (urgency_str == "normal") {
      requests[i].urgency = NORMAL;
    } else if (urgency_str == "exploratory") {
      requests[i].urgency = EXPLORATORY;
    } else {
      throw runtime_error("Invalid urgency level: " + urgency_str);
    }

    // Read the week when the request was submitted
    cin >> requests[i].week;
  }

  // Normalizing the topic names
  {
    for(auto &mentor : mentors) {
      for(auto &specialty : mentor.specialties) {
        for(auto &c : specialty) {
          c = tolower(c);
        }
      }
    }
  
    for(auto &request : requests) {
      for(auto &c : request.requested_topic) {
        c = tolower(c);
      }
    }
  }

  vector<int> assignments;
  bool timed_out = false;
  backtracking_solution(mentors, slots, requests, assignments, timed_out);

  // If backtracking timed out, use the greedy heuristic solution
  if (timed_out) {
    cout << "Backtracking timed out. Using greedy heuristic solution.\n";
    greedy_heuristic_solution(mentors, slots, requests, assignments);
  }

  // Output the assignments
  for(int i = 0; i < R; i++) {
    cout << "Request " << i << ": ";
    if (assignments[i] != -1) {
      cout << "Assigned to Slot " << assignments[i] << " (Mentor ID: " << slots[assignments[i]].mentor_id << ", Week: " << slots[assignments[i]].week << ")";
    } else {
      cout << "Not Assigned";
    }
    cout << '\n';
  }
}
