#include "Scheduler.h"

#include <utility>

Scheduler::Scheduler() : algorithm("") {}

Scheduler::Scheduler(std::string algorithm) : algorithm(std::move(algorithm)) {}

Scheduler::~Scheduler() {}
