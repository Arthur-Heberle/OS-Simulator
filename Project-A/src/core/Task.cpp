#include "core/Task.h"

#include <utility>

Task::Task()
    : id(0), color(""), arrival(0), duration(0), period(0), deadline(0) {}

Task::Task(int id, std::string color, int arrival, int duration, int period, int deadline)
    : id(id), color(std::move(color)), arrival(arrival), duration(duration),
      period(period), deadline(deadline) {}

Task::~Task() {}
