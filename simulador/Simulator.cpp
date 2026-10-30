// =============================================================================
// Simulator.cpp — Implementação do motor (design.md §6) e da classe Simulator.
// Ver o cabeçalho de Simulator.h.
// =============================================================================
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

// =============================================================================
// TEMPORÁRIO (Fase 1): REMOVER NA FASE 7 — a entrega não pode ter código morto.
//
// demoHistoryG1: histórico do cenário G1 de docs/testes.md MONTADO À MÃO, sem
// chamar o motor (as ações abaixo são lambdas locais, não as regras de
// makeNext). Serve para a Fase 5 desenhar o Gantt antes de o motor ficar
// pronto, e tests.cpp confere este histórico contra a grade de G1.
//
// G1: RM, quantum 2, 1 CPU, semente 1.
//   T1 (índice 0): ingresso 0, duração 1, período 4, prazo 4, cor FF0000
//   T2 (índice 1): ingresso 0, duração 2, período 6, prazo 6, cor 0000FF
//
// Instantes 0 a 12 (D3: snapshot k = estado depois de processar o instante k;
// a CPU dele diz quem executa em [k, k+1); as ocorrências aconteceram em x = k):
//   k | CPU1 | ocorrências em k
//   0 |  T1  | Arrival T1, Arrival T2, Dispatch T1   (RM: período 4 < 6)
//   1 |  T2  | Termination T1, Dispatch T2
//   2 |  T2  | -
//   3 |  -   | Termination T2, CpuOff
//   4 |  T1  | Arrival T1, Dispatch T1, CpuOn
//   5 |  -   | Termination T1, CpuOff
//   6 |  T2  | Arrival T2, Dispatch T2, CpuOn
//   7 |  T2  | -
//   8 |  T1  | Termination T2, Arrival T1, Dispatch T1
//   9 |  -   | Termination T1, CpuOff
//  10 |  -   | -
//  11 |  -   | -
//  12 |  T1  | Arrival T1, Arrival T2, Dispatch T1, CpuOn
// Conferência (testes.md G1): CPU desligada em [0,12) por 5 ticks (3, 5, 9,
// 10, 11), visível em ticksOff do instante 12; nenhuma perda de prazo nem
// preempção. A ordem das ocorrências dentro do instante segue o design.md §6.
// =============================================================================
std::vector<SystemSnapshot> demoHistoryG1() {
    const int T1 = 0, T2 = 1, CPU1 = 0;

    SystemSnapshot s;
    s.tick = 0;
    s.algorithm = "RM";
    s.quantum = 2;
    s.seed = 1;
    Task t1;
    t1.id = "1"; t1.color = "FF0000"; t1.arrival = 0; t1.duration = 1; t1.period = 4; t1.deadline = 4;
    Task t2;
    t2.id = "2"; t2.color = "0000FF"; t2.arrival = 0; t2.duration = 2; t2.period = 6; t2.deadline = 6;
    s.tasks = {t1, t2};      // nextRelease = arrival = 0 (valor padrão)
    s.cpus.resize(1);        // CPU1 (índice 0) começa ligada: só as transições geram ocorrência

    // --- Ações escritas à mão para este histórico (não são as do motor) ---

    // Consome o tick [t, t+1) e vai para o instante t+1: quem está na CPU
    // executa; CPU vazia conta tempo desligada; quem está pronta conta espera.
    auto nextInstant = [](SystemSnapshot& x) {
        x.occurrences.clear();
        for (CPU& cpu : x.cpus) {
            if (cpu.taskIndex < 0) {
                cpu.ticksOff++;
                continue;
            }
            Task& t = x.tasks[cpu.taskIndex];
            t.jobs.front().remaining--;
            t.runTicks++;
            cpu.ticksBusy++;
            cpu.quantumUsed++;
        }
        for (Task& t : x.tasks) {
            if (t.state == TaskState::Ready) t.waitTicks++;
        }
        x.tick++;
    };
    // Ativação: nova ocorrência (Job) entra na fila de prontos (Arrival, Q5).
    auto release = [](SystemSnapshot& x, int i) {
        Task& t = x.tasks[i];
        t.jobs.push_back(Job{x.tick, x.tick + t.deadline, t.duration, t.duration, false});
        t.released++;
        t.nextRelease += t.period;
        if (t.state == TaskState::NotArrived || t.state == TaskState::Dormant) t.state = TaskState::Ready;
        x.occurrences.push_back(Occurrence{OccurrenceType::Arrival, i, -1, ""});
    };
    // Fim da ocorrência corrente: libera a CPU e a tarefa espera o próximo
    // período (Dormant; em G1 nenhuma chega à 10ª ocorrência até o instante 12).
    auto complete = [](SystemSnapshot& x, int i) {
        Task& t = x.tasks[i];
        x.occurrences.push_back(Occurrence{OccurrenceType::Termination, i, t.cpu, ""});
        t.jobs.erase(t.jobs.begin());
        t.completed++;
        x.cpus[t.cpu].taskIndex = -1;
        t.lastCpu = t.cpu;
        t.cpu = -1;
        t.state = TaskState::Dormant;
    };
    // Despacho: a tarefa passa a executar na CPU c (religando-a se estava desligada).
    auto dispatch = [](SystemSnapshot& x, int i, int c) {
        Task& t = x.tasks[i];
        CPU& cpu = x.cpus[c];
        cpu.taskIndex = i;
        cpu.quantumUsed = 0;
        t.cpu = c;
        t.state = TaskState::Running;
        if (t.firstStart < 0) t.firstStart = x.tick;
        x.occurrences.push_back(Occurrence{OccurrenceType::Dispatch, i, c, ""});
        if (!cpu.on) {
            cpu.on = true;
            x.occurrences.push_back(Occurrence{OccurrenceType::CpuOn, -1, c, ""});
        }
    };
    // CPU sem tarefa é desligada [A1.2].
    auto cpuOff = [](SystemSnapshot& x, int c) {
        x.cpus[c].on = false;
        x.occurrences.push_back(Occurrence{OccurrenceType::CpuOff, -1, c, ""});
    };

    std::vector<SystemSnapshot> history;

    // Instante 0: as duas chegam; RM escolhe T1 (período 4); T2 fica na fila.
    release(s, T1); release(s, T2); dispatch(s, T1, CPU1);
    s.readyQueue = {T2};
    history.push_back(s);
    // Instante 1: T1 conclui a 1ª ocorrência; T2 assume a CPU.
    nextInstant(s); complete(s, T1); dispatch(s, T2, CPU1);
    s.readyQueue.clear();
    history.push_back(s);
    // Instante 2: T2 continua (falta 1 tick).
    nextInstant(s);
    history.push_back(s);
    // Instante 3: T2 conclui; ninguém pronto: CPU desligada.
    nextInstant(s); complete(s, T2); cpuOff(s, CPU1);
    history.push_back(s);
    // Instante 4: 2ª ativação de T1; CPU religada.
    nextInstant(s); release(s, T1); dispatch(s, T1, CPU1);
    history.push_back(s);
    // Instante 5: T1 conclui; CPU desligada.
    nextInstant(s); complete(s, T1); cpuOff(s, CPU1);
    history.push_back(s);
    // Instante 6: 2ª ativação de T2; CPU religada.
    nextInstant(s); release(s, T2); dispatch(s, T2, CPU1);
    history.push_back(s);
    // Instante 7: T2 continua.
    nextInstant(s);
    history.push_back(s);
    // Instante 8: T2 conclui e, no mesmo instante, T1 é ativada (3ª) e assume a CPU.
    nextInstant(s); complete(s, T2); release(s, T1); dispatch(s, T1, CPU1);
    history.push_back(s);
    // Instante 9: T1 conclui; CPU desligada até o instante 12.
    nextInstant(s); complete(s, T1); cpuOff(s, CPU1);
    history.push_back(s);
    // Instantes 10 e 11: nada acontece (CPU desligada).
    nextInstant(s);
    history.push_back(s);
    nextInstant(s);
    history.push_back(s);
    // Instante 12: as duas são ativadas (T1 4ª, T2 3ª); RM escolhe T1; CPU religada.
    nextInstant(s); release(s, T1); release(s, T2); dispatch(s, T1, CPU1);
    s.readyQueue = {T2};
    history.push_back(s);

    return history;
}
