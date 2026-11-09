// =============================================================================
// Simulator.h — Motor da simulação e histórico navegável.
//
// As regras de cada instante (concluir, ativar, perder prazo, escolher quem
// executa, despachar, ligar e desligar CPUs) são funções livres sobre o
// SystemSnapshot, sem estado escondido: cada regra é testável isolada e a
// mesma entrada produz o mesmo histórico, inclusive o sorteio (D14). A classe
// Simulator guarda o histórico de snapshots e o cursor; como cada instante é
// uma cópia completa, retroceder é mover o cursor (D16, D21).
//
// Requisitos: A1.1, A1.2, A1.4, A1.5, A3.3 (perda de prazo), A4.3, A4.4.
// Decisões: D1, D3, D5, D6, D9, D10, D11, D14, D15, D16, D21, D28.
// =============================================================================
#pragma once
#include <cstddef>
#include <limits>
#include <string>
#include <vector>
#include "Model.h"
#include "Scheduler.h"

// ---------------------------------------------------------------------------
// Regras da simulação (D21, design.md §6)
// ---------------------------------------------------------------------------

// Estado antes de processar o instante 0: tarefas do arquivo com a primeira
// ativação no ingresso e CPUs criadas. Base para editar o instante 0.
SystemSnapshot baseState(const Config& config);

// Instante 0 já processado: baseState + beginInstant + scheduleAndDispatch.
SystemSnapshot initState(const Config& config);

// Instante seguinte: consome o tick [t, t+1) e processa o instante t+1 (D3).
SystemSnapshot makeNext(const SystemSnapshot& prev);

// Fim da simulação: toda tarefa simulada (não ignorada) está Terminated (D6).
bool finished(const SystemSnapshot& s);

// Nada mais muda sem edição do usuário: toda tarefa simulada está terminada
// ou suspensa, com as 10 ativações feitas e sem prazo futuro a registrar. Uma
// tarefa suspensa não termina sozinha, então runToEnd para aqui com aviso.
bool stalled(const SystemSnapshot& s);

// As duas metades de um instante (design.md §6). A edição manual é aplicada
// entre elas (D28).
void beginInstant       (SystemSnapshot& s);  // conclusões -> ativações -> perdas de prazo
void scheduleAndDispatch(SystemSnapshot& s);  // escalonador -> chooseTasks -> dispatch

// Etapas, na ordem em que makeNext as chama.
// PERGUNTA EM ABERTO Q-A10: qual a ordem dos eventos dentro de um tick?
// Padrão adotado: a ordem fixa abaixo (design.md §6).
void runOneTick        (SystemSnapshot& s);  // 1. executa [t, t+1) com as CPUs decididas em t
void finishActivations (SystemSnapshot& s);  // 2. conclusões no instante t+1
void releaseActivations(SystemSnapshot& s);  // 3. ativações
void checkDeadlines    (SystemSnapshot& s);  // 4. perdas de prazo
// 5. quem executa: os grupos do escalonador em ordem até ocupar as CPUs; só o
//    grupo empatado que não cabe é sorteado (critério 5 do A4.3). 'drawn'
//    recebe as escolhidas por sorteio (ícone no Gantt).
std::vector<int> chooseTasks(const SystemSnapshot& s, const PriorityGroups& groups,
                             std::vector<int>& drawn);
// 6. despacho: preempção, afinidade de CPU, CPU ligada/desligada, fila de prontos.
void dispatch(SystemSnapshot& s, const std::vector<int>& chosen, const std::vector<int>& drawn);

// Valida 'edit' contra 's' (já depois de beginInstant) e, se aceita, aplica e
// registra a ocorrência ManualEdit. Se recusada, devolve ok = false com o
// motivo [A3.4]; 's' pode ter sido alterado, por isso o chamador passa uma
// cópia. Regras de cada tipo: design.md §9.
EditResult applyEditTo(SystemSnapshot& s, const Edit& edit);

// Sem limite de instantes: a simulação não pode ser limitada a um tempo máximo
// [A2.4]. O único limite é a memória do histórico, abaixo.
constexpr Tick NO_TICK_LIMIT = std::numeric_limits<Tick>::max();

