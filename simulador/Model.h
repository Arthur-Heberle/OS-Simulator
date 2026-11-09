// =============================================================================
// Model.h — Tipos de dados do núcleo: o contrato entre o motor, que produz o
// histórico, e a interface, que o exibe (design.md §5).
//
// Contém o TCB (Task) e suas ocorrências (Job), o processador (CPU), os eventos
// desenhados no Gantt (Occurrence), o estado completo de um instante
// (SystemSnapshot), a configuração lida do arquivo (Config, ParseResult) e os
// pedidos de edição manual (Edit).
//
// Só dados: a lógica fica em funções livres (D21). Não há ponteiros entre as
// structs (as referências são índices), para que um SystemSnapshot possa ser
// copiado por valor no histórico (D16). Alterações neste arquivo são
// combinadas com a dupla antes.
//
// Requisitos: A1.2, A1.3, A2.2, A3.3.5, A4.3, A4.4.
// Decisões: D1, D3, D5, D6, D7, D12, D14, D21, D24.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Máximo de CPUs aceito no arquivo e na edição [RG2]. Justificativa em D12.
inline constexpr int MAX_CPUS = 64;

// ---------------------------------------------------------------------------
// Tarefa (TCB) e ocorrências
// ---------------------------------------------------------------------------

// Relógio global, em ticks (D3). 64 bits: com números do arquivo de até 10^9,
// os instantes calculados passam do limite de um int.
using Tick = long long;

// Ocorrências por tarefa periódica: a tarefa termina ao concluir 10 [A4.4].
// PERGUNTA EM ABERTO Q-A8: "executar 10 vezes" = 10 ocorrências concluídas?
// Padrão adotado: libera 10 ocorrências e termina quando as 10 concluem (D6).
constexpr int PERIODIC_ACTIVATION_LIMIT = 10;

// Estados da tarefa (D1). Cada estado corresponde a uma fila com semântica
// própria (resposta Q3) e tem um desenho próprio no Gantt (D2).
enum class TaskState {
    NotArrived,  // antes da primeira ativação
    Ready,       // tem ocorrência pendente e espera CPU na fila global
    Running,     // executando em uma CPU
    Dormant,     // concluiu a ocorrência do período e espera a próxima ativação
    Suspended,   // impedida de executar: manual no A; mutex/E/S no B (blockReason)
    Terminated   // concluiu as 10 ocorrências ou foi encerrada pelo usuário
};

// Tipo de tarefa e de servidor. Só Periodic/None são usados no Projeto A; os
// demais valores reservam o lugar do Projeto B (design.md §14).
enum class TaskKind   { Periodic, Aperiodic, Sporadic, Server };
enum class ServerType { None, BS, PS, DS, SS };

// Uma ocorrência (job) de uma tarefa periódica (D5). Uma ativação que chega
// antes de a anterior terminar é enfileirada, não descartada, porque a tarefa
// que perde o prazo continua executando [A3.3].
struct Job {
    Tick release = 0;      // instante da ativação
    Tick absDeadline = 0;  // release + prazo: chave do EDF e critério (2) do desempate [A4.3]
    Tick duration = 0;     // tempo de execução desta ocorrência
    Tick remaining = 0;    // tempo que falta executar
    bool missed = false;   // perda de prazo já registrada (D9: uma vez por ocorrência)
};

