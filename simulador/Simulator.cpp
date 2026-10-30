// =============================================================================
// Simulator.cpp — Implementação do motor (design.md §6) e da classe Simulator.
// Ver o cabeçalho de Simulator.h.
// =============================================================================
#include "Simulator.h"
#include "Parser.h"   // regras de validação do arquivo (AddTask, SetColor)
#include <algorithm>
#include <cctype>
#include <string>

// ---------------------------------------------------------------------------
// Linha do tempo (D3): o tick t é o intervalo [t, t+1). history[k] é o estado
// no instante k DEPOIS de processá-lo: as CPUs dele dizem quem executa em
// [k, k+1) e as ocorrências dele aconteceram exatamente em x = k. Assim o
// Gantt lê a coluna k direto de history[k], e um término em k+1 aparece em
// history[k+1] sem que a informação de quem executou em [k, k+1) se perca.
// ---------------------------------------------------------------------------

// Um instante é processado em duas metades (design.md §6). A edição manual
// entra ENTRE elas (D28): o escalonador já enxerga o estado editado.

// 1ª metade: conclusões -> ativações -> perdas de prazo.
void beginInstant(SystemSnapshot& s) {
    finishActivations(s);
    releaseActivations(s);
    checkDeadlines(s);
}

// 2ª metade: escalonamento -> despacho (e fila de prontos).
void scheduleAndDispatch(SystemSnapshot& s) {
    // Política: o algoritmo vem do snapshot (pode mudar durante a simulação, A3.1).
    SchedulerFn scheduler = findScheduler(s.algorithm);
    PriorityGroups groups = scheduler ? scheduler(s) : PriorityGroups{};

    // Mecanismo: escolher quem executa e despachar.
    std::vector<int> drawn;
    std::vector<int> chosen = chooseTasks(s, groups, drawn);
    dispatch(s, chosen, drawn);
}

SystemSnapshot baseState(const Config& config) {
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
    return s;
}

SystemSnapshot initState(const Config& config) {
    SystemSnapshot s = baseState(config);
    beginInstant(s);
    scheduleAndDispatch(s);
    return s;
}

