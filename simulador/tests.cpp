// =============================================================================
// tests.cpp — Testes automáticos do núcleo da simulação (executável sim_tests).
//
// O quê: verifica o núcleo (simcore) sem nenhuma janela e sem Qt.
//   - Fase 0: teste de fumaça (build, structs copiáveis).
//   - Fase 1: ferramentas de teste do contrato (serializeSnapshot, verificador
//     de invariantes de histórico, helpers de consulta), naturalIdLess e o
//     histórico de demonstração de G1 (demoHistoryG1).
//   - Fase 2: parser P1–P14 e robustez (docs/testes.md §3).
//   - Fase 3: motor (G1–G4, FIFO, invariantes 1–9, fuzz). 'sim_tests --grid'
//     imprime as grades de G1, G2 e G3.
//   - Fase 4: G5, G6 e invariante 10.
//
// Por quê assim:
//   - Sem GoogleTest ou outra biblioteca: a macro CHECK abaixo basta e evita
//     dependência extra (design.md §2, "Testes"); tudo cabe em um arquivo
//     (orçamento de 18 arquivos, D23).
//   - CHECK não aborta no primeiro erro: conta as falhas e mostra todas, o que
//     agiliza a depuração. O main devolve código != 0 se houver falha, para que
//     scripts e a defesa vejam o resultado de imediato.
//
// Requisitos: RG6 (comentários), A4.2 (núcleo separado da interface).
// Decisões: D22 (sim_tests liga só no simcore), D23 (arquivo único de testes).
//
// Rodar: cmake --build build && build\sim_tests.exe   (Linux: ./build/sim_tests)
// =============================================================================

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Model.h"
#include "Parser.h"
#include "Scheduler.h"
#include "Simulator.h"

// -----------------------------------------------------------------------------
// Harness mínimo (docs/testes.md §1).
// Contadores globais só deste executável de testes: o núcleo em si não tem
// globais mutáveis (regra do projeto); aqui eles são apenas o placar.
// -----------------------------------------------------------------------------
static int g_checks = 0;  // quantas verificações foram feitas
static int g_fails = 0;   // quantas falharam

// Verifica uma condição; se falsa, imprime arquivo, linha e a expressão que falhou.
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_fails;                                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << "  CHECK falhou: " #cond \
                      << "\n";                                                   \
        }                                                                        \
    } while (0)

// Igualdade entre dois valores (açúcar sintático para CHECK).
#define CHECK_EQ(a, b) CHECK((a) == (b))

// -----------------------------------------------------------------------------
// Ferramentas de teste (docs/testes.md §1 e §4).
// -----------------------------------------------------------------------------

// Nome legível de cada estado e ocorrência (usado na serialização e nas mensagens).
static const char* stateName(TaskState state) {
    switch (state) {
        case TaskState::NotArrived: return "NotArrived";
        case TaskState::Ready:      return "Ready";
        case TaskState::Running:    return "Running";
        case TaskState::Dormant:    return "Dormant";
        case TaskState::Suspended:  return "Suspended";
        case TaskState::Terminated: return "Terminated";
    }
    return "?";
}

static const char* occurrenceName(OccurrenceType type) {
    switch (type) {
        case OccurrenceType::Arrival:      return "Arrival";
        case OccurrenceType::Termination:  return "Termination";
        case OccurrenceType::TaskFinished: return "TaskFinished";
        case OccurrenceType::DeadlineMiss: return "DeadlineMiss";
        case OccurrenceType::Dispatch:     return "Dispatch";
        case OccurrenceType::Preemption:   return "Preemption";
        case OccurrenceType::Migration:    return "Migration";
        case OccurrenceType::TieBreakDraw: return "TieBreakDraw";
        case OccurrenceType::CpuOn:        return "CpuOn";
        case OccurrenceType::CpuOff:       return "CpuOff";
        case OccurrenceType::ManualEdit:   return "ManualEdit";
    }
    return "?";
}

// Texto determinístico com TODO o conteúdo de um snapshot (todos os campos de
// tarefas, ocorrências (jobs), CPUs, fila e ocorrências do instante). Dois
// snapshots são iguais se e só se os textos são iguais: é assim que G6 compara
// históricos (retroceder/avançar não muda history[k], invariante 10) e que um
// teste mostra exatamente o que divergiu. Como o modelo não tem ponteiros
// (D21), o texto depende só dos valores.
static std::string serializeSnapshot(const SystemSnapshot& s) {
    std::ostringstream out;
    out << "tick=" << s.tick << " algorithm=" << s.algorithm << " quantum=" << s.quantum
        << " seed=" << s.seed << "\n";
    for (std::size_t i = 0; i < s.tasks.size(); ++i) {
        const Task& t = s.tasks[i];
        out << "task[" << i << "] id=" << t.id << " color=" << t.color << " arrival=" << t.arrival
            << " duration=" << t.duration << " period=" << t.period << " deadline=" << t.deadline
            << " kind=" << static_cast<int>(t.kind) << " server=" << static_cast<int>(t.server)
            << " ignored=" << t.ignored << " ignoredReason=" << t.ignoredReason
            << " rawEvents=" << t.rawEvents << " state=" << stateName(t.state) << " cpu=" << t.cpu
            << " lastCpu=" << t.lastCpu << " nextRelease=" << t.nextRelease
            << " released=" << t.released << " completed=" << t.completed
            << " runTicks=" << t.runTicks << " waitTicks=" << t.waitTicks
            << " firstStart=" << t.firstStart << " finishTime=" << t.finishTime
            << " preemptions=" << t.preemptions << " migrations=" << t.migrations
            << " deadlineMisses=" << t.deadlineMisses << " basePriority=" << t.basePriority
            << " effPriority=" << t.effPriority << " blockReason=" << t.blockReason
            << " serverBudget=" << t.serverBudget << " jobs=";
        for (const Job& j : t.jobs) {
            out << "(" << j.release << "," << j.absDeadline << "," << j.duration << ","
                << j.remaining << "," << j.missed << ")";
        }
        out << "\n";
    }
    for (std::size_t c = 0; c < s.cpus.size(); ++c) {
        const CPU& cpu = s.cpus[c];
        out << "cpu[" << c << "] id=" << cpu.id << " task=" << cpu.taskIndex << " on=" << cpu.on
            << " quantumUsed=" << cpu.quantumUsed << " busy=" << cpu.ticksBusy
            << " off=" << cpu.ticksOff << "\n";
    }
    out << "ready=";
    for (int index : s.readyQueue) out << index << ",";
    out << "\n";
    for (const Occurrence& o : s.occurrences) {
        out << "occ " << occurrenceName(o.type) << " task=" << o.taskIndex << " cpu=" << o.cpuIndex
            << " note=" << o.note << "\n";
    }
    return out.str();
}

// Id da tarefa que ocupa a CPU 'cpu' em [tick, tick+1), ou "-" se a CPU está
// sem tarefa (D3: a coluna 'tick' do Gantt é lida de history[tick]).
static std::string taskOn(const std::vector<SystemSnapshot>& history, Tick tick, int cpu) {
    const SystemSnapshot& s = history[static_cast<std::size_t>(tick)];
    int index = s.cpus[static_cast<std::size_t>(cpu)].taskIndex;
    return index < 0 ? "-" : s.tasks[static_cast<std::size_t>(index)].id;
}