// TCB: todas as informações de uma tarefa, antes, durante e depois da
// simulação, numa única estrutura [A1.3].
struct Task {
    // Campos do arquivo de configuração
    std::string id;                // identificador único; texto, porque o Projeto B usa ids como "BS01"
    std::string color = "FFFFFF";  // cor RGB hexadecimal, em maiúsculas (ex.: "F0E0D0")
    Tick arrival = 0;              // instante de ingresso (primeira ativação)
    Tick duration = 0;             // tempo de execução de cada ocorrência
    Tick period = 0;               // > 0 periódica; 0 aperiódica (guardado no TCB, B1.2)
    Tick deadline = 0;             // prazo relativo à ativação; vazio ou 0 no arquivo = período (D8)
    // PERGUNTA EM ABERTO Q-A9: quais eventos (mutex, E/S) o Projeto A deve simular?
    // Padrão adotado: só guardar o texto, sem simular (A3.3.5 diz que é do B) (D30).
    std::string rawEvents;         // lista_eventos sem interpretação
    TaskKind kind = TaskKind::Periodic;     // no A só Periodic é simulada (A4.4, D7)
    ServerType server = ServerType::None;   // no A o prefixo do id não é interpretado (Q12)
    bool ignored = false;          // carregada mas não simulada (aperiódica no A, D7)
    std::string ignoredReason;     // motivo mostrado ao usuário

    // Estado dinâmico
    TaskState state = TaskState::NotArrived;
    int cpu = -1;                  // CPU onde executa (índice em cpus); -1 = nenhuma
    int lastCpu = -1;              // última CPU usada: afinidade e migração (D10)
    std::vector<Job> jobs;         // ocorrências pendentes; a da frente é a corrente (D5)
    Tick nextRelease = 0;          // próxima ativação; começa em 'arrival'
    int released = 0;              // ocorrências já ativadas (até PERIODIC_ACTIVATION_LIMIT)
    int completed = 0;             // ocorrências já concluídas

    // Estatísticas exibidas ao usuário [A1.5.1]
    Tick runTicks = 0, waitTicks = 0;        // ticks executando / ticks pronta sem CPU
    Tick firstStart = -1, finishTime = -1;   // primeiro instante em CPU / instante do término; -1 = ainda não
    int preemptions = 0, migrations = 0, deadlineMisses = 0;

    // Reservados para o Projeto B (sem uso no A)
    int basePriority = 0, effPriority = 0;  // herança e teto de prioridade
    std::string blockReason;                // causa da suspensão (manual no A; mutex, E/S no B)
    Tick serverBudget = 0;                  // orçamento de servidor
};

// Ocorrência corrente da tarefa, ou nullptr se não há nenhuma pendente.
inline const Job* currentJob(const Task& t) {
    return t.jobs.empty() ? nullptr : &t.jobs.front();
}

// Tarefa que disputa CPU neste instante: pronta ou executando, e simulada no A.
inline bool isCandidate(const Task& t) {
    return !t.ignored && (t.state == TaskState::Ready || t.state == TaskState::Running);
}

// Ordem natural de ids, usada no eixo Y do Gantt [A2.5] e nas tabelas (D4).
// PERGUNTA EM ABERTO Q-A6: o id é sempre numérico? Padrão adotado: id é texto,
// em ordem natural (D4).
// Cada id é dividido em blocos de dígitos e de não dígitos, comparados em
// ordem: dígitos pelo valor ("T2" < "T10"), texto sem diferenciar maiúsculas
// [A3.3.2], número antes de texto, prefixo antes do id mais longo. Empate
// total ("01" e "1") é desfeito pelo texto original.
// A regra mais simples "numérica se os dois forem números, senão texto" não é
// transitiva (2 < 10, 10 < "1A", "1A" < 2) e invalidaria o std::sort.
inline bool naturalIdLess(const std::string& a, const std::string& b) {
    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    auto upper = [](char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; };

    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        bool aDigit = isDigit(a[i]);
        bool bDigit = isDigit(b[j]);
        if (aDigit != bDigit) {
            return aDigit;  // bloco numérico antes de bloco de texto
        }
        // Blocos atuais: [i, endA) em 'a' e [j, endB) em 'b'.
        std::size_t endA = i, endB = j;
        while (endA < a.size() && isDigit(a[endA]) == aDigit) ++endA;
        while (endB < b.size() && isDigit(b[endB]) == bDigit) ++endB;

        if (aDigit) {
            // Compara o valor sem converter para inteiro (ids longos não
            // estouram): ignora zeros à esquerda; mais algarismos = maior.
            std::size_t startA = i, startB = j;
            while (startA + 1 < endA && a[startA] == '0') ++startA;
            while (startB + 1 < endB && b[startB] == '0') ++startB;
            std::size_t lenA = endA - startA, lenB = endB - startB;
            if (lenA != lenB) {
                return lenA < lenB;
            }
            int cmp = a.compare(startA, lenA, b, startB, lenB);
            if (cmp != 0) {
                return cmp < 0;
            }
        } else {
            std::size_t k = 0;
            while (i + k < endA && j + k < endB) {
                char ca = upper(a[i + k]), cb = upper(b[j + k]);
                if (ca != cb) {
                    return ca < cb;
                }
                ++k;
            }
            if ((endA - i) != (endB - j)) {
                return (endA - i) < (endB - j);
            }
        }
        i = endA;
        j = endB;
    }
    if (i == a.size() && j != b.size()) return true;   // 'a' é prefixo de 'b'
    if (j == b.size() && i != a.size()) return false;
    return a < b;
}