SystemSnapshot makeNext(const SystemSnapshot& prev) {
    SystemSnapshot next = prev;
    next.occurrences.clear();          // a cópia trouxe as do tick anterior; começa vazio

    runOneTick(next);                  // consome [prev.tick, prev.tick + 1)
    next.tick = prev.tick + 1;
    beginInstant(next);
    scheduleAndDispatch(next);
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

bool stalled(const SystemSnapshot& s) {
    bool anySuspended = false;
    for (const Task& t : s.tasks) {
        if (t.ignored || t.state == TaskState::Terminated) continue;
        if (t.state != TaskState::Suspended || t.released < PERIODIC_ACTIVATION_LIMIT) return false;
        for (const Job& job : t.jobs) {
            if (!job.missed && job.absDeadline > s.tick) return false;  // ainda vai registrar perda de prazo
        }
        anySuspended = true;
    }
    return anySuspended;
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

// Pequeno atalho para registrar uma ocorrência no instante atual (A2.2: o Gantt
// desenha cada uma em x = s.tick).
static void addOccurrence(SystemSnapshot& s, OccurrenceType type, int taskIndex, int cpuIndex,
                          const std::string& note = "") {
    s.occurrences.push_back(Occurrence{type, taskIndex, cpuIndex, note});
}

// Passo 1 do instante (design.md §6): conclusões. Uma ocorrência cujo tempo
// restante chegou a 0 terminou exatamente agora (no fim do tick anterior).
// Vem ANTES das perdas de prazo: quem termina no próprio prazo não perde (D9).
void finishActivations(SystemSnapshot& s) {
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        Task& t = s.tasks[i];
        if (t.jobs.empty() || t.jobs.front().remaining > 0) continue;

        // Termination = fim de UMA ocorrência (resposta Q5), com a CPU em que rodou.
        addOccurrence(s, OccurrenceType::Termination, i, t.cpu);
        t.jobs.erase(t.jobs.begin());
        t.completed++;
        if (t.cpu >= 0) {               // libera a CPU; lastCpu guarda a afinidade (D10)
            s.cpus[t.cpu].taskIndex = -1;
            t.lastCpu = t.cpu;
            t.cpu = -1;
        }

        if (!t.jobs.empty()) {
            t.state = TaskState::Ready;     // havia ocorrência enfileirada (D5): volta a concorrer
        } else if (t.released >= PERIODIC_ACTIVATION_LIMIT) {
            t.state = TaskState::Terminated;  // concluiu a 10ª ocorrência [A4.4] (D6)
            t.finishTime = s.tick;
            addOccurrence(s, OccurrenceType::TaskFinished, i, -1);
        } else {
            t.state = TaskState::Dormant;   // espera a próxima ativação, sem concorrer (D1)
        }
    }
}

// Passo 2: ativações. Cada período cria uma ocorrência (Job) nova da tarefa
// periódica, com prazo absoluto = ativação + prazo relativo [A3.3]. Se a
// anterior ainda não terminou (perdeu o prazo), a nova é ENFILEIRADA, não
// descartada (D5): a tarefa continua executando até cumprir tudo.
void releaseActivations(SystemSnapshot& s) {
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        Task& t = s.tasks[i];
        if (t.ignored || t.state == TaskState::Terminated) continue;  // fora do A (D7) ou encerrada
        if (t.released >= PERIODIC_ACTIVATION_LIMIT || t.nextRelease != s.tick) continue;

        const bool queued = !t.jobs.empty();
        t.jobs.push_back(Job{s.tick, s.tick + t.deadline, t.duration, t.duration, false});
        t.released++;
        t.nextRelease += t.period;
        // Arrival = entrada na fila de prontos, a CADA ativação (resposta Q5) [A2.2].
        addOccurrence(s, OccurrenceType::Arrival, i, -1, queued ? "enfileirada" : "");

        // Ready/Running continuam (só ganham trabalho); Suspended continua impedida.
        if (t.state == TaskState::NotArrived || t.state == TaskState::Dormant) {
            t.state = TaskState::Ready;
        }
    }
}

// Passo 3: perdas de prazo. Chegou o prazo absoluto e ainda falta executar:
// é um erro que o Gantt mostra [A3.3]. Registrado UMA vez por ocorrência
// (D9), e a tarefa continua executando até terminar (A3.3).
void checkDeadlines(SystemSnapshot& s) {
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        Task& t = s.tasks[i];
        for (Job& job : t.jobs) {
            // '<=' e não '==': numa execução normal dá no mesmo (o prazo é visto
            // no instante exato), mas uma edição (SetDeadline) pode trazer o prazo
            // para antes do instante atual, e a perda tem de ser registrada mesmo assim.
            if (job.absDeadline <= s.tick && job.remaining > 0 && !job.missed) {
                job.missed = true;
                t.deadlineMisses++;
                addOccurrence(s, OccurrenceType::DeadlineMiss, i, t.cpu);
            }
        }
    }
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

