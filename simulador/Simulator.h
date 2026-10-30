#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "Model.h"
#include "Scheduler.h"

// ---------------------------------------------------------------------------
// Regras da simulação: funções livres sobre o snapshot (D21, design.md §6).
// Não guardam estado nem fazem I/O: recebem um snapshot e o transformam, o
// que deixa cada regra testável isolada e o histórico determinístico.
// ---------------------------------------------------------------------------

// Instante 0 já processado (ativações do instante 0 + primeiro escalonamento).
SystemSnapshot initState(const Config& config);

// Instante seguinte: consome o tick [t, t+1) e processa o instante t+1 (D3).
SystemSnapshot makeNext(const SystemSnapshot& prev);

// Fim da simulação: toda tarefa simulada (não ignorada) está Terminated (D6).
bool finished(const SystemSnapshot& s);

// Etapas, na ordem em que makeNext as chama (ordem fixa: design.md §6, Q-A10).
void runOneTick        (SystemSnapshot& s);  // 1. executa [t, t+1) com as CPUs decididas em t
void finishActivations (SystemSnapshot& s);  // 2. conclusões no instante t+1
void releaseActivations(SystemSnapshot& s);  // 3. ativações
void checkDeadlines    (SystemSnapshot& s);  // 4. perdas de prazo
// 5. quem executa: pega os grupos do escalonador até lotar as CPUs; sorteia
//    (critério 5 do A4.3) só o grupo empatado que não cabe. 'drawn' recebe
//    as escolhidas por sorteio (ícone no Gantt).
std::vector<int> chooseTasks(const SystemSnapshot& s, const PriorityGroups& groups,
                             std::vector<int>& drawn);
// 6. despacho: preempção, afinidade de CPU, CPU ligada/desligada, fila de prontos.
void dispatch(SystemSnapshot& s, const std::vector<int>& chosen, const std::vector<int>& drawn);

// Limite de instantes do "executar até o fim" (A1.5.3, design.md §9): impede que
// uma configuração enorme trave a interface ou esgote a memória do histórico.
constexpr Tick DEFAULT_MAX_TICKS = 2000000;

// Resultado de runToEnd: até onde a simulação foi e por quê parou.
struct RunSummary {
    bool finished = false;  // true se todas as tarefas simuladas terminaram (D6)
    Tick ticks = 0;         // instante alcançado (current().tick ao parar)
    std::string message;    // motivo em português (fim, limite de ticks ou de memória) [A5]
};

// TEMPORÁRIO (Fase 1): REMOVER NA FASE 7 — a entrega não pode ter código morto.
// Histórico do cenário G1 de docs/testes.md (instantes 0 a 12) montado à mão,
// SEM o motor: a Fase 5 desenha o Gantt com ele antes de o motor existir, e
// tests.cpp confere contra a grade de G1. Definido no fim de Simulator.cpp.
std::vector<SystemSnapshot> demoHistoryG1();

// ---------------------------------------------------------------------------
// Simulator: a única classe do núcleo. Só guarda o histórico e o cursor;
// todo o estado da simulação está nos snapshots (D16, D21).
// ---------------------------------------------------------------------------
class Simulator {
public:
    Simulator() : history_(1) {}            // snapshot vazio: current() é sempre válido

    // Zera o histórico e gera history[0]. false se o algoritmo não está registrado.
    bool load(const Config& config);

    bool stepForward();                     // avança 1 instante (reaproveita se já existe); false no fim
    bool stepBack();                        // volta 1 instante (só move o cursor); false no instante 0
    bool isFinished() const { return finished(current()); }

    // --- Declaradas na Fase 1, definidas na Fase 4 (design.md §9) ---------
    // Volta ao instante 0, descartando todo o histórico calculado depois dele.
    void reset();
    // Leva o cursor ao instante 'tick' (para trás só move o cursor; para a frente
    // calcula o que faltar). false se 'tick' < 0 ou se a simulação termina antes.
    // O relógio exibido acompanha o cursor [A1.5.2].
    bool jumpTo(Tick tick);
    // Modo "execução completa" [A1.5.3]: avança até o fim ou até 'maxTicks'
    // (ou o limite de memória estimada), sem nunca travar.
    RunSummary runToEnd(Tick maxTicks = DEFAULT_MAX_TICKS);
    // Edição manual no instante do cursor [A1.5.2, A3.4]: valida numa cópia; se
    // aceita, aplica antes do escalonador (D28), registra ManualEdit, descarta o
    // futuro e recalcula o instante (D15). Se recusada, nada muda e a mensagem
    // explica o motivo.
    EditResult applyEdit(const Edit& edit);

    const SystemSnapshot& current() const { return history_[cursor_]; }
    const std::vector<SystemSnapshot>& history() const { return history_; }
    std::size_t cursor() const { return cursor_; }

private:
    std::vector<SystemSnapshot> history_;   // history_[k] = estado no instante k
    std::size_t cursor_ = 0;                // instante mostrado ao usuário (relógio global, A1.5.2)
};
