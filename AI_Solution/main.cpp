#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <cassert>

using namespace std;

struct Mentor {
    int id;
    unordered_set<string> topics;
};

struct Slot {
    string id;
    int mentorId;
    string timeBlock;
    bool canceled = false;
};

struct Request {
    string id; // e.g. W1_R2
    int fellowId;
    vector<string> requiredTopics;
    vector<string> acceptableTimes;
    string urgency;
    int preferredMentor;
    int week;
    bool canceled = false;
};

struct Assignment {
    string requestId;
    string slotId;
};

struct Unserved {
    string requestId;
    string reason;
};

struct DPResult {
    int served = 0;
    int benefit = 0;
};

struct WeekOutput {
    vector<Assignment> assignments;
    vector<Unserved> unserved;
    int totalServed = 0;
    int totalBenefit = 0;
};

const int ALPHA = 5;
const int BETA = 2;
const int GAMMA = 1;
const int DELTA = 2;

int urgencyValue(const string& urgency) {
    if (urgency == "high") return 3;
    if (urgency == "medium") return 2;
    return 1;
}

vector<string> splitComma(const string& s) {
    vector<string> result;
    if (s == "\"\"" || s.empty()) return result;

    string current;
    for (char c : s) {
        if (c == ',') {
            if (!current.empty()) result.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    if (!current.empty()) result.push_back(current);
    return result;
}

bool contains(const vector<string>& v, const string& x) {
    for (const string& item : v) {
        if (item == x) return true;
    }
    return false;
}

int topicMatch(const Request& r, const Slot& s, const unordered_map<int, Mentor>& mentors) {
    auto it = mentors.find(s.mentorId);
    if (it == mentors.end()) return 0;

    int count = 0;
    for (const string& topic : r.requiredTopics) {
        if (it->second.topics.count(topic)) count++;
    }

    return count;
}

bool compatible(const Request& r, const Slot& s, const unordered_map<int, Mentor>& mentors) {
    if (r.canceled || s.canceled) return false;
    if (!contains(r.acceptableTimes, s.timeBlock)) return false;
    return topicMatch(r, s, mentors) >= 1;
}

int benefit(const Request& r, const Slot& s, int currentWeek, const unordered_map<int, Mentor>& mentors) {
    int u = urgencyValue(r.urgency);
    int tm = topicMatch(r, s, mentors);
    int p = (r.preferredMentor > 0 && r.preferredMentor == s.mentorId) ? 1 : 0;
    int age = currentWeek - r.week;

    return ALPHA * u + BETA * tm + GAMMA * p + DELTA * age;
}

bool better(const DPResult& a, const DPResult& b) {
    if (a.served != b.served) return a.served > b.served;
    return a.benefit > b.benefit;
}

class OptimalMatcher {
private:
    vector<Request> requests;
    vector<Slot> slots;
    unordered_map<int, Mentor> mentors;
    int currentWeek;

    vector<vector<bool>> seen;
    vector<vector<DPResult>> memo;
    vector<vector<int>> choice;

public:
    OptimalMatcher(
        const vector<Request>& reqs,
        const vector<Slot>& sls,
        const unordered_map<int, Mentor>& m,
        int week
    ) {
        requests = reqs;
        slots = sls;
        mentors = m;
        currentWeek = week;

        int R = requests.size();
        int S = slots.size();
        int masks = 1 << S;

        seen.assign(R + 1, vector<bool>(masks, false));
        memo.assign(R + 1, vector<DPResult>(masks));
        choice.assign(R + 1, vector<int>(masks, -2));
    }

    DPResult solve(int i, int mask) {
        if (i == (int)requests.size()) {
            return {0, 0};
        }

        if (seen[i][mask]) return memo[i][mask];
        seen[i][mask] = true;

        DPResult best = solve(i + 1, mask);
        choice[i][mask] = -1; // skip request

        for (int j = 0; j < (int)slots.size(); j++) {
            if (mask & (1 << j)) continue;

            if (compatible(requests[i], slots[j], mentors)) {
                DPResult candidate = solve(i + 1, mask | (1 << j));
                candidate.served += 1;
                candidate.benefit += benefit(requests[i], slots[j], currentWeek, mentors);

                if (better(candidate, best)) {
                    best = candidate;
                    choice[i][mask] = j;
                }
            }
        }

        memo[i][mask] = best;
        return best;
    }

    WeekOutput buildOutput() {
        DPResult optimal = solve(0, 0);

        WeekOutput output;
        output.totalServed = optimal.served;
        output.totalBenefit = optimal.benefit;

        int mask = 0;
        unordered_set<string> servedRequests;

        for (int i = 0; i < (int)requests.size(); i++) {
            int ch = choice[i][mask];

            if (ch >= 0) {
                output.assignments.push_back({requests[i].id, slots[ch].id});
                servedRequests.insert(requests[i].id);
                mask |= (1 << ch);
            }
        }

        for (const Request& r : requests) {
            if (servedRequests.count(r.id)) continue;

            if (r.canceled) {
                output.unserved.push_back({r.id, "request canceled"});
                continue;
            }

            bool hasFeasibleSlot = false;
            for (const Slot& s : slots) {
                if (compatible(r, s, mentors)) {
                    hasFeasibleSlot = true;
                    break;
                }
            }

            if (hasFeasibleSlot) {
                output.unserved.push_back({r.id, "lower priority"});
            } else {
                output.unserved.push_back({r.id, "no feasible slot"});
            }
        }

        return output;
    }
};

WeekOutput solveWeek(
    vector<Request>& pending,
    vector<Slot> slots,
    const unordered_map<int, Mentor>& mentors,
    int currentWeek
) {
    vector<Request> activeRequests;
    vector<Request> canceledRequests;

    for (const Request& r : pending) {
        if (r.canceled) canceledRequests.push_back(r);
        else activeRequests.push_back(r);
    }

    vector<Slot> activeSlots;
    for (const Slot& s : slots) {
        if (!s.canceled) activeSlots.push_back(s);
    }

    OptimalMatcher matcher(activeRequests, activeSlots, mentors, currentWeek);
    WeekOutput output = matcher.buildOutput();

    for (const Request& r : canceledRequests) {
        output.unserved.push_back({r.id, "request canceled"});
    }

    unordered_set<string> served;
    unordered_set<string> canceled;

    for (const Assignment& a : output.assignments) served.insert(a.requestId);
    for (const Request& r : canceledRequests) canceled.insert(r.id);

    vector<Request> newPending;
    for (const Request& r : pending) {
        if (!served.count(r.id) && !canceled.count(r.id)) {
            newPending.push_back(r);
        }
    }

    pending = newPending;
    return output;
}

void printWeekOutput(const WeekOutput& output) {
    cout << "Assignments:\n";
    for (const Assignment& a : output.assignments) {
        cout << "  " << a.requestId << " -> " << a.slotId << "\n";
    }

    cout << "Unserved:\n";
    for (const Unserved& u : output.unserved) {
        cout << "  " << u.requestId << ": " << u.reason << "\n";
    }

    cout << "Served = " << output.totalServed << "\n";
    cout << "Benefit = " << output.totalBenefit << "\n";
}

void runTests() {
    unordered_map<int, Mentor> mentors;

    mentors[201] = {201, {"database", "python", "api"}};
    mentors[202] = {202, {"networking"}};

    {
        cout << "\nTEST 1: all requests can be served\n";

        vector<Request> pending = {
            {"W1_R1", 101, {"database"}, {"Mon10"}, "high", 0, 1},
            {"W1_R2", 102, {"networking"}, {"Tue14"}, "medium", 0, 1}
        };

        vector<Slot> slots = {
            {"W1_S1", 201, "Mon10"},
            {"W1_S2", 202, "Tue14"}
        };

        WeekOutput out = solveWeek(pending, slots, mentors, 1);

        assert(out.totalServed == 2);
        assert(pending.empty());

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 2: greedy trap, DP must serve 2 not 1\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"x"}};

        vector<Request> pending = {
            {"W1_R1", 1, {"x"}, {"A"}, "low", 0, 1},
            {"W1_R2", 2, {"x"}, {"A", "B"}, "high", 0, 1}
        };

        vector<Slot> slots = {
            {"W1_S1", 1, "A"},
            {"W1_S2", 1, "B"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 2);
        assert(pending.empty());

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 3: older medium request beats new high request\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"x"}};

        vector<Request> pending = {
            {"W1_R1", 1, {"x"}, {"A"}, "medium", 0, 1},
            {"W6_R1", 2, {"x"}, {"A"}, "high", 0, 6}
        };

        vector<Slot> slots = {
            {"W6_S1", 1, "A"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 6);

        assert(out.totalServed == 1);
        assert(out.assignments[0].requestId == "W1_R1");

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 4: no feasible slot because topic does not match\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"math"}};

        vector<Request> pending = {
            {"W1_R1", 1, {"python"}, {"A"}, "high", 0, 1}
        };

        vector<Slot> slots = {
            {"W1_S1", 1, "A"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 0);
        assert(out.unserved[0].reason == "no feasible slot");

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 5: preferred mentor increases benefit\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"x"}};
        m[2] = {2, {"x"}};

        vector<Request> pending = {
            {"W1_R1", 1, {"x"}, {"A"}, "medium", 2, 1}
        };

        vector<Slot> slots = {
            {"W1_S1", 1, "A"},
            {"W1_S2", 2, "A"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 1);
        assert(out.assignments[0].slotId == "W1_S2");

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 6: same fellow can receive multiple sessions\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"x", "y"}};

        vector<Request> pending = {
            {"W1_R1", 101, {"x"}, {"A"}, "high", 0, 1},
            {"W1_R2", 101, {"y"}, {"B"}, "high", 0, 1}
        };

        vector<Slot> slots = {
            {"W1_S1", 1, "A"},
            {"W1_S2", 1, "B"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 2);
        assert(pending.empty());

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 7: canceled request is not assigned\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"x"}};

        vector<Request> pending = {
            {"W1_R1", 1, {"x"}, {"A"}, "high", 0, 1, true}
        };

        vector<Slot> slots = {
            {"W1_S1", 1, "A"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 0);
        assert(out.unserved[0].reason == "request canceled");

        cout << "PASSED\n";
    }

    {
        cout << "\nTEST 8: canceled slot cannot be used\n";

        unordered_map<int, Mentor> m;
        m[1] = {1, {"x"}};

        vector<Request> pending = {
            {"W1_R1", 1, {"x"}, {"A"}, "high", 0, 1}
        };

        vector<Slot> slots = {
            {"W1_S1", 1, "A", true}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 0);
        assert(out.unserved[0].reason == "no feasible slot");

        cout << "PASSED\n";
    }

    cout << "\nALL TESTS PASSED\n";
}