// Quantas ocorrências do tipo 'type' a tarefa 'taskIndex' teve no histórico
// inteiro (-1 = qualquer tarefa).
static int countOcc(const std::vector<SystemSnapshot>& history, OccurrenceType type, int taskIndex) {
    int count = 0;
    for (const SystemSnapshot& s : history) {
        for (const Occurrence& o : s.occurrences) {
            if (o.type == type && (taskIndex < 0 || o.taskIndex == taskIndex)) ++count;
        }
    }
    return count;
}

// Instantes em que a ocorrência 'type' aconteceu para a tarefa 'taskIndex'
// (-1 = qualquer), em ordem: compara direto com as listas de docs/testes.md.
static std::vector<Tick> occTicks(const std::vector<SystemSnapshot>& history, OccurrenceType type,
                                  int taskIndex) {
    std::vector<Tick> ticks;
    for (const SystemSnapshot& s : history) {
        for (const Occurrence& o : s.occurrences) {
            if (o.type == type && (taskIndex < 0 || o.taskIndex == taskIndex)) {
                ticks.push_back(s.tick);
            }
        }
    }
    return ticks;
}

// Verifica as invariantes de docs/testes.md §4 que valem para QUALQUER
// histórico (feito à mão ou pelo motor): 1, 2, 3, 4, 5, 7 e 8. Devolve a
// lista de violações, cada uma com o instante e o número da invariante;
// vazia = tudo certo. Ficam para as fases do motor: 6 (estado final após 10
// ocorrências), 9 (determinismo entre duas execuções) e 10 (ida e volta).
static std::vector<std::string> checkInvariants(const std::vector<SystemSnapshot>& history) {
    std::vector<std::string> errors;
    auto fail = [&errors](Tick tick, int number, const std::string& what) {
        errors.push_back("instante " + std::to_string(tick) + ", invariante " +
                         std::to_string(number) + ": " + what);
    };

    for (std::size_t k = 0; k < history.size(); ++k) {
        const SystemSnapshot& s = history[k];
        const int cpuCount = static_cast<int>(s.cpus.size());
        const int taskCount = static_cast<int>(s.tasks.size());

        // 5. O relógio anda de 1 em 1: history[k].tick == k.
        if (s.tick != static_cast<Tick>(k)) {
            fail(s.tick, 5, "history[" + std::to_string(k) + "].tick errado");
        }

        // 1. Espelho tarefa <-> CPU: Running está em exatamente uma CPU e a CPU aponta para ela.
        for (int i = 0; i < taskCount; ++i) {
            const Task& t = s.tasks[i];
            if (t.state == TaskState::Running) {
                if (t.cpu < 0 || t.cpu >= cpuCount || s.cpus[t.cpu].taskIndex != i) {
                    fail(s.tick, 1, "tarefa " + t.id + " Running sem CPU coerente");
                }
            } else if (t.cpu != -1) {
                fail(s.tick, 1, "tarefa " + t.id + " fora de Running mas com cpu = " + std::to_string(t.cpu));
            }
        }
        bool someCpuOff = false;
        for (int c = 0; c < cpuCount; ++c) {
            const CPU& cpu = s.cpus[c];
            if (cpu.taskIndex != -1) {
                if (cpu.taskIndex < 0 || cpu.taskIndex >= taskCount ||
                    s.tasks[cpu.taskIndex].state != TaskState::Running ||
                    s.tasks[cpu.taskIndex].cpu != c) {
                    fail(s.tick, 1, "CPU " + std::to_string(c) + " aponta para tarefa incoerente");
                }
            }
            // 2. CPU desligada <=> sem tarefa (A1.2).
            if (cpu.on != (cpu.taskIndex != -1)) {
                fail(s.tick, 2, "CPU " + std::to_string(c) + " com 'on' incoerente com a tarefa");
            }
            if (!cpu.on) someCpuOff = true;
            // 7. Todo tick já executado foi contado como ocupado ou desligado.
            if (cpu.ticksBusy + cpu.ticksOff != s.tick) {
                fail(s.tick, 7, "CPU " + std::to_string(c) + ": busy + off != tick");
            }
        }
        // 2. Conservação de trabalho: nenhuma tarefa pronta com CPU desligada.
        for (int i = 0; i < taskCount; ++i) {
            if (someCpuOff && isCandidate(s.tasks[i]) && s.tasks[i].state == TaskState::Ready) {
                fail(s.tick, 2, "tarefa " + s.tasks[i].id + " Ready com CPU desligada");
            }
        }

        // 3. Tempo restante nunca negativo, e só quem estava Running consumiu tempo.
        for (int i = 0; i < taskCount; ++i) {
            const Task& t = s.tasks[i];
            for (const Job& j : t.jobs) {
                if (j.remaining < 0 || j.remaining > j.duration) {
                    fail(s.tick, 3, "tarefa " + t.id + " com remaining fora de [0, duração]");
                }
            }
            if (k > 0 && i < static_cast<int>(history[k - 1].tasks.size())) {
                const Task& before = history[k - 1].tasks[i];
                Tick expected = before.runTicks + (before.state == TaskState::Running ? 1 : 0);
                if (t.runTicks != expected) {
                    fail(s.tick, 3, "tarefa " + t.id + " consumiu tempo sem estar Running");
                }
            }
        }

        // 4. Prioridade: nenhuma Ready está num grupo de prioridade ANTES de uma
        // Running (o escalonador ordena; quem executa são os primeiros grupos).
        if (SchedulerFn scheduler = findScheduler(s.algorithm)) {
            PriorityGroups groups = scheduler(s);
            std::vector<int> groupOf(static_cast<std::size_t>(taskCount), -1);
            for (std::size_t g = 0; g < groups.size(); ++g) {
                for (int index : groups[g]) groupOf[static_cast<std::size_t>(index)] = static_cast<int>(g);
            }
            for (int r = 0; r < taskCount; ++r) {
                if (s.tasks[r].state != TaskState::Ready || groupOf[r] < 0) continue;
                for (int u = 0; u < taskCount; ++u) {
                    if (s.tasks[u].state == TaskState::Running && groupOf[u] > groupOf[r]) {
                        fail(s.tick, 4, "Ready " + s.tasks[r].id + " mais prioritária que Running " + s.tasks[u].id);
                    }
                }
            }
        }
    }

    // 8. Cada perda de prazo contada no TCB tem exatamente uma ocorrência DeadlineMiss.
    if (!history.empty()) {
        const SystemSnapshot& last = history.back();
        for (int i = 0; i < static_cast<int>(last.tasks.size()); ++i) {
            if (countOcc(history, OccurrenceType::DeadlineMiss, i) != last.tasks[i].deadlineMisses) {
                fail(last.tick, 8, "tarefa " + last.tasks[i].id + ": deadlineMisses != nº de DeadlineMiss");
            }
        }
    }
    return errors;
}

// Imprime as violações (se houver), para o teste que falhou mostrar o motivo.
static bool noViolations(const std::vector<std::string>& violations) {
    for (const std::string& v : violations) std::cerr << "  " << v << "\n";
    return violations.empty();
}

