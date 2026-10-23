#pragma once

#include <string>

// Task control block (TCB): keeps all the information of one task
// (before, during and after the simulation) in a single structure.
class Task {
    private:
        std::string id;     // Unique task identifier (text: Project B uses ids like "BS01")
        std::string color;  // RGB hexadecimal color (e.g. "F0E0D0")
        int arrival;        // Tick at which the task is created
        int duration;       // Total execution time needed
        int period;         // Activation period (> 0 periodic, 0 aperiodic)
        int deadline;       // Deadline, counted from each activation

    public:
        // Creates a task with neutral values (zeros and empty color).
        Task();

        // Creates a task from the fields of a line of the input file.
        Task(std::string id, std::string color, int arrival, int duration, int period, int deadline);

        ~Task();
};
