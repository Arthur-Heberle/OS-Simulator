#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "core/CPU.h"
#include "core/Task.h"

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
    std::vector<CPU> cpus;
    std::vector<Occurrence> occurrences;    // o que aconteceu NESTE instante
};
