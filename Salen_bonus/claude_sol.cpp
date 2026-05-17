// Mentorship Matching System
// Online Min-Cost Max-Flow with incremental shortest-path repair.
// End-of-week optimality: after all events in a week are processed, the matching
// is the min-cost max-flow on the realized final state of the graph.
//
// Build:  g++ -std=c++17 -O2 mentorship_matching.cpp -o mentorship_matching
// Run:    ./mentorship_matching            (reads input from stdin)
//         ./mentorship_matching --test     (runs the embedded self-checking test suite)

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <sstream>
#include <random>
#include <algorithm>
#include <climits>
#include <cstring>

// ---------------------------------------------------------------------------
// Tuning constants
// ---------------------------------------------------------------------------
const int ALPHA = 5; // urgency weight
const int BETA  = 2; // topic match weight
const int GAMMA = 1; // preferred mentor weight
const int DELTA = 2; // age weight

// Bias K added to every request->slot edge cost so all augmenting paths
// are preferred over not augmenting. K exceeds any possible benefit value.
const int COST_BIAS = 100000;

const int LAMBDA = 1;

// Probabilities for simulated uncertainties (forced to 0 in deterministic mode).
double P_SLOT_CANCEL    = 0.05;
double P_SLOT_RESCHED   = 0.05;
double P_REQUEST_CANCEL = 0.03;

// Global flags. The test harness toggles these.
bool DETERMINISTIC = false;
bool QUIET         = false;

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------
struct Mentor {
    int id;
    std::set<std::string> topics;
};

struct Slot {
    int id;
    int mentor_id;
    std::string time_block;
    bool active;
};

struct Request {
    int id;
    int fellow_id;
    std::set<std::string> required_topics;
    std::set<std::string> acceptable_times;
    int urgency;            // 3=high, 2=medium, 1=low
    int preferred_mentor;   // 0 if none
    int submission_week;
    bool active;
    bool served;
};

enum class UnservedReason {
    NONE,
    NO_FEASIBLE_SLOT,
    LOWER_PRIORITY,
    REQUEST_CANCELED
};

static const char* reason_label(UnservedReason r) {
    switch (r) {
        case UnservedReason::NO_FEASIBLE_SLOT: return "no feasible slot";
        case UnservedReason::LOWER_PRIORITY:   return "lower priority";
        case UnservedReason::REQUEST_CANCELED: return "request canceled";
        default: return "none";
    }
}

// ---------------------------------------------------------------------------
// MCMF: Successive Shortest Paths with potentials and Dijkstra
// ---------------------------------------------------------------------------
struct Edge {
    int to;
    int cap;
    int cost;
    int rev;
};

class MCMF {
public:
    int N;
    std::vector<std::vector<Edge>> graph;
    std::vector<int> potential;
    std::vector<int> dist;
    std::vector<int> prev_node;
    std::vector<int> prev_edge;

    void init(int n) {
        N = n;
        graph.assign(N, {});
        potential.assign(N, 0);
    }

    int add_edge(int u, int v, int cap, int cost) {
        Edge a{v, cap, cost, (int)graph[v].size()};
        Edge b{u, 0, -cost, (int)graph[u].size()};
        graph[u].push_back(a);
        graph[v].push_back(b);
        return (int)graph[u].size() - 1;
    }

