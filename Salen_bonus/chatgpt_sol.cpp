#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <queue>
#include <limits>
#include <algorithm>

using namespace std;

const long long ALPHA = 5;
const long long BETA = 2;
const long long GAMMA = 1;
const long long DELTA = 2;
const long long LAMBDA = 1;
const long long BENEFIT_SCALE = 1000000000LL;

struct Mentor {
    int id;
    vector<string> topics;
};

struct Slot {
    int id;
    int mentorId;
    string timeBlock;
    bool active;

    Slot() : id(0), mentorId(0), timeBlock(""), active(true) {}
};

struct Request {
    int id;
    int fellowId;
    vector<string> requiredTopics;
    vector<string> acceptableTimes;
    string urgency;
    int preferredMentor;
    int submitWeek;
    bool activePending;
    bool served;
    bool canceled;

    Request()
        : id(0), fellowId(0), preferredMentor(0), submitWeek(0),
          activePending(true), served(false), canceled(false) {}
};

struct WeekResult {
    int week;
    int servedCount;
    long long benefitTotal;
    vector<pair<int, int> > assignments;
    vector<pair<int, string> > reasons;
};

struct SolveResult {
    vector<WeekResult> weeks;
    long long totalServed;
    long long totalBenefit;
    long long penalty;

    SolveResult() : totalServed(0), totalBenefit(0), penalty(0) {}
};

struct ExpectedWeek {
    int week;
    int servedCount;
    long long benefitTotal;
    vector<pair<int, int> > assignments;
    vector<pair<int, string> > reasons;
};

struct TestCase {
    string name;
    string input;
    vector<ExpectedWeek> weeks;
    long long totalServed;
    long long totalBenefit;
    long long penalty;
};

vector<string> splitCSV(const string& text) {
    vector<string> result;

    if (text == "\"\"" || text == "") {
        return result;
    }

    string current = "";
    for (int i = 0; i < (int)text.size(); i++) {
        if (text[i] == ',') {
            if (!current.empty()) result.push_back(current);
            current = "";
        } else {
            current += text[i];
        }
    }

    if (!current.empty()) result.push_back(current);
    return result;
}

vector<int> parseIntLine(const string& line) {
    vector<int> values;
    stringstream ss(line);
    int x;
    while (ss >> x) values.push_back(x);
    return values;
}

int parseIdBeforeColon(const string& token) {
    string idPart = "";
    for (int i = 0; i < (int)token.size(); i++) {
        if (token[i] == ':') break;
        idPart += token[i];
    }
    stringstream ss(idPart);
    int id = 0;
    ss >> id;
    return id;
}

int urgencyValue(const string& urgency) {
    if (urgency == "high") return 3;
    if (urgency == "medium") return 2;
    return 1;
}

bool containsString(const vector<string>& values, const string& target) {
    for (int i = 0; i < (int)values.size(); i++) {
        if (values[i] == target) return true;
    }
    return false;
}

int topicMatchCount(const Request& request, const Mentor& mentor) {
    int count = 0;
    for (int i = 0; i < (int)request.requiredTopics.size(); i++) {
        if (containsString(mentor.topics, request.requiredTopics[i])) count++;
    }
    return count;
}

bool timeCompatible(const Request& request, const Slot& slot) {
    return containsString(request.acceptableTimes, slot.timeBlock);
}

long long computeBenefit(const Request& request, const Slot& slot, const Mentor& mentor, int currentWeek) {
    int topicMatch = topicMatchCount(request, mentor);
    int preferred = 0;
    if (request.preferredMentor > 0 && request.preferredMentor == slot.mentorId) preferred = 1;
    int age = currentWeek - request.submitWeek;

    long long benefit = 0;
    benefit += ALPHA * urgencyValue(request.urgency);
    benefit += BETA * topicMatch;
    benefit += GAMMA * preferred;
    benefit += DELTA * age;
    return benefit;
}

bool compatible(const Request& request, const Slot& slot, const Mentor& mentor) {
    if (!slot.active) return false;
    if (!timeCompatible(request, slot)) return false;
    if (topicMatchCount(request, mentor) <= 0) return false;
    return true;
}

struct Edge {
    int to;
    int rev;
    int cap;
    int originalCap;
    long long cost;
    int requestLocalIndex;
    int slotLocalIndex;
};

