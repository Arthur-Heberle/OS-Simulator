#pragma once

#include <string>
#include <vector>

// Relógio global, em ticks (D3). long long: simulações longas (100 000+ ticks
// com períodos grandes) não estouram; o parser limita cada número a 10^9.
using Tick = long long;

// Uma tarefa periódica termina depois de concluir 10 ocorrências [A4.4] (D6).
constexpr int PERIODIC_ACTIVATION_LIMIT = 10;

// Estados da tarefa (D1). Cada estado tem uma semântica própria (resposta do
// professor Q3), e o Gantt desenha cada um de um jeito (D2).
enum class TaskState {
    NotArrived,  // ainda não houve a primeira ativação (antes do 'arrival')
    Ready,       // tem ocorrência pendente e espera CPU na fila global
    Running,     // executando em uma CPU
    Dormant,     // concluiu a ocorrência do período e espera a próxima ativação; não concorre à CPU
    Suspended,   // impedida de executar: manual no A; mutex/E/S no B (motivo em blockReason)
    Terminated   // concluiu as 10 ocorrências (ou foi encerrada manualmente)
};

// Tipos de tarefa e de servidor: só o Projeto B usa os demais valores (ganchos, design.md §14).
enum class TaskKind   { Periodic, Aperiodic, Sporadic, Server };
enum class ServerType { None, BS, PS, DS, SS };

// Uma ocorrência (job) de uma tarefa periódica (D5). Se o próximo período
// chegar antes de a ocorrência atual terminar, a nova é ENFILEIRADA (não
// descartada): a tarefa que perde o prazo continua executando [A3.3].
struct Job {
    Tick release = 0;      // instante em que a ocorrência foi ativada
    Tick absDeadline = 0;  // release + prazo relativo: chave do EDF e desempate (2) [A4.3]
    Tick duration = 0;     // tempo de execução desta ocorrência
    Tick remaining = 0;    // quanto falta executar
    bool missed = false;   // perda de prazo já registrada (D9: uma vez por ocorrência)
};

// Task control block (TCB): keeps all the information of one task
// (before, during and after the simulation) in a single structure. [A1.3]
// Só dados, sem ponteiros: o SystemSnapshot inteiro é copiado a cada tick.
struct Task {
    // --- campos do arquivo de configuração ---
    std::string id;                // Unique task identifier (text: Project B uses ids like "BS01")
    std::string color = "FFFFFF";  // RGB hexadecimal color (e.g. "F0E0D0"), sempre em maiúsculas
    Tick arrival = 0;              // Tick at which the task is created
    Tick duration = 0;             // Total execution time needed (por ocorrência)
    Tick period = 0;               // Activation period (> 0 periodic, 0 aperiodic) [B1.2: guardado no TCB]
    Tick deadline = 0;             // Deadline, counted from each activation (vazio/0 no arquivo => período, D8)
    std::string rawEvents;         // lista_eventos crua (D30); o Projeto B interpreta
    TaskKind kind = TaskKind::Periodic;
    ServerType server = ServerType::None;
    bool ignored = false;          // fora do escopo do A (aperiódica, D7): carregada, mas não simulada
    std::string ignoredReason;     // explica ao usuário por que foi ignorada

    // --- estado dinâmico ---
    TaskState state = TaskState::NotArrived;
    int cpu = -1;                  // índice em SystemSnapshot::cpus onde executa; -1 = nenhuma
    int lastCpu = -1;              // última CPU usada (afinidade e migração, D10)
    std::vector<Job> jobs;         // ocorrências pendentes; frente = corrente (NÃO usar std::deque)
    Tick nextRelease = 0;          // próxima ativação; começa em 'arrival'
    int released = 0;              // ocorrências já ativadas (limite PERIODIC_ACTIVATION_LIMIT)
    int completed = 0;             // ocorrências já concluídas

    // --- estatísticas (painel e Gantt) ---
    Tick runTicks = 0, waitTicks = 0;
    Tick firstStart = -1, finishTime = -1;
    int preemptions = 0, migrations = 0, deadlineMisses = 0;

    // --- ganchos do Projeto B (presentes, sem uso no A) ---
    int basePriority = 0, effPriority = 0;  // herança/teto de prioridade
    std::string blockReason;                // causa da suspensão (manual, mutex, E/S)
    Tick serverBudget = 0;                  // orçamento de servidor
};

// Ocorrência corrente da tarefa, ou nullptr se não houver nenhuma pendente.
inline const Job* currentJob(const Task& t) {
    return t.jobs.empty() ? nullptr : &t.jobs.front();
}

// Candidata a CPU neste instante: pronta ou executando, e simulada no A.
inline bool isCandidate(const Task& t) {
    return !t.ignored && (t.state == TaskState::Ready || t.state == TaskState::Running);
}