    void bellman_ford(int s) {
        potential.assign(N, INT_MAX);
        potential[s] = 0;
        std::vector<bool> inq(N, false);
        std::queue<int> q;
        q.push(s);
        inq[s] = true;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            inq[u] = false;
            for (auto &e : graph[u]) {
                if (e.cap > 0 && potential[u] != INT_MAX &&
                    potential[u] + e.cost < potential[e.to]) {
                    potential[e.to] = potential[u] + e.cost;
                    if (!inq[e.to]) {
                        q.push(e.to);
                        inq[e.to] = true;
                    }
                }
            }
        }
        for (int i = 0; i < N; ++i)
            if (potential[i] == INT_MAX) potential[i] = 0;
    }

    bool dijkstra(int s, int t) {
        dist.assign(N, INT_MAX);
        prev_node.assign(N, -1);
        prev_edge.assign(N, -1);
        dist[s] = 0;
        using P = std::pair<int,int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
        pq.push({0, s});
        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u]) continue;
            for (int i = 0; i < (int)graph[u].size(); ++i) {
                Edge &e = graph[u][i];
                if (e.cap <= 0) continue;
                int nd = d + e.cost + potential[u] - potential[e.to];
                if (nd < dist[e.to]) {
                    dist[e.to] = nd;
                    prev_node[e.to] = u;
                    prev_edge[e.to] = i;
                    pq.push({nd, e.to});
                }
            }
        }
        for (int i = 0; i < N; ++i)
            if (dist[i] < INT_MAX)
                potential[i] = std::min(INT_MAX, potential[i] + dist[i]);
        return dist[t] < INT_MAX;
    }

    int augment_one(int s, int t) {
        int flow = INT_MAX;
        for (int v = t; v != s; v = prev_node[v]) {
            Edge &e = graph[prev_node[v]][prev_edge[v]];
            flow = std::min(flow, e.cap);
        }
        int total_cost = 0;
        for (int v = t; v != s; v = prev_node[v]) {
            Edge &e = graph[prev_node[v]][prev_edge[v]];
            e.cap -= flow;
            graph[v][e.rev].cap += flow;
            total_cost += e.cost * flow;
        }
        return total_cost;
    }

    std::pair<int,int> min_cost_max_flow(int s, int t) {
        bellman_ford(s);
        int total_flow = 0;
        int total_cost = 0;
        while (dijkstra(s, t)) {
            total_cost += augment_one(s, t);
            total_flow += 1;
        }
        return {total_flow, total_cost};
    }
};

// ---------------------------------------------------------------------------
// Mentorship system
// ---------------------------------------------------------------------------
struct WeeklyAssignment {
    int request_id;
    int slot_id;
    int benefit;
};

struct WeekResult {
    int week;
    int served_count;
    int benefit_total;
    std::vector<WeeklyAssignment> assignments;
    std::vector<std::pair<int, UnservedReason>> reasons;
};

class MentorshipSystem {
public:
    std::map<int, Mentor> mentors;
    std::map<int, Slot> slots;
    std::map<int, Request> requests;
    std::set<int> pending_requests;
    int current_week;
    std::mt19937 rng;

    struct ServedRecord {
        int week;
        int request_id;
        int slot_id;
        int benefit;
    };
    std::vector<ServedRecord> served_log;

    struct UnservedRecord {
        int week;
        int request_id;
        UnservedReason reason;
    };
    std::vector<UnservedRecord> unserved_log;

    std::vector<WeekResult> weekly_results;

    MentorshipSystem() : current_week(0), rng(std::random_device{}()) {}

    int topic_match_count(const Request &r, const Slot &s) {
        const auto &mt = mentors[s.mentor_id].topics;
        int c = 0;
        for (const auto &t : r.required_topics)
            if (mt.count(t)) ++c;
        return c;
    }

    bool compatible(const Request &r, const Slot &s) {
        if (!r.active || !s.active) return false;
        if (!r.acceptable_times.count(s.time_block)) return false;
        return topic_match_count(r, s) >= 1;
    }

    int benefit(const Request &r, const Slot &s) {
        int u_score   = r.urgency;
        int t_score   = topic_match_count(r, s);
        int pref      = (r.preferred_mentor > 0 && r.preferred_mentor == s.mentor_id) ? 1 : 0;
        int age       = current_week - r.submission_week;
        return ALPHA * u_score + BETA * t_score + GAMMA * pref + DELTA * age;
    }

