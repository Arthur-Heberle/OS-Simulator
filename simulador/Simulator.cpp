// =============================================================================
// Simulator.cpp — Motor da simulação (design.md §6) e classe Simulator.
// Contrato em Simulator.h.
// =============================================================================
#include "Simulator.h"
#include "Parser.h"   // regras de validação do arquivo (AddTask, SetColor)
#include <algorithm>
#include <cctype>
#include <string>

// ---------------------------------------------------------------------------
// Linha do tempo (D3): o tick t é o intervalo [t, t+1). history[k] é o estado
// depois de processar o instante k: suas CPUs dizem quem executa em [k, k+1)
// e suas ocorrências aconteceram em x = k. O Gantt lê a coluna k direto de
// history[k].
// ---------------------------------------------------------------------------

// Um instante é processado em duas metades; a edição manual entra entre elas
// (D28), para o escalonador já ver o estado editado.

// 1ª metade: conclusões -> ativações -> perdas de prazo.
void beginInstant(SystemSnapshot& s) {
    finishActivations(s);
    releaseActivations(s);
    checkDeadlines(s);
}

// 2ª metade: escalonamento -> despacho (e fila de prontos).
void scheduleAndDispatch(SystemSnapshot& s) {
    // O algoritmo vem do snapshot porque pode mudar durante a simulação [A3.1].
    SchedulerFn scheduler = findScheduler(s.algorithm);
    PriorityGroups groups = scheduler ? scheduler(s) : PriorityGroups{};

    // Escolhe quem executa e despacha.
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
    next.occurrences.clear();          // a cópia trouxe os eventos do instante anterior

    runOneTick(next);                  // consome [prev.tick, prev.tick + 1)
    next.tick = prev.tick + 1;
    beginInstant(next);
    scheduleAndDispatch(next);
    return next;
}

bool finished(const SystemSnapshot& s) {
    for (const Task& t : s.tasks) {
        if (!t.ignored && t.state != TaskState::Terminated) {
            return false;
        }
    }
    return true;
}

bool stalled(const SystemSnapshot& s) {
    bool anySuspended = false;
    for (const Task& t : s.tasks) {
        if (t.ignored || t.state == TaskState::Terminated) continue;
        if (t.state != TaskState::Suspended || t.released < PERIODIC_ACTIVATION_LIMIT) return false;
        for (const Job& job : t.jobs) {
            if (!job.missed && job.absDeadline > s.tick) return false;  // perda de prazo ainda por registrar
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
    // Tempo de espera: tarefas prontas que não executaram neste tick.
    for (Task& task : s.tasks) {
        if (task.state == TaskState::Ready) task.waitTicks++;
    }
}

// Registra uma ocorrência no instante atual; o Gantt a desenha em x = s.tick [A2.2].
static void addOccurrence(SystemSnapshot& s, OccurrenceType type, int taskIndex, int cpuIndex,
                          const std::string& note = "") {
    s.occurrences.push_back(Occurrence{type, taskIndex, cpuIndex, note});
}

// Passo 1: conclusões. Ocorrência com tempo restante 0 terminou neste instante.
// Vem antes das perdas de prazo: quem termina no próprio prazo não perde (D9).
void finishActivations(SystemSnapshot& s) {
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        Task& t = s.tasks[i];
        if (t.jobs.empty() || t.jobs.front().remaining > 0) continue;

        // Termination marca o fim de uma ocorrência (Q5), com a CPU em que executou.
        addOccurrence(s, OccurrenceType::Termination, i, t.cpu);
        t.jobs.erase(t.jobs.begin());
        t.completed++;
        if (t.cpu >= 0) {               // libera a CPU; lastCpu guarda a afinidade (D10)
            s.cpus[t.cpu].taskIndex = -1;
            t.lastCpu = t.cpu;
            t.cpu = -1;
        }

        if (!t.jobs.empty()) {
            t.state = TaskState::Ready;     // há ocorrência enfileirada (D5)
        } else if (t.released >= PERIODIC_ACTIVATION_LIMIT) {
            t.state = TaskState::Terminated;  // concluiu a 10ª ocorrência [A4.4] (D6)
            t.finishTime = s.tick;
            addOccurrence(s, OccurrenceType::TaskFinished, i, -1);
        } else {
            t.state = TaskState::Dormant;   // espera a próxima ativação sem disputar CPU (D1)
        }
    }
}

// Passo 2: ativações. Cada período cria uma ocorrência com prazo absoluto =
// ativação + prazo [A3.3]. Se a anterior ainda não terminou, a nova é
// enfileirada (D5).
void releaseActivations(SystemSnapshot& s) {
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        Task& t = s.tasks[i];
        if (t.ignored || t.state == TaskState::Terminated) continue;  // fora do A (D7) ou encerrada
        if (t.released >= PERIODIC_ACTIVATION_LIMIT || t.nextRelease != s.tick) continue;

        const bool queued = !t.jobs.empty();
        t.jobs.push_back(Job{s.tick, s.tick + t.deadline, t.duration, t.duration, false});
        t.released++;
        t.nextRelease += t.period;
        // Arrival a cada ativação: a ocorrência entra na fila de prontos (Q5) [A2.2].
        addOccurrence(s, OccurrenceType::Arrival, i, -1, queued ? "enfileirada" : "");

        // Ready e Running só ganham trabalho; Suspended continua suspensa.
        if (t.state == TaskState::NotArrived || t.state == TaskState::Dormant) {
            t.state = TaskState::Ready;
        }
    }
}