// ============================================================
// EXTENDED TEST SUITE
// Drop this anywhere after solveWeek() / printWeekOutput().
// Then call runExtendedTests() from main().
// ============================================================

int countReason(const WeekOutput& out, const string& reason) {
    int c = 0;
    for (const auto& u : out.unserved) if (u.reason == reason) c++;
    return c;
}

string assignedSlot(const WeekOutput& out, const string& reqId) {
    for (const auto& a : out.assignments) {
        if (a.requestId == reqId) return a.slotId;
    }
    return "";
}

bool isServed(const WeekOutput& out, const string& reqId) {
    return !assignedSlot(out, reqId).empty();
}

void runExtendedTests() {
    cout << "\n=== EXTENDED TESTS ===\n";

    // ------------------------------------------------------------
    // TEST 9: Topic-match score drives slot choice.
    // One request feasible with two slots; the one giving higher
    // topicMatch (and thus higher benefit) must be picked.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 9: higher topicMatch slot wins when both feasible\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database", "python", "api"}};
        m[202] = {202, {"database"}};

        vector<Request> pending = {
            {"W1_R1", 101, {"database", "python", "api"}, {"Mon10"}, "medium", 0, 1}
        };

        vector<Slot> slots = {
            {"S_201", 201, "Mon10"},
            {"S_202", 202, "Mon10"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        // 201 gives tm=3, benefit = 5*2 + 2*3 = 16
        // 202 gives tm=1, benefit = 5*2 + 2*1 = 12
        assert(out.totalServed == 1);
        assert(out.totalBenefit == 16);
        assert(assignedSlot(out, "W1_R1") == "S_201");

        cout << "PASSED  served=" << out.totalServed
             << " benefit=" << out.totalBenefit << "\n";
    }

    // ------------------------------------------------------------
    // TEST 10: Both unserved reasons present in one week.
    // One served, one bumped to "lower priority", two infeasible.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 10: lower-priority and no-feasible-slot in same week\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database"}};

        vector<Request> pending = {
            {"W1_R1", 101, {"database"}, {"Mon10"}, "high",   0, 1},  // served
            {"W1_R2", 102, {"database"}, {"Mon10"}, "medium", 0, 1},  // lower priority
            {"W1_R3", 103, {"python"},   {"Mon10"}, "high",   0, 1},  // no feasible slot (topic)
            {"W1_R4", 104, {"database"}, {"Wed09"}, "high",   0, 1}   // no feasible slot (time)
        };

        vector<Slot> slots = {
            {"S_201_Mon10", 201, "Mon10"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 1);
        assert(isServed(out, "W1_R1"));
        assert(countReason(out, "lower priority") == 1);
        assert(countReason(out, "no feasible slot") == 2);
        assert(out.totalBenefit == 17);  // 5*3 + 2*1

        cout << "PASSED  served=" << out.totalServed
             << " lower_priority=" << countReason(out, "lower priority")
             << " no_feasible=" << countReason(out, "no feasible slot") << "\n";
    }

    // ------------------------------------------------------------
    // TEST 11: Backlog persists then drains. Verifies age delta
    // is applied correctly when a request waits a week before
    // being served.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 11: pending request persists across weeks then drains\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database"}};

        vector<Request> pending = {
            {"W1_R1", 101, {"database"}, {"Mon10"}, "medium", 0, 1}
        };

        // Week 1: no slots offered
        WeekOutput w1 = solveWeek(pending, {}, m, 1);
        assert(w1.totalServed == 0);
        assert(w1.unserved.size() == 1);
        assert(w1.unserved[0].reason == "no feasible slot");
        assert(pending.size() == 1);

        // Week 2: slot arrives
        vector<Slot> slots2 = {{"S1", 201, "Mon10"}};
        WeekOutput w2 = solveWeek(pending, slots2, m, 2);

        // age = 2-1 = 1: benefit = 5*2 + 2*1 + 0 + 2*1 = 14
        assert(w2.totalServed == 1);
        assert(w2.totalBenefit == 14);
        assert(isServed(w2, "W1_R1"));
        assert(pending.empty());

        cout << "PASSED  w1_served=0 w2_served=1 w2_benefit="
             << w2.totalBenefit << " (age=1 contributed +2)\n";
    }

    // ------------------------------------------------------------
    // TEST 12: Persistently infeasible request stays unserved
    // for multiple weeks with the correct reason every week.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 12: persistently infeasible request across 3 weeks\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database"}};

        vector<Request> pending = {
            {"W1_R1", 101, {"database"}, {"Fri17"}, "low", 0, 1}
        };

        WeekOutput w1 = solveWeek(pending, {{"S1", 201, "Mon10"}}, m, 1);
        assert(w1.totalServed == 0);
        assert(w1.unserved[0].reason == "no feasible slot");
        assert(pending.size() == 1);

        WeekOutput w2 = solveWeek(pending, {{"S2", 201, "Tue14"}}, m, 2);
        assert(w2.totalServed == 0);
        assert(w2.unserved[0].reason == "no feasible slot");
        assert(pending.size() == 1);

        WeekOutput w3 = solveWeek(pending, {{"S3", 201, "Wed09"}}, m, 3);
        assert(w3.totalServed == 0);
        assert(w3.unserved[0].reason == "no feasible slot");
        assert(pending.size() == 1);

        cout << "PASSED  request remained unserved for 3 weeks\n";
    }

    // ------------------------------------------------------------
    // TEST 13: Preferred mentor preference cannot override
    // compatibility. Preferred mentor lacks the topic, so the
    // request falls back to the only compatible mentor with p=0.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 13: preferred mentor preference cannot override topic mismatch\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"python"}};       // preferred, but wrong topic
        m[202] = {202, {"database"}};      // compatible

        vector<Request> pending = {
            {"W1_R1", 101, {"database"}, {"Mon10"}, "high", 201, 1}
        };

        vector<Slot> slots = {
            {"S_201", 201, "Mon10"},
            {"S_202", 202, "Mon10"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 1);
        assert(assignedSlot(out, "W1_R1") == "S_202");
        // p=0 since 202 != preferred 201
        assert(out.totalBenefit == 17);  // 5*3 + 2*1 + 0 + 0

        cout << "PASSED  fell back to compatible mentor, p=0\n";
    }

    // ------------------------------------------------------------
    // TEST 14: Empty required topics list. |empty intersect *| = 0,
    // and score 0 makes the pair infeasible per spec.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 14: empty required topics yields no feasible slot\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database"}};

        vector<Request> pending = {
            {"W1_R1", 101, {}, {"Mon10"}, "high", 0, 1}
        };

        vector<Slot> slots = {{"S1", 201, "Mon10"}};

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 0);
        assert(out.unserved[0].reason == "no feasible slot");

        cout << "PASSED  empty topics treated as infeasible\n";
    }

    // ------------------------------------------------------------
    // TEST 15: Mentor with no topics is unmatchable.
    // Any pair (request, slot) where the slot's mentor has no
    // topics has topicMatch=0, so the slot is effectively dead.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 15: mentor with no topics is unmatchable\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {}};                // empty topic set
        m[202] = {202, {"database"}};

        vector<Request> pending = {
            {"W1_R1", 101, {"database"}, {"Mon10"}, "high", 0, 1},
            {"W1_R2", 102, {"database"}, {"Mon10"}, "high", 0, 1}
        };

        vector<Slot> slots = {
            {"S_201", 201, "Mon10"},   // dead slot
            {"S_202", 202, "Mon10"}    // only usable slot
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        // Only one slot truly usable, so only one served
        assert(out.totalServed == 1);
        assert(countReason(out, "lower priority") == 1);

        string servedSlot;
        for (const auto& a : out.assignments) servedSlot = a.slotId;
        assert(servedSlot == "S_202");

        cout << "PASSED  topicless mentor unused, 1 of 2 served\n";
    }

    // ------------------------------------------------------------
    // TEST 16: Dense bipartite case. Five requests, four slots.
    // R4 has only one feasible slot (S_201_Tue14), forcing R3 to
    // take S_203_Tue14. R2 and R5 contest S_202_Mon10. Max
    // matching is 4; a greedy that doesn't respect the forced
    // edge would get only 3.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 16: dense bipartite, max matching = 4 of 5\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database", "python"}};
        m[202] = {202, {"networking", "api"}};
        m[203] = {203, {"database", "api"}};

        vector<Request> pending = {
            {"R1", 101, {"database"},   {"Mon10"},  "high",   0, 1}, // only S_201_Mon10
            {"R2", 102, {"api"},        {"Mon10"},  "high",   0, 1}, // only S_202_Mon10
            {"R3", 103, {"database"},   {"Tue14"},  "medium", 0, 1}, // S_201_Tue14 or S_203_Tue14
            {"R4", 104, {"python"},     {"Tue14"},  "low",  201, 1}, // only S_201_Tue14
            {"R5", 105, {"networking"}, {"Mon10"},  "high",   0, 1}  // only S_202_Mon10
        };

        vector<Slot> slots = {
            {"S_201_Mon10", 201, "Mon10"},
            {"S_202_Mon10", 202, "Mon10"},
            {"S_203_Tue14", 203, "Tue14"},
            {"S_201_Tue14", 201, "Tue14"}
        };

        WeekOutput out = solveWeek(pending, slots, m, 1);

        assert(out.totalServed == 4);
        assert(isServed(out, "R1"));
        assert(isServed(out, "R3"));
        assert(isServed(out, "R4"));
        assert(isServed(out, "R2") != isServed(out, "R5"));    // exactly one
        assert(assignedSlot(out, "R4") == "S_201_Tue14");      // R4's only option

        cout << "PASSED  served=" << out.totalServed
             << " benefit=" << out.totalBenefit << "\n";
    }

    // ------------------------------------------------------------
    // TEST 17: End-of-program accounting across multiple weeks.
    // Verifies that totals stay consistent as a request lingers
    // for two weeks then gets served, with the correct age bonus.
    // ------------------------------------------------------------
    {
        cout << "\nTEST 17: program-level accounting across 3 weeks\n";

        unordered_map<int, Mentor> m;
        m[201] = {201, {"database", "python"}};
        m[202] = {202, {"networking", "api"}};

        vector<Request> pending;
        int totalServed = 0;
        int totalBenefit = 0;

        // Week 1
        pending.push_back({"W1_R1", 101, {"database"},   {"Mon10"}, "high",   0, 1});
        pending.push_back({"W1_R2", 102, {"networking"}, {"Tue14"}, "medium", 0, 1});
        pending.push_back({"W1_R3", 103, {"api"},        {"Wed09"}, "low",    0, 1}); // infeasible

        vector<Slot> w1_slots = {
            {"W1_S1", 201, "Mon10"},
            {"W1_S2", 202, "Tue14"}
        };

        WeekOutput w1 = solveWeek(pending, w1_slots, m, 1);
        totalServed += w1.totalServed;
        totalBenefit += w1.totalBenefit;

        assert(w1.totalServed == 2);
        assert(w1.totalBenefit == 29);            // 17 + 12
        assert(pending.size() == 1);               // W1_R3 carries over

        // Week 2: still no Wed09 slot
        pending.push_back({"W2_R1", 104, {"python"}, {"Mon10"}, "high", 0, 2});

        vector<Slot> w2_slots = {{"W2_S1", 201, "Mon10"}};

        WeekOutput w2 = solveWeek(pending, w2_slots, m, 2);
        totalServed += w2.totalServed;
        totalBenefit += w2.totalBenefit;

        assert(w2.totalServed == 1);
        assert(isServed(w2, "W2_R1"));
        assert(w2.totalBenefit == 17);
        assert(pending.size() == 1);               // W1_R3 still hanging

        // Week 3: finally a Wed09 slot with api topic
        vector<Slot> w3_slots = {{"W3_S1", 202, "Wed09"}};

        WeekOutput w3 = solveWeek(pending, w3_slots, m, 3);
        totalServed += w3.totalServed;
        totalBenefit += w3.totalBenefit;

        // W1_R3 age = 3-1 = 2: benefit = 5*1 + 2*1 + 0 + 2*2 = 11
        assert(w3.totalServed == 1);
        assert(isServed(w3, "W1_R3"));
        assert(w3.totalBenefit == 11);
        assert(pending.empty());

        assert(totalServed == 4);
        assert(totalBenefit == 57);                // 29 + 17 + 11

        cout << "PASSED  program_served=" << totalServed
             << " program_benefit=" << totalBenefit << "\n";
    }

    cout << "\n=== ALL EXTENDED TESTS PASSED ===\n";
}

int main() {
    runTests();
    runExtendedTests();

    return 0;
}
