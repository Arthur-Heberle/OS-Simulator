// =============================================================================
// Model.h — Contrato de dados do simulador: tudo o que o motor produz e o
// Gantt/interface consomem (design.md §5).
//
// O quê: os tipos de dados do núcleo — tempo (Tick), tarefa (Task = TCB) e
// suas ocorrências (Job), processador (CPU), eventos desenhados no Gantt
// (Occurrence), o estado completo de um instante (SystemSnapshot), a
// configuração lida do arquivo (Config/ParseResult) e a edição manual (Edit).
//
// Por quê um arquivo só:
//   - é o ponto de acordo entre núcleo e interface: ninguém altera sem avisar
//     a dupla (CLAUDE.md); estar num lugar só deixa isso fácil de revisar;
//   - economiza arquivos no orçamento de entrega (D23, ≤ 18 arquivos).
//
// Regras do contrato: só dados (a lógica fica em funções livres, D21); sem
// ponteiros entre structs (referências por índice), para que um
// SystemSnapshot seja copiado por valor no histórico (D16); nada de Qt.
//
// Requisitos: A1.3 (todas as informações da tarefa numa única estrutura),
// A1.2, A2.2, A3.3.5, A4.3, A4.4. Decisões: D1, D3, D5, D6, D7, D12, D14,
// D21, D24.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Limite de CPUs aceitas no arquivo e na edição [RG2] (D12: justificativa no design.md).
inline constexpr int MAX_CPUS = 64;

// ---------------------------------------------------------------------------
// Tarefa (TCB) e ocorrências
// ---------------------------------------------------------------------------

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
    TaskKind kind = TaskKind::Periodic;     // tipo da tarefa; no A só Periodic é simulada (A4.4, D7)
    ServerType server = ServerType::None;   // tipo de servidor (B2); no A o prefixo do id não é interpretado (Q12)
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
    Tick runTicks = 0, waitTicks = 0;        // ticks executando / ticks pronta sem CPU
    Tick firstStart = -1, finishTime = -1;   // primeiro instante em CPU / instante em que virou Terminated (-1 = ainda não)
    int preemptions = 0, migrations = 0, deadlineMisses = 0;  // contadores mostrados ao usuário (A1.5.1)

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

// ---------------------------------------------------------------------------
// Processador
// ---------------------------------------------------------------------------

// Represents one simulated processor.
struct CPU {
    int id = 0;            // índice da CPU (0..n-1); a interface mostra id+1 ("CPU1")
    int taskIndex = -1;    // Index in snapshot.tasks; -1 = no task (CPU is off) [A1.2]
    bool on = true;        // false = desligada [A1.2]; começa true para registrar só as transições (CpuOff/CpuOn)
    int quantumUsed = 0;   // Consecutive ticks the current task has run (para algoritmos com quantum, D13)
    Tick ticksBusy = 0;    // ticks executando alguma tarefa
    Tick ticksOff = 0;     // Total ticks spent off, shown to the user [A1.2]
};

// ---------------------------------------------------------------------------
// Estado de um instante (histórico e Gantt)
// ---------------------------------------------------------------------------

// Kinds of events the Gantt chart must draw at the exact tick they happen.
// Cada tipo tem um ícone distinto e entra na legenda [RG3] (design.md §10).
enum class OccurrenceType {
    Arrival,       // ocorrência entra na fila de prontos (a CADA ativação, resposta Q5) [A2.2]
    Termination,   // fim de UMA ocorrência (Q5) [A2.2]
    TaskFinished,  // fim da 10ª ocorrência: a tarefa termina [A4.4]
    DeadlineMiss,  // prazo absoluto atingido com execução pendente [A3.3]
    Dispatch,      // tarefa (re)começa numa CPU (só log, sem ícone)
    Preemption,    // tarefa perdeu a CPU para outra mais prioritária
    Migration,     // tarefa voltou a executar em CPU diferente da última (D10)
    TieBreakDraw,  // escolha decidida por sorteio [A4.3]
    CpuOn,         // CPU religada [A1.2]
    CpuOff,        // CPU desligada por falta de tarefa [A1.2]
    ManualEdit     // edição manual do usuário neste instante [A1.5.2] (D15)
};

// One event that happened at the snapshot's tick. It has no tick field
// because it is stored inside the snapshot of that tick.
struct Occurrence {
    OccurrenceType type = OccurrenceType::Arrival;
    int taskIndex = -1;   // Task involved (index in tasks); -1 = none
    int cpuIndex = -1;    // CPU involved (index in cpus); -1 = none
    std::string note;     // texto para log/tooltip (ex.: "enfileirada", descrição da edição)
};