// Passos 5 e 6: despacho (design.md §6). 'chosen' são as tarefas que executam
// em [tick, tick+1); 'drawn' as que foram escolhidas por sorteio [A4.3].
void dispatch(SystemSnapshot& s, const std::vector<int>& chosen, const std::vector<int>& drawn) {
    const int taskCount = static_cast<int>(s.tasks.size());
    const int cpuCount = static_cast<int>(s.cpus.size());
    std::vector<bool> isChosen(static_cast<std::size_t>(taskCount), false);
    for (int i : chosen) isChosen[static_cast<std::size_t>(i)] = true;

    // (a) Preempção: estava executando e não foi escolhida (uma mais prioritária
    // ocupou a vaga). Volta para a fila de prontos e libera a CPU.
    for (int i = 0; i < taskCount; ++i) {
        Task& t = s.tasks[i];
        if (t.state != TaskState::Running || isChosen[static_cast<std::size_t>(i)]) continue;
        addOccurrence(s, OccurrenceType::Preemption, i, t.cpu);
        s.cpus[t.cpu].taskIndex = -1;
        t.lastCpu = t.cpu;
        t.cpu = -1;
        t.state = TaskState::Ready;
        t.preemptions++;
    }

    // (b) Escolhida que já executa fica na MESMA CPU (D10: sem troca de
    // contexto nem migração). (c) Escolhida nova vai para uma CPU livre.
    // Afinidade (D10) em duas passadas: primeiro quem pode voltar para a
    // última CPU usada (se ela estiver livre); depois as demais, na CPU livre
    // de menor índice. Assim uma tarefa sem afinidade não "rouba" a CPU de
    // outra e provoca uma migração desnecessária.
    auto start = [&](int i, int c) {
        Task& t = s.tasks[i];
        CPU& cpu = s.cpus[c];
        cpu.taskIndex = i;
        cpu.quantumUsed = 0;
        t.cpu = c;
        t.state = TaskState::Running;
        if (t.firstStart < 0) t.firstStart = s.tick;
        addOccurrence(s, OccurrenceType::Dispatch, i, c);
        if (t.lastCpu != -1 && t.lastCpu != c) {
            addOccurrence(s, OccurrenceType::Migration, i, c);
            t.migrations++;
        }
        // Ícone de sorteio no momento em que a escolhida (re)começa [A4.3].
        if (std::find(drawn.begin(), drawn.end(), i) != drawn.end()) {
            addOccurrence(s, OccurrenceType::TieBreakDraw, i, c);
        }
    };
    std::vector<int> waiting;  // escolhidas novas sem CPU de afinidade livre
    for (int i : chosen) {
        Task& t = s.tasks[i];
        if (t.state == TaskState::Running) continue;  // (b)
        if (t.lastCpu >= 0 && t.lastCpu < cpuCount && s.cpus[t.lastCpu].taskIndex == -1) {
            start(i, t.lastCpu);
        } else {
            waiting.push_back(i);
        }
    }
    for (int i : waiting) {
        int c = 0;
        while (c < cpuCount && s.cpus[c].taskIndex != -1) ++c;
        if (c == cpuCount) break;  // não acontece: chooseTasks escolhe no máximo M tarefas
        start(i, c);
    }

    // (d) CPU sem tarefa é desligada; CPU desligada que recebeu tarefa é
    // religada [A1.2]. Só as transições geram ocorrência (o Gantt mostra o período).
    for (int c = 0; c < cpuCount; ++c) {
        CPU& cpu = s.cpus[c];
        const bool busy = cpu.taskIndex != -1;
        if (!busy && cpu.on) {
            cpu.on = false;
            addOccurrence(s, OccurrenceType::CpuOff, -1, c);
        } else if (busy && !cpu.on) {
            cpu.on = true;
            addOccurrence(s, OccurrenceType::CpuOn, -1, c);
        }
    }

    // Passo 6: fila global de prontos [A1.2], por ordem de chegada da
    // ocorrência corrente (e índice, para desempatar sempre igual).
    s.readyQueue.clear();
    for (int i = 0; i < taskCount; ++i) {
        if (s.tasks[i].state == TaskState::Ready) s.readyQueue.push_back(i);
    }
    std::sort(s.readyQueue.begin(), s.readyQueue.end(), [&s](int a, int b) {
        Tick ra = s.tasks[a].jobs.front().release;
        Tick rb = s.tasks[b].jobs.front().release;
        return ra != rb ? ra < rb : a < b;
    });
}

