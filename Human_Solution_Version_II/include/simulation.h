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
  for (auto [w, t] : r.acceptable_times) {
    if (t == s.time_block.second) {
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
  
  // Weekly Logs
  vector<string> weekly_logs;
  
  // Logs for reasons
  vector<string> unserved_reasons;

  pair<int, int> get_expiration_time(int r_idx) {
    pair<int, int> exp_t = {0, 0};
    for (pair<int, int> acc_t : requests[r_idx].acceptable_times) {
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
  
  void process_week(
    int w, const vector<int>& new_reqs, const vector<int>& new_slots,
    const vector<pair<int, pair<int, int>>>& slot_cancellations,
    const vector<pair<int, pair<pair<int, int>, pair<int, int>>>>& slot_reschedules,
    const vector<pair<int, pair<int, int>>>& req_cancellations
  ) {
    current_week = w;
    for (int idx : new_reqs) P.push_back(idx);
    for (int idx : new_slots) A.push_back(idx);

    // Check if there are no matching slots for any request
    for(auto &p : P) {
      bool has_no_matching_slot = true;
      for(auto &s : A) {
        if (is_compatible(requests[p], slots[s])) {
          has_no_matching_slot = false;
          break;
        }
      }
      if (has_no_matching_slot) {
        unserved_reasons[p] = "no feasible slot";
      }
    }

    // Start of the Week Execution
    run_core_matching();
    
    map<pair<int, int>, vector<int>> slot_cancels_at;
    map<pair<int, int>, vector<pair<int, pair<int, int>>>> slot_rescheds_at;
    map<pair<int, int>, vector<int>> req_cancels_at;

    set<pair<int, int>> unique_times;
    
    // Collect times from existing P, A and L
    for (int r_idx : P) {
      for (auto t : requests[r_idx].acceptable_times) unique_times.insert(t);
    }
    for (auto s_idx : A) unique_times.insert(slots[s_idx].time_block);
    for (auto& pair : L) unique_times.insert(slots[pair.second].time_block);
    
    for (const auto& c : slot_cancellations) {
      slot_cancels_at[c.second].push_back(c.first);
      unique_times.insert(c.second);
    }
    for (const auto& r : slot_reschedules) {
      slot_rescheds_at[r.second.first].push_back({r.first, r.second.second});
      unique_times.insert(r.second.first);
      unique_times.insert(r.second.second);
    }
    for (const auto& q : req_cancellations) {
      req_cancels_at[q.second].push_back(q.first);
      unique_times.insert(q.second);
    }
    
    // Handle assignments that can be finalized as well as requests that have expired
    for (auto t : unique_times) {
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
      
      // 1. Request Cancellations
      if (req_cancels_at.count(t)) {
        for (int r_idx : req_cancels_at[t]) {
          handle_request_cancellation(t, r_idx);
        }
      }

      // 2. Slot Cancellations
      if (slot_cancels_at.count(t)) {
        for (int s_idx : slot_cancels_at[t]) {
          handle_slot_cancellation(t, s_idx);
        }
      }

      // 3. Slot Reschedules
      if (slot_rescheds_at.count(t)) {
        for (const auto& sr : slot_rescheds_at[t]) {
          int s_idx = sr.first;
          auto new_t = sr.second;
          handle_slot_reschedule(t, s_idx, new_t);
        }
      }
    }
    
    // Finalize any remaining locked assignments after the week's times are over
    for (auto& pair : L) {
      finalize_assignment(pair.first, pair.second);
    }
    
    L.clear();
  }
  

  /*
  Case 1 (Request cancellation): If not assigned, ignore. Otherwise, greadily assign its slot to the request with 
  the heighest benefit if possible
  Case 2 (Slot cancellation): If not assigned, ignore. Otherwise, greadily assign its slot to the request with 
  the heighest benefit if possible
  Case 3 (Slot reschduling): Some edges are removed. Otheredges can be added. Greadily re-consider all the possible edges.
  This way, we can avoid rerunning the entire algorithm.
  */
  void handle_request_cancellation(pair<int, int> t, int r_idx) {
    if (requests[r_idx].state == RequestState::PENDING) {
      auto it = find(P.begin(), P.end(), r_idx);
      if (it != P.end()) P.erase(it);
      requests[r_idx].state = RequestState::CANCELED;
      unserved_reasons[r_idx] = "request canceled";
      weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[r_idx].id) + " canceled (Pending)");
    } else {
      auto it = find_if(L.begin(), L.end(), [r_idx](const pair<int, int>& p){ return p.first == r_idx; });
      if (it != L.end()) {
        L.erase(it);
        weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[r_idx].id) + " canceled (assigned)");
        int s_idx = it->second;
        slots[s_idx].state = SlotState::AVAILABLE;
        A.push_back(s_idx);
        int best_r = -1;
        for(int r : P) {
          if (is_compatible(requests[r], slots[s_idx]) && requests[r].state == RequestState::PENDING) {
            if (best_r == -1 || calculate_weight(requests[r], slots[s_idx], current_week) > calculate_weight(requests[best_r], slots[s_idx], current_week)) {
              best_r = r;
            }
          }
        }
        if (best_r != -1) {
          requests[best_r].state = RequestState::SERVED;
          A.erase(find(A.begin(), A.end(), s_idx));
          P.erase(find(P.begin(), P.end(), best_r));
          L.push_back({best_r, s_idx});
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[best_r].id) + " assigned to Slot " + to_string(slots[s_idx].id));
        }
      }
    }
  }

  void handle_slot_cancellation(pair<int, int> t, int s_idx)  {
    if (slots[s_idx].state == SlotState::AVAILABLE) {
      auto it = find(A.begin(), A.end(), s_idx);
      if (it != A.end()) A.erase(it);
      slots[s_idx].state = SlotState::CANCELED;
      weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Slot " + to_string(slots[s_idx].id) + " canceled (available)");
    } else {
      auto it = find_if(L.begin(), L.end(), [s_idx](const pair<int, int>& p) { return p.second == s_idx; });
      if (it != L.end()) {
        int r_idx = it->first;
        L.erase(it);
        slots[s_idx].state = SlotState::CANCELED;
        requests[r_idx].state = RequestState::PENDING;
        P.push_back(r_idx);
        weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Slot " + to_string(slots[s_idx].id) + " canceled (breaking assignment with Request " + to_string(requests[r_idx].id) + ")");
        int best_s = -1;
        for(int s : A) {
          if (is_compatible(requests[r_idx], slots[s])) {
            if (best_s == -1 || calculate_weight(requests[r_idx], slots[s], current_week) > calculate_weight(requests[best_s], slots[s], current_week)) {
              best_s = s;
            }
          }
        }
        if (best_s != -1) {
          requests[r_idx].state = RequestState::SERVED;
          slots[best_s].state = SlotState::ASSIGNED;
          P.erase(find(P.begin(), P.end(), r_idx));
          A.erase(find(A.begin(), A.end(), best_s));
          L.push_back({r_idx, best_s});
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[r_idx].id) + " assigned to Slot " + to_string(slots[best_s].id));
        }
      }
    }
  }

  /*
  if (slots[s_idx].state == SlotState::AVAILABLE) {
    auto old_t = slots[s_idx].time_block;
    slots[s_idx].time_block = new_t;
    weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Slot " + to_string(slots[s_idx].id) + " rescheduled (available): t=" + to_string(old_t.second) + " -> t=" + to_string(new_t.second));
    state_changed = true;
  } else {
    auto it = find_if(L.begin(), L.end(), [s_idx](const pair<int, int>& p) { return p.second == s_idx; });
    if (it != L.end()) {
      int r_idx = it->first;
      L.erase(it);
      auto old_t = slots[s_idx].time_block;
      slots[s_idx].time_block = new_t;
      slots[s_idx].state = SlotState::AVAILABLE;
      requests[r_idx].state = RequestState::PENDING;
      A.push_back(s_idx);
      P.push_back(r_idx);
      weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Slot " + to_string(slots[s_idx].id) + " rescheduled (assigned): t=" + to_string(old_t.second) + " -> t=" + to_string(new_t.second) + " (Request " + to_string(requests[r_idx].id) + " released)");
    state_changed = true;
  }
  */
  void handle_slot_reschedule(pair<int, int> t, int s_idx, pair<int, int> new_t) {
    if (slots[s_idx].state == SlotState::AVAILABLE) {
      auto old_t = slots[s_idx].time_block;
      slots[s_idx].time_block = new_t;
      weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Slot " + to_string(slots[s_idx].id) + " rescheduled (available): t=" + to_string(old_t.second) + " -> t=" + to_string(new_t.second));
      int best_r = -1;
      for(int r : P) {
        if (is_compatible(requests[r], slots[s_idx]) && requests[r].state == RequestState::PENDING) {
          if (best_r == -1 || calculate_weight(requests[r], slots[s_idx], current_week) > calculate_weight(requests[best_r], slots[s_idx], current_week)) {
            best_r = r;
          }
        }
      }
      if (best_r != -1) {
        requests[best_r].state = RequestState::SERVED;
        slots[s_idx].state = SlotState::ASSIGNED;
        P.erase(find(P.begin(), P.end(), best_r));
        A.erase(find(A.begin(), A.end(), s_idx));
        L.push_back({best_r, s_idx});
        weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[best_r].id) + " assigned to Slot " + to_string(slots[s_idx].id));
      }
    } else {
      auto it = find_if(L.begin(), L.end(), [s_idx](const pair<int, int>& p) { return p.second == s_idx; });
      if (it != L.end()) {
        int r_idx = it->first;
        L.erase(it);
        auto old_t = slots[s_idx].time_block;
        slots[s_idx].time_block = new_t;
        weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Slot " + to_string(slots[s_idx].id) + " rescheduled (assigned): t=" + to_string(old_t.second) + " -> t=" + to_string(new_t.second) + " (Request " + to_string(requests[r_idx].id) + " released)");

        int best_non_s = -1;
        for(int s : A) {
          if (s != s_idx && is_compatible(requests[r_idx], slots[s])) {
            if (best_non_s == -1 || calculate_weight(requests[r_idx], slots[s], current_week) > calculate_weight(requests[r_idx], slots[best_non_s], current_week)) {
              best_non_s = s;
            }
          }
        }
        int best_non_r = -1;
        for(int r : P) {
          if (is_compatible(requests[r], slots[s_idx]) && requests[r].state == RequestState::PENDING) {
            if (best_non_r == -1 || calculate_weight(requests[r], slots[s_idx], current_week) > calculate_weight(requests[best_non_r], slots[s_idx], current_week)) {
              best_non_r = r;
            }
          }
        }
        if (best_non_r != -1 && best_non_s != -1) {
          requests[best_non_r].state = RequestState::SERVED;
          slots[s_idx].state = SlotState::AVAILABLE;
          P.erase(find(P.begin(), P.end(), best_non_r));
          A.erase(find(A.begin(), A.end(), s_idx));
          L.push_back({best_non_r, s_idx});
          L.push_back({r_idx, best_non_s});
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[best_non_r].id) + " assigned to Slot " + to_string(slots[s_idx].id));
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(r_idx) + " assigned to Slot " + to_string(slots[best_non_s].id));
        } else if (best_non_r != -1) {
          requests[best_non_r].state = RequestState::SERVED;
          slots[s_idx].state = SlotState::AVAILABLE;
          P.erase(find(P.begin(), P.end(), best_non_r));
          A.erase(find(A.begin(), A.end(), s_idx));
          L.push_back({best_non_r, s_idx});
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(requests[best_non_r].id) + " assigned to Slot " + to_string(slots[s_idx].id));
        } else if (best_non_s != -1) {
          slots[best_non_s].state = SlotState::AVAILABLE;
          requests[r_idx].state = RequestState::PENDING;
          A.push_back(best_non_s);
          P.push_back(r_idx);
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(r_idx) + " assigned to Slot " + to_string(slots[best_non_s].id));
          weekly_logs.push_back("  [t=" + to_string(t.first) + ", " + to_string(t.second) + "] Request " + to_string(r_idx) + " released from Slot " + to_string(slots[best_non_s].id));
        }
      }
    }
  }

  void finalize_assignment(int r_idx, int s_idx) {
    requests[r_idx].state = RequestState::SERVED;
    
    // Calculate the pure benefit (without the Big Constant) to record
    ll raw_weight = calculate_weight(requests[r_idx], slots[s_idx], current_week);
    total_benefit += (raw_weight - K);
    total_served_requests++;
    
    weekly_logs.push_back("[Week " + to_string(current_week) + "] Assigned Request " + to_string(requests[r_idx].id) + " to Slot " + to_string(slots[s_idx].id));
  }

};

#endif // SIMULATION_H