// Teto padrão da memória estimada do histórico (1 GiB). O histórico inteiro fica
// em memória [A1.5.2]; passar da memória livre faria o programa fechar por falta
// de memória. A interface troca este valor pela metade da memória livre do
// computador (MainWindow::historyMemoryBudget).
constexpr std::size_t DEFAULT_MAX_HISTORY_BYTES = std::size_t(1) << 30;

// Memória aproximada de um snapshot (structs, vetores e textos), usada para o
// teto acima.
std::size_t estimateSnapshotBytes(const SystemSnapshot& s);

// Resultado de runToEnd e jumpTo: até onde a simulação foi e por que parou.
struct RunSummary {
    bool finished = false;  // true se todas as tarefas simuladas terminaram (D6)
    Tick ticks = 0;         // instante alcançado (current().tick ao parar)
    std::string message;    // motivo da parada [A5]
};

// ---------------------------------------------------------------------------
// Simulator: guarda o histórico e o cursor; o estado da simulação está nos
// snapshots (D16, D21).
// ---------------------------------------------------------------------------
class Simulator {
public:
    Simulator() : history_(1), edits_(1) {} // um snapshot vazio: current() é sempre válido

    // Zera o histórico e gera history[0]; false se o algoritmo não está
    // registrado. Guarda a Config, base para editar o instante 0.
    bool load(const Config& config);

    bool stepForward();                     // avança 1 instante (reaproveita se já existe); false no fim
    bool stepBack();                        // volta 1 instante (só move o cursor); false no instante 0
    bool isFinished() const { return finished(current()); }

    // --- Navegação e edição (design.md §9) ---------------------------------
    // Volta ao instante 0 e descarta o histórico seguinte; as edições do
    // instante 0 continuam valendo.
    void reset();
    // Leva o cursor ao instante 'tick' [A1.5.2]: para trás só move o cursor; para
    // a frente calcula o que faltar, mesmo com tarefas suspensas. Se chegou, a
    // mensagem fica vazia. Se a simulação termina antes, se 'tick' < 0 ou se o
    // histórico passaria de 'maxBytes', o cursor fica no último instante
    // possível e a mensagem explica o motivo [A5].
    RunSummary jumpTo(Tick tick, std::size_t maxBytes = DEFAULT_MAX_HISTORY_BYTES);
    // Execução completa [A1.5.3]: avança até o fim. Para antes, com a mensagem do
    // motivo, no instante 'maxTicks', quando só restam tarefas suspensas
    // (stalled) ou quando o histórico passaria de 'maxBytes'.
    RunSummary runToEnd(Tick maxTicks = NO_TICK_LIMIT,
                        std::size_t maxBytes = DEFAULT_MAX_HISTORY_BYTES);
    // Edição manual no instante do cursor [A1.5.2, A3.4]. Se aceita, é aplicada
    // antes do escalonador (D28) e o futuro é descartado e recalculado (D15).
    // Se recusada, nada muda e a mensagem explica o motivo.
    EditResult applyEdit(const Edit& edit);

    const SystemSnapshot& current() const { return history_[cursor_]; }
    const std::vector<SystemSnapshot>& history() const { return history_; }
    std::size_t cursor() const { return cursor_; }

private:
    // Avança até o instante 'target' ou até o fim. Para antes se o histórico
    // passaria de 'maxBytes' ou, com 'stopWhenStalled', se nada mais muda sem
    // edição. A mensagem fica vazia quando para por ter chegado ao alvo ou ao fim.
    RunSummary advance(Tick target, std::size_t maxBytes, bool stopWhenStalled);

    std::vector<SystemSnapshot> history_;   // history_[k] = estado no instante k
    std::size_t cursor_ = 0;                // instante exibido (relógio global, A1.5.2)
    Config config_;                         // configuração carregada (base do instante 0)
    // edits_[k]: edições aplicadas no instante k, em ordem. Uma nova edição em k
    // reaplica as anteriores antes dela (design.md §9).
    std::vector<std::vector<Edit>> edits_;
};
