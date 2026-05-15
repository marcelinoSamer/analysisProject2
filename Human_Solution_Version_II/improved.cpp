#include "include/config.h"
#include "include/models.h"
#include "include/simulation.h"

// Global Data & Compression
unordered_map<int, int> fellow_map;
unordered_map<int, int> mentor_map;
unordered_map<string, int> topic_map;
vector<vector<int>> mentor_specialties; // mapped by compressed mentor ID

int get_topic_id(const string& t) {
  if (topic_map.find(t) == topic_map.end()) {
    topic_map[t] = topic_map.size() + 1;
  }
  return topic_map[t];
}

int parse_time_block(const string& s) {
  if (int(s.length()) < 4) {
    throw runtime_error("Invalid time block format: " + s);
  }
  string day = s.substr(0, 3);
  int hour = 0;
  try {
    hour = stoi(s.substr(3));
  } catch (...) {
    throw runtime_error("Invalid hour format: " + s.substr(3));
  }
  
  int d = 0;
  if (day == "Mon") d = 1;
  else if (day == "Tue") d = 2;
  else if (day == "Wed") d = 3;
  else if (day == "Thu") d = 4;
  else if (day == "Fri") d = 5;
  else if (day == "Sat") d = 6;
  else if (day == "Sun") d = 7;
  return d * 100 + hour;
}

vector<int> parse_topics(const string& s) {
  vector<int> res;
  if (s == "\"\"" || s == "") return res;
  stringstream ss(s);
  string token;
  while(getline(ss, token, ',')) {
    res.push_back(get_topic_id(token));
  }
  return res;
}

vector<int> parse_times(const string& s) {
  vector<int> res;
  if (s == "\"\"" || s == "") return res;
  stringstream ss(s);
  string token;
  while(getline(ss, token, ',')) {
    res.push_back(parse_time_block(token));
  }
  return res;
}

void solve() {
  string line;
  // Read Fellows
  if (!getline(cin, line)) {
    throw runtime_error("Failed to read fellows");
  }
  stringstream f_ss(line);
  int f_id;
  int f_idx = 1;
  while (f_ss >> f_id) {
    fellow_map[f_id] = f_idx++;
  }

  // Read Mentors
  if (!getline(cin, line)) {
    throw runtime_error("Failed to read mentors");
  }
  stringstream m_ss(line);
  int m_id;
  int m_idx = 1;
  while (m_ss >> m_id) {
    mentor_map[m_id] = m_idx++;
  }
  mentor_specialties.resize(int(mentor_map.size()) + 1);

  // Mentor Specialties
  for (int i = 0; i < int(mentor_map.size()); ++i) {
    getline(cin, line);
    stringstream ss(line);
    int m_raw;
    ss >> m_raw;
    int m_c = mentor_map[m_raw];
    string topic;
    while (ss >> topic) {
      mentor_specialties[m_c].push_back(get_topic_id(topic));
    }
  }

  vector<Request> global_requests;
  vector<Slot> global_slots;
  
  Simulation sim(global_requests, global_slots);

  // Read Weeks
  while (getline(cin, line)) {
    if (line.empty()) continue;
    if (line.substr(0, 4) == "WEEK") {
      int w;
      stringstream ss(line);
      string temp;
      ss >> temp >> w;
      
      // Read Slots
      getline(cin, line);
      int S = stoi(line);
      
      vector<int> week_slot_ids;
      if (S > 0) {
        getline(cin, line);
        stringstream s_ss(line);
        string token;
        for (int i = 0; i < S; ++i) {
          s_ss >> token;
          int colon_pos = int(token.find(':'));
          int m_raw = stoi(token.substr(0, colon_pos));
          string time_str = token.substr(colon_pos + 1);
          
          Slot s_obj;
          s_obj.id = int(global_slots.size());
          s_obj.mentor_id = mentor_map[m_raw];
          s_obj.mentor_topics = mentor_specialties[s_obj.mentor_id];
          s_obj.time_block = parse_time_block(time_str);
          
          week_slot_ids.push_back(s_obj.id);
          global_slots.push_back(s_obj);
        }
      }
      
      // Read Requests
      getline(cin, line);
      int R = stoi(line);
      
      vector<int> week_req_ids;
      for (int i = 0; i < R; ++i) {
        getline(cin, line);
        stringstream r_ss(line);
        int f_raw;
        string t_list, times_list, urg_str;
        int pref_m_raw;
        
        r_ss >> f_raw >> t_list >> times_list >> urg_str >> pref_m_raw;
        
        Request r_obj;
        r_obj.id = int(global_requests.size());
        r_obj.fellow_id = fellow_map[f_raw];
        r_obj.required_topics = parse_topics(t_list);
        r_obj.acceptable_times = parse_times(times_list);
        r_obj.submission_week = w;
        
        if (urg_str == "high") r_obj.urgency = Urgency::HIGH;
        else if (urg_str == "medium") r_obj.urgency = Urgency::MEDIUM;
        else r_obj.urgency = Urgency::LOW;
        
        if (pref_m_raw > 0) {
          r_obj.preferred_mentor = mentor_map[pref_m_raw];
        } else {
          r_obj.preferred_mentor = 0;
        }
        
        week_req_ids.push_back(r_obj.id);
        global_requests.push_back(r_obj);
        sim.unserved_reasons.push_back(""); // Match size
      }
      
      // Process the week
      sim.process_week(w, week_req_ids, week_slot_ids);
      
      // Print Weekly Assignments
      bool has_assignments = false;
      for (const string& log : sim.weekly_logs) {
        if (log.find("Assigned") != string::npos) {
          has_assignments = true;
          break;
        }
      }
      if (has_assignments) {
        cout << "\n[Week " << w << "] Assignments:\n";
        for (const string& log : sim.weekly_logs) {
          if (log.find("Assigned") != string::npos) cout << log << "\n";
        }
      }
      
      // Print Event Timeline
      bool has_events = false;
      for (const string& log : sim.weekly_logs) {
        if (log.find("[t=") != string::npos) {
          has_events = true;
          break;
        }
      }
      if (has_events) {
        cout << "[Week " << w << "] Event Timeline:\n";
        for (const string& log : sim.weekly_logs) {
          if (log.find("[t=") != string::npos) cout << log << "\n";
        }
      }
      
      sim.weekly_logs.clear();
    }
  }
  
  // Print Evaluation
  cout << "Total Served Requests: " << sim.total_served_requests << "\n";
  cout << "Total Benefit: " << sim.total_benefit << "\n";
  
  // Output unserved requests
  cout << "\nUnserved Requests Breakdown:\n";
  for (size_t i = 0; i < global_requests.size(); ++i) {
    if (global_requests[i].state != RequestState::SERVED) {
      string reason = sim.unserved_reasons[i];
      if (reason.empty()) {
        // If no reason was explicitly set by a dynamic event or expiration,
        // and it's still pending at the end of the program
        reason = "lower priority"; 
      }
      cout << "Request " << i + 1 << " (Week " << global_requests[i].submission_week << "): " << reason << "\n";
    }
  }
}

int main() {
  FAST_IO;
  solve();
  return 0;
}
