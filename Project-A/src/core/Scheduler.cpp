#include "core/Scheduler.h"
#include <algorithm>

// Candidatas = tarefas prontas ou executando. Suspensas e em espera não entram.
static std::vector<int> candidates(const SystemSnapshot& s) {
    std::vector<int> result;
    for (auto& task : s.tasks){
        if (task.state == TaskState::Executing || 
            task.state == TaskState::Ready)
            result.emplace_back(task.id);
    }
    return result;
}

std::vector<int> scheduleRM(const SystemSnapshot& s) {
    std::vector<int> order = candidates(s);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const Task& ta = s.tasks[a];
        const Task& tb = s.tasks[b];
        if (ta.period != tb.period) return ta.period < tb.period;  // RM: menor período primeiro
        if ((ta.state == TaskState::Executing && tb.state != TaskState::Executing) ||
             ta.state != TaskState::Executing && tb.state == TaskState::Executing) {
            return ta.state == TaskState::Executing;
        } 
        if (ta.deadline != tb.deadline) return ta.deadline < tb.deadline;
        if (ta.arrival  != tb.arrival ) return ta.arrival  < ta.arrival;
        if (ta.duration != tb.duration) return ta.duration < tb.duration;

        std::bernoulli_distribution data(0.5); // 50% chance 
        bool choose_task_a = data(s.rng);
        
        return choose_task_a; // [A4.3]
    });
    return order;
}

std::vector<int> scheduleEDF(const SystemSnapshot& s) {
    // igual ao RM, comparando absoluteDeadline em vez de period
    return {};
}

const std::map<std::string, SchedulerFn>& schedulerRegistry() {
    static const std::map<std::string, SchedulerFn> registry = {
        {"RM",  scheduleRM},
        {"EDF", scheduleEDF},
    };
    return registry;
}