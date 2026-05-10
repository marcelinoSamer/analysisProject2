#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <limits>
#include <cmath>
#include <climits>
#include <utility>

using namespace std;

struct Mentor {
    string id;
    unordered_set<string> specialties;
};

struct Slot {
    string id;
    string mentorId;
    string timeBlock;
};

struct Request {
    string fellowId;
    vector<string> availableSlotIds;
    string requestedTopic;
    string urgency;
    long long timestamp;
};

struct WeeklyResult {
    unordered_map<string, string> assignment;      // fellow_id -> slot_id
    unordered_map<string, string> classification;  // fellow_id -> reason if unassigned
    long long weeklyPenalty = 0;
};

int urgencyWeight(const string& urgency) {
    if (urgency == "blocker") return 3;
    if (urgency == "normal") return 2;
    if (urgency == "exploratory") return 1;

    return 0;
}

int urgencyRank(const string& urgency) {
    if (urgency == "blocker") return 3;
    if (urgency == "normal") return 2;
    if (urgency == "exploratory") return 1;

    return 0;
}

long long skippedPenalty(const Request& request, long long starvationCount) {
    long long w = urgencyWeight(request.urgency);
    long long nextStarvation = starvationCount + 1;

    return w * nextStarvation * nextStarvation;
}

class ScarcityAwareGreedySolver {
private:
    vector<string> fellowIds;
    unordered_map<string, long long> starvation;
    unordered_map<string, Mentor> mentors;

public:
    ScarcityAwareGreedySolver(
        const vector<string>& fellowIds_,
        const unordered_map<string, Mentor>& mentors_
    ) {
        fellowIds = fellowIds_;
        mentors = mentors_;

        for (const string& fellowId : fellowIds) {
            starvation[fellowId] = 0;
        }
    }

    WeeklyResult solveWeek(
        const vector<Slot>& slots,
        const vector<Request>& requests
    ) {
        WeeklyResult result;

        unordered_map<string, Slot> slotById;
        unordered_set<string> freeSlots;

        for (const Slot& slot : slots) {
            slotById[slot.id] = slot;
            freeSlots.insert(slot.id);
        }

        int R = static_cast<int>(requests.size());

        vector<unordered_set<string>> compatibleSlots(R);
        vector<long long> penalty(R, 0);
        vector<bool> infeasible(R, false);
        vector<bool> assigned(R, false);
        vector<string> assignedSlot(R, "");

        for (int i = 0; i < R; i++) {
            const Request& req = requests[i];

            for (const string& slotId : req.availableSlotIds) {
                if (slotById.find(slotId) == slotById.end()) {
                    continue;
                }

                const Slot& slot = slotById[slotId];

                if (mentors.find(slot.mentorId) == mentors.end()) {
                    continue;
                }

                const Mentor& mentor = mentors[slot.mentorId];

                if (mentor.specialties.find(req.requestedTopic) != mentor.specialties.end()) {
                    compatibleSlots[i].insert(slotId);
                }
            }

            if (compatibleSlots[i].empty()) {
                infeasible[i] = true;
            }

            penalty[i] = skippedPenalty(req, starvation[req.fellowId]);
        }

        auto currentFreeCompatibleCount = [&](int i) -> int {
            int count = 0;

            for (const string& slotId : compatibleSlots[i]) {
                if (freeSlots.find(slotId) != freeSlots.end()) {
                    count++;
                }
            }

            return count;
        };

        auto betterFellow = [&](int a, int b, const vector<int>& k) -> bool {
            long long Pa = penalty[a];
            long long Pb = penalty[b];

            long long ka = k[a];
            long long kb = k[b];

            /*
                Compare:
                    Pa / ka > Pb / kb

                Instead of using floating point, compare:
                    Pa * kb > Pb * ka
            */
            long double left = static_cast<long double>(Pa) * kb;
            long double right = static_cast<long double>(Pb) * ka;

            if (left != right) {
                return left > right;
            }

            if (Pa != Pb) {
                return Pa > Pb;
            }

            int ua = urgencyRank(requests[a].urgency);
            int ub = urgencyRank(requests[b].urgency);

            if (ua != ub) {
                return ua > ub;
            }

            long long sa = starvation[requests[a].fellowId];
            long long sb = starvation[requests[b].fellowId];

            if (sa != sb) {
                return sa > sb;
            }

            if (requests[a].timestamp != requests[b].timestamp) {
                return requests[a].timestamp < requests[b].timestamp;
            }

            return requests[a].fellowId < requests[b].fellowId;
        };

        auto chooseLeastDamagingSlot = [&](int selected, const vector<int>& k) -> string {
            string bestSlot = "";
            long double bestHarm = numeric_limits<long double>::infinity();
            int bestClaimants = INT_MAX;

            for (const string& slotId : compatibleSlots[selected]) {
                if (freeSlots.find(slotId) == freeSlots.end()) {
                    continue;
                }

                long double harm = 0.0L;
                int claimants = 0;

                for (int j = 0; j < R; j++) {
                    if (j == selected) {
                        continue;
                    }

                    if (assigned[j] || infeasible[j]) {
                        continue;
                    }

                    if (k[j] == 0) {
                        continue;
                    }

                    if (compatibleSlots[j].find(slotId) != compatibleSlots[j].end()) {
                        harm += static_cast<long double>(penalty[j]) / k[j];
                        claimants++;
                    }
                }

                bool better = false;

                if (harm < bestHarm) {
                    better = true;
                } else if (fabsl(harm - bestHarm) <= 1e-18L) {
                    if (claimants < bestClaimants) {
                        better = true;
                    } else if (claimants == bestClaimants) {
                        if (bestSlot.empty() || slotId < bestSlot) {
                            better = true;
                        }
                    }
                }

                if (better) {
                    bestHarm = harm;
                    bestClaimants = claimants;
                    bestSlot = slotId;
                }
            }

            return bestSlot;
        };

        while (true) {
            vector<int> k(R, 0);

            for (int i = 0; i < R; i++) {
                if (!assigned[i] && !infeasible[i]) {
                    k[i] = currentFreeCompatibleCount(i);
                }
            }

            int best = -1;

            for (int i = 0; i < R; i++) {
                if (assigned[i] || infeasible[i]) {
                    continue;
                }

                if (k[i] == 0) {
                    continue;
                }

                if (best == -1 || betterFellow(i, best, k)) {
                    best = i;
                }
            }

            if (best == -1) {
                break;
            }

            string slotToUse = chooseLeastDamagingSlot(best, k);

            if (slotToUse.empty()) {
                break;
            }

            assigned[best] = true;
            assignedSlot[best] = slotToUse;
            freeSlots.erase(slotToUse);

            result.assignment[requests[best].fellowId] = slotToUse;
        }

        unordered_set<string> assignedFellows;

        for (int i = 0; i < R; i++) {
            const string& fellowId = requests[i].fellowId;

            if (assigned[i]) {
                assignedFellows.insert(fellowId);
                continue;
            }

            result.weeklyPenalty += penalty[i];

            if (compatibleSlots[i].empty()) {
                result.classification[fellowId] = "infeasibly_unserved";
            } else {
                bool hasFreeCompatibleSlot = false;

                for (const string& slotId : compatibleSlots[i]) {
                    if (freeSlots.find(slotId) != freeSlots.end()) {
                        hasFreeCompatibleSlot = true;
                        break;
                    }
                }

                if (hasFreeCompatibleSlot) {
                    result.classification[fellowId] = "algorithm_failure";
                } else {
                    result.classification[fellowId] = "competitively_unserved";
                }
            }
        }

        for (const string& fellowId : fellowIds) {
            if (assignedFellows.find(fellowId) != assignedFellows.end()) {
                starvation[fellowId] = 0;
            } else {
                starvation[fellowId]++;
            }
        }

        return result;
    }

