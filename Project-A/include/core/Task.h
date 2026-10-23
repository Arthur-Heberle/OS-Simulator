#pragma once

#include <string>

enum class TaskState {
    Ready,
    Executing,
    Suspended,
    Terminated
};

// Task control block (TCB): keeps all the information of one task
// (before, during and after the simulation) in a single structure.
struct Task {
    std::string id;     // Unique task identifier (text: Project B uses ids like "BS01")
    std::string color;  // RGB hexadecimal color (e.g. "F0E0D0")
    int arrival;        // Tick at which the task is created
    int duration;       // Total execution time needed
    int period;         // Activation period (> 0 periodic, 0 aperiodic)
    int deadline;       // Deadline, counted from each activation
    int remaining;
    TaskState state = TaskState::Ready;
};


