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

            long double left = static_cast<long double>(Pa) * static_cast<long double>(kb);
            long double right = static_cast<long double>(Pb) * static_cast<long double>(ka);

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
                        harm += static_cast<long double>(penalty[j]) / static_cast<long double>(k[j]);
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
        unordered_map<string, long long>::const_iterator it = starvation.find(fellowId);

        if (it == starvation.end()) {
            return 0;
        }

        return it->second;
    }
};

struct TestRunner {
    int passed = 0;
    int failed = 0;

    void expect(bool condition, const string& message) {
        if (condition) {
            passed++;
            cout << "[PASS] " << message << "\n";
        } else {
            failed++;
            cout << "[FAIL] " << message << "\n";
        }
    }

    void summary() const {
        cout << "\n==============================\n";
        cout << "Tests passed: " << passed << "\n";
        cout << "Tests failed: " << failed << "\n";
        cout << "==============================\n";
    }
};

Mentor makeMentor(const string& id, const vector<string>& topics) {
    Mentor mentor;
    mentor.id = id;

    for (const string& topic : topics) {
        mentor.specialties.insert(topic);
    }

    return mentor;
}

Slot makeSlot(const string& id, const string& mentorId, const string& timeBlock) {
    Slot slot;
    slot.id = id;
    slot.mentorId = mentorId;
    slot.timeBlock = timeBlock;

    return slot;
}

Request makeRequest(
    const string& fellowId,
    const vector<string>& availableSlots,
    const string& topic,
    const string& urgency,
    long long timestamp
) {
    Request request;
    request.fellowId = fellowId;
    request.availableSlotIds = availableSlots;
    request.requestedTopic = topic;
    request.urgency = urgency;
    request.timestamp = timestamp;

    return request;
}

unordered_map<string, Mentor> makeMentors(const vector<Mentor>& mentorList) {
    unordered_map<string, Mentor> mentors;

    for (const Mentor& mentor : mentorList) {
        mentors[mentor.id] = mentor;
    }

    return mentors;
}

bool hasAssignment(const WeeklyResult& result, const string& fellowId) {
    return result.assignment.find(fellowId) != result.assignment.end();
}

string assignmentOf(const WeeklyResult& result, const string& fellowId) {
    unordered_map<string, string>::const_iterator it = result.assignment.find(fellowId);

    if (it == result.assignment.end()) {
        return "";
    }

    return it->second;
}

string classificationOf(const WeeklyResult& result, const string& fellowId) {
    unordered_map<string, string>::const_iterator it = result.classification.find(fellowId);

    if (it == result.classification.end()) {
        return "";
    }

    return it->second;
}

bool hasNoAlgorithmFailure(const WeeklyResult& result) {
    for (
        unordered_map<string, string>::const_iterator it = result.classification.begin();
        it != result.classification.end();
        ++it
    ) {
        if (it->second == "algorithm_failure") {
            return false;
        }
    }

    return true;
}

void testAllFeasibleAssigned(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"}),
        makeMentor("M2", vector<string>{"ai"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"F1", "F2"}, mentors);

    vector<Slot> slots;
    slots.push_back(makeSlot("S1", "M1", "Mon9"));
    slots.push_back(makeSlot("S2", "M2", "Tue9"));

    vector<Request> requests;
    requests.push_back(makeRequest("F1", vector<string>{"S1"}, "cpp", "blocker", 10));
    requests.push_back(makeRequest("F2", vector<string>{"S2"}, "ai", "normal", 20));

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(assignmentOf(result, "F1") == "S1", "all feasible: F1 gets S1");
    test.expect(assignmentOf(result, "F2") == "S2", "all feasible: F2 gets S2");
    test.expect(result.classification.empty(), "all feasible: no unserved fellows");
    test.expect(result.weeklyPenalty == 0, "all feasible: weekly penalty is 0");
    test.expect(solver.getStarvation("F1") == 0, "all feasible: F1 starvation resets to 0");
    test.expect(solver.getStarvation("F2") == 0, "all feasible: F2 starvation resets to 0");
}

