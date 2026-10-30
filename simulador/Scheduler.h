// =============================================================================
// Scheduler.h — Algoritmos de escalonamento (POLÍTICA) e o registro deles.
//
// O quê: cada algoritmo é uma função que recebe o estado e devolve a ORDEM de
// prioridade das tarefas candidatas, já agrupando as que continuam empatadas
// depois dos critérios (1)–(4) do A4.3. Quem escolhe as CPUs, sorteia o
// critério (5) e muda estados é o Simulator (MECANISMO).
//
// Por quê: o A4.2 exige incluir algoritmos novos sem mexer no código da
// simulação. Aqui um algoritmo novo é uma função-chave (menor = mais
// prioritário) + uma linha no registro, ou uma chamada a registerScheduler.
// O motor só conhece SchedulerFn.
//
// Requisitos: A4, A4.1, A4.2, A4.3. Decisões: D14 (sorteio sem estado),
// design.md §7 (grupos de empate, ideia do Arthur).
// =============================================================================
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

PriorityGroups scheduleRM (const SystemSnapshot& s);   // RM: menor período primeiro (prioridade fixa)
PriorityGroups scheduleEDF(const SystemSnapshot& s);   // EDF: menor prazo absoluto primeiro (dinâmica)

// Priority key of a task for one algorithm: the smaller, the higher the priority.
// É a ÚNICA coisa que muda entre RM, EDF e um algoritmo novo [A4.2].
using KeyFn = Tick (*)(const Task&);

// Mecanismo comum a todos os algoritmos por chave: ordena as candidatas
// (Ready/Running, não ignoradas) pela chave e pelos critérios (1)–(4) do A4.3
// e agrupa as que continuam empatadas. Público para que um algoritmo novo
// (ex.: o FIFO do teste de extensibilidade) seja só uma função-chave.
PriorityGroups orderAndGroup(const SystemSnapshot& s, KeyFn key);

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