// ---------------------------------------------------------------------------
// Processador
// ---------------------------------------------------------------------------

struct CPU {
    int id = 0;            // índice 0..n-1; a interface mostra id+1 ("CPU1")
    int taskIndex = -1;    // tarefa em execução (índice em tasks); -1 = desligada [A1.2]
    bool on = true;        // false = desligada [A1.2]; começa true para registrar só as transições
    int quantumUsed = 0;   // ticks seguidos da tarefa atual, para algoritmos com quantum (D13)
    Tick ticksBusy = 0;    // ticks executando alguma tarefa
    Tick ticksOff = 0;     // ticks desligada, exibidos ao usuário [A1.2]
};

// ---------------------------------------------------------------------------
// Estado de um instante (histórico e Gantt)
// ---------------------------------------------------------------------------

// Eventos desenhados no Gantt no instante exato em que acontecem [A2.2]. Cada
// tipo tem um ícone distinto na legenda [RG3] (design.md §10).
enum class OccurrenceType {
    Arrival,       // ocorrência entra na fila de prontos, a cada ativação (Q5)
    Termination,   // fim de uma ocorrência (Q5)
    TaskFinished,  // fim da 10ª ocorrência: a tarefa termina [A4.4]
    DeadlineMiss,  // prazo absoluto atingido com execução pendente [A3.3]
    Dispatch,      // tarefa (re)começa numa CPU; só no registro, sem ícone
    Preemption,    // tarefa perdeu a CPU para outra mais prioritária
    Migration,     // tarefa voltou a executar em CPU diferente da última (D10)
    TieBreakDraw,  // escolha decidida por sorteio [A4.3]
    CpuOn,         // CPU religada [A1.2]
    CpuOff,        // CPU desligada por falta de tarefa [A1.2]
    ManualEdit     // edição manual neste instante [A1.5.2] (D15)
};

// Um evento do instante. Não tem campo de tempo: fica guardado no snapshot do
// próprio instante.
struct Occurrence {
    OccurrenceType type = OccurrenceType::Arrival;
    int taskIndex = -1;   // tarefa envolvida (índice em tasks); -1 = nenhuma
    int cpuIndex = -1;    // CPU envolvida (índice em cpus); -1 = nenhuma
    std::string note;     // detalhe para o registro e a dica (ex.: "enfileirada")
};

// Estado completo do sistema em um instante; o histórico é um vetor destes
// [A1.5.2]. Algoritmo, quantum e semente ficam aqui porque podem mudar durante
// a simulação [A3.1] e precisam voltar junto ao retroceder (D21).
struct SystemSnapshot {
    Tick tick = 0;                          // instante já processado (D3)
    std::string algorithm = "RM";           // nome no registro de escalonadores, em maiúsculas
    int quantum = 2;                        // RM e EDF não usam (Q4, D13)
    uint64_t seed = 1;                      // semente do sorteio sem estado (D14)
    std::vector<Task> tasks;                // índices estáveis: tarefas nunca são apagadas
    std::vector<int> readyQueue;            // fila global de prontos (índices em tasks) [A1.2]
    std::vector<CPU> cpus;                  // uma por CPU; o tamanho muda com a edição [A3.1]
    std::vector<Occurrence> occurrences;    // eventos deste instante
};