// Copy of the system state at one tick, stored in the history so the
// simulation can go backward and forward.
// Todo o estado mutável mora aqui (D21), inclusive algoritmo, quantum e
// semente: eles podem mudar durante a simulação [A3.1] e, ao retroceder,
// precisam voltar junto com o resto do estado.
struct SystemSnapshot {
    Tick tick = 0;                          // instante já processado (D3)
    std::string algorithm = "RM";           // nome no registro de escalonadores, em maiúsculas
    int quantum = 2;                        // lido do arquivo; RM/EDF ignoram (Q4, D13)
    uint64_t seed = 1;                      // semente do sorteio sem estado (D14): 8 bytes, e não ~2,5 KB do mt19937
    std::vector<Task> tasks;                // índice estável: nunca remover (marcar Terminated)
    std::vector<int> readyQueue;            // index in tasks (global queue) [A1.2]
    std::vector<CPU> cpus;                  // tamanho = nº de CPUs atual (pode mudar por edição, A3.1)
    std::vector<Occurrence> occurrences;    // o que aconteceu NESTE instante
};

// ---------------------------------------------------------------------------
// Configuração lida do arquivo
// ---------------------------------------------------------------------------

// System configuration. The initial values are the defaults [A3.2].
// Resultado do parser e ponto de partida da simulação (initState, Simulator::load).
struct Config {
    std::string algorithm = "RM";  // nome do escalonador em MAIÚSCULAS, igual à chave do registro [A4.1]
    int quantum = 2;               // lido e guardado; RM/EDF ignoram (Q4, D13)
    int cpuCount = 1;              // 1..MAX_CPUS [RG2, A1.2] (D12)
    uint64_t seed = 1;        // semente do sorteio (D14); alterável na interface
    std::vector<Task> tasks;  // file fields filled in; runtime fields keep their defaults
};

// Resultado da leitura do arquivo [A3.3.6]: 'errors' não vazio => nada é carregado (D18).
struct ParseResult {
    Config config;                      // o que foi lido (com os padrões aplicados)
    std::vector<std::string> errors;    // if not empty, the simulation must not run
    std::vector<std::string> warnings;  // shown to the user, but the simulation can run

    // true se o arquivo pode ser carregado (avisos não impedem) [A3.3.7].
    bool ok() const { return errors.empty(); }
};

// ---------------------------------------------------------------------------
// Edição manual [A1.5.2, A3.1, A3.4] (design.md §9)
// ---------------------------------------------------------------------------
// O usuário pode mudar o estado ou as características de qualquer tarefa (e
// do sistema) em qualquer instante. Cada pedido vira um Edit; o Simulator o
// valida numa cópia e, se for aceito, aplica no instante atual depois de
// conclusões/ativações/prazos e antes do escalonador (D28), descarta o futuro
// e recalcula (D15). Se for recusado, nada muda e EditResult explica o motivo.

// Tipos de edição. Regras de validação de cada uma: tabela do design.md §9.
// Mudar o estado para Running/Ready diretamente NÃO existe de propósito:
// quem executa é decidido pelo escalonador.
enum class EditKind {
    // --- estado da tarefa ---
    SetRemaining,  // tempo restante da ocorrência corrente (1..duração)
    Suspend,       // Ready/Running/Dormant -> Suspended (libera a CPU)
    Resume,        // Suspended -> Ready (tem ocorrência pendente) ou Dormant
    ForceFinish,   // encerra a tarefa agora (Terminated)
    Remove,        // retira a tarefa da simulação (Terminated; o índice não é apagado)
    ActivateNow,   // Dormant: cria a próxima ocorrência neste instante
    // --- características da tarefa (valem para as próximas ocorrências) ---
    SetArrival,    // só antes do ingresso (NotArrived); novo valor >= instante atual
    SetDuration,
    SetPeriod,
    SetDeadline,   // recalcula também o prazo absoluto da ocorrência corrente
    SetColor,      // hex RRGGBB; aviso se repetida ou preta (D17)
    // --- sistema ---
    SetAlgorithm,  // qualquer nome do registro de escalonadores [A3.1]
    SetCpuCount,   // 1..MAX_CPUS; ao reduzir, quem estava nas CPUs removidas volta a Ready
    SetQuantum,
    SetSeed,       // semente do sorteio (D14)
    AddTask        // nova tarefa (newTask), com as mesmas validações do parser
};

// Um pedido de edição. Só os campos que o tipo usa são lidos.
struct Edit {
    EditKind kind = EditKind::SetRemaining;  // o que mudar
    int taskIndex = -1;    // tarefa alvo (índice em SystemSnapshot::tasks); -1 = edição do sistema
    long long value = 0;   // valor numérico (tempo, nº de CPUs, quantum, semente)
    std::string text;      // valor textual (cor, nome do algoritmo)
    Task newTask;          // tarefa completa, só para AddTask
};

// Resposta a uma edição [A3.4]: se recusada, 'message' diz em português o motivo.
struct EditResult {
    bool ok = false;
    std::string message;
};
