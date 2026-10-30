// =============================================================================
// Simulator.h — Motor da simulação (MECANISMO) e o histórico navegável.
//
// O quê: as regras de cada instante (ativar, concluir, perder prazo, escolher
// quem executa, despachar, ligar/desligar CPUs) como funções livres sobre o
// SystemSnapshot, e a classe Simulator, que só guarda o histórico de
// snapshots e o cursor (passo a passo, retroceder/avançar, edição).
//
// Por quê: funções livres sem estado escondido deixam cada regra testável
// isolada e o histórico determinístico (mesma entrada => mesmo resultado,
// inclusive o sorteio, D14). O snapshot é copiado por valor a cada instante,
// então retroceder é só mover o cursor (D16, D21).
//
// Requisitos: A1.1, A1.2, A1.4, A1.5, A3.3 (perda de prazo), A4.3, A4.4.
// Decisões: D1, D3, D5, D6, D9, D10, D11, D14, D15, D16, D21, D28.
// =============================================================================
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

// Estado inicial ANTES de processar o instante 0: tarefas do arquivo com
// nextRelease = ingresso, CPUs criadas. É a base para editar o instante 0.
SystemSnapshot baseState(const Config& config);

// Instante 0 já processado: baseState + beginInstant + scheduleAndDispatch.
SystemSnapshot initState(const Config& config);

// Instante seguinte: consome o tick [t, t+1) e processa o instante t+1 (D3).
SystemSnapshot makeNext(const SystemSnapshot& prev);

// Fim da simulação: toda tarefa simulada (não ignorada) está Terminated (D6).
bool finished(const SystemSnapshot& s);

// As duas metades de um instante (design.md §6). A edição manual é aplicada
// entre elas (D28).
void beginInstant       (SystemSnapshot& s);  // conclusões -> ativações -> perdas de prazo
void scheduleAndDispatch(SystemSnapshot& s);  // escalonador -> chooseTasks -> dispatch

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

// Regra da edição manual (D21: função livre): valida 'edit' contra o estado
// 's' (já depois de beginInstant) e, se for aceita, aplica e registra a
// ocorrência ManualEdit. Se for recusada, devolve ok = false com o motivo em
// português [A3.4] e pode ter mexido em 's' (o chamador usa uma cópia).
// Regras de cada tipo: design.md §9.
EditResult applyEditTo(SystemSnapshot& s, const Edit& edit);

// Limite de instantes do "executar até o fim" (A1.5.3, design.md §9): impede que
// uma configuração enorme trave a interface ou esgote a memória do histórico.
constexpr Tick DEFAULT_MAX_TICKS = 2000000;

// Limite de memória estimada do histórico no "executar até o fim" (design.md
// §9): ~1 GiB. Acima disso a máquina começaria a usar disco (ou faltaria
// memória) e a interface pareceria travada; melhor parar e explicar.
constexpr std::size_t DEFAULT_MAX_HISTORY_BYTES = std::size_t(1) << 30;

// Estimativa (em bytes) do que um snapshot ocupa na memória: tamanho das
// structs + vetores e textos. Usada para o limite acima; não precisa ser exata.
std::size_t estimateSnapshotBytes(const SystemSnapshot& s);

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
    Simulator() : history_(1), edits_(1) {} // snapshot vazio: current() é sempre válido

    // Zera o histórico e gera history[0]. false se o algoritmo não está registrado.
    // Guarda a Config: é a base para editar o instante 0 (não existe history[-1]).
    bool load(const Config& config);

    bool stepForward();                     // avança 1 instante (reaproveita se já existe); false no fim
    bool stepBack();                        // volta 1 instante (só move o cursor); false no instante 0
    bool isFinished() const { return finished(current()); }

    // --- Navegação e edição (design.md §9) ---------------------------------
    // Volta ao instante 0, descartando todo o histórico calculado depois dele
    // (as edições feitas no próprio instante 0 continuam valendo).
    void reset();
    // Leva o cursor ao instante 'tick' (para trás só move o cursor; para a frente
    // calcula o que faltar). false se 'tick' < 0 ou se a simulação termina antes
    // (nesse caso o cursor fica no último instante). O relógio exibido acompanha
    // o cursor [A1.5.2].
    bool jumpTo(Tick tick);
    // Modo "execução completa" [A1.5.3]: avança até o fim, ou para (com a
    // mensagem do motivo) ao chegar ao instante 'maxTicks' ou quando o
    // histórico estimado passar de 'maxBytes'. Nunca trava nem esgota a memória.
    RunSummary runToEnd(Tick maxTicks = DEFAULT_MAX_TICKS,
                        std::size_t maxBytes = DEFAULT_MAX_HISTORY_BYTES);
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
    Config config_;                         // configuração carregada (base do instante 0)
    // edits_[k] = edições aplicadas no instante k, em ordem (paralelo a history_).
    // Não é estado da simulação: é o registro de COMO history_[k] foi produzido,
    // para uma nova edição em k reaplicar as anteriores (design.md §9).
    std::vector<std::vector<Edit>> edits_;
};
