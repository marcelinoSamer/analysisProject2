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

int main() {
  ios::sync_with_stdio(0); cin.tie(0);
  
  // Read the number of fellows, mentors, slots, and requests
  int F, M, S, R;
  cin >> F >> M >> S >> R;

  // Read fellows
  vector<Fellow> fellows(F);
  for (int i = 0; i < F; i++) {
    cin >> fellows[i].id;
  }

  // Read mentors and their specialties
  vector<Mentor> mentors(M);
  for (int i = 0; i < M; i++) {
    // Read mentor ID
    cin >> mentors[i].id;
    int num_specialties;

    // Read the number of specialties and the specialties themselves
    cin >> num_specialties;
    mentors[i].specialties.resize(num_specialties);
    mentors[i].speciality_ids.resize(num_specialties, -1);
    for (int j = 0; j < num_specialties; j++) {
      cin >> mentors[i].specialties[j];
    }
  }

  // Read slots
  vector<Slot> slots(S);
  for (int i = 0; i < S; i++) {
    cin >> slots[i].mentor_id >> slots[i].week;
  }

  // Read requests
  vector<Request> requests(R);
  for (int i = 0; i < R; i++) {
    // Read fellow ID for the request
    cin >> requests[i].fellow_id;

    // Read the number of available slots and the slot IDs
    int num_available_slots;
    cin >> num_available_slots;
    requests[i].available_slot_ids.resize(num_available_slots);
    for (int j = 0; j < num_available_slots; j++) {
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
  for (auto &mentor : mentors) {
    for (auto &specialty : mentor.specialties) {
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