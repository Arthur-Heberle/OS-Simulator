#include "Simulator.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// Linha do tempo (D3): o tick t é o intervalo [t, t+1). history[k] é o estado
// no instante k DEPOIS de processá-lo: as CPUs dele dizem quem executa em
// [k, k+1) e as ocorrências dele aconteceram exatamente em x = k. Assim o
// Gantt lê a coluna k direto de history[k], e um término em k+1 aparece em
// history[k+1] sem que a informação de quem executou em [k, k+1) se perca.
// ---------------------------------------------------------------------------

// Processa um instante já com o relógio no valor certo: conclusões ->
// ativações -> perdas de prazo -> escalonamento -> despacho (design.md §6).
static void processInstant(SystemSnapshot& s) {
    finishActivations(s);
    releaseActivations(s);
    checkDeadlines(s);

    // Política: o algoritmo vem do snapshot (pode mudar durante a simulação, A3.1).
    SchedulerFn scheduler = findScheduler(s.algorithm);
    PriorityGroups groups = scheduler ? scheduler(s) : PriorityGroups{};

    // Mecanismo: escolher quem executa e despachar.
    std::vector<int> drawn;
    std::vector<int> chosen = chooseTasks(s, groups, drawn);
    dispatch(s, chosen, drawn);
}

SystemSnapshot initState(const Config& config) {
    SystemSnapshot s;
    s.tick = 0;
    s.algorithm = config.algorithm;
    s.quantum = config.quantum;
    s.seed = config.seed;
    s.tasks = config.tasks;
    for (Task& task : s.tasks) {
        task.nextRelease = task.arrival;   // primeira ativação no instante de ingresso
    }
    s.cpus.resize(config.cpuCount);
    for (int i = 0; i < config.cpuCount; i++) {
        s.cpus[i].id = i;
    }
    processInstant(s);
    return s;
}

SystemSnapshot makeNext(const SystemSnapshot& prev) {
    SystemSnapshot next = prev;
    next.occurrences.clear();          // a cópia trouxe as do tick anterior; começa vazio

    runOneTick(next);                  // consome [prev.tick, prev.tick + 1)
    next.tick = prev.tick + 1;
    processInstant(next);
    return next;
}

bool finished(const SystemSnapshot& s) {
    for (const Task& t : s.tasks) {
        if (!t.ignored && t.state != TaskState::Terminated) {
            return false;   // achou uma que ainda não terminou: a simulação continua
        }
    }
    return true;            // Every simulated task is terminated
}

/*----------------------------------------------------*/
/*  Function: runOneTick
    responsable to manage all the CPUs updating their tick
        and descrease the time remaining for the tasks
    Args:
     SystemSnapshot& s = current snapshot of the system
                                                       */
void runOneTick(SystemSnapshot& s) {
    for (auto& cpu : s.cpus){
        if (cpu.taskIndex == -1){
            cpu.ticksOff++;
        }
        else{
            Task& task = s.tasks[cpu.taskIndex];
            task.jobs.front().remaining--;   // consome a ocorrência corrente (D5)
            task.runTicks++;
            cpu.ticksBusy++;
            cpu.quantumUsed++;
        }
    }
    // Tempo de espera: quem estava pronto e não executou neste tick.
    for (Task& task : s.tasks) {
        if (task.state == TaskState::Ready) task.waitTicks++;
    }
}

// TODO (Fase 3): para cada tarefa com jobs.front().remaining == 0: ocorrência
// Termination; remove o job; completed++; libera a CPU (lastCpu permanece);
// ainda há job => Ready; senão released == limite => Terminated + TaskFinished;
// senão Dormant. Registrar finishTime. (design.md §6, passo 1)
void finishActivations(SystemSnapshot& /*s*/) {
}

// TODO (Fase 3): tarefa não ignorada com nextRelease == tick e released < limite:
// cria Job{tick, tick + deadline, duration, duration}; released++;
// nextRelease += period; ocorrência Arrival (nota "enfileirada" se já havia job).
// NotArrived/Dormant => Ready; Ready/Running/Suspended continuam. (§6, passo 2)
void releaseActivations(SystemSnapshot& /*s*/) {
}

// TODO (Fase 3): job com absDeadline == tick, remaining > 0 e !missed:
// missed = true; deadlineMisses++; ocorrência DeadlineMiss. A tarefa continua
// executando [A3.3]. (§6, passo 3)
void checkDeadlines(SystemSnapshot& /*s*/) {
}

std::vector<int> chooseTasks(const SystemSnapshot& s, const PriorityGroups& groups,
                             std::vector<int>& drawn) {
    std::vector<int> chosen;
    drawn.clear();
    int slotsLeft = static_cast<int>(s.cpus.size());

    for (const std::vector<int>& group : groups) {
        if (slotsLeft == 0) break;

        if (static_cast<int>(group.size()) <= slotsLeft) {
            // O grupo inteiro cabe: todos executam, sem sorteio.
            chosen.insert(chosen.end(), group.begin(), group.end());
            slotsLeft -= static_cast<int>(group.size());
        } else {
            // Empate que não cabe nas CPUs livres: sorteio, critério (5) do A4.3.
            // Ordena pelo hash sem estado (D14); o índice só desempata uma
            // colisão de hash, para o resultado nunca depender da ordem do sort.
            std::vector<int> order = group;
            std::sort(order.begin(), order.end(), [&s](int a, int b) {
                uint64_t ha = tieHash(s.seed, s.tick, a);
                uint64_t hb = tieHash(s.seed, s.tick, b);
                return ha != hb ? ha < hb : a < b;
            });
            for (int i = 0; i < slotsLeft; i++) {
                chosen.push_back(order[i]);
                drawn.push_back(order[i]);
            }
            slotsLeft = 0;
        }
    }
    return chosen;
}

// TODO (Fase 3): (a) Running fora de 'chosen' => Ready, libera CPU,
// preemptions++, Preemption; (b) escolhida já Running fica na mesma CPU;
// (c) escolhida nova vai para lastCpu se livre, senão a de menor índice:
// Running, Dispatch, Migration se trocou de CPU, firstStart, quantumUsed = 0,
// TieBreakDraw se está em 'drawn'; (d) CPU sem tarefa => on = false (+ CpuOff),
// CPU com tarefa e desligada => on = true (+ CpuOn); readyQueue = Ready
// ordenadas por (jobs.front().release, índice). (§6, passos 5 e 6)
void dispatch(SystemSnapshot& /*s*/, const std::vector<int>& /*chosen*/,
              const std::vector<int>& /*drawn*/) {
}

// ---------------------------------------------------------------------------
// Simulator
// ---------------------------------------------------------------------------

bool Simulator::load(const Config& config) {
    if (findScheduler(config.algorithm) == nullptr) return false;
    history_.clear();
    history_.push_back(initState(config));
    cursor_ = 0;
    return true;
}

bool Simulator::stepForward() {
    if (cursor_ + 1 < history_.size()) {   // o próximo já existe: reaproveita
        cursor_++;
        return true;
    }
    if (isFinished()) return false;        // nada mais a simular
    history_.push_back(makeNext(history_[cursor_]));
    cursor_++;
    return true;
}

bool Simulator::stepBack() {               // voltar: só move o índice
    if (cursor_ == 0) return false;
    cursor_--;
    return true;
}
