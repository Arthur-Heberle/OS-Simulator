#pragma once

// Represents one simulated processor.
class CPU {
    private:
        int id;  // Processor identifier, shown in the Gantt chart

    public:
        // Creates a processor with neutral values.
        CPU();

        // Creates a processor with the given identifier.
        CPU(int id);

        ~CPU();
};
