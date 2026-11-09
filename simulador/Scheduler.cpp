// =============================================================================
// Scheduler.cpp — RM, EDF, critérios de desempate [A4.3], registro de
// algoritmos [A4.2] e hash do sorteio (D14). Contrato em Scheduler.h.
// =============================================================================
#include "Scheduler.h"
#include <algorithm>
#include <cctype>
#include <map>

static Tick rmKey (const Task& t) { return t.period; }                     // RM: prioridade fixa
static Tick edfKey(const Task& t) { return t.jobs.front().absDeadline; }   // EDF: prazo absoluto da ocorrência corrente

// Compara duas candidatas pela chave do algoritmo e depois pelos critérios de
// desempate do A4.3, nessa ordem. Devolve -1 se 'a' vem antes, 1 se 'b' vem
// antes e 0 se continuam empatadas. O sorteio (5) não entra aqui: um
// comparador aleatório tornaria o std::sort indefinido.
static int compareTasks(const Task& a, const Task& b, KeyFn key) {
    if (key(a) != key(b)) return key(a) < key(b) ? -1 : 1;

    // (1) A que já executava: evita uma troca de contexto desnecessária.
    bool aRunning = a.state == TaskState::Running;
    bool bRunning = b.state == TaskState::Running;
    if (aRunning != bRunning) return aRunning ? -1 : 1;

    // (2) Menor prazo absoluto da ocorrência corrente (Q11). Toda candidata
    // tem ocorrência pendente, então jobs.front() existe.
    Tick aDeadline = a.jobs.front().absDeadline;
    Tick bDeadline = b.jobs.front().absDeadline;
    if (aDeadline != bDeadline) return aDeadline < bDeadline ? -1 : 1;

    // (3) Menor instante de ingresso.
    if (a.arrival != b.arrival) return a.arrival < b.arrival ? -1 : 1;

    // (4) Menor duração.
    if (a.duration != b.duration) return a.duration < b.duration ? -1 : 1;

    return 0;
}

// Índices das tarefas que disputam CPU (isCandidate: prontas ou executando,
// não ignoradas).
static std::vector<int> candidates(const SystemSnapshot& s) {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(s.tasks.size()); i++) {
        if (isCandidate(s.tasks[i]))
            result.push_back(i);
    }
    return result;
}

PriorityGroups orderAndGroup(const SystemSnapshot& s, KeyFn key) {
    std::vector<int> order = candidates(s);

    // stable_sort mantém a ordem original entre empatadas: resultado determinístico.
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return compareTasks(s.tasks[a], s.tasks[b], key) < 0;
    });

    PriorityGroups groups;
    for (int index : order) {
        // Com a lista ordenada, um empate só pode ser com o último grupo.
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

// Nome em maiúsculas, a forma usada como chave do registro [A3.3.2].
static std::string toUpperName(std::string name) {
    for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return name;
}

// Mapa nome -> função. Variável estática local: criada na primeira chamada,
// sem depender da ordem de inicialização de globais entre arquivos. Um
// algoritmo embutido novo é uma função-chave e uma linha nesta tabela.
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
    for (const auto& entry : registry()) names.push_back(entry.first);  // std::map já vem ordenado
    return names;
}

// splitmix64 (Steele, Lea e Flood, 2014): curto, espalha bem os bits e dá o
// mesmo resultado em qualquer compilador, o que std::uniform_int_distribution
// não garante. Os três parâmetros são misturados em sequência.
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