void testInfeasibleRequests(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"}),
        makeMentor("M2", vector<string>{"ai"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"F1", "F2", "F3"}, mentors);

    vector<Slot> slots;
    slots.push_back(makeSlot("S1", "M1", "Mon9"));
    slots.push_back(makeSlot("S2", "M2", "Tue9"));

    vector<Request> requests;
    requests.push_back(makeRequest("F1", vector<string>{"S1"}, "ai", "normal", 10));
    requests.push_back(makeRequest("F2", vector<string>{"Missing"}, "cpp", "blocker", 20));
    requests.push_back(makeRequest("F3", vector<string>{"S1"}, "security", "exploratory", 30));

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(result.assignment.empty(), "infeasible: no assignments are produced");
    test.expect(classificationOf(result, "F1") == "infeasibly_unserved", "infeasible: F1 topic mismatch classified correctly");
    test.expect(classificationOf(result, "F2") == "infeasibly_unserved", "infeasible: F2 missing slot classified correctly");
    test.expect(classificationOf(result, "F3") == "infeasibly_unserved", "infeasible: F3 uncovered topic classified correctly");
    test.expect(result.weeklyPenalty == 6, "infeasible: penalty is 2 + 3 + 1 = 6");
    test.expect(hasNoAlgorithmFailure(result), "infeasible: no algorithm failure is reported");
}

void testTopicBottleneckCapacity(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"F1", "F2", "F3"}, mentors);

    vector<Slot> slots;
    slots.push_back(makeSlot("S1", "M1", "Mon9"));
    slots.push_back(makeSlot("S2", "M1", "Tue9"));

    vector<Request> requests;
    requests.push_back(makeRequest("F1", vector<string>{"S1", "S2"}, "cpp", "blocker", 10));
    requests.push_back(makeRequest("F2", vector<string>{"S1", "S2"}, "cpp", "normal", 20));
    requests.push_back(makeRequest("F3", vector<string>{"S1", "S2"}, "cpp", "exploratory", 30));

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(hasAssignment(result, "F1"), "bottleneck: highest penalty fellow F1 is assigned");
    test.expect(hasAssignment(result, "F2"), "bottleneck: second-highest penalty fellow F2 is assigned");
    test.expect(!hasAssignment(result, "F3"), "bottleneck: lowest penalty fellow F3 is unassigned");
    test.expect(classificationOf(result, "F3") == "competitively_unserved", "bottleneck: F3 is competitively unserved");
    test.expect(result.weeklyPenalty == 1, "bottleneck: penalty is only F3's skipped penalty = 1");
    test.expect(hasNoAlgorithmFailure(result), "bottleneck: no algorithm failure is reported");
}

void testStarvationRescueBeatsUrgency(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"A", "B"}, mentors);

    vector<Slot> prepSlots;
    prepSlots.push_back(makeSlot("PB", "M1", "Prep"));

    vector<Request> prepRequests;
    prepRequests.push_back(makeRequest("B", vector<string>{"PB"}, "cpp", "blocker", 1));

    solver.solveWeek(prepSlots, prepRequests);
    solver.solveWeek(prepSlots, prepRequests);
    solver.solveWeek(prepSlots, prepRequests);

    test.expect(solver.getStarvation("A") == 3, "starvation rescue setup: A has starvation 3");
    test.expect(solver.getStarvation("B") == 0, "starvation rescue setup: B has starvation 0");

    vector<Slot> slots;
    slots.push_back(makeSlot("S1", "M1", "Mon9"));

    vector<Request> requests;
    requests.push_back(makeRequest("A", vector<string>{"S1"}, "cpp", "exploratory", 20));
    requests.push_back(makeRequest("B", vector<string>{"S1"}, "cpp", "blocker", 10));

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(assignmentOf(result, "A") == "S1", "starvation rescue: starved exploratory A beats blocker B");
    test.expect(classificationOf(result, "B") == "competitively_unserved", "starvation rescue: B is competitively unserved");
    test.expect(result.weeklyPenalty == 3, "starvation rescue: penalty is B's skipped blocker penalty = 3");
    test.expect(solver.getStarvation("A") == 0, "starvation rescue: A starvation resets");
    test.expect(solver.getStarvation("B") == 1, "starvation rescue: B starvation increments");
}