// ---------------------------------------------------------------------------
// Edição manual [A1.5.2, A3.1, A3.4] (design.md §9, D15, D28)
// ---------------------------------------------------------------------------

// Nome do estado em português, para as mensagens de recusa.
static std::string stateText(TaskState state) {
    switch (state) {
        case TaskState::NotArrived: return "ainda não ingressou";
        case TaskState::Ready:      return "está pronta";
        case TaskState::Running:    return "está executando";
        case TaskState::Dormant:    return "está dormente (esperando a próxima ativação)";
        case TaskState::Suspended:  return "está suspensa";
        case TaskState::Terminated: return "já terminou";
    }
    return "";
}

// Tira a tarefa da CPU (sem ocorrência): usado por suspender e encerrar.
static void releaseCpu(SystemSnapshot& s, Task& t) {
    if (t.cpu >= 0) {
        s.cpus[t.cpu].taskIndex = -1;
        t.lastCpu = t.cpu;
        t.cpu = -1;
    }
}

// Valida a tarefa nova com as MESMAS regras do arquivo [A3.3] (D18): monta um
// arquivo com as tarefas atuais + a nova, lê com parseConfig e olha só as
// mensagens da última linha. Assim id duplicado (sem caixa), números, cor,
// prazo padrão e paleta automática seguem exatamente o parser.
static EditResult checkNewTask(const SystemSnapshot& s, const Task& candidate, Task& out) {
    // ';' e quebra de linha mudariam a divisão em campos/linhas do arquivo montado.
    if (candidate.id.find_first_of(";\r\n") != std::string::npos ||
        candidate.color.find_first_of(";\r\n") != std::string::npos) {
        return {false, "O id e a cor não podem conter ';' nem quebra de linha."};
    }
    if (candidate.rawEvents.find_first_of("\r\n") != std::string::npos) {
        return {false, "A lista de eventos não pode conter quebra de linha."};
    }
    Config config;
    config.algorithm = s.algorithm;
    config.quantum = s.quantum;
    config.cpuCount = std::max(1, std::min(MAX_CPUS, static_cast<int>(s.cpus.size())));
    config.tasks = s.tasks;
    config.tasks.push_back(candidate);
    const ParseResult parsed = parser::parseConfig(parser::serializeConfig(config));

    // A tarefa nova é a última linha do arquivo montado.
    const std::string prefix = "Linha " + std::to_string(config.tasks.size() + 1) + ": ";
    auto fromNewLine = [&prefix](const std::vector<std::string>& messages) {
        std::string joined;
        for (const std::string& m : messages) {
            std::size_t at = m.find(prefix);
            if (at == std::string::npos) continue;
            if (!joined.empty()) joined += " ";
            joined += "Nova tarefa: " + m.substr(at + prefix.size());
        }
        return joined;
    };
    const std::string errors = fromNewLine(parsed.errors);
    if (!errors.empty()) return {false, errors};

    out = parsed.config.tasks.back();  // campos já normalizados (cor, prazo padrão, aperiódica)
    return {true, fromNewLine(parsed.warnings)};
}