// -----------------------------------------------------------------------------
// Teste trivial da Fase 0: o núcleo compila, liga e suas structs funcionam.
// Prova também uma propriedade que o histórico vai exigir (D16, D21): um
// SystemSnapshot é copiado por valor e a cópia é independente do original.
// -----------------------------------------------------------------------------
static void testBuildSmoke() {
    // CPU começa sem tarefa (taskIndex = -1), como definido em Model.h (A1.2).
    CPU cpu;
    CHECK_EQ(cpu.taskIndex, -1);
    CHECK_EQ(cpu.ticksOff, 0);

    // Snapshot com uma tarefa e duas CPUs; a cópia não pode compartilhar dados.
    SystemSnapshot original;
    original.tasks.push_back(Task());
    original.cpus.resize(2);
    SystemSnapshot copy = original;
    copy.cpus[0].taskIndex = 0;
    copy.tick = 1;
    CHECK_EQ(copy.tasks.size(), 1u);
    CHECK_EQ(original.cpus[0].taskIndex, -1);  // o original não mudou
    CHECK_EQ(original.tick, 0);

    // Ligação com simcore: parser, registro de escalonadores e Simulator
    // funcionam juntos (os testes completos vêm nas Fases 2 a 4).
    ParseResult parsed = parser::parseConfig("EDF;2;2\n1;FF0000;0;1;4;4\n");
    CHECK(parsed.errors.empty());
    CHECK(findScheduler("edf") != nullptr);  // sem diferenciar caixa (A3.3.2)
    Simulator simulator;
    CHECK(simulator.load(parsed.config));
    CHECK_EQ(simulator.current().cpus.size(), 2u);
    CHECK_EQ(simulator.cursor(), 0u);
}

// -----------------------------------------------------------------------------
// Fase 1: contrato do Model.h.
// -----------------------------------------------------------------------------

// serializeSnapshot: cópia => mesmo texto; qualquer campo diferente => texto diferente.
static void testSerializeSnapshot() {
    SystemSnapshot a;
    a.tasks.push_back(Task());
    a.tasks[0].id = "T1";
    a.tasks[0].jobs.push_back(Job{0, 4, 1, 1, false});
    a.cpus.resize(1);
    a.occurrences.push_back(Occurrence{OccurrenceType::Arrival, 0, -1, ""});
    SystemSnapshot b = a;
    CHECK_EQ(serializeSnapshot(a), serializeSnapshot(b));
    b.tasks[0].jobs[0].remaining = 0;       // campo de um Job
    CHECK(serializeSnapshot(a) != serializeSnapshot(b));
    b = a;
    b.cpus[0].ticksOff = 1;                 // campo de uma CPU
    CHECK(serializeSnapshot(a) != serializeSnapshot(b));
    b = a;
    b.occurrences[0].note = "enfileirada";  // ocorrência do instante
    CHECK(serializeSnapshot(a) != serializeSnapshot(b));
}

// ParseResult::ok() e valores padrão do Edit (nenhum campo sem inicializar).
static void testModelDefaults() {
    ParseResult result;
    CHECK(result.ok());
    result.warnings.push_back("aviso");
    CHECK(result.ok());                     // aviso não impede carregar [A3.3.7]
    result.errors.push_back("erro");
    CHECK(!result.ok());

    Edit edit;
    CHECK_EQ(edit.taskIndex, -1);
    CHECK_EQ(edit.value, 0);
    EditResult editResult;
    CHECK(!editResult.ok);
}

// naturalIdLess: ordem natural por blocos [A2.5] (D4), e é um critério de
// ordenação válido (irreflexivo, assimétrico e transitivo).
static void testNaturalIdLess() {
    // O trio que quebra a regra "numérico se ambos forem números, senão texto".
    CHECK(naturalIdLess("2", "10"));
    CHECK(naturalIdLess("1A", "2"));
    CHECK(naturalIdLess("1A", "10"));
    CHECK(!naturalIdLess("10", "1A"));
    CHECK(naturalIdLess("T2", "T10"));
    CHECK(naturalIdLess("BS01", "BS2"));
    CHECK(naturalIdLess("a", "B"));         // texto sem diferenciar maiúsculas
    CHECK(!naturalIdLess("7", "7"));

    std::vector<std::string> ids = {"10", "2", "1A", "T10", "T2", "BS2", "BS01",
                                    "b", "A", "01", "1", "a2", "PS1"};
    int bad = 0;
    for (const std::string& x : ids) {
        if (naturalIdLess(x, x)) ++bad;                                  // irreflexiva
        for (const std::string& y : ids) {
            if (naturalIdLess(x, y) && naturalIdLess(y, x)) ++bad;       // assimétrica
            for (const std::string& z : ids) {
                if (naturalIdLess(x, y) && naturalIdLess(y, z) && !naturalIdLess(x, z)) ++bad;  // transitiva
            }
        }
    }
    CHECK_EQ(bad, 0);

    std::sort(ids.begin(), ids.end(), naturalIdLess);
    std::vector<std::string> expected = {"01", "1", "1A", "2", "10", "A", "a2", "b",
                                         "BS01", "BS2", "PS1", "T2", "T10"};
    CHECK(ids == expected);
}

// O verificador de invariantes acusa estados impossíveis (senão um histórico
// errado passaria em silêncio nas próximas fases).
static void testInvariantChecker() {
    SystemSnapshot s;
    s.tasks.push_back(Task());
    s.tasks[0].id = "T1";
    s.tasks[0].state = TaskState::Ready;
    s.tasks[0].jobs.push_back(Job{0, 4, 1, 1, false});
    s.cpus.resize(1);
    s.cpus[0].on = false;                   // CPU desligada com tarefa pronta: viola a invariante 2
    CHECK(!checkInvariants({s}).empty());

    s.cpus[0].on = true;
    s.cpus[0].taskIndex = 0;                // CPU aponta para tarefa que não está Running: viola a 1
    CHECK(!checkInvariants({s}).empty());

    s.tasks[0].state = TaskState::Running;
    s.tasks[0].cpu = 0;                     // agora coerente
    CHECK(noViolations(checkInvariants({s})));

    s.tick = 3;                             // history[0] com tick 3: viola a 5 (e a 7)
    CHECK(!checkInvariants({s}).empty());
}