struct MinCostMaxFlow {
    vector<vector<Edge> > graph;

    MinCostMaxFlow(int n) {
        graph.assign(n, vector<Edge>());
    }

    void addEdge(int from, int to, int cap, long long cost, int requestLocalIndex, int slotLocalIndex) {
        Edge forward;
        forward.to = to;
        forward.rev = (int)graph[to].size();
        forward.cap = cap;
        forward.originalCap = cap;
        forward.cost = cost;
        forward.requestLocalIndex = requestLocalIndex;
        forward.slotLocalIndex = slotLocalIndex;

        Edge backward;
        backward.to = from;
        backward.rev = (int)graph[from].size();
        backward.cap = 0;
        backward.originalCap = 0;
        backward.cost = -cost;
        backward.requestLocalIndex = -1;
        backward.slotLocalIndex = -1;

        graph[from].push_back(forward);
        graph[to].push_back(backward);
    }

    pair<int, long long> run(int source, int sink) {
        int n = (int)graph.size();
        int flow = 0;
        long long cost = 0;
        const long long INF = numeric_limits<long long>::max() / 4;

        while (true) {
            vector<long long> dist(n, INF);
            vector<int> parentNode(n, -1);
            vector<int> parentEdge(n, -1);
            vector<bool> inQueue(n, false);
            queue<int> q;

            dist[source] = 0;
            q.push(source);
            inQueue[source] = true;

            while (!q.empty()) {
                int u = q.front();
                q.pop();
                inQueue[u] = false;

                for (int i = 0; i < (int)graph[u].size(); i++) {
                    Edge& edge = graph[u][i];
                    if (edge.cap > 0 && dist[u] != INF && dist[u] + edge.cost < dist[edge.to]) {
                        dist[edge.to] = dist[u] + edge.cost;
                        parentNode[edge.to] = u;
                        parentEdge[edge.to] = i;
                        if (!inQueue[edge.to]) {
                            q.push(edge.to);
                            inQueue[edge.to] = true;
                        }
                    }
                }
            }

            if (dist[sink] == INF) break;

            int pushed = 1000000000;
            for (int v = sink; v != source; v = parentNode[v]) {
                int u = parentNode[v];
                int edgeIndex = parentEdge[v];
                pushed = min(pushed, graph[u][edgeIndex].cap);
            }

            for (int v = sink; v != source; v = parentNode[v]) {
                int u = parentNode[v];
                int edgeIndex = parentEdge[v];
                Edge& edge = graph[u][edgeIndex];
                edge.cap -= pushed;
                graph[edge.to][edge.rev].cap += pushed;
                cost += pushed * edge.cost;
            }

            flow += pushed;
        }

        return make_pair(flow, cost);
    }
};

long long tieBonus(int requestLocalIndex, int slotLocalIndex, int requestCount, int slotCount) {
    long long requestPriority = requestCount - requestLocalIndex;
    long long slotPriority = slotCount - slotLocalIndex;
    return requestPriority * slotPriority;
}

