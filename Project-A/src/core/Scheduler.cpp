#include "core/Scheduler.h"
#include <algorithm>

// Priority key of a task for one algorithm: the smaller, the higher the priority.
// This is the ONLY thing that changes between algorithms. [A4.2]
using KeyFn = Tick (*)(const Task&);

static Tick rmKey (const Task& t) { return t.period; }                     // RM: fixed priority
static Tick edfKey(const Task& t) { return t.jobs.front().absDeadline; }   // EDF: dynamic priority (prazo absoluto da ocorrência corrente)

// Compares two tasks using the algorithm key and then the tie-break criteria [A4.3].
// Returns -1 if "a" goes first, 1 if "b" goes first and 0 if they are still tied.
// The draw (criterion 5) is NOT done here: a random result would break std::sort.
static int compareTasks(const Task& a, const Task& b, KeyFn key) {
    if (key(a) != key(b)) return key(a) < key(b) ? -1 : 1;

    // (1) The task that was already executing goes first (avoids needless switches)
    bool aRunning = a.state == TaskState::Running;
    bool bRunning = b.state == TaskState::Running;
    if (aRunning != bRunning) return aRunning ? -1 : 1;

    // (2) Smaller deadline (relative: the file field, see CLAUDE.md pending item 5)
    if (a.deadline != b.deadline) return a.deadline < b.deadline ? -1 : 1;

    // (3) Smaller arrival
    if (a.arrival != b.arrival) return a.arrival < b.arrival ? -1 : 1;

    // (4) Smaller duration
    if (a.duration != b.duration) return a.duration < b.duration ? -1 : 1;

    return 0;
}

// Candidates = ready or executing tasks (returns their indexes in s.tasks).
// New, suspended, waiting and terminated tasks do not compete for a CPU.
static std::vector<int> candidates(const SystemSnapshot& s) {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(s.tasks.size()); i++) {
        TaskState state = s.tasks[i].state;
        if (state == TaskState::Ready || state == TaskState::Running)
            result.push_back(i);
    }
    return result;
}

// Sorts the candidates by priority and puts the tied ones in the same group.
static PriorityGroups orderAndGroup(const SystemSnapshot& s, KeyFn key) {
    std::vector<int> order = candidates(s);

    // stable_sort: tied tasks keep their original order, so the result is always the same
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return compareTasks(s.tasks[a], s.tasks[b], key) < 0;
    });

    PriorityGroups groups;
    for (int index : order) {
        // The list is sorted, so a tie can only be with the last group
        bool tied = !groups.empty() &&
                    compareTasks(s.tasks[groups.back().front()], s.tasks[index], key) == 0;
        if (tied)
            groups.back().push_back(index);
        else
            groups.push_back({index});
    }
    return groups;
}

PriorityGroups scheduleRM(const SystemSnapshot& s) {
    return orderAndGroup(s, rmKey);
}

PriorityGroups scheduleEDF(const SystemSnapshot& s) {
    return orderAndGroup(s, edfKey);
}

const std::map<std::string, SchedulerFn>& schedulerRegistry() {
    static const std::map<std::string, SchedulerFn> registry = {
        {"RM",  scheduleRM},
        {"EDF", scheduleEDF},
    };
    return registry;
}
