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

void baseline_solution(
  vector<Mentor> mentors,
  vector<Slot> slots,
  vector<Request> requests,
  vector<int> &assignments
) {

  // Getting the sizes of slots and requests
  int s = int(slots.size());
  int r = int(requests.size());

  // PRECOMPUTATION
  int max_week = 0;
  for(const auto &slot : slots) {
    max_week = max(max_week, slot.week);
  }
  max_week += 3;

  // Build a map from mentor_id to specialties for topic matching
  unordered_map<int, vector<string>> mentor_specialties;
  for(const auto &mentor : mentors) {
    mentor_specialties[mentor.id] = mentor.specialties;
  }

  assignments.assign(r, -1);
  vector<int> current_assignments(r, -1);

  ll min_cost = LLONG_MAX;
  int max_assignments = 0;

  auto enumerate = [&](auto &&self, int idx) -> void {

    // Base case
    if (idx == r) {

      // Check validity
      vector<bool> slot_used(s, false);
      for(int i = 0; i < r; i++) {
        if (current_assignments[i] == -1) continue;

        int slot_id = current_assignments[i];

        // Slot conflict
        if (slot_used[slot_id]) return;
        slot_used[slot_id] = true;

        // Time feasibility
        if (requests[i].week > slots[slot_id].week) return;

        // Topic feasibility
        const auto &specialties = mentor_specialties[slots[slot_id].mentor_id];
        bool can_assign = false;
        for(const auto &specialty : specialties) {
          if (specialty == requests[i].requested_topic) {
            can_assign = true;
            break;
          }
        }
        if (!can_assign) {
          return;
        }
      }

      // Calculate cost
      ll cost = 0;
      int assigned_count = 0;

      for(int i = 0; i < r; i++) {
        int w = requests[i].urgency + 1;

        if (current_assignments[i] != -1) {
          int d = slots[current_assignments[i]].week - requests[i].week;
          cost += penalty(w, d);
          ++assigned_count;
        } else {
          int d = max_week - requests[i].week + 1;
          cost += penalty(w, d);
        }
      }

      if (assigned_count > max_assignments) {
        max_assignments = assigned_count;
        min_cost = cost;
        assignments = current_assignments;
      } else if (assigned_count == max_assignments && cost < min_cost) {
        min_cost = cost;
        assignments = current_assignments;
      }

      return;
    }

    // Recursive case
    // 1. Do not assign that request to any slot
    self(self, idx + 1);

    // 2. Try to assign that request to each of its available slots
    for(int slot_id : requests[idx].available_slot_ids) {
      current_assignments[idx] = slot_id;
      self(self, idx + 1);
      current_assignments[idx] = -1;
    }

  }; enumerate(enumerate, 0);
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
  baseline_solution(mentors, slots, requests, assignments);

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