SolveResult solveProblem(istream& in) {
    SolveResult result;
    string line;

    if (!getline(in, line)) return result;
    vector<int> fellowIds = parseIntLine(line);

    if (!getline(in, line)) return result;
    vector<int> mentorIds = parseIntLine(line);

    vector<Mentor> mentors;
    map<int, int> mentorIndex;

    for (int i = 0; i < (int)mentorIds.size(); i++) {
        getline(in, line);
        stringstream ss(line);
        Mentor mentor;
        ss >> mentor.id;

        string topic;
        while (ss >> topic) mentor.topics.push_back(topic);

        mentorIndex[mentor.id] = (int)mentors.size();
        mentors.push_back(mentor);
    }

    vector<Request> allRequests;
    map<int, int> requestIndex;
    int lastWeek = 0;

    string word;
    while (in >> word) {
        if (word != "WEEK") continue;

        int week;
        in >> week;
        lastWeek = week;

        vector<Slot> weeklySlots;
        map<int, int> slotIndex;

        int S;
        in >> S;
        for (int i = 0; i < S; i++) {
            Slot slot;
            in >> slot.id >> slot.mentorId >> slot.timeBlock;
            slot.active = true;
            slotIndex[slot.id] = (int)weeklySlots.size();
            weeklySlots.push_back(slot);
        }

        int R;
        in >> R;
        for (int i = 0; i < R; i++) {
            Request request;
            string topicsText;
            string timesText;
            in >> request.id >> request.fellowId >> topicsText >> timesText >> request.urgency >> request.preferredMentor;
            request.requiredTopics = splitCSV(topicsText);
            request.acceptableTimes = splitCSV(timesText);
            request.submitWeek = week;
            request.activePending = true;
            request.served = false;
            request.canceled = false;
            requestIndex[request.id] = (int)allRequests.size();
            allRequests.push_back(request);
        }

        int C;
        in >> C;
        for (int i = 0; i < C; i++) {
            string token;
            in >> token;
            int slotId = parseIdBeforeColon(token);
            if (slotIndex.count(slotId)) weeklySlots[slotIndex[slotId]].active = false;
        }

        int U;
        in >> U;
        for (int i = 0; i < U; i++) {
            int slotId;
            string oldTime, newTime;
            in >> slotId >> oldTime >> newTime;
            if (slotIndex.count(slotId)) {
                int index = slotIndex[slotId];
                if (weeklySlots[index].active) weeklySlots[index].timeBlock = newTime;
            }
        }

        int Q;
        in >> Q;
        for (int i = 0; i < Q; i++) {
            string token;
            in >> token;
            int requestId = parseIdBeforeColon(token);
            if (requestIndex.count(requestId)) {
                int index = requestIndex[requestId];
                if (allRequests[index].activePending && !allRequests[index].served) {
                    allRequests[index].activePending = false;
                    allRequests[index].canceled = true;
                }
            }
        }

        vector<int> activeRequests;
        vector<int> activeSlots;

        for (int i = 0; i < (int)allRequests.size(); i++) {
            if (allRequests[i].activePending && !allRequests[i].served && !allRequests[i].canceled) {
                activeRequests.push_back(i);
            }
        }

        for (int i = 0; i < (int)weeklySlots.size(); i++) {
            if (weeklySlots[i].active) activeSlots.push_back(i);
        }

        int n = (int)activeRequests.size();
        int m = (int)activeSlots.size();

        int source = 0;
        int requestStart = 1;
        int slotStart = requestStart + n;
        int sink = slotStart + m;

        MinCostMaxFlow solver(sink + 1);

        for (int i = 0; i < n; i++) solver.addEdge(source, requestStart + i, 1, 0, -1, -1);
        for (int j = 0; j < m; j++) solver.addEdge(slotStart + j, sink, 1, 0, -1, -1);

        for (int i = 0; i < n; i++) {
            int requestGlobalIndex = activeRequests[i];
            Request& request = allRequests[requestGlobalIndex];

            for (int j = 0; j < m; j++) {
                int slotWeeklyIndex = activeSlots[j];
                Slot& slot = weeklySlots[slotWeeklyIndex];

                if (!mentorIndex.count(slot.mentorId)) continue;
                Mentor& mentor = mentors[mentorIndex[slot.mentorId]];

                if (compatible(request, slot, mentor)) {
                    long long benefit = computeBenefit(request, slot, mentor, week);
                    long long score = benefit * BENEFIT_SCALE + tieBonus(i, j, n, m);
                    solver.addEdge(requestStart + i, slotStart + j, 1, -score, i, j);
                }
            }
        }

        solver.run(source, sink);

        WeekResult weekResult;
        weekResult.week = week;
        weekResult.servedCount = 0;
        weekResult.benefitTotal = 0;

        for (int i = 0; i < n; i++) {
            int requestNode = requestStart + i;
            for (int e = 0; e < (int)solver.graph[requestNode].size(); e++) {
                Edge edge = solver.graph[requestNode][e];
                if (edge.originalCap == 1 && edge.cap == 0 && edge.requestLocalIndex != -1 && edge.slotLocalIndex != -1) {
                    int requestGlobalIndex = activeRequests[edge.requestLocalIndex];
                    int slotWeeklyIndex = activeSlots[edge.slotLocalIndex];
                    Request& request = allRequests[requestGlobalIndex];
                    Slot& slot = weeklySlots[slotWeeklyIndex];
                    Mentor& mentor = mentors[mentorIndex[slot.mentorId]];
                    long long benefit = computeBenefit(request, slot, mentor, week);
                    request.served = true;
                    request.activePending = false;
                    weekResult.assignments.push_back(make_pair(request.id, slot.id));
                    weekResult.benefitTotal += benefit;
                    weekResult.servedCount++;
                }
            }
        }

        sort(weekResult.assignments.begin(), weekResult.assignments.end());

        for (int i = 0; i < (int)activeRequests.size(); i++) {
            int requestGlobalIndex = activeRequests[i];
            Request& request = allRequests[requestGlobalIndex];
            if (request.served || request.canceled) continue;

            bool hasFeasibleSlot = false;
            for (int j = 0; j < (int)activeSlots.size(); j++) {
                int slotWeeklyIndex = activeSlots[j];
                Slot& slot = weeklySlots[slotWeeklyIndex];
                if (!mentorIndex.count(slot.mentorId)) continue;
                Mentor& mentor = mentors[mentorIndex[slot.mentorId]];
                if (compatible(request, slot, mentor)) {
                    hasFeasibleSlot = true;
                    break;
                }
            }

            if (hasFeasibleSlot) weekResult.reasons.push_back(make_pair(request.id, string("lower priority")));
            else weekResult.reasons.push_back(make_pair(request.id, string("no feasible slot")));
        }

        sort(weekResult.reasons.begin(), weekResult.reasons.end());
        result.totalServed += weekResult.servedCount;
        result.totalBenefit += weekResult.benefitTotal;
        result.weeks.push_back(weekResult);
    }

    for (int i = 0; i < (int)allRequests.size(); i++) {
        Request& request = allRequests[i];
        if (!request.served) {
            long long finalAge = (lastWeek + 1) - request.submitWeek;
            result.penalty += LAMBDA * finalAge * finalAge;
        }
    }

    return result;
}

