#pragma once

// Required libraries and headers
#include <bits/stdc++.h>
#include "./enums.h"

// Using standard namespace for convenience
using namespace std;

// Structure definitions for Fellows, Mentors, Slots, and Requests
struct Fellow {
  int id;
};

struct Mentor {
  int id;
  vector<string> specialties;
  vector<int> speciality_ids;
};

struct Slot {
  int mentor_id;
  int week;
};

struct Request {
  int fellow_id;
  string requested_topic_str;
  int requested_topic_id;
  Urgency urgency;
  int week;
  vector<int> available_slot_ids;
};