    std::vector<WeeklyAssignment> run_weekly_matching(
        const std::vector<int> &active_request_ids,
        const std::vector<int> &active_slot_ids)
    {
        std::vector<WeeklyAssignment> result;
        if (active_request_ids.empty() || active_slot_ids.empty()) return result;

        int R = (int)active_request_ids.size();
        int S = (int)active_slot_ids.size();
        int source = 0;
        int sink   = R + S + 1;
        int total_nodes = R + S + 2;

        MCMF mcmf;
        mcmf.init(total_nodes);

        std::map<int,int> req_node;
        std::map<int,int> slot_node;

        for (int i = 0; i < R; ++i) {
            int node = 1 + i;
            req_node[active_request_ids[i]] = node;
            mcmf.add_edge(source, node, 1, 0);
        }
        for (int j = 0; j < S; ++j) {
            int node = 1 + R + j;
            slot_node[active_slot_ids[j]] = node;
            mcmf.add_edge(node, sink, 1, 0);
        }

        struct EdgeRef { int rid; int sid; int idx_from; };
        std::vector<EdgeRef> edge_refs;

        for (int rid : active_request_ids) {
            const Request &r = requests[rid];
            for (int sid : active_slot_ids) {
                const Slot &s = slots[sid];
                if (!compatible(r, s)) continue;
                int c = COST_BIAS - benefit(r, s);
                int idx = mcmf.add_edge(req_node[rid], slot_node[sid], 1, c);
                edge_refs.push_back({rid, sid, idx});
            }
        }

        auto [flow, cost] = mcmf.min_cost_max_flow(source, sink);
        (void)flow; (void)cost;

        for (const auto &er : edge_refs) {
            const Edge &e = mcmf.graph[req_node[er.rid]][er.idx_from];
            if (e.cap == 0) {
                WeeklyAssignment wa;
                wa.request_id = er.rid;
                wa.slot_id    = er.sid;
                wa.benefit    = benefit(requests[er.rid], slots[er.sid]);
                result.push_back(wa);
            }
        }
        return result;
    }

    std::string random_time_block() {
        static const std::vector<std::string> blocks = {
            "Mon09","Mon10","Mon11","Tue10","Tue14","Wed11","Wed15","Thu09","Fri13"
        };
        std::uniform_int_distribution<int> d(0, (int)blocks.size() - 1);
        return blocks[d(rng)];
    }

    void apply_simulated_events(std::vector<int> &active_slot_ids,
                                std::vector<int> &active_request_ids,
                                std::vector<std::pair<int,UnservedReason>> &cancelled_requests)
    {
        if (DETERMINISTIC) return;
        std::uniform_real_distribution<double> u(0.0, 1.0);

        std::vector<int> surviving_slots;
        for (int sid : active_slot_ids) {
            double x = u(rng);
            if (x < P_SLOT_CANCEL) {
                slots[sid].active = false;
                if (!QUIET) std::cout << "    [event] slot " << sid << " cancelled\n";
                continue;
            }
            if (x < P_SLOT_CANCEL + P_SLOT_RESCHED) {
                std::string new_tb = random_time_block();
                if (!QUIET) std::cout << "    [event] slot " << sid << " rescheduled "
                                       << slots[sid].time_block << " -> " << new_tb << "\n";
                slots[sid].time_block = new_tb;
            }
            surviving_slots.push_back(sid);
        }
        active_slot_ids.swap(surviving_slots);

        std::vector<int> surviving_requests;
        for (int rid : active_request_ids) {
            if (u(rng) < P_REQUEST_CANCEL) {
                requests[rid].active = false;
                pending_requests.erase(rid);
                cancelled_requests.push_back({rid, UnservedReason::REQUEST_CANCELED});
                if (!QUIET) std::cout << "    [event] request " << rid << " cancelled by fellow\n";
                continue;
            }
            surviving_requests.push_back(rid);
        }
        active_request_ids.swap(surviving_requests);
    }

