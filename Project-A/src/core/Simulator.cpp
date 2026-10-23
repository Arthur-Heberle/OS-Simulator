#include "core/Simulator.h"
#include <algorithm>

Simulator::Simulator(const Config& config)
    : quantum(config.quantum),
      scheduler(schedulerRegistry().at(config.algorithm)) {   // o parser já validou o nome
    SystemSnapshot first;
    first.tick = 0;
    first.tasks = config.tasks;
    // TODO: para cada tarefa, nextRelease = arrival
    // TODO: criar config.cpuCount CPUs (ids 0..n-1), todas desligadas
    history_.push_back(first);
}

bool Simulator::step(bool forward) {
    if (!forward) {                                   // voltar: só move o índice
        if (currentIndex_ == 0) return false;
        currentIndex_--;
        return true;
    }
    if (currentIndex_ + 1 < (int)history_.size()) {   // o próximo já existe: reaproveita
        currentIndex_++;
        return true;
    }
    if (isFinished()) return false;                   // nada mais a simular
    history_.push_back(computeNext(history_[currentIndex_]));
    currentIndex_++;
    return true;
}

bool Simulator::isFinished() const {
    const SystemSnapshot& snap = history_[currentIndex_];   
    for (const Task& t : snap.tasks) {                      
        if (t.state != TaskState::Terminated) {
            return false;   // achou uma que ainda não terminou: a simulação continua
        }
    }
    return true;            // Every task is terminated
}

SystemSnapshot Simulator::computeNext(const SystemSnapshot& now) const {
    SystemSnapshot next = now;
    next.occurrences.clear();          // a cópia trouxe as do tick anterior; começa vazio

    releaseActivations(next);
    checkDeadlines(next);
    std::vector<int> order = scheduler(next);   // política
    dispatch(next, order);                      // mecanismo
    runOneTick(next);
    finishActivations(next);

    next.tick++;
    return next;
}

// Etapas: cada uma é a próxima coisa a implementar, nesta ordem.
void Simulator::releaseActivations(SystemSnapshot& s) const { 
    /* TODO */ 
}

void Simulator::checkDeadlines(SystemSnapshot& s) const { 
    /* TODO */ 
}

void Simulator::dispatch(SystemSnapshot& s, const std::vector<int>& order) const { 
    /* TODO */ 
}


/*----------------------------------------------------*/
/*  Function: runOneTick                   
    responsable to manage all the CPUs updating their tick
        and descrease the time remaining for the tasks          
    Args: 
     SystemSnapshot& s = current snapshot of the system                            
                                                       */
void Simulator::runOneTick(SystemSnapshot& s) const { 
    for (auto& cpu : s.cpus){
        if (cpu.taskIndex == -1){
            cpu.ticksOff++;
        } 
        else{
            s.tasks[cpu.taskIndex].remaining--;
            cpu.quantumUsed++;
        }
    }
}

void Simulator::finishActivations(SystemSnapshot& s) const { 
    /* TODO */ 
}