EditResult applyEditTo(SystemSnapshot& s, const Edit& e) {
    auto refuse = [](const std::string& why) { return EditResult{false, why}; };
    const Tick now = s.tick;
    const long long v = e.value;
    std::string note;     // texto da ocorrência ManualEdit (log/tooltip)
    std::string warning;  // aviso devolvido junto com ok = true
    int target = e.taskIndex;

    // Edições de UMA tarefa: precisa existir e ser simulada no Projeto A.
    const bool onTask = e.kind != EditKind::SetAlgorithm && e.kind != EditKind::SetCpuCount &&
                        e.kind != EditKind::SetQuantum && e.kind != EditKind::SetSeed &&
                        e.kind != EditKind::AddTask;
    Task* t = nullptr;
    if (onTask) {
        if (e.taskIndex < 0 || e.taskIndex >= static_cast<int>(s.tasks.size())) {
            return refuse("Tarefa inexistente (índice " + std::to_string(e.taskIndex) + ").");
        }
        t = &s.tasks[e.taskIndex];
        if (t->ignored) {
            return refuse("A tarefa '" + t->id + "' não é simulada no Projeto A (" + t->ignoredReason + ").");
        }
    }
    const std::string name = t ? "A tarefa '" + t->id + "'" : "";
    // Faixa válida dos parâmetros de tempo: a mesma do arquivo (design.md §8).
    auto checkRange = [&](const std::string& field, long long low) -> std::string {
        if (v < low || v > MAX_FIELD_VALUE) {
            return "O valor de '" + field + "' deve ser um inteiro entre " + std::to_string(low) +
                   " e " + std::to_string(MAX_FIELD_VALUE) + " (recebido: " + std::to_string(v) + ").";
        }
        return "";
    };

    switch (e.kind) {
    case EditKind::SetRemaining: {
        if (t->jobs.empty()) {
            return refuse(name + " não tem ocorrência pendente (" + stateText(t->state) + "); não há tempo restante para alterar.");
        }
        Job& job = t->jobs.front();
        if (v < 1 || v > job.duration) {
            return refuse("O tempo restante deve ficar entre 1 e " + std::to_string(job.duration) +
                          " (duração da ocorrência corrente). Para concluir a tarefa, use 'Forçar término'.");
        }
        job.remaining = v;
        note = "tempo restante de '" + t->id + "' = " + std::to_string(v);
        break;
    }
    case EditKind::Suspend:
        if (t->state != TaskState::Ready && t->state != TaskState::Running && t->state != TaskState::Dormant) {
            return refuse(name + " " + stateText(t->state) + ": só é possível suspender uma tarefa pronta, executando ou dormente.");
        }
        releaseCpu(s, *t);
        t->state = TaskState::Suspended;
        t->blockReason = "manual";
        note = "suspendeu '" + t->id + "'";
        break;
    case EditKind::Resume:
        if (t->state != TaskState::Suspended) {
            return refuse(name + " " + stateText(t->state) + ": só uma tarefa suspensa pode ser retomada.");
        }
        t->state = t->jobs.empty() ? TaskState::Dormant : TaskState::Ready;
        t->blockReason.clear();
        note = "retomou '" + t->id + "'";
        break;
    case EditKind::ForceFinish:
    case EditKind::Remove:
        if (t->state == TaskState::Terminated) {
            return refuse(name + " já terminou; uma tarefa terminada não volta à simulação.");
        }
        releaseCpu(s, *t);
        t->jobs.clear();
        t->state = TaskState::Terminated;
        t->finishTime = now;
        if (e.kind == EditKind::ForceFinish) {
            s.occurrences.push_back(Occurrence{OccurrenceType::TaskFinished, e.taskIndex, -1, "término forçado"});
            note = "forçou o término de '" + t->id + "'";
        } else {
            note = "removeu '" + t->id + "' da simulação";
        }
        break;
    case EditKind::ActivateNow:
        if (t->state != TaskState::Dormant) {
            return refuse(name + " " + stateText(t->state) + ": só uma tarefa dormente (esperando a próxima ativação) pode ser ativada agora.");
        }
        if (t->released >= PERIODIC_ACTIVATION_LIMIT) {
            return refuse(name + " já foi ativada " + std::to_string(PERIODIC_ACTIVATION_LIMIT) + " vezes (limite do Projeto A).");
        }
        // A ativação manual antecipa a próxima: o período passa a contar daqui.
        t->jobs.push_back(Job{now, now + t->deadline, t->duration, t->duration, false});
        t->released++;
        t->nextRelease = now + t->period;
        t->state = TaskState::Ready;
        s.occurrences.push_back(Occurrence{OccurrenceType::Arrival, e.taskIndex, -1, "ativação manual"});
        note = "ativou '" + t->id + "' agora";
        break;
    case EditKind::SetArrival: {
        if (t->state != TaskState::NotArrived) {
            return refuse(name + " já ingressou (instante " + std::to_string(t->arrival) + "): o ingresso só pode mudar antes de a tarefa chegar.");
        }
        if (v < now) {
            return refuse("O novo ingresso (" + std::to_string(v) + ") não pode ficar no passado: deve ser >= " + std::to_string(now) + " (instante atual).");
        }
        std::string bad = checkRange("ingresso", 0);
        if (!bad.empty()) return refuse(bad);
        t->arrival = v;
        t->nextRelease = v;  // se for agora, a ativação acontece neste instante
        note = "ingresso de '" + t->id + "' = " + std::to_string(v);
        break;
    }
    case EditKind::SetDuration: {
        std::string bad = checkRange("duracao", 1);
        if (!bad.empty()) return refuse(bad);
        t->duration = v;  // vale para as próximas ocorrências; a corrente segue como está
        note = "duração de '" + t->id + "' = " + std::to_string(v) + " (próximas ocorrências)";
        break;
    }
    case EditKind::SetPeriod: {
        std::string bad = checkRange("periodo", 1);
        if (!bad.empty()) return refuse(bad);
        if (t->released > 0) {
            // A próxima ativação passa a ser a última + o novo período (nunca no passado).
            const Tick lastRelease = t->nextRelease - t->period;
            t->nextRelease = std::max(lastRelease + v, now);
        }
        t->period = v;
        note = "período de '" + t->id + "' = " + std::to_string(v);
        break;
    }
    case EditKind::SetDeadline: {
        std::string bad = checkRange("prazo", 1);
        if (!bad.empty()) return refuse(bad);
        t->deadline = v;
        // Ocorrências pendentes ainda sem perda de prazo ganham o novo prazo absoluto.
        for (Job& job : t->jobs) {
            if (!job.missed) job.absDeadline = job.release + v;
        }
        note = "prazo de '" + t->id + "' = " + std::to_string(v);
        break;
    }
    case EditKind::SetColor: {
        std::string color;
        if (!parser::normalizeColor(e.text, color)) {
            return refuse("A cor '" + e.text + "' não está no formato RRGGBB hexadecimal (ex.: F0E0D0).");
        }
        for (const Task& other : s.tasks) {
            if (&other != t && other.color == color) {
                return refuse("A cor " + color + " já é usada pela tarefa '" + other.id + "': cada tarefa precisa de uma cor diferente.");
            }
        }
        if (parser::isTooDarkColor(color)) {
            return refuse("A cor " + color + " é escura demais: o preto indica tarefa suspensa no gráfico.");
        }
        if (parser::isTooLightColor(color)) {
            return refuse("A cor " + color + " é clara demais: no gráfico, a ausência de cor indica tarefa pronta.");
        }
        t->color = color;
        note = "cor de '" + t->id + "' = " + color;
        break;
    }
    case EditKind::SetAlgorithm: {
        if (findScheduler(e.text) == nullptr) {
            std::string names;
            for (const std::string& n : schedulerNames()) names += (names.empty() ? "" : ", ") + n;
            return refuse("Algoritmo de escalonamento desconhecido '" + e.text + "'. Algoritmos disponíveis: " + names + ".");
        }
        std::string upper = e.text;
        for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        s.algorithm = upper;  // vale a partir deste instante [A3.1]
        note = "algoritmo = " + upper;
        break;
    }
    case EditKind::SetCpuCount: {
        if (v < 1 || v > MAX_CPUS) {
            return refuse("O número de CPUs deve ficar entre 1 e " + std::to_string(MAX_CPUS) +
                          " (recebido: " + std::to_string(v) + ").");
        }
        const int count = static_cast<int>(v);
        // Ao reduzir, quem estava numa CPU removida volta à fila (preempção).
        for (int c = count; c < static_cast<int>(s.cpus.size()); ++c) {
            const int index = s.cpus[c].taskIndex;
            if (index < 0) continue;
            Task& running = s.tasks[index];
            running.state = TaskState::Ready;
            running.cpu = -1;
            running.lastCpu = c;
            running.preemptions++;
            s.occurrences.push_back(Occurrence{OccurrenceType::Preemption, index, -1, "CPU removida"});
        }
        const int before = static_cast<int>(s.cpus.size());
        s.cpus.resize(static_cast<std::size_t>(count));
        // CPU nova: conta como desligada desde o início (busy + off = instante,
        // invariante 7) e é ligada pelo despacho se receber tarefa.
        for (int c = before; c < count; ++c) {
            s.cpus[c] = CPU();
            s.cpus[c].id = c;
            s.cpus[c].on = false;
            s.cpus[c].ticksOff = now;
        }
        note = "nº de CPUs = " + std::to_string(count);
        break;
    }
    case EditKind::SetQuantum:
        if (v < 1 || v > MAX_QUANTUM) {
            return refuse("O quantum deve ser um inteiro entre 1 e " + std::to_string(MAX_QUANTUM) + ".");
        }
        s.quantum = static_cast<int>(v);
        note = "quantum = " + std::to_string(v);
        break;
    case EditKind::SetSeed:
        if (v < 0) return refuse("A semente do sorteio deve ser um inteiro >= 0.");
        s.seed = static_cast<uint64_t>(v);
        note = "semente = " + std::to_string(v);
        break;
    case EditKind::AddTask: {
        Task added;
        EditResult check = checkNewTask(s, e.newTask, added);
        if (!check.ok) return check;
        if (added.arrival < now) {
            return refuse("O ingresso da nova tarefa (" + std::to_string(added.arrival) +
                          ") não pode ficar no passado: deve ser >= " + std::to_string(now) + " (instante atual).");
        }
        added.nextRelease = added.arrival;  // se for agora, é ativada neste instante
        target = static_cast<int>(s.tasks.size());
        s.tasks.push_back(added);
        warning = check.message;
        note = "adicionou a tarefa '" + added.id + "'";
        break;
    }
    }

    s.occurrences.push_back(Occurrence{OccurrenceType::ManualEdit, onTask || e.kind == EditKind::AddTask ? target : -1, -1, note});
    return {true, warning};
}

