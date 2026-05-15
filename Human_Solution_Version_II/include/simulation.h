#ifndef SIMULATION_H
#define SIMULATION_H

#include "models.h"
#include "hungarian.h"

// Calculate topic match score
int get_topic_match(const Request& r, const Slot& s) {
  int score = 0;
  for (int t1 : r.required_topics) {
    for (int t2 : s.mentor_topics) {
      if (t1 == t2) {
        score++;
      }
    }
  }
  return score;
}

// Check if request r and slot s are compatible
bool is_compatible(const Request& r, const Slot& s) {
  bool time_match = false;
  for (int t : r.acceptable_times) {
    if (t == s.time_block) {
      time_match = true;
      break;
    }
  }

  if (!time_match || get_topic_match(r, s) == 0) {
    return false;
  }
  
  return true;
}

// Calculate the modified edge weight W'
ll calculate_weight(const Request& r, const Slot& s, int current_week) {
  if (!is_compatible(r, s)) return -INF; // Incompatible
  
  ll u = 0;
  if (r.urgency == Urgency::HIGH) u = 3;
  else if (r.urgency == Urgency::MEDIUM) u = 2;
  else u = 1;
  
  ll topic_score = get_topic_match(r, s);
  ll p = (r.preferred_mentor != 0 && r.preferred_mentor == s.mentor_id) ? 1 : 0;
  ll age = r.age(current_week);
  
  ll benefit = ALPHA_URGENCY * u + BETA_TOPIC_MATCH * topic_score + GAMMA_PREFERRED_MENTOR * p + DELTA_AGE * age;
  return benefit + K;
}

class Simulation {
public:
  int current_week;
  
  vector<int> P; // Pending requests indices
  vector<int> A; // Available slots indices
  vector<pair<int, int>> L; // Locked assignments
  
  vector<Request>& requests;
  vector<Slot>& slots;
  
  // Statistics
  ll total_served_requests = 0;
  ll total_benefit = 0;
  
  // Logs for reasons
  vector<string> unserved_reasons;

  int get_expiration_time(int r_idx) {
    int exp_t = 0;
    for (int acc_t : requests[r_idx].acceptable_times) {
      exp_t = max(exp_t, acc_t);
    }
    return exp_t;
  }

  Simulation(vector<Request>& reqs, vector<Slot>& slts) 
    : current_week(1), requests(reqs), slots(slts) {
    unserved_reasons.resize(int(requests.size()), "");    
  }
  
  void run_core_matching() {
    int n = int(P.size());
    int m = int(A.size());
    if (n == 0 || m == 0) return;
    
    int dim = max(n, m);
    vector<vector<ll>> mat(dim + 1, vector<ll>(dim + 1, 0));
    
    for (int i = 0; i < dim; ++i) {
      for (int j = 0; j < dim; ++j) {
        if (i < n && j < m) {
          ll w = calculate_weight(requests[P[i]], slots[A[j]], current_week);
          if (w != -INF) {
            mat[i + 1][j + 1] = -w;
          }
        }
      }
    }
    
    auto res = solve_hungarian(dim, dim, mat);
    vector<int> matching = res.second;
    
    // Update states based on matching
    vector<int> new_P, new_A;
    vector<bool> slot_assigned(m, false);
    
    for (int i = 0; i < n; ++i) {
      int matched_col = matching[i + 1];
      if (matched_col != 0 && mat[i + 1][matched_col] != 0 && mat[i + 1][matched_col] != INF) {

        // Successfully matched
        int slot_idx = matched_col - 1;
        L.push_back({P[i], A[slot_idx]});
        slot_assigned[slot_idx] = true;
        
        // State updates
        requests[P[i]].state = RequestState::SERVED;
        slots[A[slot_idx]].state = SlotState::ASSIGNED;
      } else {
        new_P.push_back(P[i]);
      }
    }
    
    for (int j = 0; j < m; ++j) {
      if (!slot_assigned[j]) {
        new_A.push_back(A[j]);
      }
    }
    
    P = new_P;
    A = new_A;
  }
  
  void process_week(int w, const vector<int>& new_reqs, const vector<int>& new_slots) {
    current_week = w;
    for (int idx : new_reqs) P.push_back(idx);
    for (int idx : new_slots) A.push_back(idx);
    
    // Start of the Week Execution
    run_core_matching();
    
    // Collect all timeblocks
    set<int> unique_times;
    for (int r_idx : P) {
      for (int t : requests[r_idx].acceptable_times) unique_times.insert(t);
    }
    for (int s_idx : A) unique_times.insert(slots[s_idx].time_block);
    for (auto& pair : L) unique_times.insert(slots[pair.second].time_block);
    
    // Handle assignments that can be finalized as well as requests that have expired
    for (int t : unique_times) {
      // Finalize Past
      vector<pair<int, int>> next_L;
      for (auto& pair : L) {
        if (slots[pair.second].time_block < t) {
          finalize_assignment(pair.first, pair.second);
        } else {
          next_L.push_back(pair);
        }
      }
      L = next_L;
      
      // Expire Unserved
      vector<int> next_P;
      for (int r_idx : P) {
        bool can_be_served_later = false;
        for (int acc_t : requests[r_idx].acceptable_times) {
          if (acc_t >= t) {
            can_be_served_later = true;
            break;
          }
        }
        if (!can_be_served_later) {
          expire_request(r_idx);
        } else {
          next_P.push_back(r_idx);
        }
      }
      P = next_P;
      
      // Inject Events affecting the current timeblock t or future
      inject_and_process_events(t);
    }
    
    // Finalize any remaining locked assignments after the week's times are over
    for (auto& pair : L) {
      finalize_assignment(pair.first, pair.second);
    }
    
    L.clear();
  }
  