// ---------------------------------------------------------------------------
// Configuração lida do arquivo
// ---------------------------------------------------------------------------

// Configuração do sistema; os valores iniciais são os padrões [A3.2]. Resultado
// do parser e ponto de partida da simulação (initState, Simulator::load).
struct Config {
    std::string algorithm = "RM";  // chave do registro de escalonadores, em maiúsculas [A4.1]
    int quantum = 2;               // RM e EDF não usam (Q4, D13)
    int cpuCount = 1;              // 1..MAX_CPUS [RG2] (D12)
    uint64_t seed = 1;        // semente do sorteio (D14); alterável na interface
    std::vector<Task> tasks;  // campos do arquivo preenchidos; estado dinâmico no padrão
};

// Resultado da leitura do arquivo [A3.3.6]. Com algum erro, nada é carregado (D18).
struct ParseResult {
    Config config;                      // o que foi lido, com os padrões aplicados
    std::vector<std::string> errors;    // impedem o carregamento
    std::vector<std::string> warnings;  // mostrados ao usuário; não impedem o carregamento

    bool ok() const { return errors.empty(); }
};

// ---------------------------------------------------------------------------
// Edição manual [A1.5.2, A3.1, A3.4] (design.md §9)
// ---------------------------------------------------------------------------
// Cada pedido do usuário vira um Edit. O Simulator valida numa cópia; se
// aceito, aplica no instante atual antes do escalonador (D28) e recalcula o
// futuro (D15); se recusado, nada muda e EditResult traz o motivo.

// Tipos de edição; as regras de validação estão na tabela do design.md §9.
// Não há edição que ponha a tarefa em Running ou Ready: quem executa é decidido
// pelo escalonador.
enum class EditKind {
    // Estado da tarefa
    SetRemaining,  // tempo restante da ocorrência corrente (1..duração)
    Suspend,       // Ready/Running/Dormant -> Suspended (libera a CPU)
    Resume,        // Suspended -> Ready (com ocorrência pendente) ou Dormant
    ForceFinish,   // encerra a tarefa agora (Terminated)
    Remove,        // retira a tarefa da simulação (Terminated; o índice continua)
    ActivateNow,   // Dormant: cria a próxima ocorrência neste instante
    // Características da tarefa (valem para as próximas ocorrências)
    SetArrival,    // só antes do ingresso; novo valor >= instante atual
    SetDuration,
    SetPeriod,
    SetDeadline,   // recalcula também o prazo absoluto das ocorrências pendentes
    SetColor,      // RRGGBB; recusada se repetida, quase preta ou quase branca (D17)
    // Sistema
    SetAlgorithm,  // qualquer nome do registro de escalonadores [A3.1]
    SetCpuCount,   // 1..MAX_CPUS; ao reduzir, tarefas das CPUs removidas voltam a Ready
    SetQuantum,
    SetSeed,       // semente do sorteio (D14)
    AddTask        // nova tarefa (newTask), validada como uma linha do arquivo
};

// Pedido de edição. Cada tipo lê só os campos de que precisa.
struct Edit {
    EditKind kind = EditKind::SetRemaining;
    int taskIndex = -1;    // tarefa alvo (índice em tasks); -1 = edição do sistema
    long long value = 0;   // valor numérico (tempo, nº de CPUs, quantum, semente)
    std::string text;      // valor textual (cor, nome do algoritmo)
    Task newTask;          // tarefa completa, só para AddTask
};

// Resposta a uma edição [A3.4]: se recusada, 'message' explica o motivo.
struct EditResult {
    bool ok = false;
    std::string message;
};
