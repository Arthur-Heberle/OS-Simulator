// =============================================================================
// Scheduler.h — Algoritmos de escalonamento e o registro deles (design.md §7).
//
// Um algoritmo é só a política: recebe o estado e devolve as candidatas em
// ordem de prioridade, agrupando as que continuam empatadas depois dos
// critérios (1) a (4) do A4.3. O mecanismo (escolher CPUs, sortear pelo
// critério (5), mudar estados) fica no Simulator. Assim um algoritmo novo é
// uma função-chave e uma entrada no registro, sem mudar o motor [A4.2].
//
// Requisitos: A4, A4.1, A4.2, A4.3. Decisões: D14, design.md §7.
// =============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Model.h"

// Índices de tarefas (em SystemSnapshot::tasks) agrupados por prioridade, do
// grupo mais prioritário ao menos. Tarefas do mesmo grupo continuam empatadas
// após os critérios (1) a (4) do A4.3; o sorteio (5) é feito pelo Simulator.
using PriorityGroups = std::vector<std::vector<int>>;

// Algoritmo de escalonamento: só decide a ordem de prioridade; não altera o
// estado e não conhece as CPUs [A4.2].
using SchedulerFn = PriorityGroups (*)(const SystemSnapshot&);

PriorityGroups scheduleRM (const SystemSnapshot& s);   // RM: menor período primeiro (prioridade fixa)
PriorityGroups scheduleEDF(const SystemSnapshot& s);   // EDF: menor prazo absoluto primeiro (prioridade dinâmica)

// Chave de prioridade de uma tarefa (menor = mais prioritária). É o que muda
// entre RM, EDF e um algoritmo novo.
using KeyFn = Tick (*)(const Task&);

// Ordena as candidatas pela chave e pelos critérios (1) a (4) do A4.3 e agrupa
// as empatadas. Público para que um algoritmo novo (ex.: o FIFO de tests.cpp)
// seja apenas uma função-chave.
PriorityGroups orderAndGroup(const SystemSnapshot& s, KeyFn key);

// --- Registro de algoritmos [A4.2] ------------------------------------------
// Nome usado no arquivo de configuração (em maiúsculas) -> função. RM e EDF já
// vêm registrados.
void registerScheduler(const std::string& name, SchedulerFn fn);

// Procura pelo nome sem diferenciar maiúsculas [A3.3.2]; nullptr se não existe.
SchedulerFn findScheduler(const std::string& name);

// Nomes registrados, em ordem alfabética (mensagem de erro do parser e lista
// de algoritmos da Tela 5).
std::vector<std::string> schedulerNames();

// Sorteio do critério (5) do A4.3 sem estado (D14): hash splitmix64 de
// (semente, instante, tarefa). Os mesmos dados dão o mesmo resultado, então
// retroceder, avançar e recalcular após uma edição repetem o sorteio, e o
// snapshot só precisa guardar a semente.
uint64_t tieHash(uint64_t seed, Tick now, int taskIndex);
