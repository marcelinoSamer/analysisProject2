#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>

using namespace std;

typedef long long ll;
const ll INF = 1e18;

// Weights
const ll ALPHA_URGENCY = 5;
const ll BETA_TOPIC_MATCH = 2;
const ll GAMMA_PREFERRED_MENTOR = 1;
const ll DELTA_AGE = 2;

enum class Urgency { LOW, MEDIUM, HIGH };

struct Request {
  int id;
  int fellow_id;
  vector<string> required_topics;
  vector<string> acceptable_times;
  Urgency urgency;
  int preferred_mentor;
  int submission_week;
  
  bool is_served = false;
  string unserved_reason = "";
  
  int age(int current_week) const {
    return current_week - submission_week;
  }
};

struct Slot {
  int id;
  int mentor_id;
  string time_block;
  vector<string> mentor_topics;
  
  bool is_assigned = false;
};

// Calculate topic match score
int get_topic_match(const Request& r, const Slot& s) {
  int score = 0;
  for (const string& t1 : r.required_topics) {
    for (const string& t2 : s.mentor_topics) {
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
  for (const string& t : r.acceptable_times) {
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

ll calculate_weight(const Request& r, const Slot& s, int current_week) {
  if (!is_compatible(r, s)) return -INF; 
  
  ll u = 0;
  if (r.urgency == Urgency::HIGH) u = 3;
  else if (r.urgency == Urgency::MEDIUM) u = 2;
  else u = 1;
  
  ll topic_score = get_topic_match(r, s);
  ll p = (r.preferred_mentor != 0 && r.preferred_mentor == s.mentor_id) ? 1 : 0;
  ll age = r.age(current_week);
  
  ll benefit = ALPHA_URGENCY * u + BETA_TOPIC_MATCH * topic_score + GAMMA_PREFERRED_MENTOR * p + DELTA_AGE * age;
  return benefit;
}


void solve() {
  string line;
  if (!getline(cin, line)) return;
  
  // Read Fellows
  stringstream ss_f(line);
  int f;
  while (ss_f >> f) {}
  
  // Read Mentors
  getline(cin, line);
  stringstream ss_m(line);
  int m_id;
  map<int, vector<string>> mentor_topics;
  vector<int> mentors;
  while (ss_m >> m_id) {
    mentors.push_back(m_id);
  }
  
  // Mentor Specialties
  for (int i = 0; i < int(mentors.size()); ++i) {
    getline(cin, line);
    stringstream ss(line);
    int m_raw;
    ss >> m_raw;
    string topic;
    while (ss >> topic) {
      mentor_topics[m_raw].push_back(topic);
    }
  }

  vector<Request> global_requests;
  vector<Slot> global_slots;
  
  ll total_served_requests = 0;
  ll total_benefit = 0;

  // Read Weeks
  string token;
  while (cin >> token) {
    if (token == "WEEK") {
      int w;
      cin >> w;
      
      // Read Slots
      int S;
      cin >> S;
      
      vector<int> week_slot_ids;
      for (int i = 0; i < S; ++i) {
        int s_id, mentor;
        string time_b;
        cin >> s_id >> mentor >> time_b;
        
        Slot s_obj;
        s_obj.id = s_id;
        s_obj.mentor_id = mentor;
        s_obj.time_block = time_b;
        s_obj.mentor_topics = mentor_topics[mentor];
        s_obj.is_assigned = false;
        
        week_slot_ids.push_back(global_slots.size());
        global_slots.push_back(s_obj);
      }
      
      // Read Requests
      int R;
      cin >> R;
      
      vector<int> week_req_ids;
      for (int i = 0; i < R; ++i) {
        int r_id, f_id, pref_m;
        string req_topics, acc_times, urg_str;
        
        cin >> r_id >> f_id >> req_topics >> acc_times >> urg_str >> pref_m;
        
        Request r_obj;
        r_obj.id = r_id;
        r_obj.fellow_id = f_id;
        
        if (req_topics != "\"\"") {
          stringstream topics_stream(req_topics);
          string t;
          while (getline(topics_stream, t, ',')) r_obj.required_topics.push_back(t);
        }
        
        stringstream times_stream(acc_times);
        string t_time;
        while (getline(times_stream, t_time, ',')) r_obj.acceptable_times.push_back(t_time);
        
        if (urg_str == "high") r_obj.urgency = Urgency::HIGH;
        else if (urg_str == "medium") r_obj.urgency = Urgency::MEDIUM;
        else r_obj.urgency = Urgency::LOW;
        
        r_obj.preferred_mentor = pref_m;
        r_obj.submission_week = w;
        
        week_req_ids.push_back(int(global_requests.size()));
        global_requests.push_back(r_obj);
      }
      
      // Read and ignore dynamic events
      int C;
      cin >> C;
      for (int i = 0; i < C; ++i) {
        string dummy;
        cin >> dummy;
      }
      
      int U;
      cin >> U;
      for (int i = 0; i < U; ++i) {
        string id, t1, t2;
        cin >> id >> t1 >> t2;
      }
      
      int Q;
      cin >> Q;
      for (int i = 0; i < Q; ++i) {
        string dummy;
        cin >> dummy;
      }
      
      // Collect Pending Requests and Available Slots
      vector<int> P;
      vector<int> A;
      for (size_t i = 0; i < global_requests.size(); ++i) {
        if (!global_requests[i].is_served) P.push_back(i);
      }
      for (size_t i = 0; i < global_slots.size(); ++i) {
        if (!global_slots[i].is_assigned) A.push_back(i);
      }
      
      int n = int(P.size());
      int m = int(A.size());
      
      if (n > 0 && m > 0) {
        // Greedy heuristic: build all compatible pairs, sort by weight desc, assign greedily
        struct Edge { ll weight; int req_idx; int slot_idx; };
        vector<Edge> edges;
        edges.reserve(n * m);
        
        for (int i = 0; i < n; ++i) {
          for (int j = 0; j < m; ++j) {
            ll edge_w = calculate_weight(global_requests[P[i]], global_slots[A[j]], w);
            if (edge_w != -INF) {
              edges.push_back({edge_w, i, j});
            }
          }
        }
        
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
          return a.weight > b.weight;
        });
        
        vector<bool> req_used(n, false);
        vector<bool> slot_used(m, false);
        
        for (auto& e : edges) {
          if (!req_used[e.req_idx] && !slot_used[e.slot_idx]) {
            req_used[e.req_idx] = true;
            slot_used[e.slot_idx] = true;
            
            global_requests[P[e.req_idx]].is_served = true;
            global_slots[A[e.slot_idx]].is_assigned = true;
            
            ll raw_weight = calculate_weight(global_requests[P[e.req_idx]], global_slots[A[e.slot_idx]], w);
            total_benefit += (raw_weight);
            total_served_requests++;
            
            cout << "[Week " << w << "] Assigned Request " << global_requests[P[e.req_idx]].id
                 << " to Slot " << global_slots[A[e.slot_idx]].id << "\n";
          }
        }
        
        // Label unserved requests
        for (int i = 0; i < n; ++i) {
          if (!req_used[i]) {
            bool feasible = false;
            for (int j = 0; j < m; ++j) {
              if (calculate_weight(global_requests[P[i]], global_slots[A[j]], w) != -INF) {
                feasible = true;
                break;
              }
            }
            if (!feasible && global_requests[P[i]].unserved_reason == "") {
              global_requests[P[i]].unserved_reason = "no feasible slot";
            } else if (feasible) {
              global_requests[P[i]].unserved_reason = "lower priority";
            }
          }
        }
      } else {
        // Evaluate for reasons if m == 0
        for (int i = 0; i < n; ++i) {
          if (global_requests[P[i]].unserved_reason == "") {
             global_requests[P[i]].unserved_reason = "no feasible slot";
          }
        }
      }
    }
  }
  
  // Print Evaluation
  cout << "\nTotal Served Requests: " << total_served_requests << "\n";
  cout << "Total Benefit: " << total_benefit << "\n";
  
  // Output unserved requests
  cout << "\nUnserved Requests Breakdown:\n";
  for (size_t i = 0; i < global_requests.size(); ++i) {
    if (!global_requests[i].is_served) {
      string reason = global_requests[i].unserved_reason;
      if (reason.empty()) {
        reason = "lower priority"; 
      }
      cout << "Request " << global_requests[i].id << " (Week " << global_requests[i].submission_week << "): " << reason << "\n";
    }
  }
}

int main() {
  ios_base::sync_with_stdio(0); cin.tie(0);
  solve();
  return 0;
}