// Passo 3: perdas de prazo. Prazo absoluto atingido com execução pendente é
// registrado uma vez por ocorrência (D9); a tarefa continua executando [A3.3].
void checkDeadlines(SystemSnapshot& s) {
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        Task& t = s.tasks[i];
        for (Job& job : t.jobs) {
            // '<=' em vez de '==': uma edição de prazo (SetDeadline) pode
            // deixar o prazo absoluto antes do instante atual.
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
            // O grupo cabe inteiro nas CPUs livres: sem sorteio.
            chosen.insert(chosen.end(), group.begin(), group.end());
            slotsLeft -= static_cast<int>(group.size());
        } else {
            // Empate que não cabe nas CPUs livres: sorteio, critério (5) do A4.3,
            // pelo hash sem estado (D14). O índice desempata colisões de hash.
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

// Passos 5 e 6: despacho. 'chosen' são as tarefas que executam em
// [tick, tick+1); 'drawn', as escolhidas por sorteio [A4.3].
void dispatch(SystemSnapshot& s, const std::vector<int>& chosen, const std::vector<int>& drawn) {
    const int taskCount = static_cast<int>(s.tasks.size());
    const int cpuCount = static_cast<int>(s.cpus.size());
    std::vector<bool> isChosen(static_cast<std::size_t>(taskCount), false);
    for (int i : chosen) isChosen[static_cast<std::size_t>(i)] = true;

    // (a) Preempção: executava e não foi escolhida. Volta a Ready e libera a CPU.
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

    // (b) Escolhida que já executa fica na mesma CPU (D10). (c) Escolhida nova
    // vai para uma CPU livre, em duas passadas: primeiro as que podem voltar à
    // última CPU usada; depois as demais, na CPU livre de menor índice. Assim
    // uma tarefa sem afinidade não ocupa a CPU de outra e não força migração.
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
        // Ícone de sorteio quando a escolhida (re)começa [A4.3].
        if (std::find(drawn.begin(), drawn.end(), i) != drawn.end()) {
            addOccurrence(s, OccurrenceType::TieBreakDraw, i, c);
        }
    };
    std::vector<int> waiting;  // escolhidas sem a CPU de afinidade livre
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
        if (c == cpuCount) break;  // defensivo: chooseTasks escolhe no máximo uma tarefa por CPU
        start(i, c);
    }

    // (d) CPU sem tarefa é desligada; CPU desligada que recebe tarefa é religada
    // [A1.2]. Só as transições geram ocorrência.
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

    // Passo 6: fila global de prontos [A1.2], por ativação da ocorrência
    // corrente e, em empate, por índice.
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

// Tira a tarefa da CPU sem registrar ocorrência (suspender e encerrar).
static void releaseCpu(SystemSnapshot& s, Task& t) {
    if (t.cpu >= 0) {
        s.cpus[t.cpu].taskIndex = -1;
        t.lastCpu = t.cpu;
        t.cpu = -1;
    }
}

// Valida a tarefa nova com as regras do arquivo [A3.3] (D18): monta um texto
// com as tarefas atuais mais a nova, lê com parseConfig e considera só as
// mensagens da última linha. Id duplicado, números, cor e prazo padrão seguem
// assim exatamente o parser.
static EditResult checkNewTask(const SystemSnapshot& s, const Task& candidate, Task& out) {
    // ';' ou quebra de linha mudariam a divisão do texto montado em campos e linhas.
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

    out = parsed.config.tasks.back();  // campos já normalizados pelo parser
    return {true, fromNewLine(parsed.warnings)};
}

EditResult applyEditTo(SystemSnapshot& s, const Edit& e) {
    auto refuse = [](const std::string& why) { return EditResult{false, why}; };
    const Tick now = s.tick;
    const long long v = e.value;
    std::string note;     // texto da ocorrência ManualEdit
    std::string warning;  // aviso devolvido junto com ok = true
    int target = e.taskIndex;

    // Edição de tarefa: a tarefa precisa existir e ser simulada no Projeto A.
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
    // Faixa dos parâmetros de tempo: a mesma do arquivo (design.md §8).
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
        // A ativação manual antecipa a próxima; o período passa a contar daqui.
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
        t->duration = v;  // vale a partir da próxima ocorrência
        note = "duração de '" + t->id + "' = " + std::to_string(v) + " (próximas ocorrências)";
        break;
    }
    case EditKind::SetPeriod: {
        std::string bad = checkRange("periodo", 1);
        if (!bad.empty()) return refuse(bad);
        if (t->released > 0) {
            // Próxima ativação = última + novo período, nunca no passado.
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
        // Ocorrências pendentes ainda sem perda recebem o novo prazo absoluto.
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
        // Ao reduzir, a tarefa de uma CPU removida volta à fila (preempção).
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
        // CPU nova conta como desligada desde o instante 0 (invariante 7:
        // ocupada + desligada = instante); o despacho a liga se receber tarefa.
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
        if (v < 0 || v > MAX_QUANTUM) {
            return refuse("O quantum deve ser um inteiro entre 0 e " + std::to_string(MAX_QUANTUM) + ".");
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
    if (cursor_ + 1 < history_.size()) {   // instante já calculado
        cursor_++;
        return true;
    }
    if (isFinished()) return false;        // fim da simulação
    history_.push_back(makeNext(history_[cursor_]));
    edits_.push_back({});
    cursor_++;
    return true;
}

bool Simulator::stepBack() {               // só move o cursor
    if (cursor_ == 0) return false;
    cursor_--;
    return true;
}

void Simulator::reset() {
    history_.resize(1);
    edits_.resize(1);
    cursor_ = 0;
}

RunSummary Simulator::jumpTo(Tick tick, std::size_t maxBytes) {
    if (tick < 0) {
        RunSummary summary;
        summary.finished = isFinished();
        summary.ticks = current().tick;
        summary.message = "O instante deve ser 0 ou maior.";
        return summary;
    }
    // Instante já calculado: só move o cursor.
    if (static_cast<std::size_t>(tick) < history_.size()) {
        cursor_ = static_cast<std::size_t>(tick);
        return RunSummary{isFinished(), tick, ""};
    }
    // Para a frente: calcula o que falta. Tarefa suspensa não impede, porque o
    // usuário pediu um instante específico; param antes só o fim e a memória.
    cursor_ = history_.size() - 1;
    RunSummary summary = advance(tick, maxBytes, false);
    if (summary.message.empty() && current().tick < tick) {
        summary.message = "A simulação termina no instante " + std::to_string(current().tick) +
                          "; não existe o instante " + std::to_string(tick) + ".";
    }
    return summary;
}

EditResult Simulator::applyEdit(const Edit& edit) {
    const std::size_t k = cursor_;

    // 1. Refaz o instante k até antes do escalonador (D28), a partir de
    //    history[k-1] ou, no instante 0, da configuração carregada.
    SystemSnapshot s = (k == 0) ? baseState(config_) : history_[k - 1];
    if (k > 0) {
        s.occurrences.clear();
        runOneTick(s);
        s.tick = history_[k - 1].tick + 1;
    }
    beginInstant(s);

    // 2. Reaplica as edições anteriores deste instante, na mesma ordem.
    for (const Edit& previous : edits_[k]) applyEditTo(s, previous);

    // 3. Edição nova. Se recusada, history_, edits_ e cursor_ não mudam.
    EditResult result = applyEditTo(s, edit);
    if (!result.ok) return result;

    // 4. A edição pode trazer uma ativação ou um prazo para este instante (novo
    //    ingresso, prazo reduzido); os dois passos são idempotentes. Depois, o
    //    escalonador decide sobre o estado editado (D28).
    releaseActivations(s);
    checkDeadlines(s);
    scheduleAndDispatch(s);

    // 5. O futuro calculado antes deixou de valer (D15).
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

RunSummary Simulator::advance(Tick target, std::size_t maxBytes, bool stopWhenStalled) {
    RunSummary summary;
    // Histórico estimado = nº de snapshots x tamanho do snapshot atual (os
    // snapshots de uma simulação têm tamanhos parecidos). O tamanho é
    // recalculado a cada 64 passos para a estimativa não pesar no laço.
    std::size_t perSnapshot = estimateSnapshotBytes(current());
    std::size_t steps = 0;
    while (!isFinished() && current().tick < target) {
        if (stopWhenStalled && stalled(current())) {
            std::string ids;
            for (const Task& t : current().tasks) {
                if (!t.ignored && t.state == TaskState::Suspended) ids += (ids.empty() ? "" : ", ") + t.id;
            }
            summary.message = "Simulação parada no instante " + std::to_string(current().tick) +
                              ": só restam tarefas suspensas (" + ids + "), que não voltam sozinhas. "
                              "Retome-as ou force o término delas.";
            break;
        }
        if ((history_.size() + 1) * perSnapshot > maxBytes) {
            summary.message = "Simulação interrompida no instante " + std::to_string(current().tick) +
                              ": o histórico de todos os instantes ocuparia mais de " +
                              std::to_string(maxBytes >> 20) + " MB, o máximo de memória reservado para ele. "
                              "Reduza o número de tarefas ou os períodos.";
            break;
        }
        stepForward();
        if (++steps % 64 == 0) perSnapshot = estimateSnapshotBytes(current());
    }
    summary.finished = isFinished();
    summary.ticks = current().tick;
    return summary;
}

RunSummary Simulator::runToEnd(Tick maxTicks, std::size_t maxBytes) {
    RunSummary summary = advance(maxTicks, maxBytes, true);
    if (summary.finished) {
        summary.message = "Simulação concluída no instante " + std::to_string(summary.ticks) + ".";
    } else if (summary.message.empty()) {
        summary.message = "Simulação interrompida no instante " + std::to_string(summary.ticks) +
                          ": limite de " + std::to_string(maxTicks) +
                          " instantes atingido. Use 'avançar' para continuar daqui.";
    }
    return summary;
}