SolveResult solveProblemFromString(const string& input) {
    stringstream ss(input);
    return solveProblem(ss);
}

void printSolveResult(const SolveResult& result) {
    for (int i = 0; i < (int)result.weeks.size(); i++) {
        const WeekResult& week = result.weeks[i];
        cout << "WEEK " << week.week << "\n";
        cout << "Assignments:\n";
        if (week.assignments.empty()) cout << "None\n";
        else {
            for (int j = 0; j < (int)week.assignments.size(); j++) {
                cout << week.assignments[j].first << " -> " << week.assignments[j].second << "\n";
            }
        }

        cout << "Unserved:\n";
        if (week.reasons.empty()) cout << "None\n";
        else {
            for (int j = 0; j < (int)week.reasons.size(); j++) {
                cout << week.reasons[j].first << " : " << week.reasons[j].second << "\n";
            }
        }

        cout << "Week served: " << week.servedCount << "\n";
        cout << "Week benefit: " << week.benefitTotal << "\n\n";
    }

    cout << "END-OF-PROGRAM REPORT\n";
    cout << "Total served requests: " << result.totalServed << "\n";
    cout << "Total benefit: " << result.totalBenefit << "\n";
    cout << "Penalty for unserved requests: " << result.penalty << "\n";
}

bool sameAssignments(vector<pair<int, int> > a, vector<pair<int, int> > b) {
    sort(a.begin(), a.end());
    sort(b.begin(), b.end());
    return a == b;
}

bool sameReasons(vector<pair<int, string> > a, vector<pair<int, string> > b) {
    sort(a.begin(), a.end());
    sort(b.begin(), b.end());
    return a == b;
}

void printAssignmentList(const vector<pair<int, int> >& assignments) {
    cout << "{";
    for (int i = 0; i < (int)assignments.size(); i++) {
        if (i > 0) cout << ", ";
        cout << assignments[i].first << "->" << assignments[i].second;
    }
    cout << "}";
}

void printReasonList(const vector<pair<int, string> >& reasons) {
    cout << "{";
    for (int i = 0; i < (int)reasons.size(); i++) {
        if (i > 0) cout << ", ";
        cout << reasons[i].first << ":" << reasons[i].second;
    }
    cout << "}";
}