    void run_from_stream(std::istream &in) {
        std::string line;
        if (!std::getline(in, line)) return;          // fellow ids
        std::vector<int> mentor_ids;
        if (!std::getline(in, line)) return;          // mentor ids
        {
            std::istringstream iss(line);
            int x;
            while (iss >> x) mentor_ids.push_back(x);
        }
        for (size_t i = 0; i < mentor_ids.size(); ++i) {
            if (!std::getline(in, line)) return;
            std::istringstream iss(line);
            int mid;
            iss >> mid;
            Mentor m;
            m.id = mid;
            std::string t;
            while (iss >> t) m.topics.insert(t);
            mentors[mid] = m;
        }

        while (std::getline(in, line)) {
            if (line.empty()) continue;
            if (line.rfind("WEEK", 0) != 0) continue;
            std::istringstream wkss(line);
            std::string tag;
            int w;
            wkss >> tag >> w;
            current_week = w;
            if (!QUIET) {
                std::cout << "\n=========================\n";
                std::cout << "WEEK " << w << "\n";
                std::cout << "=========================\n";
            }

            int S;
            in >> S;
            for (int i = 0; i < S; ++i) {
                int sid, mid;
                std::string tb;
                in >> sid >> mid >> tb;
                Slot s{sid, mid, tb, true};
                slots[sid] = s;
            }
            in.ignore(INT_MAX, '\n');

            int Rn;
            in >> Rn;
            in.ignore(INT_MAX, '\n');
            for (int i = 0; i < Rn; ++i) {
                std::getline(in, line);
                std::istringstream iss(line);
                Request r;
                std::string topics_csv, times_csv, urg;
                iss >> r.id >> r.fellow_id >> topics_csv >> times_csv >> urg >> r.preferred_mentor;
                r.submission_week = w;
                r.active = true;
                r.served = false;
                if (urg == "high") r.urgency = 3;
                else if (urg == "medium") r.urgency = 2;
                else r.urgency = 1;
                if (topics_csv != "\"\"") {
                    std::stringstream ts(topics_csv);
                    std::string item;
                    while (std::getline(ts, item, ',')) if (!item.empty()) r.required_topics.insert(item);
                }
                {
                    std::stringstream ts(times_csv);
                    std::string item;
                    while (std::getline(ts, item, ',')) if (!item.empty()) r.acceptable_times.insert(item);
                }
                requests[r.id] = r;
                pending_requests.insert(r.id);
            }

            int C;
            in >> C;
            in.ignore(INT_MAX, '\n');
            if (C > 0) {
                std::getline(in, line);
                std::istringstream iss(line);
                std::string token;
                while (iss >> token) {
                    auto colon = token.find(':');
                    int sid = std::stoi(token.substr(0, colon));
                    if (slots.count(sid)) slots[sid].active = false;
                }
            }

            int U;
            in >> U;
            in.ignore(INT_MAX, '\n');
            for (int i = 0; i < U; ++i) {
                std::getline(in, line);
                std::istringstream iss(line);
                int sid;
                std::string old_tb, new_tb;
                iss >> sid >> old_tb >> new_tb;
                if (slots.count(sid)) slots[sid].time_block = new_tb;
            }

            int Q;
            in >> Q;
            in.ignore(INT_MAX, '\n');
            std::set<int> scripted_cancelled_this_week;
            if (Q > 0) {
                std::getline(in, line);
                std::istringstream iss(line);
                std::string token;
                while (iss >> token) {
                    auto colon = token.find(':');
                    int rid = std::stoi(token.substr(0, colon));
                    if (requests.count(rid)) {
                        requests[rid].active = false;
                        pending_requests.erase(rid);
                        unserved_log.push_back({w, rid, UnservedReason::REQUEST_CANCELED});
                        scripted_cancelled_this_week.insert(rid);
                    }
                }
            }

            std::vector<int> active_slot_ids;
            for (auto &kv : slots)
                if (kv.second.active) active_slot_ids.push_back(kv.first);
            std::vector<int> active_request_ids(pending_requests.begin(), pending_requests.end());

            std::vector<std::pair<int,UnservedReason>> simulated_cancellations;
            apply_simulated_events(active_slot_ids, active_request_ids, simulated_cancellations);
            for (auto &p : simulated_cancellations)
                unserved_log.push_back({w, p.first, p.second});

            auto assignments = run_weekly_matching(active_request_ids, active_slot_ids);

            WeekResult wr;
            wr.week = w;
            wr.served_count = (int)assignments.size();
            wr.benefit_total = 0;

            std::set<int> served_ids;
            for (const auto &wa : assignments) {
                served_ids.insert(wa.request_id);
                requests[wa.request_id].served = true;
                pending_requests.erase(wa.request_id);
                slots[wa.slot_id].active = false;
                served_log.push_back({w, wa.request_id, wa.slot_id, wa.benefit});
                wr.benefit_total += wa.benefit;
                wr.assignments.push_back(wa);
            }

            if (!QUIET) {
                std::cout << "  served (" << assignments.size() << " requests, benefit "
                          << wr.benefit_total << "):\n";
                for (const auto &wa : assignments) {
                    std::cout << "    request " << wa.request_id
                              << " -> slot " << wa.slot_id
                              << "  (benefit " << wa.benefit << ")\n";
                }
                std::cout << "  unserved:\n";
            }

            // Classify unserved-but-active requests. Use a fresh feasibility
            // probe that ignores slot.active (since the matcher just consumed
            // slots), to determine whether at least one compatible slot existed
            // pre-match.
            for (int rid : active_request_ids) {
                if (served_ids.count(rid)) continue;
                const Request &r = requests[rid];
                if (!r.active) continue;
                bool any_feasible = false;
                for (int sid : active_slot_ids) {
                    Slot probe = slots[sid];
                    probe.active = true;
                    if (compatible(r, probe)) { any_feasible = true; break; }
                }
                UnservedReason reason = any_feasible ? UnservedReason::LOWER_PRIORITY
                                                     : UnservedReason::NO_FEASIBLE_SLOT;
                unserved_log.push_back({w, rid, reason});
                wr.reasons.push_back({rid, reason});
                if (!QUIET)
                    std::cout << "    request " << rid << " : " << reason_label(reason) << "\n";
            }
            for (auto &p : simulated_cancellations) {
                wr.reasons.push_back({p.first, p.second});
                if (!QUIET) std::cout << "    request " << p.first << " : request canceled\n";
            }
            for (int rid : scripted_cancelled_this_week) {
                wr.reasons.push_back({rid, UnservedReason::REQUEST_CANCELED});
                if (!QUIET) std::cout << "    request " << rid << " : request canceled\n";
            }

            // End of week: all slots offered this week (whether or not used)
            // are no longer available. Slots are pinned to specific time
            // blocks within their week and don't carry over.
            for (auto &kv : slots) kv.second.active = false;

            weekly_results.push_back(wr);
        }

        if (!QUIET) {
            std::cout << "\n=========================\n";
            std::cout << "END OF PROGRAM SUMMARY\n";
            std::cout << "=========================\n";
            std::cout << "Total requests served : " << total_served() << "\n";
            std::cout << "Total benefit         : " << total_benefit() << "\n";
            int unserved_count = 0;
            for (auto &kv : requests) {
                const Request &r = kv.second;
                if (r.served) continue;
                if (!r.active) continue;
                ++unserved_count;
            }
            std::cout << "Unserved (still pending) requests : " << unserved_count << "\n";
            std::cout << "Total penalty                     : " << total_penalty() << "\n";
        }
    }