// ---------------------------------------------------------------------------
// Simulator
// ---------------------------------------------------------------------------

bool Simulator::load(const Config& config) {
    if (findScheduler(config.algorithm) == nullptr) return false;
    config_ = config;
    history_.clear();
    history_.push_back(initState(config));
    edits_.assign(1, {});
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
    edits_.push_back({});
    cursor_++;
    return true;
}

bool Simulator::stepBack() {               // voltar: só move o índice
    if (cursor_ == 0) return false;
    cursor_--;
    return true;
}

void Simulator::reset() {
    history_.resize(1);
    edits_.resize(1);
    cursor_ = 0;
}

bool Simulator::jumpTo(Tick tick) {
    if (tick < 0) return false;
    // Para trás (ou para um instante já calculado): só move o cursor, O(1).
    if (static_cast<std::size_t>(tick) < history_.size()) {
        cursor_ = static_cast<std::size_t>(tick);
        return true;
    }
    // Para a frente: calcula o que faltar, como vários "avançar".
    cursor_ = history_.size() - 1;
    while (current().tick < tick) {
        if (!stepForward()) return false;  // terminou antes de chegar lá
    }
    return true;
}

EditResult Simulator::applyEdit(const Edit& edit) {
    const std::size_t k = cursor_;

    // 1. Refaz o instante k só até ANTES do escalonador (D28): a partir de
    //    history[k-1] (consome o tick e processa conclusões/ativações/prazos)
    //    ou, no instante 0, a partir da configuração carregada.
    SystemSnapshot s = (k == 0) ? baseState(config_) : history_[k - 1];
    if (k > 0) {
        s.occurrences.clear();
        runOneTick(s);
        s.tick = history_[k - 1].tick + 1;
    }
    beginInstant(s);

    // 2. Reaplica as edições já feitas neste mesmo instante, na mesma ordem
    //    (senão a segunda edição em k apagaria a primeira).
    for (const Edit& previous : edits_[k]) applyEditTo(s, previous);

    // 3. A edição nova. Recusada => nada muda (history_, edits_ e cursor_ intactos).
    EditResult result = applyEditTo(s, edit);
    if (!result.ok) return result;

    // 4. Uma edição pode trazer uma ativação ou um prazo para agora (ex.: novo
    //    ingresso = instante atual, prazo reduzido); os dois passos são
    //    idempotentes. Depois, o escalonador já vê o estado editado (D28).
    releaseActivations(s);
    checkDeadlines(s);
    scheduleAndDispatch(s);

    // 5. O futuro calculado antes deixou de valer (D15): descarta e segue daqui.
    std::vector<Edit> done = edits_[k];
    done.push_back(edit);
    history_.resize(k);
    history_.push_back(s);
    edits_.resize(k);
    edits_.push_back(done);
    return result;
}