// demoHistoryG1 (fixture temporário da Fase 1) confere com G1 de docs/testes.md:
// grade, ocorrências, valores de conferência e invariantes. Quando o motor
// existir (Fase 3), o G1 real será comparado com este mesmo roteiro.
static void testDemoHistoryG1() {
    const std::vector<SystemSnapshot> h = demoHistoryG1();
    const int T1 = 0, T2 = 1;
    CHECK_EQ(h.size(), 13u);  // instantes 0 a 12

    // Grade dos 12 primeiros ticks (CPU1): 0:1 1:2 2:2 3:- 4:1 5:- 6:2 7:2 8:1 9:- 10:- 11:-
    const std::vector<std::string> grid = {"1", "2", "2", "-", "1", "-", "2", "2", "1", "-", "-", "-"};
    for (Tick k = 0; k < 12; ++k) {
        CHECK_EQ(taskOn(h, k, 0), grid[static_cast<std::size_t>(k)]);
    }

    // Ocorrências nos instantes exatos (A2.2, Q5).
    CHECK(occTicks(h, OccurrenceType::Arrival, T1) == std::vector<Tick>({0, 4, 8, 12}));
    CHECK(occTicks(h, OccurrenceType::Arrival, T2) == std::vector<Tick>({0, 6, 12}));
    CHECK(occTicks(h, OccurrenceType::Termination, T1) == std::vector<Tick>({1, 5, 9}));
    CHECK(occTicks(h, OccurrenceType::Termination, T2) == std::vector<Tick>({3, 8}));
    CHECK(occTicks(h, OccurrenceType::CpuOff, -1) == std::vector<Tick>({3, 5, 9}));
    CHECK(occTicks(h, OccurrenceType::CpuOn, -1) == std::vector<Tick>({4, 6, 12}));
    CHECK_EQ(countOcc(h, OccurrenceType::DeadlineMiss, -1), 0);
    CHECK_EQ(countOcc(h, OccurrenceType::Preemption, -1), 0);
    CHECK_EQ(countOcc(h, OccurrenceType::TieBreakDraw, -1), 0);
    CHECK_EQ(countOcc(h, OccurrenceType::Migration, -1), 0);

    // Valores de conferência: CPU1 desligada 5 ticks em [0,12) (3, 5, 9, 10, 11).
    const SystemSnapshot& last = h.back();
    CHECK_EQ(last.cpus[0].ticksOff, 5);
    CHECK_EQ(last.cpus[0].ticksBusy, 7);
    CHECK_EQ(last.tasks[T1].released, 4);
    CHECK_EQ(last.tasks[T1].completed, 3);
    CHECK_EQ(last.tasks[T2].released, 3);
    CHECK_EQ(last.tasks[T2].completed, 2);
    CHECK_EQ(last.tasks[T1].runTicks, 3);
    CHECK_EQ(last.tasks[T2].runTicks, 4);
    CHECK_EQ(last.tasks[T2].waitTicks, 1);  // só esperou em [0,1)
    CHECK_EQ(h[3].tasks[T2].state, TaskState::Dormant);
    CHECK_EQ(h[2].tasks[T2].jobs.front().remaining, 1);

    // Utilização informativa de G1: U = 1/4 + 2/6 ≈ 0,583 (design.md §6).
    double u = 0.0;
    for (const Task& t : h[0].tasks) u += static_cast<double>(t.duration) / static_cast<double>(t.period);
    CHECK(u > 0.583 && u < 0.584);

    // Invariantes de docs/testes.md §4 em todos os instantes.
    CHECK(noViolations(checkInvariants(h)));

    // Determinístico: montar de novo dá exatamente o mesmo histórico.
    const std::vector<SystemSnapshot> again = demoHistoryG1();
    bool same = again.size() == h.size();
    for (std::size_t k = 0; same && k < h.size(); ++k) {
        same = serializeSnapshot(again[k]) == serializeSnapshot(h[k]);
    }
    CHECK(same);
}

// -----------------------------------------------------------------------------
// Fase 2: parser (docs/testes.md §3, P1–P14 e robustez).
// -----------------------------------------------------------------------------

// Chama o parser e conta como falha se alguma exceção escapar (design.md §8:
// o parser nunca lança para o chamador).
static ParseResult parse(const std::string& text) {
    try {
        return parser::parseConfig(text);
    } catch (...) {
        ++g_checks;
        ++g_fails;
        std::cerr << "  exceção escapou do parser\n";
        return ParseResult();
    }
}

// Alguma mensagem contém TODOS os trechos pedidos? (as mensagens precisam
// trazer linha, campo, valor e motivo, A3.3.6)
static bool hasMessage(const std::vector<std::string>& messages, const std::vector<std::string>& parts) {
    for (const std::string& message : messages) {
        bool all = true;
        for (const std::string& part : parts) {
            if (message.find(part) == std::string::npos) all = false;
        }
        if (all) return true;
    }
    return false;
}

// Arquivo do P1; os casos P2–P4 e P6 são variações dele e devem dar o mesmo resultado.
static const std::string P1_TEXT =
    "RM;2;2\n"
    "1;ff0000;0;1;4;4\n"
    "2;00FF00;0;2;6;6\n"
    "3;0000ff;1;1;8;8\n";

// "Mesmo resultado" = mesma Config (comparada pelo texto de serializeConfig,
// que tem todos os campos do arquivo) e mesmas mensagens.
static bool sameResult(const ParseResult& a, const ParseResult& b) {
    return parser::serializeConfig(a.config) == parser::serializeConfig(b.config) &&
           a.errors == b.errors && a.warnings == b.warnings;
}

// P1–P4, P6: caso feliz e variações de formatação que NÃO podem mudar nada
// (A3.3.2, A3.3.3, A3.3.6, A3.3.7).
static void testParserFormatting() {
    const ParseResult p1 = parse(P1_TEXT);
    CHECK(p1.ok());
    CHECK(p1.warnings.empty());
    CHECK_EQ(p1.config.algorithm, std::string("RM"));
    CHECK_EQ(p1.config.quantum, 2);
    CHECK_EQ(p1.config.cpuCount, 2);
    CHECK_EQ(p1.config.tasks.size(), 3u);
    CHECK_EQ(p1.config.tasks[0].color, std::string("FF0000"));  // normalizada em maiúsculas
    CHECK_EQ(p1.config.tasks[2].color, std::string("0000FF"));
    CHECK_EQ(p1.config.tasks[1].duration, 2);
    CHECK_EQ(p1.config.tasks[1].period, 6);
    CHECK_EQ(p1.config.tasks[2].arrival, 1);
    CHECK_EQ(p1.config.tasks[2].deadline, 8);
    CHECK(!p1.config.tasks[0].ignored);

    // P2: fim de linha do Windows (\r\n) e também CR isolado.
    std::string crlf, cr;
    for (char c : P1_TEXT) {
        crlf += (c == '\n') ? std::string("\r\n") : std::string(1, c);
        cr += (c == '\n') ? '\r' : c;
    }
    CHECK(sameResult(parse(crlf), p1));
    CHECK(sameResult(parse(cr), p1));

    // P3: BOM UTF-8 no início (o Bloco de Notas grava assim).
    CHECK(sameResult(parse("\xEF\xBB\xBF" + P1_TEXT), p1));
    const ParseResult bomEdf = parse("\xEF\xBB\xBF" "EDF;2;1\n1;FF0000;0;1;4;4\n");
    CHECK(bomEdf.ok());
    CHECK_EQ(bomEdf.config.algorithm, std::string("EDF"));  // o BOM não "gruda" no algoritmo

    // P4: ';' final em algumas linhas (inclusive a do sistema) e em outras não.
    CHECK(sameResult(parse("RM;2;2;\n1;ff0000;0;1;4;4;\n2;00FF00;0;2;6;6\n3;0000ff;1;1;8;8;\n"), p1));

    // P6: linhas em branco, linhas só com espaços/tabs e espaços em volta dos campos.
    CHECK(sameResult(parse("\n   \n RM ; 2 ;2\n\t\n1 ; ff0000;0; 1 ;4;4\n\n2;00FF00 ;0;2;6;6  \n 3;0000ff;1;1;8;8\n   "), p1));

    // Sinal '+' é aceito (na dúvida, aceitar: A3.3.7).
    CHECK(sameResult(parse("RM;+2;2\n1;ff0000;+0;1;4;4\n2;00FF00;0;2;+6;6\n3;0000ff;1;1;8;+8\n"), p1));
}