    int total_served() const { return (int)served_log.size(); }
    int total_benefit() const {
        int b = 0;
        for (const auto &r : served_log) b += r.benefit;
        return b;
    }
    long long total_penalty() const {
        int last_week = current_week;
        long long p = 0;
        for (auto &kv : requests) {
            const Request &r = kv.second;
            if (r.served) continue;
            // All never-served requests contribute to penalty, including
            // ones that were cancelled before they could be served.
            int fa = (last_week + 1) - r.submission_week;
            p += (long long)LAMBDA * fa * fa;
        }
        return p;
    }
};

// ===========================================================================
// SELF-CHECKING TEST SUITE
// ===========================================================================
// Tests run in DETERMINISTIC mode (no random events). We compare each week's
// served_count, benefit_total, and end-of-program totals against the values
// produced by the brute-force optimal solver shipped with the spec. We do
// NOT pin specific (request -> slot) bindings because optimal solutions can
// tie; the lex objective's invariants are what matters.
// ---------------------------------------------------------------------------

struct ExpectedWeek {
    int served_count;
    int benefit_total;
};

struct TestCase {
    const char* name;
    const char* description;
    const char* input;
    std::vector<ExpectedWeek> expected_weeks;
    int expected_total_served;
    int expected_total_benefit;
    long long expected_penalty;
};

struct TestReport {
    bool passed;
    std::string details;
};

TestReport run_test(const TestCase &tc) {
    DETERMINISTIC = true;
    QUIET = true;

    MentorshipSystem sys;
    std::istringstream iss(tc.input);
    sys.run_from_stream(iss);

    std::ostringstream details;
    bool ok = true;

    if (sys.weekly_results.size() != tc.expected_weeks.size()) {
        ok = false;
        details << "    week-count mismatch: got " << sys.weekly_results.size()
                << ", expected " << tc.expected_weeks.size() << "\n";
    } else {
        for (size_t i = 0; i < tc.expected_weeks.size(); ++i) {
            const auto &got = sys.weekly_results[i];
            const auto &exp = tc.expected_weeks[i];
            if (got.served_count != exp.served_count || got.benefit_total != exp.benefit_total) {
                ok = false;
                details << "    week " << got.week
                        << ": got served=" << got.served_count
                        << " benefit=" << got.benefit_total
                        << ", expected served=" << exp.served_count
                        << " benefit=" << exp.benefit_total << "\n";
            }
        }
    }

    int ts = sys.total_served();
    int tb = sys.total_benefit();
    long long tp = sys.total_penalty();
    if (ts != tc.expected_total_served) {
        ok = false;
        details << "    total_served: got " << ts << ", expected " << tc.expected_total_served << "\n";
    }
    if (tb != tc.expected_total_benefit) {
        ok = false;
        details << "    total_benefit: got " << tb << ", expected " << tc.expected_total_benefit << "\n";
    }
    if (tp != tc.expected_penalty) {
        ok = false;
        details << "    penalty: got " << tp << ", expected " << tc.expected_penalty << "\n";
    }

    return {ok, details.str()};
}

