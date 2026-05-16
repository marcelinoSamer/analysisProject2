// Thin stdin/stdout adapter around AI_Solution/main.cpp.
//
// AI_Solution/main.cpp is a unit-test runner — the actual scheduler is the
// ScarcityAwareGreedySolver class defined in that file. We don't want to
// modify the original (you asked us to use the existing backend as-is) so
// this adapter pulls the whole translation unit in with the test runner's
// main() renamed out of the way, then exposes the solver via the same
// `F M S R / fellows / mentors / slots / requests` stdin format used by the
// Human_Solution baseline + improved backends and prints the same
// `Request i: Assigned to Slot j (Mentor ID: m, Week: w)` output lines.
//
// All AI types/identifiers are pulled in via the `#include` below.

#define main ai_solution_runner_main_unused_
#include "../../AI_Solution/main.cpp"
#undef main

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string to_lower(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int F, M, S, R;
    if (!(std::cin >> F >> M >> S >> R)) return 1;

    // Fellow IDs are integers in the Human_Solution format; the AI backend
    // expects strings, so we prefix them. The reverse mapping is trivial
    // because we only ever look them up against requests we've already
    // translated the same way.
    std::vector<std::string> fellowIds(F);
    for (int i = 0; i < F; i++) {
        int fid; std::cin >> fid;
        fellowIds[i] = "F" + std::to_string(fid);
    }

    std::unordered_map<std::string, Mentor> mentors;
    for (int i = 0; i < M; i++) {
        int mid, nSpec;
        std::cin >> mid >> nSpec;
        Mentor mentor;
        mentor.id = "M" + std::to_string(mid);
        for (int j = 0; j < nSpec; j++) {
            std::string s; std::cin >> s;
            mentor.specialties.insert(to_lower(s));
        }
        mentors[mentor.id] = mentor;
    }

    std::vector<Slot> slots(S);
    std::vector<int> slotMentorIdNum(S);
    std::vector<int> slotWeek(S);
    for (int i = 0; i < S; i++) {
        int mentorIdNum, week;
        std::cin >> mentorIdNum >> week;
        slots[i].id = "S" + std::to_string(i);
        slots[i].mentorId = "M" + std::to_string(mentorIdNum);
        slots[i].timeBlock = std::to_string(week);
        slotMentorIdNum[i] = mentorIdNum;
        slotWeek[i] = week;
    }

    std::vector<Request> requests(R);
    for (int i = 0; i < R; i++) {
        int fellowIdNum, nSlots;
        std::cin >> fellowIdNum >> nSlots;
        requests[i].fellowId = "F" + std::to_string(fellowIdNum);
        requests[i].availableSlotIds.reserve(nSlots);
        for (int j = 0; j < nSlots; j++) {
            int sid; std::cin >> sid;
            requests[i].availableSlotIds.push_back("S" + std::to_string(sid));
        }
        std::string topic, urgency;
        int week;
        std::cin >> topic >> urgency >> week;
        requests[i].requestedTopic = to_lower(topic);
        requests[i].urgency = urgency;
        // The AI solver uses `timestamp` purely as a tie-breaker. The
        // Human_Solution format only carries a per-request `week`, so we
        // reuse it as the timestamp — the relative ordering is preserved
        // and that's all the tie-breaker cares about.
        requests[i].timestamp = week;
    }

    ScarcityAwareGreedySolver solver(fellowIds, mentors);
    WeeklyResult result = solver.solveWeek(slots, requests);

    for (int i = 0; i < R; i++) {
        const auto &req = requests[i];
        auto it = result.assignment.find(req.fellowId);
        if (it == result.assignment.end()) {
            std::cout << "Request " << i << ": Not Assigned\n";
            continue;
        }
        // it->second is "S<idx>" by construction.
        int sIdx = std::stoi(it->second.substr(1));
        std::cout << "Request " << i << ": Assigned to Slot " << sIdx
                  << " (Mentor ID: " << slotMentorIdNum[sIdx]
                  << ", Week: " << slotWeek[sIdx] << ")\n";
    }
    return 0;
}