// P5, P7–P10: linha do sistema, mínimo de linhas e ids.
static void testParserSystemAndIds() {
    // P5: maiúsculas e minúsculas tratadas como iguais [A3.3.2].
    const ParseResult rm = parse("rM;2;1\n1;f0e0d0;0;1;4;4\n");
    CHECK(rm.ok());
    CHECK_EQ(rm.config.algorithm, std::string("RM"));
    CHECK_EQ(rm.config.tasks[0].color, std::string("F0E0D0"));
    const ParseResult edf = parse("edf;1;1\n1;FF0000;0;1;4;4\n");
    CHECK_EQ(edf.config.algorithm, std::string("EDF"));
    CHECK_EQ(edf.config.quantum, 1);

    // Linha do sistema com campos vazios: padrões com aviso [A3.2].
    const ParseResult defaults = parse(";;\n1;FF0000;0;1;4;4\n");
    CHECK(defaults.ok());
    CHECK_EQ(defaults.config.algorithm, std::string("RM"));
    CHECK_EQ(defaults.config.quantum, 2);
    CHECK_EQ(defaults.config.cpuCount, 1);
    CHECK_EQ(defaults.warnings.size(), 3u);

    // P7: só a linha do sistema (linhas em branco não contam).
    const ParseResult onlySystem = parse("RM;2;1\n\n   \n");
    CHECK(!onlySystem.ok());
    CHECK(hasMessage(onlySystem.errors, {"ao menos 2 linhas"}));

    // P8: algoritmo desconhecido: erro com a linha e a lista do registro.
    const ParseResult unknown = parse("XYZ;2;1\n1;FF0000;0;1;4;4\n");
    CHECK(!unknown.ok());
    CHECK(hasMessage(unknown.errors, {"Linha 1", "XYZ", "EDF, RM"}));

    // P9: qtde_cpus fora de 1..MAX_CPUS ou não inteiro [RG2] (D12).
    for (const std::string bad : {"0", "65", "abc", "1.5", "-1"}) {
        const ParseResult r = parse("RM;2;" + bad + "\n1;FF0000;0;1;4;4\n");
        CHECK(!r.ok());
        CHECK(hasMessage(r.errors, {"Linha 1", "qtde_cpus", "'" + bad + "'", "entre 1 e 64"}));
    }
    CHECK_EQ(parse("RM;2;1\n1;FF0000;0;1;4;4\n").config.cpuCount, 1);
    CHECK_EQ(parse("RM;2;64\n1;FF0000;0;1;4;4\n").config.cpuCount, MAX_CPUS);
    CHECK(!parse("RM;0;1\n1;FF0000;0;1;4;4\n").ok());  // quantum >= 1

    // P10: id duplicado, sem diferenciar maiúsculas, citando as duas linhas.
    const ParseResult dup = parse("RM;2;1\na1;FF0000;0;1;4;4\nX;00FF00;0;1;4;4\nA1;0000FF;0;1;4;4\n");
    CHECK(!dup.ok());
    CHECK(hasMessage(dup.errors, {"Linha 4", "'A1'", "linha 2"}));
    CHECK(!parse("RM;2;1\n;FF0000;0;1;4;4\n").ok());  // id vazio
}

// P11: cores (D17, A2.1).
static void testParserColors() {
    for (const std::string bad : {"GG0000", "FFF", "1234567"}) {
        const ParseResult r = parse("RM;2;1\n1;" + bad + ";0;1;4;4\n");
        CHECK(!r.ok());
        CHECK(hasMessage(r.errors, {"Linha 2", "'" + bad + "'", "RRGGBB"}));
    }
    const ParseResult hash = parse("RM;2;1\n1;#ff0000;0;1;4;4\n");
    CHECK(hash.ok());
    CHECK_EQ(hash.config.tasks[0].color, std::string("FF0000"));

    // Vazia: paleta automática, sem erro; repetida ou preta: aviso e outra cor livre.
    const ParseResult r = parse("RM;2;1\n1;FF0000;0;1;4;4\n2;;0;1;4;4\n3;ff0000;0;1;4;4\n4;000000;0;1;4;4\n");
    CHECK(r.ok());
    CHECK(hasMessage(r.warnings, {"Linha 3", "não tem cor"}));
    CHECK(hasMessage(r.warnings, {"Linha 4", "FF0000", "já é usada na linha 2"}));
    CHECK(hasMessage(r.warnings, {"Linha 5", "000000", "escura"}));
    std::vector<std::string> colors;
    for (const Task& t : r.config.tasks) colors.push_back(t.color);
    std::sort(colors.begin(), colors.end());
    CHECK(std::adjacent_find(colors.begin(), colors.end()) == colors.end());  // todas diferentes
    CHECK(r.config.tasks[3].color != "000000");

    // Quase branca vinda do arquivo: aviso e outra cor (pareceria "pronta", A2.1).
    const ParseResult white = parse("RM;2;1\n1;FFFFFF;0;1;4;4\n2;f0f0f0;0;1;4;4\n");
    CHECK(white.ok());
    CHECK(hasMessage(white.warnings, {"Linha 2", "FFFFFF", "clara demais"}));
    CHECK(hasMessage(white.warnings, {"Linha 3", "F0F0F0", "clara demais"}));
    CHECK(white.config.tasks[0].color != "FFFFFF");

    // A paleta automática nunca escolhe preto nem branco, mesmo com muitas tarefas.
    std::string many = "RM;2;1\n";
    for (int i = 0; i < 300; ++i) many += "T" + std::to_string(i) + ";;0;1;5\n";
    bool extremes = false;
    for (const Task& t : parse(many).config.tasks) {
        unsigned v = static_cast<unsigned>(std::stoul(t.color, nullptr, 16));
        unsigned red = (v >> 16) & 0xFF, green = (v >> 8) & 0xFF, blue = v & 0xFF;
        if ((red < 0x40 && green < 0x40 && blue < 0x40) || (red > 0xE0 && green > 0xE0 && blue > 0xE0)) extremes = true;
    }
    CHECK(!extremes);
}

// P12: número inválido em cada campo: erro com linha, campo e valor.
static void testParserNumbers() {
    struct Case { std::string line; std::string field; std::string value; };
    const std::vector<Case> cases = {
        {"1;FF0000;-1;1;4;4", "ingresso", "-1"},
        {"1;FF0000;0;0;4;4", "duracao", "0"},
        {"1;FF0000;0;1;x;4", "periodo", "x"},
        {"1;FF0000;0;1;4;-2", "prazo", "-2"},
        {"1;FF0000;0;1.5;4;4", "duracao", "1.5"},
        {"1;FF0000;0;1;4;4;", "", ""},  // controle: válido
        {"1;FF0000;0;1;1000000001;4", "periodo", "1000000001"},
        {"1;FF0000;0;1", "periodo", ""},  // obrigatório ausente
    };
    for (const Case& c : cases) {
        const ParseResult r = parse("RM;2;1\n" + c.line + "\n");
        if (c.field.empty()) {
            CHECK(r.ok());
        } else {
            CHECK(!r.ok());
            CHECK(hasMessage(r.errors, {"Linha 2", "'" + c.field + "'", c.value}));
        }
    }
    // periodo < 0 é erro no Projeto A (resposta do professor Q10), com mensagem própria.
    const ParseResult negative = parse("RM;2;1\n1;FF0000;0;1;-5;4\n");
    CHECK(!negative.ok());
    CHECK(hasMessage(negative.errors, {"Linha 2", "'periodo'", "-5", "Projeto A"}));
    // Vários erros na mesma linha aparecem todos de uma vez.
    CHECK_EQ(parse("RM;2;1\n1;FF0000;-1;0;x;-2\n").errors.size(), 4u);
}

