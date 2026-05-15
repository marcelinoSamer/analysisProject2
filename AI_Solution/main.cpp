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

int main() {
    runTests();

    return 0;
}