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
  
  // Weekly Logs
  vector<string> weekly_logs;
  
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


    // Check if there are no matching slots for any request
    for(auto &p : P) {
      bool has_no_matching_slot = true;
      for(auto &s : A) {
        if (is_compatible(requests[p], slots[s])) {
          has_no_matching_slot = false;
          break;
        }
      }
      if(has_no_matching_slot) {
        requests[p].state = RequestState::CANCELED;
        unserved_reasons[p] = "no feasible slot";
      }
    }

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
    
    weekly_logs.push_back("[Week " + to_string(current_week) + "] Assigned Request " + to_string(r_idx + 1) + " to Slot " + to_string(s_idx + 1));
  }
  
  void expire_request(int r_idx) {
    requests[r_idx].state = RequestState::CANCELED;
    unserved_reasons[r_idx] = "no feasible slot"; 
  }

  
  void inject_and_process_events(int current_time_block) {
    // Process P Cancellations
    for (auto it = P.begin(); it != P.end(); ) {
      if (getRandomPercentage() < PROB_REQUEST_CANCEL) {
        requests[*it].state = RequestState::CANCELED;
        unserved_reasons[*it] = "request canceled";
        weekly_logs.push_back("  [t=" + to_string(current_time_block) + "] Request " + to_string(*it + 1) + " canceled (pending)");
        it = P.erase(it);
      } else {
        ++it;
      }
    }

    // Process A Cancellations & Reschedules
    for (auto it = A.begin(); it != A.end(); ) {
      if (slots[*it].time_block >= current_time_block) {
        int r = getRandomPercentage();
        if (r < PROB_SLOT_CANCEL) {
          slots[*it].state = SlotState::CANCELED;
          weekly_logs.push_back("  [t=" + to_string(current_time_block) + "] Slot " + to_string(*it + 1) + " canceled (available)");
          it = A.erase(it);
          continue;
        } else if (r < PROB_SLOT_RESCHEDULE) {
          int old_t = slots[*it].time_block;
          slots[*it].time_block = min(100 * 7 + 23, slots[*it].time_block + getRandomDelay());
          slots[*it].state = SlotState::AVAILABLE;
          weekly_logs.push_back("  [t=" + to_string(current_time_block) + "] Slot " + to_string(*it + 1) + " rescheduled (available): t=" + to_string(old_t) + " -> t=" + to_string(slots[*it].time_block));
          // Slot stays in A (time updated in-place; iterator already valid)
          ++it;
          continue;
        }
      }
      ++it;
    }

    // Process L Breakages — push freed resources directly back into the pools
    vector<pair<int, int>> next_L;
    for (auto& pair : L) {
      if (slots[pair.second].time_block >= current_time_block) {
        int r1 = getRandomPercentage();
        if (r1 < PROB_SLOT_CANCEL) {
          // Slot Cancellation: orphaned request goes back to P
          slots[pair.second].state = SlotState::CANCELED;
          requests[pair.first].state = RequestState::PENDING;
          P.push_back(pair.first);
          weekly_logs.push_back("  [t=" + to_string(current_time_block) + "] Slot " + to_string(pair.second + 1) + " canceled (breaking assignment with Request " + to_string(pair.first + 1) + ")");
          continue;
        }

        int r2 = getRandomPercentage();
        if (r2 < PROB_REQUEST_CANCEL) {
          // Request Cancellation: freed slot goes back to A
          requests[pair.first].state = RequestState::CANCELED;
          unserved_reasons[pair.first] = "request canceled";
          slots[pair.second].state = SlotState::AVAILABLE;
          A.push_back(pair.second);
          weekly_logs.push_back("  [t=" + to_string(current_time_block) + "] Request " + to_string(pair.first + 1) + " canceled (breaking assignment with Slot " + to_string(pair.second + 1) + ")");
          continue;
        }

        int r3 = getRandomPercentage();
        if (r3 < PROB_SLOT_RESCHEDULE) {
          // Slot Rescheduling: both resources freed
          int old_t = slots[pair.second].time_block;
          slots[pair.second].time_block = min(100 * 7 + 23, slots[pair.second].time_block + getRandomDelay());
          slots[pair.second].state = SlotState::AVAILABLE;
          requests[pair.first].state = RequestState::PENDING;
          A.push_back(pair.second);
          P.push_back(pair.first);
          weekly_logs.push_back("  [t=" + to_string(current_time_block) + "] Slot " + to_string(pair.second + 1) + " rescheduled (assigned): t=" + to_string(old_t) + " -> t=" + to_string(slots[pair.second].time_block) + " (Request " + to_string(pair.first + 1) + " released)");
          continue;
        }
      }
      next_L.push_back(pair);
    }
    L = next_L;

    // Rerun the matching algorithm on the updated pools
    run_core_matching();
  }
};

#endif // SIMULATION_H