// P13: aperiódica ignorada com aviso (D7); BS01 com período > 0 é periódica comum (Q12).
static void testParserScope() {
    const ParseResult r = parse("RM;2;1\n1;FF0000;0;1;4;4\n2;00FF00;0;1;0;4\nBS01;0000FF;0;1;5;5\n");
    CHECK(r.ok());
    CHECK_EQ(r.config.tasks.size(), 3u);
    const Task& aperiodic = r.config.tasks[1];
    CHECK(aperiodic.ignored);
    CHECK(aperiodic.kind == TaskKind::Aperiodic);
    CHECK(!aperiodic.ignoredReason.empty());
    CHECK(hasMessage(r.warnings, {"Linha 3", "'2'", "aperiódica"}));
    const Task& server = r.config.tasks[2];
    CHECK(!server.ignored);
    CHECK(server.kind == TaskKind::Periodic);
    CHECK(server.server == ServerType::None);
    CHECK(!hasMessage(r.warnings, {"BS01"}));

    // Prazo: vazio ou 0 => período, com aviso (A3.2, D8); maior que o período => aviso.
    const ParseResult d = parse("RM;2;1\n1;FF0000;0;1;4;\n2;00FF00;0;1;5;0\n3;0000FF;0;5;4;8\n");
    CHECK(d.ok());
    CHECK_EQ(d.config.tasks[0].deadline, 4);
    CHECK_EQ(d.config.tasks[1].deadline, 5);
    CHECK(hasMessage(d.warnings, {"Linha 2", "prazo", "Usando o período (4)"}));
    CHECK(hasMessage(d.warnings, {"Linha 4", "prazo (8)", "maior que o período"}));
    CHECK(hasMessage(d.warnings, {"Linha 4", "duração (5)", "maior que o período"}));
}

// P14: ida e volta parseConfig -> serializeConfig -> parseConfig.
static void testParserRoundTrip() {
    const ParseResult first = parse("edf;3;2\n1;ff0000;0;1;4;\n2;;1;2;6;5;a;b;c\n3;00FF00;0;1;0;\n");
    CHECK(first.ok());
    const std::string text = parser::serializeConfig(first.config);
    const ParseResult second = parse(text);
    CHECK(second.ok());
    CHECK_EQ(parser::serializeConfig(second.config), text);
    CHECK_EQ(second.config.algorithm, std::string("EDF"));
    CHECK_EQ(second.config.quantum, 3);
    CHECK_EQ(second.config.cpuCount, 2);
    CHECK_EQ(second.config.tasks[0].deadline, 4);                    // prazo vazio => período
    CHECK_EQ(second.config.tasks[1].rawEvents, std::string("a;b;c"));  // ';' internos preservados
    CHECK_EQ(second.config.tasks[1].color, first.config.tasks[1].color);  // cor automática fixada
    CHECK(second.config.tasks[2].ignored);                           // aperiódica continua ignorada
    CHECK(hasMessage(first.warnings, {"Linha 3", "lista de eventos"}));
}

// Robustez (testes.md §3): nada disso pode lançar exceção nem travar.
static void testParserRobustness() {
    CHECK(hasMessage(parse("").errors, {"vazio"}));
    CHECK(hasMessage(parse("   \n\t\n  \r\n").errors, {"vazio"}));

    // Nenhuma tarefa simulável (todas aperiódicas): erro claro.
    const ParseResult allIgnored = parse("RM;2;1\n1;FF0000;0;1;0;4\n2;00FF00;0;1;0;4\n");
    CHECK(!allIgnored.ok());
    CHECK(hasMessage(allIgnored.errors, {"Nenhuma tarefa periódica"}));

    // Linha de 1 MB (vai inteira para a lista de eventos).
    const ParseResult hugeLine = parse("RM;2;1\n1;FF0000;0;1;4;4;" + std::string(1 << 20, 'x') + "\n");
    CHECK(hugeLine.ok());
    CHECK_EQ(hugeLine.config.tasks[0].rawEvents.size(), static_cast<std::size_t>(1 << 20));

    // Bytes binários (todos os 256 valores, repetidos): erro, sem exceção.
    std::string binary;
    for (int i = 0; i < 4096; ++i) binary += static_cast<char>(i * 37);
    CHECK(!parse(binary).ok());

    // 100 000 tarefas sem cor carregam em tempo razoável (< 5 s, folgado).
    std::string big = "EDF;2;4\n";
    for (int i = 0; i < 100000; ++i) big += std::to_string(i) + ";;0;1;" + std::to_string(10 + i % 50) + "\n";
    const auto start = std::chrono::steady_clock::now();
    const ParseResult many = parse(big);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start).count();
    CHECK(many.ok());
    CHECK_EQ(many.config.tasks.size(), 100000u);
    CHECK(ms < 5000);
    std::cout << "  (parser: 100 000 tarefas em " << ms << " ms)\n";
}

// -----------------------------------------------------------------------------
// Fase 3: motor de ticks (docs/testes.md §2 G1–G4, §4, §5, §6).
// -----------------------------------------------------------------------------

// Executa a configuração do instante 0 até o fim com makeNext em laço (a
// classe Simulator com runToEnd é da Fase 4). 'maxTicks' é só uma trava de
// segurança para um teste com defeito não rodar para sempre.
static std::vector<SystemSnapshot> runConfig(const std::string& text, uint64_t seed = 1,
                                             Tick maxTicks = 100000) {
    std::vector<SystemSnapshot> history;
    ParseResult parsed = parser::parseConfig(text);
    if (!parsed.ok()) {
        ++g_checks;
        ++g_fails;
        std::cerr << "  configuração de teste inválida:\n" << text;
        for (const std::string& e : parsed.errors) std::cerr << "  " << e << "\n";
        return history;
    }
    parsed.config.seed = seed;
    history.push_back(initState(parsed.config));
    while (!finished(history.back()) && history.back().tick < maxTicks) {
        history.push_back(makeNext(history.back()));
    }
    return history;
}

// Imprime uma linha por instante: quem ocupa cada CPU em [k, k+1) e as
// ocorrências de x = k (docs/testes.md §1). Usada para depurar e para o
// relatório da fase: 'sim_tests --grid'.
static void printGrid(const std::vector<SystemSnapshot>& history, Tick maxTicks) {
    for (const SystemSnapshot& s : history) {
        if (s.tick >= maxTicks) break;
        std::string line = std::to_string(s.tick);
        line.insert(0, 4 - std::min<std::size_t>(4, line.size()), ' ');
        line += " |";
        for (int c = 0; c < static_cast<int>(s.cpus.size()); ++c) {
            std::string id = taskOn(history, s.tick, c);
            line += " " + std::string(id.size() < 3 ? 3 - id.size() : 0, ' ') + id;
        }
        line += " |";
        for (const Occurrence& o : s.occurrences) {
            line += " " + std::string(occurrenceName(o.type));
            if (o.taskIndex >= 0) line += "(" + s.tasks[o.taskIndex].id + ")";
            else if (o.cpuIndex >= 0) line += "(CPU" + std::to_string(o.cpuIndex + 1) + ")";
            if (!o.note.empty()) line += "[" + o.note + "]";
        }
        std::cout << line << "\n";
    }
}

