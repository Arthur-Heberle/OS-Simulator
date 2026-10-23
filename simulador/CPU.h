#pragma once

// Represents one simulated processor.
struct CPU {
    int id = 0;
    int taskIndex = -1;    // Index in snapshot.tasks; -1 = no task (CPU is off) [A1.2]
    int quantumUsed = 0;   // Consecutive ticks the current task has run
    int ticksOff = 0;      // Total ticks spent off, shown to the user [A1.2]
};