  void finalize_assignment(int r_idx, int s_idx) {
    requests[r_idx].state = RequestState::SERVED;
    
    // Calculate the pure benefit (without the Big Constant) to record
    ll raw_weight = calculate_weight(requests[r_idx], slots[s_idx], current_week);
    total_benefit += (raw_weight - K);
    total_served_requests++;
  }
  
  void expire_request(int r_idx) {
    requests[r_idx].state = RequestState::CANCELED;
    unserved_reasons[r_idx] = "no feasible slot"; 
  }

  // O(N) Greedy Optimization for a single isolated slot
  void greedy_assign_slot(int s_idx) {
    ll best_w = -INF;
    int best_p_idx = -1;
    for (int i = 0; i < int(P.size()); ++i) {
      ll w = calculate_weight(requests[P[i]], slots[s_idx], current_week);
      if (w > best_w && w != -INF) {
        best_w = w;
        best_p_idx = i;
      }
    }
    
    if (best_p_idx != -1) {
      int r_idx = P[best_p_idx];
      L.push_back({r_idx, s_idx});
      requests[r_idx].state = RequestState::SERVED;
      slots[s_idx].state = SlotState::ASSIGNED;
      P.erase(P.begin() + best_p_idx);
    } else {
      slots[s_idx].state = SlotState::AVAILABLE;
      A.push_back(s_idx);
    }
  }

  // O(N) Greedy Optimization for a single isolated request
  void greedy_assign_request(int r_idx) {
    ll best_w = -INF;
    int best_a_idx = -1;
    for (int i = 0; i < int(A.size()); ++i) {
      ll w = calculate_weight(requests[r_idx], slots[A[i]], current_week);
      if (w > best_w && w != -INF) {
        best_w = w;
        best_a_idx = i;
      }
    }
    
    if (best_a_idx != -1) {
      int s_idx = A[best_a_idx];
      L.push_back({r_idx, s_idx});
      requests[r_idx].state = RequestState::SERVED;
      slots[s_idx].state = SlotState::ASSIGNED;
      A.erase(A.begin() + best_a_idx);
    } else {
      requests[r_idx].state = RequestState::PENDING;
      P.push_back(r_idx);
    }
  }
  
  void inject_and_process_events(int current_time_block) {
    // Process P Cancellations
    for (auto it = P.begin(); it != P.end(); ) {
      if (getRandomPercentage() < PROB_REQUEST_CANCEL) {
        requests[*it].state = RequestState::CANCELED;
        unserved_reasons[*it] = "request canceled";
        it = P.erase(it);
      } else {
        ++it;
      }
    }
    
    vector<int> freed_slots;
    vector<int> freed_requests;
    bool need_rematch = false;

    // Process A Cancellations & Reschedules
    for (auto it = A.begin(); it != A.end(); ) {
      if (slots[*it].time_block >= current_time_block) {
        int r = getRandomPercentage();
        if (r < PROB_SLOT_CANCEL) {
          slots[*it].state = SlotState::CANCELED;
          it = A.erase(it);
          continue;
        } else if (r < PROB_SLOT_RESCHEDULE) {
          slots[*it].time_block += 3; // Simplistic reschedule
          freed_slots.push_back(*it);
          it = A.erase(it);
          continue;
        }
      }
      ++it;
    }
    
    // Process L Breakages
    vector<pair<int, int>> next_L;
    for (auto& pair : L) {
      if (slots[pair.second].time_block >= current_time_block) {
        int r1 = getRandomPercentage();
        if (r1 < PROB_SLOT_CANCEL) {
          // Slot Cancellation
          slots[pair.second].state = SlotState::CANCELED;
          freed_requests.push_back(pair.first);
          continue;
        }
        
        int r2 = getRandomPercentage();
        if (r2 < PROB_REQUEST_CANCEL) {
          // Request Cancellation
          requests[pair.first].state = RequestState::CANCELED;
          unserved_reasons[pair.first] = "request canceled";
          freed_slots.push_back(pair.second);
          continue;
        }
        
        int r3 = getRandomPercentage();
        if (r3 < PROB_SLOT_RESCHEDULE) {
          // Slot Rescheduling (Assigned)
          slots[pair.second].time_block += 3;
          slots[pair.second].state = SlotState::AVAILABLE;
          requests[pair.first].state = RequestState::PENDING;
          A.push_back(pair.second);
          P.push_back(pair.first);
          need_rematch = true;
          continue;
        }
      }
      next_L.push_back(pair);
    }
    
    L = next_L;
    
    if (freed_slots.size() + freed_requests.size() > 1) {
      need_rematch = true;
    }
    
    if (need_rematch) {
      // Re-add any freed resources before rematching
      for (int s : freed_slots) {
        slots[s].state = SlotState::AVAILABLE;
        A.push_back(s);
      }
      for (int r : freed_requests) {
        requests[r].state = RequestState::PENDING;
        P.push_back(r);
      }
      run_core_matching();
    } else {
      // O(N) Greedy Assignments for isolated breakages
      for (int s : freed_slots) greedy_assign_slot(s);
      for (int r : freed_requests) greedy_assign_request(r);
      
      // Re-sort to maintain chronological pointer efficiency
      if (!freed_slots.empty() || !freed_requests.empty()) {
        sort(L.begin(), L.end(), [&](const pair<int, int>& a, const pair<int, int>& b) {
          return slots[a.second].time_block < slots[b.second].time_block;
        });
        sort(P.begin(), P.end(), [&](int a, int b) {
          return get_expiration_time(a) < get_expiration_time(b);
        });
      }
    }
  }
};

#endif // SIMULATION_H
