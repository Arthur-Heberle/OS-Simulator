#pragma once

#include <vector>
#include <random>
#include "CPU.h"
#include "Task.h"

// Kinds of events the Gantt chart must draw at the exact tick they happen.
// Names follow the assignment text: arrival and termination [A2.2],
// deadline miss [A3.3], tie-break draw [A4.3].
enum class OccurrenceType {
    Arrival,
    Termination,
    DeadlineMiss,
    TieBreakDraw
};

// One event that happened at the snapshot's tick. It has no tick field
// because it is stored inside the snapshot of that tick.
struct Occurrence {
    OccurrenceType type = OccurrenceType::Arrival;
    int taskIndex = -1;   // Task involved (index in tasks); -1 = none
    int cpuIndex = -1;    // CPU involved (index in cpus); -1 = none
};

// Copy of the system state at one tick, stored in the history so the
// simulation can go backward and forward.
struct SystemSnapshot {
    int tick = 0;
    std::vector<Task> tasks;
    std::vector<int> readyQueue;            // index in tasks (global queue)
    std::vector<CPU> cpus;
    std::vector<Occurrence> occurrences;    
    std::mt19937 rng{12345};                // reproducible draw [A4.3]
};
