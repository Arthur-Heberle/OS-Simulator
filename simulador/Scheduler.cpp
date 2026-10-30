// =============================================================================
// Scheduler.cpp — RM, EDF, critérios de desempate (A4.3), registro de
// algoritmos (A4.2) e o hash do sorteio (D14). Ver o cabeçalho de Scheduler.h.
// =============================================================================
#include "Scheduler.h"
#include <algorithm>
#include <cctype>
#include <map>

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

    // (2) Menor prazo ABSOLUTO da ocorrência corrente (resposta do professor Q11, D14).
    // Candidatas sempre têm ocorrência pendente, então jobs.front() existe.
    Tick aDeadline = a.jobs.front().absDeadline;
    Tick bDeadline = b.jobs.front().absDeadline;
    if (aDeadline != bDeadline) return aDeadline < bDeadline ? -1 : 1;

    // (3) Smaller arrival
    if (a.arrival != b.arrival) return a.arrival < b.arrival ? -1 : 1;

    // (4) Smaller duration
    if (a.duration != b.duration) return a.duration < b.duration ? -1 : 1;

    return 0;
}

// Candidates = ready or executing tasks (returns their indexes in s.tasks).
// New, suspended, waiting and terminated tasks do not compete for a CPU.
// Tarefas ignoradas no Projeto A (D7) também não (isCandidate, Task.h).
static std::vector<int> candidates(const SystemSnapshot& s) {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(s.tasks.size()); i++) {
        if (isCandidate(s.tasks[i]))
            result.push_back(i);
    }
    return result;
}

// Sorts the candidates by priority and puts the tied ones in the same group.
PriorityGroups orderAndGroup(const SystemSnapshot& s, KeyFn key) {
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

// ---------------------------------------------------------------------------
// Registro [A4.2]
// ---------------------------------------------------------------------------

static std::string toUpperName(std::string name) {
    for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return name;
}

// Mapa nome -> função. É um static LOCAL: é criado na primeira chamada, já com
// os algoritmos embutidos, o que evita o problema de ordem de inicialização de
// variáveis globais entre arquivos. Para incluir um algoritmo embutido novo:
// escrever a função-chave e acrescentar uma linha aqui.
static std::map<std::string, SchedulerFn>& registry() {
    static std::map<std::string, SchedulerFn> table = {
        {"RM",  scheduleRM},
        {"EDF", scheduleEDF},
    };
    return table;
}

void registerScheduler(const std::string& name, SchedulerFn fn) {
    registry()[toUpperName(name)] = fn;
}

SchedulerFn findScheduler(const std::string& name) {
    auto it = registry().find(toUpperName(name));
    return it == registry().end() ? nullptr : it->second;
}

std::vector<std::string> schedulerNames() {
    std::vector<std::string> names;
    for (const auto& entry : registry()) names.push_back(entry.first);  // std::map já é ordenado
    return names;
}

// splitmix64 (Steele, Lea e Flood, 2014): poucas linhas, espalha bem os bits
// e dá o mesmo resultado em qualquer compilador (std::uniform_int_distribution
// não dá). Cada parâmetro é misturado em sequência.
uint64_t tieHash(uint64_t seed, Tick now, int taskIndex) {
    auto mix = [](uint64_t x) {
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    };
    uint64_t h = mix(seed);
    h = mix(h ^ static_cast<uint64_t>(now));
    h = mix(h ^ static_cast<uint64_t>(taskIndex));
    return h;
}