std::size_t estimateSnapshotBytes(const SystemSnapshot& s) {
    std::size_t bytes = sizeof(SystemSnapshot) + s.algorithm.capacity();
    for (const Task& t : s.tasks) {
        bytes += sizeof(Task) + t.jobs.capacity() * sizeof(Job) + t.id.capacity() +
                 t.color.capacity() + t.rawEvents.capacity() + t.ignoredReason.capacity() +
                 t.blockReason.capacity();
    }
    bytes += s.cpus.capacity() * sizeof(CPU) + s.readyQueue.capacity() * sizeof(int);
    for (const Occurrence& o : s.occurrences) bytes += sizeof(Occurrence) + o.note.capacity();
    return bytes;
}

RunSummary Simulator::runToEnd(Tick maxTicks, std::size_t maxBytes) {
    RunSummary summary;
    // Tamanho estimado do histórico = nº de snapshots x tamanho do snapshot atual
    // (os snapshots de uma simulação têm tamanhos parecidos). Recalculado a cada
    // 64 passos para não custar mais que a própria simulação.
    std::size_t perSnapshot = estimateSnapshotBytes(current());
    std::size_t steps = 0;
    while (!isFinished()) {
        if (stalled(current())) {
            std::string ids;
            for (const Task& t : current().tasks) {
                if (!t.ignored && t.state == TaskState::Suspended) ids += (ids.empty() ? "" : ", ") + t.id;
            }
            summary.message = "Simulação parada no instante " + std::to_string(current().tick) +
                              ": só restam tarefas suspensas (" + ids + "), que não voltam sozinhas. "
                              "Retome-as ou force o término delas.";
            break;
        }
        if (current().tick >= maxTicks) {
            summary.message = "Simulação interrompida no instante " + std::to_string(current().tick) +
                              ": limite de " + std::to_string(maxTicks) +
                              " instantes atingido. Use 'avançar' para continuar daqui.";
            break;
        }
        if (history_.size() * perSnapshot > maxBytes) {
            summary.message = "Simulação interrompida no instante " + std::to_string(current().tick) +
                              ": o histórico ocuparia mais de " + std::to_string(maxBytes >> 20) +
                              " MB de memória. Reduza o número de tarefas ou os períodos.";
            break;
        }
        stepForward();
        if (++steps % 64 == 0) perSnapshot = estimateSnapshotBytes(current());
    }
    summary.finished = isFinished();
    summary.ticks = current().tick;
    if (summary.finished) {
        summary.message = "Simulação concluída no instante " + std::to_string(summary.ticks) + ".";
    }
    return summary;
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