    long long getStarvation(const string& fellowId) const {
        auto it = starvation.find(fellowId);

        if (it == starvation.end()) {
            return 0;
        }

        return it->second;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int F, M, T;
    cin >> F >> M >> T;

    vector<string> fellowIds(F);

    for (int i = 0; i < F; i++) {
        cin >> fellowIds[i];
    }

    unordered_map<string, Mentor> mentors;

    for (int i = 0; i < M; i++) {
        string mentorId;
        int K;

        cin >> mentorId >> K;

        Mentor mentor;
        mentor.id = mentorId;

        for (int j = 0; j < K; j++) {
            string topic;
            cin >> topic;
            mentor.specialties.insert(topic);
        }

        mentors[mentorId] = mentor;
    }

    ScarcityAwareGreedySolver solver(fellowIds, mentors);

    long long totalPenalty = 0;

    for (int week = 1; week <= T; week++) {
        int S;
        cin >> S;

        vector<Slot> slots(S);

        for (int i = 0; i < S; i++) {
            cin >> slots[i].id >> slots[i].mentorId >> slots[i].timeBlock;
        }

        int R;
        cin >> R;

        vector<Request> requests(R);

        for (int i = 0; i < R; i++) {
            int A;
            cin >> requests[i].fellowId >> A;

            requests[i].availableSlotIds.resize(A);

            for (int j = 0; j < A; j++) {
                cin >> requests[i].availableSlotIds[j];
            }

            cin >> requests[i].requestedTopic
                >> requests[i].urgency
                >> requests[i].timestamp;
        }

        WeeklyResult result = solver.solveWeek(slots, requests);

        totalPenalty += result.weeklyPenalty;

        cout << "Week " << week << "\n";

        cout << "Assignments:\n";

        if (result.assignment.empty()) {
            cout << "None\n";
        } else {
            vector<pair<string, string>> assignments(
                result.assignment.begin(),
                result.assignment.end()
            );

            sort(assignments.begin(), assignments.end());

            for (const pair<string, string>& assignment : assignments) {
                cout << assignment.first << " -> " << assignment.second << "\n";
            }
        }

        cout << "Unserved:\n";

        if (result.classification.empty()) {
            cout << "None\n";
        } else {
            vector<pair<string, string>> unserved(
                result.classification.begin(),
                result.classification.end()
            );

            sort(unserved.begin(), unserved.end());

            for (const pair<string, string>& item : unserved) {
                cout << item.first << " : " << item.second << "\n";
            }
        }

        cout << "Penalty_week = " << result.weeklyPenalty << "\n";
        cout << "Penalty_total_so_far = " << totalPenalty << "\n";
        cout << "\n";
    }

    cout << "Final Penalty_total = " << totalPenalty << "\n";

    return 0;
}