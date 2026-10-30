// =============================================================================
// tests.cpp — Testes automáticos do núcleo da simulação (executável sim_tests).
//
// O quê: verifica o núcleo (simcore) sem nenhuma janela e sem Qt.
//   - Fase 0: teste de fumaça (build, structs copiáveis).
//   - Fase 1: ferramentas de teste do contrato (serializeSnapshot, verificador
//     de invariantes de histórico, helpers de consulta), naturalIdLess e o
//     histórico de demonstração de G1 (demoHistoryG1).
//   - Próximas fases: parser P1–P14, cenários dourados G1–G6, fuzz
//     (docs/testes.md).
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

// Executa todos os testes e imprime o placar "N checks, M falhas".
int main() {
    testBuildSmoke();
    testSerializeSnapshot();
    testModelDefaults();
    testNaturalIdLess();
    testInvariantChecker();

    std::cout << g_checks << " checks, " << g_fails << " falhas\n";
    return g_fails == 0 ? 0 : 1;
}
