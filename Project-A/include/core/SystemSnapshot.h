#pragma once

// Copy of the system state at one tick, stored in the history so the
// simulation can go backward and forward.
class SystemSnapshot {
    private:
        int tick;  // Global clock value when the snapshot was taken

    public:
        // Creates a snapshot with neutral values.
        SystemSnapshot();

        // Creates a snapshot for the given tick.
        SystemSnapshot(int tick);

        ~SystemSnapshot();
};
