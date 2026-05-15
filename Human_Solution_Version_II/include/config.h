#ifndef CONFIG_H
#define CONFIG_H

#include <bits/stdc++.h>

// Using standard namespace
using namespace std;

// Fast I/O
#define FAST_IO ios_base::sync_with_stdio(0); cin.tie(0);

// Typedefs for convenience
typedef long long ll;

// Global Constants
const ll INF = 1e18; // Infinity
const ll K = 1000000000LL; // Big Constant

// Weights
const ll ALPHA_URGENCY = 5;
const ll BETA_TOPIC_MATCH = 2;
const ll GAMMA_PREFERRED_MENTOR = 1;
const ll DELTA_AGE = 2;

// Event Probabilities
const int PROB_REQUEST_CANCEL = 0; // Chance a pending request cancels
const int PROB_SLOT_CANCEL = 0; // Chance an available slot cancels
const int PROB_SLOT_RESCHEDULE = 0; // Chance an available slot reschedules

// Random generator setup
mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

int getRandomPercentage() {
  return uniform_int_distribution<int>(0, 99)(rng);
}

#endif // CONFIG_H