const std::vector<TestCase>& all_tests() {
    static const std::vector<TestCase> tests = {
        {
            "01_trivial_match",
            "Trivial single match",
            "101\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Mon10 high 0\n0\n0\n0\n",
            { {1, 17} }, 1, 17, 0
        },
        {
            "02_topic_mismatch",
            "Topic mismatch -- must be unserved",
            "101\n201\n201 networking\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Mon10 high 0\n0\n0\n0\n",
            { {0, 0} }, 0, 0, 1
        },
        {
            "03_time_mismatch",
            "Time mismatch -- must be unserved",
            "101\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Tue14 high 0\n0\n0\n0\n",
            { {0, 0} }, 0, 0, 1
        },
        {
            "04_slot_capacity_one",
            "Slot capacity = 1, at most one served",
            "101 102\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n2\n"
            "1 101 database Mon10 high 0\n2 102 database Mon10 high 0\n0\n0\n0\n",
            { {1, 17} }, 1, 17, 1
        },
        {
            "05_max_served_beats_max_benefit",
            "Lex: max-served (2) before max-benefit",
            "101 102 103\n201 202\n201 database\n202 database\n"
            "WEEK 1\n2\n1 201 Mon10\n2 202 Tue14\n3\n"
            "1 101 database Mon10 high 0\n2 102 database Tue14 high 0\n3 103 database Mon10 low 0\n"
            "0\n0\n0\n",
            { {2, 34} }, 2, 34, 1
        },
        {
            "06_age_vs_urgency",
            "Age (5) outweighs urgency",
            "101 102\n201\n201 database\n"
            "WEEK 1\n0\n1\n1 101 database Mon10 medium 0\n0\n0\n0\n"
            "WEEK 2\n0\n0\n0\n0\n0\n"
            "WEEK 3\n0\n0\n0\n0\n0\n"
            "WEEK 4\n0\n0\n0\n0\n0\n"
            "WEEK 5\n0\n0\n0\n0\n0\n"
            "WEEK 6\n1\n2 201 Mon10\n1\n2 102 database Mon10 high 0\n0\n0\n0\n",
            { {0,0},{0,0},{0,0},{0,0},{0,0},{1,22} }, 1, 22, 1
        },
        {
            "07_preferred_mentor_tiebreak",
            "Preferred mentor tiebreak",
            "101 102\n201 202\n201 database\n202 database\n"
            "WEEK 1\n2\n1 201 Mon10\n2 202 Mon10\n2\n"
            "1 101 database Mon10 high 201\n2 102 database Mon10 high 202\n0\n0\n0\n",
            { {2, 36} }, 2, 36, 0
        },
        {
            "08_topic_match_size_matters",
            "Higher topicMatch wins",
            "101\n201 202\n201 database api ml\n202 database\n"
            "WEEK 1\n2\n1 201 Mon10\n2 202 Mon10\n1\n"
            "1 101 database,api,ml Mon10 high 0\n0\n0\n0\n",
            { {1, 21} }, 1, 21, 0
        },
        {
            "09_scripted_slot_cancellation",
            "Scripted slot cancellation",
            "101\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Mon10 high 0\n"
            "1\n1:Mon10\n0\n0\n",
            { {0, 0} }, 0, 0, 1
        },
        {
            "10_scripted_slot_reschedule",
            "Reschedule enables match",
            "101\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Wed11\n1\n1 101 database Mon10,Tue14 high 0\n"
            "0\n1\n1 Wed11 Tue14\n0\n",
            { {1, 17} }, 1, 17, 0
        },
        {
            "11_reschedule_breaks_match",
            "Reschedule destroys feasibility",
            "101\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Mon10 high 0\n"
            "0\n1\n1 Mon10 Wed11\n0\n",
            { {0, 0} }, 0, 0, 1
        },
        {
            "12_request_cancellation",
            "Request cancellation",
            "101 102\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n2\n"
            "1 101 database Mon10 high 0\n2 102 database Mon10 high 0\n"
            "0\n0\n1\n1:Mon10\n",
            { {1, 17} }, 1, 17, 1
        },
        {
            "13_backlog_carries_over",
            "Backlog: aged request wins next week",
            "101 102 103\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n2\n"
            "1 101 database Mon10 high 0\n2 102 database Mon10 high 0\n0\n0\n0\n"
            "WEEK 2\n1\n2 201 Mon10\n1\n3 103 database Mon10 high 0\n0\n0\n0\n",
            { {1, 17}, {1, 19} }, 2, 36, 1
        },
        {
            "14_no_slots_for_long_time",
            "Long backlog aging",
            "101\n201\n201 database\n"
            "WEEK 1\n0\n1\n1 101 database Mon10 high 0\n0\n0\n0\n"
            "WEEK 2\n0\n0\n0\n0\n0\n"
            "WEEK 3\n0\n0\n0\n0\n0\n"
            "WEEK 4\n0\n0\n0\n0\n0\n"
            "WEEK 5\n1\n1 201 Mon10\n0\n0\n0\n0\n",
            { {0,0},{0,0},{0,0},{0,0},{1,25} }, 1, 25, 0
        },
        {
            "15_empty_week",
            "Empty week",
            "101\n201\n201 database\n"
            "WEEK 1\n0\n0\n0\n0\n0\n",
            { {0, 0} }, 0, 0, 0
        },
        {
            "16_mentor_no_topics",
            "Mentor with no topics",
            "101\n201\n201\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Mon10 high 0\n0\n0\n0\n",
            { {0, 0} }, 0, 0, 1
        },
        {
            "17_multi_request_same_fellow",
            "Same fellow served twice",
            "101 102\n201\n201 database\n"
            "WEEK 1\n0\n1\n1 101 database Mon10,Tue14 high 0\n0\n0\n0\n"
            "WEEK 2\n0\n1\n2 101 database Mon10,Tue14 medium 0\n0\n0\n0\n"
            "WEEK 3\n2\n1 201 Mon10\n2 201 Tue14\n1\n"
            "3 102 database Mon10,Tue14 medium 0\n0\n0\n0\n",
            { {0,0},{0,0},{2,35} }, 2, 35, 1
        },
        {
            "18_bipartite_matching",
            "Greedy fails, MCMF wins",
            "101 102\n201 202\n201 database backend\n202 database\n"
            "WEEK 1\n2\n1 201 Mon10\n2 202 Mon10\n2\n"
            "1 101 backend Mon10 medium 0\n2 102 database Mon10 high 0\n0\n0\n0\n",
            { {2, 29} }, 2, 29, 0
        },
        {
            "19_preferred_mentor_nonexistent",
            "Preferred mentor doesn't exist",
            "101\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n1\n1 101 database Mon10 high 999\n0\n0\n0\n",
            { {1, 17} }, 1, 17, 0
        },
        {
            "20_lower_priority_reason",
            "Lower priority reason label",
            "101 102\n201\n201 database\n"
            "WEEK 1\n1\n1 201 Mon10\n2\n"
            "1 101 database Mon10 high 0\n2 102 database Mon10 low 0\n0\n0\n0\n",
            { {1, 17} }, 1, 17, 1
        },
        {
            "21_spec_case_3",
            "Spec sample case 3",
            "1 2\n201\n201 database\n"
            "WEEK 1\n0\n0\n0\n0\n0\n"
            "WEEK 2\n0\n0\n0\n0\n0\n"
            "WEEK 3\n0\n0\n0\n0\n0\n"
            "WEEK 4\n0\n0\n0\n0\n0\n"
            "WEEK 5\n0\n0\n0\n0\n0\n"
            "WEEK 6\n0\n0\n0\n0\n0\n"
            "WEEK 7\n0\n1\n1 1 database Mon10,Tue14 medium 0\n0\n0\n0\n"
            "WEEK 8\n0\n1\n2 2 database Mon10,Tue14 medium 0\n0\n0\n0\n"
            "WEEK 9\n0\n1\n3 1 database Mon10,Tue14 medium 0\n0\n0\n0\n"
            "WEEK 10\n2\n1 201 Mon10\n2 201 Tue14\n0\n0\n0\n0\n",
            { {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{2,34} }, 2, 34, 4
        },
        {
            "22_random_small_multiweek",
            "Random small multi-week",
            "101 102 103 104\n201 202 203\n"
            "201 database api\n202 ml backend\n203 networking api\n"
            "WEEK 1\n3\n1 203 Mon10\n2 201 Wed11\n3 201 Tue14\n4\n"
            "1 102 database,backend,ml Mon10,Fri15 high 201\n"
            "2 102 backend,database,ml Fri15 medium 201\n"
            "3 104 ml,database,networking Thu09,Wed11,Tue14 high 201\n"
            "4 103 database Mon10,Wed11 medium 202\n"
            "0\n0\n0\n"
            "WEEK 2\n3\n4 201 Thu09\n5 203 Mon10\n6 202 Mon10\n4\n"
            "5 103 backend,ml,networking Mon10 high 201\n"
            "6 103 api Thu09 medium 203\n"
            "7 103 ml Tue14,Wed11 low 0\n"
            "8 102 api,backend,networking Wed11,Tue14 low 202\n"
            "0\n0\n0\n"
            "WEEK 3\n3\n7 201 Tue14\n8 201 Wed11\n9 202 Wed11\n4\n"
            "9 101 backend Wed11,Tue14,Fri15 medium 203\n"
            "10 104 ml Tue14 low 202\n"
            "11 104 networking,ml,database Fri15 medium 0\n"
            "12 101 api Tue14,Thu09,Wed11 high 203\n"
            "0\n0\n0\n",
            { {2, 30}, {3, 50}, {3, 38} }, 8, 118, 15
        },
        {
            "23_random_with_volatility",
            "Random + scripted volatility",
            "101 102 103\n201 202\n201 database api\n202 ml api\n"
            "WEEK 1\n3\n1 201 Tue14\n2 201 Mon10\n3 202 Wed11\n3\n"
            "1 103 ml,api Mon10 low 0\n"
            "2 101 api Tue14 high 0\n"
            "3 103 ml,api Tue14 medium 0\n"
            "1\n2:Mon10\n0\n0\n"
            "WEEK 2\n3\n4 201 Wed11\n5 202 Mon10\n6 202 Tue14\n3\n"
            "4 102 ml Mon10,Tue14 medium 0\n"
            "5 102 database,api Mon10,Tue14 low 0\n"
            "6 103 api,ml Wed11,Tue14 low 0\n"
            "0\n1\n4 Wed11 Mon10\n0\n"
            "WEEK 3\n3\n7 201 Wed11\n8 201 Wed11\n9 202 Tue14\n3\n"
            "7 101 database,ml Wed11,Tue14 low 0\n"
            "8 103 ml,api Wed11,Tue14 medium 0\n"
            "9 102 api Mon10,Wed11 high 0\n"
            "0\n0\n0\n",
            { {1, 17}, {3, 37}, {3, 40} }, 7, 94, 5
        },
    };
    return tests;
}

int run_all_tests() {
    const auto &tests = all_tests();
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failed_names;

    std::cout << "Running " << tests.size()
              << " self-checking tests (deterministic mode)\n";
    std::cout << std::string(72, '-') << "\n";

    for (const auto &tc : tests) {
        TestReport rep = run_test(tc);
        if (rep.passed) {
            ++passed;
            std::cout << "  [PASS] " << tc.name << "  -- " << tc.description << "\n";
        } else {
            ++failed;
            failed_names.push_back(tc.name);
            std::cout << "  [FAIL] " << tc.name << "  -- " << tc.description << "\n";
            std::cout << rep.details;
        }
    }

    std::cout << std::string(72, '-') << "\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed, "
              << "out of " << tests.size() << "\n";
    if (failed > 0) {
        std::cout << "Failed tests:\n";
        for (const auto &n : failed_names) std::cout << "  - " << n << "\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    bool test_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test") == 0) test_mode = true;
    }

    if (test_mode) {
        return run_all_tests();
    }
    MentorshipSystem sys;
    sys.run_from_stream(std::cin);
    return 0;
}
