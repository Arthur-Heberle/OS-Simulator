#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Model.h"

// Task indexes (positions in SystemSnapshot::tasks) grouped by priority,
// from the highest-priority group to the lowest.
// Tasks inside the same group are still tied after tie-break criteria (1) to (4)
// of [A4.3]: the Simulator does the draw (criterion 5), not the scheduler.
using PriorityGroups = std::vector<std::vector<int>>;

// A scheduling algorithm only decides the priority order. It does not change
// the state and does not know the CPUs [A4.2].
using SchedulerFn = PriorityGroups (*)(const SystemSnapshot&);

PriorityGroups scheduleRM (const SystemSnapshot& s);
PriorityGroups scheduleEDF(const SystemSnapshot& s);

// --- Registro de algoritmos [A4.2] (design.md §7) ---------------------------
// Nome no arquivo de configuração (MAIÚSCULAS) -> função. RM e EDF já vêm
// registrados. registerScheduler permite incluir um algoritmo novo de fora
// deste arquivo (ex.: o teste FIFO em tests.cpp) sem mexer no motor.
void registerScheduler(const std::string& name, SchedulerFn fn);

// Procura pelo nome sem diferenciar maiúsculas [A3.3.2]; nullptr se não existe.
SchedulerFn findScheduler(const std::string& name);

// Nomes registrados, em ordem alfabética: o parser os cita na mensagem de
// algoritmo desconhecido e a interface os oferece na troca de algoritmo.
std::vector<std::string> schedulerNames();

// Sorteio do critério (5) do A4.3, SEM estado (D14): um hash (splitmix64) de
// (semente, instante, tarefa). Mesmos dados => mesmo resultado, então
// retroceder/avançar e re-simular após uma edição repetem o mesmo sorteio,
// e nada precisa ser guardado no snapshot além da semente.
uint64_t tieHash(uint64_t seed, Tick now, int taskIndex);