void testLeastDamagingSlotChoice(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"A", "B"}, mentors);

    vector<Slot> prepSlots;
    prepSlots.push_back(makeSlot("PB", "M1", "Prep"));

    vector<Request> prepRequests;
    prepRequests.push_back(makeRequest("B", vector<string>{"PB"}, "cpp", "normal", 1));

    solver.solveWeek(prepSlots, prepRequests);
    solver.solveWeek(prepSlots, prepRequests);

    test.expect(solver.getStarvation("A") == 2, "least damaging setup: A has starvation 2");
    test.expect(solver.getStarvation("B") == 0, "least damaging setup: B has starvation 0");

    vector<Slot> slots;
    slots.push_back(makeSlot("S1", "M1", "Mon9"));
    slots.push_back(makeSlot("S2", "M1", "Tue9"));

    vector<Request> requests;
    requests.push_back(makeRequest("A", vector<string>{"S1", "S2"}, "cpp", "blocker", 10));
    requests.push_back(makeRequest("B", vector<string>{"S1"}, "cpp", "normal", 20));

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(assignmentOf(result, "A") == "S2", "least damaging: A avoids B's only slot and takes S2");
    test.expect(assignmentOf(result, "B") == "S1", "least damaging: B still gets its only slot S1");
    test.expect(result.weeklyPenalty == 0, "least damaging: both fellows assigned, penalty 0");
    test.expect(hasNoAlgorithmFailure(result), "least damaging: no algorithm failure is reported");
}

void testTimestampTieBreaker(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"Early", "Late"}, mentors);

    vector<Slot> slots;
    slots.push_back(makeSlot("S1", "M1", "Mon9"));

    vector<Request> requests;
    requests.push_back(makeRequest("Late", vector<string>{"S1"}, "cpp", "normal", 20));
    requests.push_back(makeRequest("Early", vector<string>{"S1"}, "cpp", "normal", 10));

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(assignmentOf(result, "Early") == "S1", "tie breaker: earlier timestamp wins");
    test.expect(classificationOf(result, "Late") == "competitively_unserved", "tie breaker: later timestamp is competitively unserved");
    test.expect(result.weeklyPenalty == 2, "tie breaker: skipped normal fellow penalty is 2");
}

void testNoRequestsWeek(TestRunner& test) {
    unordered_map<string, Mentor> mentors = makeMentors(vector<Mentor>{
        makeMentor("M1", vector<string>{"cpp"})
    });

    ScarcityAwareGreedySolver solver(vector<string>{"F1", "F2"}, mentors);

    vector<Slot> slots;
    vector<Request> requests;

    WeeklyResult result = solver.solveWeek(slots, requests);

    test.expect(result.assignment.empty(), "no requests: no assignments");
    test.expect(result.classification.empty(), "no requests: no unserved request classifications");
    test.expect(result.weeklyPenalty == 0, "no requests: weekly penalty is 0");
    test.expect(solver.getStarvation("F1") == 1, "no requests: F1 starvation increments because not assigned");
    test.expect(solver.getStarvation("F2") == 1, "no requests: F2 starvation increments because not assigned");
}

void runAllTests() {
    TestRunner test;

    testAllFeasibleAssigned(test);
    testInfeasibleRequests(test);
    testTopicBottleneckCapacity(test);
    testStarvationRescueBeatsUrgency(test);
    testLeastDamagingSlotChoice(test);
    testTimestampTieBreaker(test);
    testNoRequestsWeek(test);

    test.summary();

    if (test.failed == 0) {
        cout << "All self-checking tests passed.\n";
    } else {
        cout << "Some tests failed. Review the failed cases above.\n";
    }
}

int main() {
    runAllTests();
    return 0;
}