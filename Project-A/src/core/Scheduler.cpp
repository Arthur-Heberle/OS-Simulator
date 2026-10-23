#include "core/Scheduler.h"
#include <algorithm>

// Candidatas = tarefas prontas ou executando. Suspensas e em espera não entram.
static std::vector<int> candidates(const SystemSnapshot& s) {
    std::vector<int> result;
    // TODO: percorrer s.tasks e incluir os índices das prontas/executando
    return result;
}

std::vector<int> scheduleRM(const SystemSnapshot& s) {
    std::vector<int> order = candidates(s);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const Task& ta = s.tasks[a];
        const Task& tb = s.tasks[b];
        if (ta.period != tb.period) return ta.period < tb.period;  // RM: menor período primeiro
        return false;  // TODO: critérios de desempate (1) a (4) do [A4.3]
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