// Nota da ocorrência 'type' da tarefa 'taskIndex' no instante 'tick' ("?" se não houver).
static std::string occNote(const std::vector<SystemSnapshot>& history, Tick tick,
                           OccurrenceType type, int taskIndex) {
    for (const Occurrence& o : history[static_cast<std::size_t>(tick)].occurrences) {
        if (o.type == type && o.taskIndex == taskIndex) return o.note;
    }
    return "?";
}

// Grade da CPU 'cpu' nos ticks [0, n) como "1 1 2 - ..." (compara com testes.md).
static std::string gridOf(const std::vector<SystemSnapshot>& history, int cpu, Tick n) {
    std::string out;
    for (Tick k = 0; k < n && k < static_cast<Tick>(history.size()); ++k) {
        if (!out.empty()) out += ' ';
        out += taskOn(history, k, cpu);
    }
    return out;
}

// Ocorrências do tipo 'type' (da tarefa 'taskIndex', -1 = qualquer) em [from, to).
static int countOccIn(const std::vector<SystemSnapshot>& history, OccurrenceType type,
                      int taskIndex, Tick from, Tick to) {
    int count = 0;
    for (Tick k : occTicks(history, type, taskIndex)) {
        if (k >= from && k < to) ++count;
    }
    return count;
}

// Invariante 6 (testes.md §4): ao final, cada tarefa simulada liberou e
// concluiu exatamente 10 ocorrências, com 10 Arrival, 10 Termination,
// 1 TaskFinished e runTicks = 10 · duração [A4.4] (D6).
static std::vector<std::string> checkFinalState(const std::vector<SystemSnapshot>& history) {
    std::vector<std::string> errors;
    if (history.empty()) return {"histórico vazio"};
    const SystemSnapshot& last = history.back();
    if (!finished(last)) errors.push_back("a simulação não terminou (instante " + std::to_string(last.tick) + ")");
    for (int i = 0; i < static_cast<int>(last.tasks.size()); ++i) {
        const Task& t = last.tasks[i];
        if (t.ignored) continue;
        const int L = PERIODIC_ACTIVATION_LIMIT;
        if (t.released != L || t.completed != L ||
            countOcc(history, OccurrenceType::Arrival, i) != L ||
            countOcc(history, OccurrenceType::Termination, i) != L ||
            countOcc(history, OccurrenceType::TaskFinished, i) != 1 ||
            t.runTicks != L * t.duration || t.state != TaskState::Terminated || !t.jobs.empty()) {
            errors.push_back("invariante 6: estado final errado da tarefa " + t.id);
        }
    }
    return errors;
}

// Mesmo histórico, snapshot a snapshot (serialização completa)? Invariante 9.
static bool sameHistory(const std::vector<SystemSnapshot>& a, const std::vector<SystemSnapshot>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t k = 0; k < a.size(); ++k) {
        if (serializeSnapshot(a[k]) != serializeSnapshot(b[k])) return false;
    }
    return true;
}

static const std::string G1_TEXT = "RM;2;1\n1;FF0000;0;1;4;4\n2;0000FF;0;2;6;6\n";
static const std::string G2_TASKS = "1;FF0000;0;2;5;5\n2;0000FF;0;4;7;7\n";
static const std::string G3_TEXT = "EDF;2;2\n1;FF0000;0;2;4;4\n2;00FF00;0;2;4;4\n3;0000FF;0;2;4;4\n";
static const std::string G4_TEXT = "EDF;2;1\n1;FF0000;0;3;10;10\n2;0000FF;1;3;10;9\n";

// G1 — RM, 1 CPU, folgado; também valida CPU desligada.
static void testGoldenG1() {
    const std::vector<SystemSnapshot> h = runConfig(G1_TEXT);
    const int T1 = 0, T2 = 1;
    CHECK_EQ(gridOf(h, 0, 12), std::string("1 2 2 - 1 - 2 2 1 - - -"));
    CHECK_EQ(countOcc(h, OccurrenceType::DeadlineMiss, -1), 0);
    CHECK_EQ(countOcc(h, OccurrenceType::Preemption, -1), 0);
    CHECK(occTicks(h, OccurrenceType::Termination, T1).size() >= 3);
    CHECK(occTicks(h, OccurrenceType::Termination, T2).size() >= 2);
    CHECK(occTicks(h, OccurrenceType::Termination, T1)[2] == 9);
    CHECK(occTicks(h, OccurrenceType::Termination, T2)[1] == 8);
    CHECK_EQ(h[12].cpus[0].ticksOff, 5);  // CPU1 desligada em 3, 5, 9, 10 e 11
    CHECK(noViolations(checkInvariants(h)));
    CHECK(noViolations(checkFinalState(h)));

    // O motor reproduz EXATAMENTE o histórico montado à mão na Fase 1
    // (todos os campos, todas as ocorrências, na mesma ordem).
    const std::vector<SystemSnapshot> demo = demoHistoryG1();
    bool same = h.size() >= demo.size();
    for (std::size_t k = 0; same && k < demo.size(); ++k) {
        same = serializeSnapshot(h[k]) == serializeSnapshot(demo[k]);
        if (!same) std::cerr << "  G1 diverge do demoHistoryG1 no instante " << k << "\n";
    }
    CHECK(same);
}

// G2 — RM x EDF, 1 CPU, utilização alta: o RM perde prazo, o EDF não.
static void testGoldenG2() {
    const int T1 = 0, T2 = 1;
    const std::vector<SystemSnapshot> rm = runConfig("RM;2;1\n" + G2_TASKS);
    CHECK_EQ(gridOf(rm, 0, 14), std::string("1 1 2 2 2 1 1 2 2 2 1 1 2 2"));
    CHECK(occTicks(rm, OccurrenceType::Preemption, T2).front() == 5);   // T1 preempta T2 em 5
    CHECK(occTicks(rm, OccurrenceType::DeadlineMiss, T2).front() == 7); // prazo absoluto 7, falta 1
    CHECK(occTicks(rm, OccurrenceType::Termination, T2).front() == 8);  // 1ª ocorrência de T2 termina em 8
    CHECK_EQ(occNote(rm, 7, OccurrenceType::Arrival, T2), std::string("enfileirada"));
    CHECK_EQ(rm[7].tasks[T2].jobs.size(), 2u);                         // 2ª ocorrência na fila (D5)
    CHECK_EQ(countOccIn(rm, OccurrenceType::DeadlineMiss, T1, 0, 14), 0);
    CHECK(occTicks(rm, OccurrenceType::Termination, T2)[1] == 14);      // termina no prazo: não perde
    CHECK_EQ(countOccIn(rm, OccurrenceType::DeadlineMiss, T2, 14, 15), 0);
    CHECK(noViolations(checkInvariants(rm)));
    CHECK(noViolations(checkFinalState(rm)));

    const std::vector<SystemSnapshot> edf = runConfig("EDF;2;1\n" + G2_TASKS);
    CHECK_EQ(gridOf(edf, 0, 14), std::string("1 1 2 2 2 2 1 1 2 2 2 2 1 1"));
    CHECK_EQ(countOccIn(edf, OccurrenceType::DeadlineMiss, -1, 0, 14), 0);
    CHECK_EQ(countOccIn(edf, OccurrenceType::Preemption, -1, 0, 14), 0);
    CHECK(occTicks(edf, OccurrenceType::Termination, T2)[0] == 6);
    CHECK(occTicks(edf, OccurrenceType::Termination, T1)[1] == 8);
    CHECK(occTicks(edf, OccurrenceType::Termination, T2)[1] == 12);
    CHECK(noViolations(checkInvariants(edf)));
    CHECK(noViolations(checkFinalState(edf)));
}

