#pragma once
#include <vector>
#include "core/Parser.h"        // Config
#include "core/Scheduler.h"
#include "core/SystemSnapshot.h"

class Simulator {
public:
    explicit Simulator(const Config& config);

    bool step(bool forward);    // false se não for possível (início ou fim)
    bool isFinished() const;

    const SystemSnapshot& current() const { return history_[currentIndex_]; }
    const std::vector<SystemSnapshot>& history() const { return history_; }
    int currentIndex() const { return currentIndex_; }

private:
    SystemSnapshot computeNext(const SystemSnapshot& now) const;

    void releaseActivations(SystemSnapshot& s) const;
    void checkDeadlines   (SystemSnapshot& s) const;
    void dispatch         (SystemSnapshot& s, const std::vector<int>& order) const;
    void runOneTick       (SystemSnapshot& s) const;
    void finishActivations(SystemSnapshot& s) const;

    int quantum;
    SchedulerFn scheduler;                  // RM ou EDF, escolhido no construtor
    std::vector<SystemSnapshot> history_;   // history_[k] = estado no instante k
    int currentIndex_ = 0;
};