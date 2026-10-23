#pragma once

#include <string>

// Chooses the next task to run. A single class holds every algorithm:
// each one (RM, EDF) will be its own member function.
class Scheduler {
    private:
        std::string algorithm;  // Selected algorithm name ("RM" or "EDF")

    public:
        // Creates a scheduler with no algorithm selected.
        Scheduler();

        // Creates a scheduler that uses the given algorithm.
        Scheduler(std::string algorithm);

        ~Scheduler();
};
