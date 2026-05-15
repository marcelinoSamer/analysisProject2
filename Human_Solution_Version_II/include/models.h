#ifndef MODELS_H
#define MODELS_H

#include "config.h"

enum class Urgency {
  LOW = 1,
  MEDIUM = 2,
  HIGH = 3
};

enum class RequestState {
  PENDING,
  SERVED,
  CANCELED
};

enum class SlotState {
  AVAILABLE,
  ASSIGNED,
  CANCELED
};

struct Request {
  int id;
  int fellow_id;
  vector<int> required_topics;
  vector<int> acceptable_times;
  Urgency urgency;
  int preferred_mentor;
  int submission_week;
  
  // State tracking
  RequestState state = RequestState::PENDING;
  
  int age(int current_week) const {
    return current_week - submission_week;
  }
};

struct Slot {
  int id;
  int mentor_id;
  vector<int> mentor_topics;
  int time_block;
  
  // State tracking
  SlotState state = SlotState::AVAILABLE;
};

#endif // MODELS_H