vector<TestCase> buildTests() {
    vector<TestCase> tests;
    {
        TestCase tc;
        tc.name = "01_trivial_match";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
1
1 201 Mon10
1
1 101 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 17;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(1, 1));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "02_topic_mismatch";
        tc.input = R"TESTCASE(101
201
201 networking
WEEK 1
1
1 201 Mon10
1
1 101 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 0;
        tc.totalBenefit = 0;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "03_time_mismatch";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
1
1 201 Mon10
1
1 101 database Tue14 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 0;
        tc.totalBenefit = 0;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "04_slot_capacity_one";
        tc.input = R"TESTCASE(101 102
201
201 database
WEEK 1
1
1 201 Mon10
2
1 101 database Mon10 high 0
2 102 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 17;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(1, 1));
            ew.reasons.push_back(make_pair(2, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "05_max_served_beats_max_benefit";
        tc.input = R"TESTCASE(101 102 103
201 202
201 database
202 database
WEEK 1
2
1 201 Mon10
2 202 Tue14
3
1 101 database Mon10 high 0
2 102 database Tue14 high 0
3 103 database Mon10 low 0
0
0
0
)TESTCASE";
        tc.totalServed = 2;
        tc.totalBenefit = 34;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 2;
            ew.benefitTotal = 34;
            ew.assignments.push_back(make_pair(1, 1));
            ew.assignments.push_back(make_pair(2, 2));
            ew.reasons.push_back(make_pair(3, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "06_age_vs_urgency";
        tc.input = R"TESTCASE(101 102
201
201 database
WEEK 1
0
1
1 101 database Mon10 medium 0
0
0
0
WEEK 2
0
0
0
0
0
WEEK 3
0
0
0
0
0
WEEK 4
0
0
0
0
0
WEEK 5
0
0
0
0
0
WEEK 6
1
2 201 Mon10
1
2 102 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 22;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 3;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 4;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 5;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 6;
            ew.servedCount = 1;
            ew.benefitTotal = 22;
            ew.assignments.push_back(make_pair(1, 2));
            ew.reasons.push_back(make_pair(2, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "07_preferred_mentor_tiebreak";
        tc.input = R"TESTCASE(101 102
201 202
201 database
202 database
WEEK 1
2
1 201 Mon10
2 202 Mon10
2
1 101 database Mon10 high 201
2 102 database Mon10 high 202
0
0
0
)TESTCASE";
        tc.totalServed = 2;
        tc.totalBenefit = 36;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 2;
            ew.benefitTotal = 36;
            ew.assignments.push_back(make_pair(1, 1));
            ew.assignments.push_back(make_pair(2, 2));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "08_topic_match_size_matters";
        tc.input = R"TESTCASE(101
201 202
201 database api ml
202 database
WEEK 1
2
1 201 Mon10
2 202 Mon10
1
1 101 database,api,ml Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 21;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 21;
            ew.assignments.push_back(make_pair(1, 1));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "09_scripted_slot_cancellation";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
1
1 201 Mon10
1
1 101 database Mon10 high 0
1
1:Mon10
0
0
)TESTCASE";
        tc.totalServed = 0;
        tc.totalBenefit = 0;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "10_scripted_slot_reschedule";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
1
1 201 Wed11
1
1 101 database Mon10,Tue14 high 0
0
1
1 Wed11 Tue14
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 17;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(1, 1));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "11_reschedule_breaks_match";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
1
1 201 Mon10
1
1 101 database Mon10 high 0
0
1
1 Mon10 Wed11
0
)TESTCASE";
        tc.totalServed = 0;
        tc.totalBenefit = 0;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "12_request_cancellation";
        tc.input = R"TESTCASE(101 102
201
201 database
WEEK 1
1
1 201 Mon10
2
1 101 database Mon10 high 0
2 102 database Mon10 high 0
0
0
1
1:Mon10
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 17;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(2, 1));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "13_backlog_carries_over";
        tc.input = R"TESTCASE(101 102 103