// G3 — EDF, 2 CPUs, empate total: sorteio determinístico (A4.3, D14).
static void testGoldenG3() {
    for (uint64_t seed : {1ULL, 2ULL}) {
        const std::vector<SystemSnapshot> h = runConfig(G3_TEXT, seed);
        int running0 = 0;
        for (const Task& t : h[0].tasks) running0 += t.state == TaskState::Running ? 1 : 0;
        CHECK_EQ(running0, 2);                                            // uma em cada CPU
        CHECK_EQ(countOccIn(h, OccurrenceType::TieBreakDraw, -1, 0, 1), 2); // uma por escolhida
        // Ticks 2 e 3: só a que não foi sorteada executa, na CPU1; CPU2 desligada.
        int third = -1;
        for (int i = 0; i < 3; ++i) {
            if (h[0].tasks[i].state == TaskState::Ready) third = i;
        }
        CHECK(third >= 0);
        CHECK_EQ(h[2].cpus[0].taskIndex, third);
        CHECK_EQ(h[3].cpus[0].taskIndex, third);
        CHECK_EQ(taskOn(h, 2, 1), std::string("-"));
        CHECK(std::find(occTicks(h, OccurrenceType::CpuOff, -1).begin(),
                        occTicks(h, OccurrenceType::CpuOff, -1).end(), 2) != occTicks(h, OccurrenceType::CpuOff, -1).end());
        CHECK_EQ(countOccIn(h, OccurrenceType::TieBreakDraw, -1, 2, 3), 0); // um só candidato
        CHECK_EQ(countOccIn(h, OccurrenceType::TieBreakDraw, -1, 4, 5), 2); // em 4, novo sorteio
        CHECK_EQ(h[8].cpus[1].ticksOff, 4);                                 // CPU2 em [0,8)
        CHECK(noViolations(checkInvariants(h)));
        CHECK(noViolations(checkFinalState(h)));
        CHECK(sameHistory(h, runConfig(G3_TEXT, seed)));                    // determinismo (inv. 9)
    }
}

// G4 — desempate (1): quem está executando continua (A4.3).
static void testGoldenG4() {
    const std::vector<SystemSnapshot> h = runConfig(G4_TEXT);
    CHECK_EQ(gridOf(h, 0, 10), std::string("1 1 1 2 2 2 - - - -"));
    CHECK_EQ(countOccIn(h, OccurrenceType::Preemption, -1, 0, 10), 0);
    CHECK_EQ(countOcc(h, OccurrenceType::TieBreakDraw, -1), 0);
    CHECK_EQ(h.back().tasks[1].firstStart, 3);
    CHECK(noViolations(checkInvariants(h)));
    CHECK(noViolations(checkFinalState(h)));

    // G4b — isola o critério (1). No G4 acima, T1 também venceria pelo
    // critério (3) (chegou antes). Aqui (RM, 1 CPU) as duas têm período 10
    // (chave empatada); em 2, T2 chega com prazo absoluto 7 < 10 de T1, então
    // o critério (2) escolheria T2, mas o (1) mantém T1, que já executa.
    const std::vector<SystemSnapshot> b = runConfig("RM;2;1\n1;FF0000;0;4;10;10\n2;00FF00;2;2;10;5\n");
    CHECK_EQ(gridOf(b, 0, 7), std::string("1 1 1 1 2 2 -"));
    CHECK_EQ(countOccIn(b, OccurrenceType::Preemption, -1, 0, 10), 0);
    CHECK_EQ(countOccIn(b, OccurrenceType::DeadlineMiss, -1, 0, 10), 0);  // T2 termina em 6 < 7
    CHECK(noViolations(checkInvariants(b)));
}

// Extensibilidade (A4.2, testes.md §6): um algoritmo novo registrado de
// FORA do núcleo, sem nenhuma alteração no Simulator nem no parser.
static Tick fifoKey(const Task& t) { return t.jobs.front().release; }  // ocorrência mais antiga primeiro
static PriorityGroups scheduleFIFO(const SystemSnapshot& s) { return orderAndGroup(s, fifoKey); }

static void testFifoExtensibility() {
    registerScheduler("FIFO", scheduleFIFO);
    const std::string text = "fifo;2;1\n1;FF0000;0;2;5;5\n2;0000FF;1;1;3;3\n";
    const ParseResult parsed = parser::parseConfig(text);
    CHECK(parsed.ok());                                       // (a) o parser aceita o nome novo
    CHECK_EQ(parsed.config.algorithm, std::string("FIFO"));
    const std::vector<SystemSnapshot> h = runConfig(text);    // (b) o motor executa sem mudança
    // Em 1, T2 chega com período menor (RM a escolheria), mas FIFO mantém T1.
    CHECK_EQ(gridOf(h, 0, 3), std::string("1 1 2"));
    CHECK(noViolations(checkInvariants(h)));
    CHECK(noViolations(checkFinalState(h)));
}

// Executa todos os testes e imprime o placar "N checks, M falhas".
int main(int argc, char* argv[]) {
    // 'sim_tests --grid': imprime as grades de G1, G2 (RM e EDF) e G3 e sai.
    if (argc > 1 && std::string(argv[1]) == "--grid") {
        std::cout << "G1 (RM, 1 CPU)\n";
        printGrid(runConfig(G1_TEXT), 14);
        std::cout << "\nG2 (RM, 1 CPU)\n";
        printGrid(runConfig("RM;2;1\n" + G2_TASKS), 15);
        std::cout << "\nG2 (EDF, 1 CPU)\n";
        printGrid(runConfig("EDF;2;1\n" + G2_TASKS), 15);
        std::cout << "\nG3 (EDF, 2 CPUs, semente 1)\n";
        printGrid(runConfig(G3_TEXT), 9);
        return 0;
    }
    testBuildSmoke();
    testSerializeSnapshot();
    testModelDefaults();
    testNaturalIdLess();
    testInvariantChecker();
    testDemoHistoryG1();
    testParserFormatting();
    testParserSystemAndIds();
    testParserColors();
    testParserNumbers();
    testParserScope();
    testParserRoundTrip();
    testParserRobustness();
    testGoldenG1();
    testGoldenG2();
    testGoldenG3();
    testGoldenG4();
    testFifoExtensibility();

    std::cout << g_checks << " checks, " << g_fails << " falhas\n";
    return g_fails == 0 ? 0 : 1;
}
