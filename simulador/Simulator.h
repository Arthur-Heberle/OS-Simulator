#pragma once

#include "Parser.h"
#include "CPU.h"
#include "SystemSnapshot.h"
#include "Task.h"

// Runs the simulation: owns the global clock and drives the tasks,
// processors and scheduler tick by tick.
class Simulator {
    private:
        int cpuCount;  // Number of simulated processors

    public:
        // Creates a simulator with neutral values.
        Simulator();

        // Creates a simulator with the given number of processors.
        Simulator(int cpuCount);

        ~Simulator();

        ParseResult parseConfig(const std::string& text);
};
