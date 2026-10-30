#pragma once
#include <cstddef>
#include <vector>
#include "core/Parser.h"        // Config
#include "core/Scheduler.h"
#include "core/SystemSnapshot.h"

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

    const SystemSnapshot& current() const { return history_[cursor_]; }
    const std::vector<SystemSnapshot>& history() const { return history_; }
    std::size_t cursor() const { return cursor_; }

private:
    std::vector<SystemSnapshot> history_;   // history_[k] = estado no instante k
    std::size_t cursor_ = 0;                // instante mostrado ao usuário (relógio global, A1.5.2)
};
