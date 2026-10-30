#pragma once

#include "Task.h"  // Tick

// Represents one simulated processor.
struct CPU {
    int id = 0;
    int taskIndex = -1;    // Index in snapshot.tasks; -1 = no task (CPU is off) [A1.2]
    bool on = true;        // false = desligada [A1.2]; começa true para registrar só as transições (CpuOff/CpuOn)
    int quantumUsed = 0;   // Consecutive ticks the current task has run (para algoritmos com quantum, D13)
    Tick ticksBusy = 0;    // ticks executando alguma tarefa
    Tick ticksOff = 0;     // Total ticks spent off, shown to the user [A1.2]
};
