#pragma once
#include <map>
#include <string>
#include <vector>
#include "core/SystemSnapshot.h"

// Tipo "ponteiro para função de escalonamento": recebe o estado e devolve
// os índices das tarefas candidatas, da maior para a menor prioridade.
using SchedulerFn = std::vector<int> (*)(const SystemSnapshot&);

std::vector<int> scheduleRM (const SystemSnapshot& s);
std::vector<int> scheduleEDF(const SystemSnapshot& s);

// Nome no arquivo de configuração (minúsculo) -> função. [A4.2]
const std::map<std::string, SchedulerFn>& schedulerRegistry();