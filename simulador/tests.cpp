// =============================================================================
// tests.cpp — Testes automáticos do núcleo da simulação (executável sim_tests).
//
// O quê: verifica o núcleo (simcore) sem nenhuma janela e sem Qt. Na Fase 0 há
// só um teste trivial que prova que o build funciona e que as structs do núcleo
// são copiáveis; as fases seguintes acrescentam os casos de docs/testes.md
// (parser P1–P14, cenários dourados G1–G6, invariantes, fuzz).
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

#include <iostream>
#include <vector>

#include "CPU.h"
#include "Parser.h"
#include "Scheduler.h"
#include "Simulator.h"
#include "SystemSnapshot.h"
#include "Task.h"

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
// Teste trivial da Fase 0: o núcleo compila, liga e suas structs funcionam.
// Prova também uma propriedade que o histórico vai exigir (D16, D21): um
// SystemSnapshot é copiado por valor e a cópia é independente do original.
// -----------------------------------------------------------------------------
static void testBuildSmoke() {
    // CPU começa sem tarefa (taskIndex = -1), como definido em CPU.h (A1.2).
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

// Executa todos os testes e imprime o placar "N checks, M falhas".
int main() {
    testBuildSmoke();

    std::cout << g_checks << " checks, " << g_fails << " falhas\n";
    return g_fails == 0 ? 0 : 1;
}