201
201 database
WEEK 1
1
1 201 Mon10
2
1 101 database Mon10 high 0
2 102 database Mon10 high 0
0
0
0
WEEK 2
1
2 201 Mon10
1
3 103 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 2;
        tc.totalBenefit = 36;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(1, 1));
            ew.reasons.push_back(make_pair(2, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 1;
            ew.benefitTotal = 19;
            ew.assignments.push_back(make_pair(2, 2));
            ew.reasons.push_back(make_pair(3, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "14_no_slots_for_long_time";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
0
1
1 101 database Mon10 high 0
0
0
0
WEEK 2
0
0
0
0
0
WEEK 3
0
0
0
0
0
WEEK 4
0
0
0
0
0
WEEK 5
1
1 201 Mon10
0
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 25;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 3;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 4;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 5;
            ew.servedCount = 1;
            ew.benefitTotal = 25;
            ew.assignments.push_back(make_pair(1, 1));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "15_empty_week";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
0
0
0
0
0
)TESTCASE";
        tc.totalServed = 0;
        tc.totalBenefit = 0;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "16_mentor_no_topics";
        tc.input = R"TESTCASE(101
201
201
WEEK 1
1
1 201 Mon10
1
1 101 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 0;
        tc.totalBenefit = 0;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "17_multi_request_same_fellow";
        tc.input = R"TESTCASE(101 102
201
201 database
WEEK 1
0
1
1 101 database Mon10,Tue14 high 0
0
0
0
WEEK 2
0
1
2 101 database Mon10,Tue14 medium 0
0
0
0
WEEK 3
2
1 201 Mon10
2 201 Tue14
1
3 102 database Mon10,Tue14 medium 0
0
0
0
)TESTCASE";
        tc.totalServed = 2;
        tc.totalBenefit = 35;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            ew.reasons.push_back(make_pair(2, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 3;
            ew.servedCount = 2;
            ew.benefitTotal = 35;
            ew.assignments.push_back(make_pair(1, 1));
            ew.assignments.push_back(make_pair(2, 2));
            ew.reasons.push_back(make_pair(3, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "18_bipartite_matching";
        tc.input = R"TESTCASE(101 102
201 202
201 database backend
202 database
WEEK 1
2
1 201 Mon10
2 202 Mon10
2
1 101 backend Mon10 medium 0
2 102 database Mon10 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 2;
        tc.totalBenefit = 29;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 2;
            ew.benefitTotal = 29;
            ew.assignments.push_back(make_pair(1, 1));
            ew.assignments.push_back(make_pair(2, 2));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "19_preferred_mentor_nonexistent";
        tc.input = R"TESTCASE(101
201
201 database
WEEK 1
1
1 201 Mon10
1
1 101 database Mon10 high 999
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 17;
        tc.penalty = 0;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(1, 1));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "20_lower_priority_reason";
        tc.input = R"TESTCASE(101 102
201
201 database
WEEK 1
1
1 201 Mon10
2
1 101 database Mon10 high 0
2 102 database Mon10 low 0
0
0
0
)TESTCASE";
        tc.totalServed = 1;
        tc.totalBenefit = 17;
        tc.penalty = 1;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(1, 1));
            ew.reasons.push_back(make_pair(2, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "21_spec_case_3";
        tc.input = R"TESTCASE(1 2
201
201 database
WEEK 1
0
0
0
0
0
WEEK 2
0
0
0
0
0
WEEK 3
0
0
0
0
0
WEEK 4
0
0
0
0
0
WEEK 5
0
0
0
0
0
WEEK 6
0
0
0
0
0
WEEK 7
0
1
1 1 database Mon10,Tue14 medium 0
0
0
0
WEEK 8
0
1
2 2 database Mon10,Tue14 medium 0
0
0
0
WEEK 9
0
1
3 1 database Mon10,Tue14 medium 0
0
0
0
WEEK 10
2
1 201 Mon10
2 201 Tue14
0
0
0
0
)TESTCASE";
        tc.totalServed = 2;
        tc.totalBenefit = 34;
        tc.penalty = 4;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 3;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 4;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 5;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 6;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 7;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 8;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            ew.reasons.push_back(make_pair(2, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 9;
            ew.servedCount = 0;
            ew.benefitTotal = 0;
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            ew.reasons.push_back(make_pair(2, string("no feasible slot")));
            ew.reasons.push_back(make_pair(3, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 10;
            ew.servedCount = 2;
            ew.benefitTotal = 34;
            ew.assignments.push_back(make_pair(1, 1));
            ew.assignments.push_back(make_pair(2, 2));
            ew.reasons.push_back(make_pair(3, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "22_random_small_multiweek";
        tc.input = R"TESTCASE(101 102 103 104
201 202 203
201 database api
202 ml backend
203 networking api
WEEK 1
3
1 203 Mon10
2 201 Wed11
3 201 Tue14
4
1 102 database,backend,ml Mon10,Fri15 high 201
2 102 backend,database,ml Fri15 medium 201
3 104 ml,database,networking Thu09,Wed11,Tue14 high 201
4 103 database Mon10,Wed11 medium 202
0
0
0
WEEK 2
3
4 201 Thu09
5 203 Mon10
6 202 Mon10
4
5 103 backend,ml,networking Mon10 high 201
6 103 api Thu09 medium 203
7 103 ml Tue14,Wed11 low 0
8 102 api,backend,networking Wed11,Tue14 low 202
0
0
0
WEEK 3
3
7 201 Tue14
8 201 Wed11
9 202 Wed11
4
9 101 backend Wed11,Tue14,Fri15 medium 203
10 104 ml Tue14 low 202
11 104 networking,ml,database Fri15 medium 0
12 101 api Tue14,Thu09,Wed11 high 203
0
0
0
)TESTCASE";
        tc.totalServed = 8;
        tc.totalBenefit = 118;
        tc.penalty = 15;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 2;
            ew.benefitTotal = 30;
            ew.assignments.push_back(make_pair(3, 3));
            ew.assignments.push_back(make_pair(4, 2));
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            ew.reasons.push_back(make_pair(2, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 3;
            ew.benefitTotal = 50;
            ew.assignments.push_back(make_pair(1, 6));
            ew.assignments.push_back(make_pair(5, 5));
            ew.assignments.push_back(make_pair(6, 4));
            ew.reasons.push_back(make_pair(2, string("no feasible slot")));
            ew.reasons.push_back(make_pair(7, string("no feasible slot")));
            ew.reasons.push_back(make_pair(8, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 3;
            ew.servedCount = 3;
            ew.benefitTotal = 38;
            ew.assignments.push_back(make_pair(8, 7));
            ew.assignments.push_back(make_pair(9, 9));
            ew.assignments.push_back(make_pair(12, 8));
            ew.reasons.push_back(make_pair(2, string("no feasible slot")));
            ew.reasons.push_back(make_pair(7, string("lower priority")));
            ew.reasons.push_back(make_pair(10, string("no feasible slot")));
            ew.reasons.push_back(make_pair(11, string("no feasible slot")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    {
        TestCase tc;
        tc.name = "23_random_with_volatility";
        tc.input = R"TESTCASE(101 102 103
201 202
201 database api
202 ml api
WEEK 1
3
1 201 Tue14
2 201 Mon10
3 202 Wed11
3
1 103 ml,api Mon10 low 0
2 101 api Tue14 high 0
3 103 ml,api Tue14 medium 0
1
2:Mon10
0
0
WEEK 2
3
4 201 Wed11
5 202 Mon10
6 202 Tue14
3
4 102 ml Mon10,Tue14 medium 0
5 102 database,api Mon10,Tue14 low 0
6 103 api,ml Wed11,Tue14 low 0
0
1
4 Wed11 Mon10
0
WEEK 3
3
7 201 Wed11
8 201 Wed11
9 202 Tue14
3
7 101 database,ml Wed11,Tue14 low 0
8 103 ml,api Wed11,Tue14 medium 0
9 102 api Mon10,Wed11 high 0
0
0
0
)TESTCASE";
        tc.totalServed = 7;
        tc.totalBenefit = 94;
        tc.penalty = 5;
        {
            ExpectedWeek ew;
            ew.week = 1;
            ew.servedCount = 1;
            ew.benefitTotal = 17;
            ew.assignments.push_back(make_pair(2, 1));
            ew.reasons.push_back(make_pair(1, string("no feasible slot")));
            ew.reasons.push_back(make_pair(3, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 2;
            ew.servedCount = 3;
            ew.benefitTotal = 37;
            ew.assignments.push_back(make_pair(1, 4));
            ew.assignments.push_back(make_pair(3, 6));
            ew.assignments.push_back(make_pair(4, 5));
            ew.reasons.push_back(make_pair(5, string("lower priority")));
            ew.reasons.push_back(make_pair(6, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        {
            ExpectedWeek ew;
            ew.week = 3;
            ew.servedCount = 3;
            ew.benefitTotal = 40;
            ew.assignments.push_back(make_pair(6, 7));
            ew.assignments.push_back(make_pair(8, 9));
            ew.assignments.push_back(make_pair(9, 8));
            ew.reasons.push_back(make_pair(5, string("lower priority")));
            ew.reasons.push_back(make_pair(7, string("lower priority")));
            tc.weeks.push_back(ew);
        }
        tests.push_back(tc);
    }
    return tests;
}

bool runOneTest(const TestCase& test, bool verboseFailures) {
    SolveResult actual = solveProblemFromString(test.input);
    bool ok = true;

    if ((int)actual.weeks.size() != (int)test.weeks.size()) {
        ok = false;
        if (verboseFailures) {
            cout << "  Wrong number of weeks. Expected " << test.weeks.size()
                 << ", got " << actual.weeks.size() << "\n";
        }
    }

    int sharedWeeks = min((int)actual.weeks.size(), (int)test.weeks.size());

    for (int i = 0; i < sharedWeeks; i++) {
        const WeekResult& got = actual.weeks[i];
        const ExpectedWeek& exp = test.weeks[i];

        if (got.week != exp.week) {
            ok = false;
            if (verboseFailures) {
                cout << "  Week index mismatch at position " << i
                     << ". Expected week " << exp.week << ", got week " << got.week << "\n";
            }
        }

        if (got.servedCount != exp.servedCount) {
            ok = false;
            if (verboseFailures) {
                cout << "  Week " << exp.week << " served_count mismatch. Expected "
                     << exp.servedCount << ", got " << got.servedCount << "\n";
            }
        }

        if (got.benefitTotal != exp.benefitTotal) {
            ok = false;
            if (verboseFailures) {
                cout << "  Week " << exp.week << " benefit_total mismatch. Expected "
                     << exp.benefitTotal << ", got " << got.benefitTotal << "\n";
            }
        }

        if (!sameAssignments(got.assignments, exp.assignments)) {
            ok = false;
            if (verboseFailures) {
                cout << "  Week " << exp.week << " assignments mismatch. Expected ";
                printAssignmentList(exp.assignments);
                cout << ", got ";
                printAssignmentList(got.assignments);
                cout << "\n";
            }
        }

        if (!sameReasons(got.reasons, exp.reasons)) {
            ok = false;
            if (verboseFailures) {
                cout << "  Week " << exp.week << " reasons mismatch. Expected ";
                printReasonList(exp.reasons);
                cout << ", got ";
                printReasonList(got.reasons);
                cout << "\n";
            }
        }
    }

    if (actual.totalServed != test.totalServed) {
        ok = false;
        if (verboseFailures) {
            cout << "  total_served mismatch. Expected " << test.totalServed
                 << ", got " << actual.totalServed << "\n";
        }
    }

    if (actual.totalBenefit != test.totalBenefit) {
        ok = false;
        if (verboseFailures) {
            cout << "  total_benefit mismatch. Expected " << test.totalBenefit
                 << ", got " << actual.totalBenefit << "\n";
        }
    }

    if (actual.penalty != test.penalty) {
        ok = false;
        if (verboseFailures) {
            cout << "  penalty mismatch. Expected " << test.penalty
                 << ", got " << actual.penalty << "\n";
        }
    }

    return ok;
}

void runSelfTests() {
    vector<TestCase> tests = buildTests();
    int passed = 0;

    cout << "Running " << tests.size() << " self-checking tests...\n";

    for (int i = 0; i < (int)tests.size(); i++) {
        bool ok = runOneTest(tests[i], true);
        if (ok) {
            passed++;
            cout << "PASS " << tests[i].name << "\n";
        } else {
            cout << "FAIL " << tests[i].name << "\n";
        }
    }

    cout << "\nSummary: " << passed << "/" << tests.size() << " tests passed.\n";
}

bool isOnlyWhitespace(const string& text) {
    for (int i = 0; i < (int)text.size(); i++) {
        char c = text[i];
        if (!(c == ' ' || c == '\n' || c == '\t' || c == '\r')) return false;
    }
    return true;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string input = "";
    string line;
    while (getline(cin, line)) {
        input += line;
        input += '\n';
    }

    if (isOnlyWhitespace(input)) {
        runSelfTests();
        return 0;
    }

    stringstream tokenReader(input);
    string firstToken;
    tokenReader >> firstToken;

    if (firstToken == "RUN_TESTS") {
        runSelfTests();
        return 0;
    }

    SolveResult result = solveProblemFromString(input);
    printSolveResult(result);
    return 0;
}
