#pragma once
#include <map>
#include <string>
#include <vector>
#include "core/SystemSnapshot.h"

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

// Algorithm name in the config file (UPPERCASE) -> function. [A4.2]
const std::map<std::string, SchedulerFn>& schedulerRegistry();
