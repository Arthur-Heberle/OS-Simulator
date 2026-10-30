# design.md — COMO o simulador é construído (decisões, arquitetura, contratos)

> **Para o Claude Code:** este arquivo define as decisões já tomadas pela equipe. **Não as reabra** sem pedir ao usuário.
> Se algo aqui contradisser `enunciado.md`, o enunciado vence: pare e avise o usuário.
> A coluna "Porquê" de cada decisão é também o roteiro de justificativa para a **defesa oral** (o professor penaliza respostas sem justificativa técnica). Mantenha esses porquês nos comentários do código (RG6).

---

## 1. Visão geral e princípios inegociáveis

Simulador de SO multitarefa preemptivo, de tempo compartilhado, com **N CPUs**, **fila de prontos global**, escalonadores **RM** e **EDF** (Projeto A), gráfico de **Gantt** (tela + imagem exportada), simulação **passo a passo com retroceder/avançar e edição manual**, configuração por **arquivo de texto**. O Projeto B (aperiódicas, esporádicas, servidores, teste de escalonabilidade) será somado depois **no mesmo código**.

Princípios (valem para toda linha de código):

1. **Núcleo sem Qt.** Tudo da simulação é C++17 puro (STL). Qt só existe na camada visual. O núcleo compila e é testado sem janela.
2. **Determinismo total.** Mesma configuração + mesma semente + mesmas edições ⇒ mesmo histórico, bit a bit. Nada de relógio real, `rand()`, ponteiros como chave, ordem de `unordered_map`.
3. **Todo o estado mutável vive em `SystemSnapshot`** (copiável por valor, **sem ponteiros**: referências por índice). O `Simulator` só guarda `history` + `cursor`.
4. **Regras são funções livres** sobre o snapshot; **escalonador é função** registrada por nome (A4.2). Nada de "classe-deus".
5. **Um único renderizador** desenha na tela, em PNG e em SVG (A2.4: nunca capturar a tela).
6. **Toda informação de tarefa mora no `Task` (TCB)** (A1.3).
7. **Comentários em português** explicando o quê **e o porquê** (RG6). Identificadores em inglês (convenção do repositório).
8. **Nunca falhar em arquivo válido** (A3.3.7). Na dúvida, aceitar com aviso.

---

## 2. Stack e ferramentas

| Item | Escolha | Porquê |
|---|---|---|
| Linguagem | **C++17** | Já escolhida; STL cobre o núcleo; sem runtime extra (RG5) |
| Interface | **Qt 6 (Widgets + Svg)**, ≥ 6.5 LTS | `QPainter` desenha em widget, `QImage` (PNG) e `QSvgGenerator` (SVG) com o mesmo código (A2.4); `QFileDialog`/`QFile` carregam arquivo de qualquer lugar (A3.3.4); tabelas e docks para o modo depurador (A1.5.1). Biblioteca externa é permitida pelo professor (Q1) |
| Build | **CMake ≥ 3.21** + Ninja ou Make; compiladores MinGW-w64 (g++), MSVC ou g++ no Linux — **detectar o que o usuário tem; não presumir** | O professor exige compilar na defesa com Make/CMake. Makefile é frágil com `moc`, caminhos do Qt e deploy no Windows |
| Biblioteca do núcleo | `simcore` (estática), usada por `sim_tests` e `sim_gui` | Testes sem Qt; separação de camadas |
| Testes | Executável `sim_tests` com macro `CHECK` própria (sem GoogleTest) | Evita dependência extra; cabe em 1 arquivo (`tests.cpp`) |
| Empacotamento | **Qt dinâmico + deploy** (`qt_generate_deploy_app_script` + `cmake --install`, ou `windeployqt`) → pasta `dist/` com `.exe` + DLLs + plugins | **Confirmado pelo professor (Q9):** atende se não precisar instalar o Qt. Ver D26 |
| Runtime do compilador | MinGW: `-static-libgcc -static-libstdc++` (e `libwinpthread` junto) | Evita depender de DLL do MinGW na máquina alvo |
| Versionamento | Git; `main` sempre compila e passa em `sim_tests`; uma branch por fase (`fase-N-tema`) e `fix/tema`, ver `docs/fluxo-git.md`; tag `entrega-A` | Cada um integra sem depender da aprovação do outro, mas lê o que o outro integrou; entrega reproduzível |
| Formato de cada arquivo | UTF-8, **LF** (`.gitattributes`) | Repositório hoje tem CRLF misturado |

Observação prática: o caminho local do usuário contém espaços (`...\UTFPR - Eng. Computacao\...`). Se o `moc`/Ninja/CMake reclamarem de caminho, **clonar em caminho sem espaços** (ex.: `C:\dev\OS-Simulator`).

---

## 3. Repositório: estado atual, alvo e migração

### 3.1 Estado atual (zip de 23/10/2026, 3 commits, só esqueleto)

```
OS-Simulator/
  .gitignore                       <- contém "*.md" (!!) e "build/ "
  simulador-escalonamento-v20261015.pdf
  Project-A/
    Makefile                       <- g++ -std=c++17 -Wall -Wextra -Iinclude; alvo "windows" com mingw -static
    include/core/{CPU,Parser,Scheduler,Simulator,SystemSnapshot,Task}.h
    src/core/{Scheduler,Simulator,Task}.cpp
    src/main.cpp                   <- int main(){return 0;}
  Project-B/CHECKLIST.md           <- checklist do B (a do A foi apagada, mas existe no histórico: 8ba93a4)
```

**Problemas encontrados que precisam ser corrigidos (Fase 0/1):**

| # | Problema | Correção |
|---|---|---|
| 1 | `.gitignore` tem `*.md` ⇒ **todos os `.md` (inclusive estes docs) seriam ignorados** | Remover `*.md`; ignorar só `build/`, `dist/`, binários, `CMakeLists.txt.user` |
| 2 | `Task` é `class` com membros `private` e sem acessores; o commit diz "classes to structs" mas `Task`, `Scheduler`, `Simulator` ainda são classes | `Task` vira `struct` (TCB). Parser e renderizador precisam acessar os campos |
| 3 | `SystemSnapshot` guarda `std::mt19937` (~2,5 KB **por tick** no histórico) | Trocar por `uint64_t seed` + hash sem estado (D14) |
| 4 | `Occurrence` só tem 4 tipos | Estender (ver §5) |
| 5 | `Scheduler` é uma classe com string e switch futuro; viola A4.2 | Trocar por registro de funções (§7) |
| 6 | `Simulator::parseConfig` mistura leitura de arquivo com orquestração | Mover para função livre `parseConfig` em `Parser.h/.cpp` (`Parser.cpp` foi apagado) |
| 7 | Includes `"core/Task.h"` e pastas `include/`, `src/core/` | Achatar em `simulador/` com `#include "Task.h"` (o Moodle não aceita pastas) |
| 8 | Comentários misturam inglês e português | Padronizar em português |
| 9 | Makefile | Substituir por `CMakeLists.txt` (reaproveitar a ideia do `-static` do alvo `windows`) |
| 10 | CRLF/LF misturados | `.gitattributes` com `* text=auto` e normalizar |
| 11 | `std::deque` seria tentador para os `Job` pendentes | **Não usar**: aloca ~512 B por instância e é copiado a cada tick. Usar `std::vector<Job>` |

### 3.2 Estrutura-alvo

```
OS-Simulator/
  .gitignore  .gitattributes  CLAUDE.md
  simulador/                 <- EXATAMENTE o que vai ao Moodle (≤ 18 arquivos, SEM subpastas)
    CMakeLists.txt
    Model.h                  (enums, Task, Job, CPU, Occurrence, SystemSnapshot, Config, Edit)
    Parser.h  Parser.cpp
    Scheduler.h  Scheduler.cpp
    Simulator.h  Simulator.cpp
    GanttRenderer.h  GanttRenderer.cpp
    Dialogs.h
    MainWindow.h  MainWindow.cpp
    main.cpp                 (entrada do Qt)
    tests.cpp                (sim_tests; contém também o executor em texto)
    exemplo_rm.txt  exemplo_edf.txt
    telas_projeto_a.pdf      (gerado na Fase 8)
  docs/                      enunciado.md  design.md  planejamento.md  testes.md
                             checklist-A.md  checklist-B.md  duvidas_projeto_a.txt  simulador-escalonamento-v20261015.pdf
```

**Orçamento: 18 de 20 arquivos** (sobram 2). Regra: se um arquivo crescer demais, dividir por responsabilidade, não por tamanho; se precisar de arquivo novo, antes fundir outro. No B o orçamento aperta: prever fusões (ex.: `Dialogs.h` dentro de `MainWindow.h`) ou pedir autorização por escrito (Q-A1).

### 3.3 Mapa de migração

| Existe | Vira |
|---|---|
| `Task.h`, `CPU.h`, `SystemSnapshot.h` | `Model.h` (mantendo os nomes `Task`, `CPU`, `SystemSnapshot`, `Occurrence`, `OccurrenceType`) |
| `Task.cpp` | apagar (sem lógica) |
| `Parser.h` (`Config`, `ParseResult`) | `Parser.h` + novo `Parser.cpp`; `Config` migra para `Model.h` |
| `Scheduler.h/.cpp` | reescritos como registro de funções |
| `Simulator.h/.cpp` | mantidos como orquestrador; tick/despacho/edição como funções livres |
| `src/main.cpp` | descartar; o executor em texto vive em `tests.cpp`; `main.cpp` novo = entrada do Qt |
| `Makefile` | `CMakeLists.txt` (apagar o Makefile após o CMake funcionar) |
| `Project-B/CHECKLIST.md` | `docs/checklist-B.md` |
| PDF do enunciado | `docs/` |

---

## 4. Decisões de projeto e justificativas

| ID | Assunto | Decisão | Porquê (usar na defesa) |
|---|---|---|---|
| D1 | Estados e filas | `NotArrived`, `Ready`, `Running`, **`Dormant`** (concluiu a ocorrência do período e espera a próxima ativação), **`Suspended`** (impedida: manual no A; mutex/E/S no B), `Terminated`. Filas lógicas: prontos (global), dormentes (ordenada por próxima ativação), suspensos; `Running` é por CPU | O professor (Q3) mandou decidir e justificar: cada fila tem uma semântica. `Dormant` não concorre à CPU até o próximo release; misturar com `Suspended` esconderia o motivo e atrapalharia o B |
| D2 | Desenho dos estados | `Running`: sólido na cor + rótulo `CPUk`; `Ready`: só contorno, sem cor; `Suspended`: preto pontilhado; `Dormant`/`NotArrived`: só linha-base | A2.1 (ausência de cor = pronta; preto + padrão diferente = suspensa); distingue "pronta" de "fora do sistema" |
| D3 | Tempo e snapshot | `Tick = long long`. Tick `t` cobre `[t, t+1)`. **`snapshot[k]` = estado do sistema no instante k, após processar o instante k**; descreve o que ocorre em `[k,k+1)`. Ativação, perda de prazo, preempção, sorteio ficam em `x=k`; término em `x=k+1` (aparece em `snapshot[k+1]`) | A2.2 ("momento exato") + alinha com o `SystemSnapshot` já criado (`occurrences` guardadas no snapshot do tick). O Gantt lê uma coluna por snapshot |
| D4 | ID | `std::string` única, sem diferenciar caixa. Eixo Y em **ordem natural por blocos** (`naturalIdLess`): blocos de dígitos comparados pelo valor, blocos de texto sem caixa, número antes de texto, empate pelo texto original | O B usa `BS01`, `PS02`…; A2.5 exige ordem por ID. A regra "numérico se ambos forem números, senão texto" não é transitiva (2 < 10, 10 < 1A, 1A < 2) e quebraria o `std::sort`; por blocos dá o mesmo resultado para ids só numéricos |
| D5 | Ocorrência (job) | `Task.jobs` é `std::vector<Job>` (frente = ocorrência corrente). Se o próximo período chega antes de a anterior terminar, a ativação é **enfileirada**, não descartada | A3.3: a tarefa que perde o prazo continua executando; descartar ativações esconderia a sobrecarga |
| D6 | Fim | Cada periódica libera exatamente 10 ocorrências (`PERIODIC_ACTIVATION_LIMIT`) e termina quando as 10 concluem; simulação acaba quando todas as não ignoradas estão `Terminated` | A4.4 |
| D7 | Fora do escopo do A | `periodo == 0` (aperiódica): **carregada, marcada `ignored`, não simulada, listada no relatório de carga como aviso**. `periodo < 0`: **erro do arquivo** no A (Q10). ID `BSxx/PSxx/DSxx/SSxx` com `periodo > 0`: **tarefa periódica comum** no A, sem política de servidor (Q12) | A4.4 + A3.3 ("valores menores que zero representam erros no arquivo") + respostas Q10 e Q12. No B: `ignored` deixa de valer para aperiódicas, `periodo < 0` vira esporádica (B1.3) e o prefixo do ID liga a política de servidor (B2) |
| D8 | Prazo | Relativo à ativação; absoluto = `release + prazo`. Campo vazio ou 0 ⇒ `prazo = período`. Prazo > período: aceito com aviso | A3.2 (padrões) |
| D9 | Perda de prazo | Detectada no instante `absDeadline` se `remaining > 0`, **uma vez por ocorrência**; a ocorrência continua | A3.3. Quem termina exatamente em `absDeadline` não perde (término é processado antes) |
| D10 | Multiprocessador | **Escalonamento global**, fila única. A cada instante escolhem-se as M melhores (M = nº de CPUs). Quem já executa e foi escolhido **permanece na mesma CPU**; novos vão para CPU livre, preferindo a última usada | A1.2 (fila global, sem ociosidade); minimiza trocas de contexto e migrações |
| D11 | CPU sem tarefa | Fica **desligada**; contam-se ticks desligados por CPU (painel + hachura cinza no Gantt). Custo de troca de contexto = 0 (documentado) | A1.2 |
| D12 | Limite N de CPUs | `MAX_CPUS = 64` (uma constante) | Servidores comuns chegam a 64 núcleos por socket; acima disso o Gantt por CPU perde legibilidade; o histórico cresce com ticks × (tarefas + CPUs). Justificativa técnica exigida em Q6 |
| D13 | Quantum | Lido, validado (inteiro ≥ 1), guardado, exibido; RM e EDF ignoram; `CPU.quantumUsed` é mantido para algoritmos futuros | Q4: específico de cada escalonador |
| D14 | Desempate (A4.3) | Ordem: (1) estava `Running` antes do escalonamento; (2) menor prazo **absoluto** da ocorrência corrente; (3) menor ingresso; (4) menor duração; (5) **sorteio determinístico** `tieHash(seed, tick, taskIndex)` (splitmix64), sem estado, feito pelo `Simulator` sobre os grupos de empate que o escalonador devolve (§7). Ícone de dado em `x=k` na tarefa escolhida. Só se emite o ícone quando o sorteio realmente decide algo (`|grupo empatado| > slotsLeft`). (2) confirmado pelo professor (Q11) | Sem estado ⇒ retroceder/avançar e re-simular após edição reproduzem o mesmo sorteio; evita guardar `mt19937` (~2,5 KB) em cada snapshot; `std::uniform_int_distribution` não é portável entre compiladores |
| D15 | Edição no passado | Editar no passo `k` descarta `history[k+1..]` e segue da nova linha do tempo; ícone "edição manual" em `x=k` | Causalidade: o futuro antigo deixou de valer |
| D16 | Histórico | 1 snapshot completo por tick, em memória; sem threads | A1.5.2; sem threads não há corrida e o determinismo é trivial |
| D17 | Cores | Hex RGB de 6 dígitos (tolera `#`, caixa livre); fora disso é erro. Cor ausente ⇒ paleta automática. Cor repetida ou escura demais (preta) ⇒ **aviso**, e a tarefa recebe a próxima cor livre da paleta (ideia do Arthur) | A2.1 exige uma cor diferente por tarefa e reserva o preto para "suspensa": trocar a cor garante isso sem barrar um arquivo válido |
| D18 | Erros de arquivo | **Fatal** (nada é carregado; mostra arquivo, linha, campo, valor, motivo) vs **aviso** (carrega e informa). Tolera espaços, linhas vazias, CRLF/LF, BOM UTF-8, `;` final opcional, campos opcionais ausentes | A3.3.3, A3.3.6, A3.3.7 |
| D19 | Erro de redação do enunciado | A2.3 cita "requisito 1.4"; implementar o **1.5** | O conteúdo é sobre os modos de execução |
| D20 | Código único A+B | Uma só árvore; entrega do A = tag `entrega-A`; B continua dali. Sem `Project-A/` e `Project-B/` | O B complementa o A; duas pastas geram divergência |
| D21 | Estado e `Simulator` | `SystemSnapshot` carrega todo o estado; `Simulator` guarda `history` + `cursor` + validação; ativação, despacho, perda de prazo, conclusão e edição são funções livres | Snapshot = cópia; retroceder = mexer no cursor; cada regra testável isolada |
| D22 | Build | CMake com `simcore` + `sim_tests` + `sim_gui` | Ver §2 |
| D23 | Layout de entrega | `simulador/` plano, ≤ 18 arquivos, sem subpastas, includes planos | Moodle não aceita pastas nem zip; o que compila no Git é o que o professor recebe |
| D24 | Nomes | Manter os do repositório: `Task`, `CPU`, `SystemSnapshot`, `Occurrence`; PascalCase, `.h/.cpp`; `Model.h` reúne as structs | Menos arquivos; respeita o trabalho já feito |
| D25 | Ordem de trabalho | Núcleo primeiro; em paralelo apenas o teste de Qt standalone e o renderizador sobre dados falsos | O maior risco técnico (empacotar Qt) independe do motor; o `SystemSnapshot` precisa nascer pensando no que o Gantt consome |
| D26 | Standalone | **Opção A: Qt dinâmico + deploy** (pasta com `.exe`+DLLs+plugins). Testar em VM/máquina sem Qt e com `PATH` mínimo | Resposta do professor (Q9). Qt estático foi descartado: compilar Qt é lento e não traz ganho agora |
| D27 | Idioma | Comentários em português; identificadores em inglês; mensagens ao usuário em português | Defesa em português; convenção do repo |
| D28 | Semântica da edição | Edição no instante `k` é aplicada **depois** de conclusões/ativações/perdas de prazo do instante e **antes** do escalonador; reexecuta o instante a partir de `history[k-1]` | O escalonador já enxerga o estado editado; preserva determinismo |
| D29 | Parser fora do `Simulator` | `ParseResult parseConfig(const std::string&)` é função livre; a GUI lê o arquivo com `QFile` e passa o texto | Núcleo sem I/O de arquivo; testes passam strings; `QFile` aceita qualquer caminho/pendrive/Unicode (A3.3.4) |
| D30 | `lista_eventos` | Campo = **tudo depois do 6º `;`** (sem o `;` final), guardado cru em `Task.rawEvents` | O formato dos eventos só é definido no B; não quebrar se contiverem `;` |

---

## 5. Modelo de dados (`Model.h`) — contrato

> Estas são as definições-alvo. Estruturas são **dados**; a lógica fica em funções. **Sem ponteiros entre structs.** Tudo copiável por valor.

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

using Tick = long long;                       // relógio global, em ticks
constexpr int  MAX_CPUS = 64;                 // D12
constexpr int  PERIODIC_ACTIVATION_LIMIT = 10;// D6 / A4.4

enum class TaskState  { NotArrived, Ready, Running, Dormant, Suspended, Terminated };   // D1
enum class TaskKind   { Periodic, Aperiodic, Sporadic, Server };                        // B: previsto
enum class ServerType { None, BS, PS, DS, SS };                                         // B: previsto

// Uma ocorrência (job) de uma tarefa periódica. D5.
struct Job {
    Tick release = 0, absDeadline = 0;
    Tick duration = 0, remaining = 0;
    bool missed = false;                      // perda de prazo já registrada (D9)
};

// TCB: ÚNICA estrutura com tudo da tarefa (A1.3).
struct Task {
    // --- campos do arquivo ---
    std::string id;                           // única, sem diferenciar caixa (D4)
    std::string color = "FFFFFF";             // "F0E0D0" normalizado em maiúsculas (D17); o parser sempre define
    Tick arrival = 0, duration = 0, period = 0, deadline = 0;   // 'deadline' já com o padrão aplicado (D8)
    std::string rawEvents;                    // lista_eventos crua (D30); B: vetor de eventos
    TaskKind kind = TaskKind::Periodic;
    ServerType server = ServerType::None;
    bool ignored = false;                     // fora do escopo do A (D7)
    std::string ignoredReason;
    // --- estado dinâmico ---
    TaskState state = TaskState::NotArrived;
    int cpu = -1, lastCpu = -1;               // índices em SystemSnapshot::cpus; -1 = nenhuma
    std::vector<Job> jobs;                    // frente = ocorrência corrente (NÃO usar std::deque)
    Tick nextRelease = 0;                     // próxima ativação; inicia em 'arrival'
    int released = 0, completed = 0;          // limite = PERIODIC_ACTIVATION_LIMIT
    // --- estatísticas ---
    Tick runTicks = 0, waitTicks = 0, firstStart = -1, finishTime = -1;
    int preemptions = 0, migrations = 0, deadlineMisses = 0;
    // --- ganchos do B (já presentes, sem uso no A) ---
    int basePriority = 0, effPriority = 0;    // herança/teto de prioridade
    std::string blockReason;                  // causa da suspensão (mutex, E/S, manual)
    Tick serverBudget = 0;                    // orçamento de servidor
};

struct CPU {
    int id = 0;
    int taskIndex = -1;                       // índice em tasks; -1 = sem tarefa
    bool on = true;                           // false = desligada (A1.2). Inicia true para só marcar transições
    int quantumUsed = 0;                      // ticks consecutivos da tarefa atual (para algoritmos com quantum)
    Tick ticksBusy = 0, ticksOff = 0;         // exibidos ao usuário (A1.2)
};

// Eventos que o Gantt desenha no instante exato. Q5: Arrival = entrada na fila de prontos (a cada ativação);
// Termination = fim de UMA ocorrência.
enum class OccurrenceType {
    Arrival, Termination, TaskFinished /*10ª ocorrência*/, DeadlineMiss,
    Dispatch /*só log, sem ícone*/, Preemption, Migration, TieBreakDraw,
    CpuOn, CpuOff, ManualEdit
};
struct Occurrence {
    OccurrenceType type = OccurrenceType::Arrival;
    int taskIndex = -1;                       // -1 = nenhuma
    int cpuIndex = -1;                        // -1 = nenhuma
    std::string note;                         // texto para log/tooltip (ex.: "enfileirada", descrição da edição)
};

// Estado completo no instante 'tick', depois de processá-lo (D3). É a ÚNICA fonte para o Gantt e para o histórico.
struct SystemSnapshot {
    Tick tick = 0;
    std::string algorithm = "RM";             // maiúsculas
    int quantum = 2;
    uint64_t seed = 1;                        // D14: sorteio sem estado
    std::vector<Task> tasks;                  // índice estável: nunca remover (marcar Terminated)
    std::vector<CPU> cpus;                    // tamanho = nº de CPUs atual
    std::vector<int> readyQueue;              // índices Ready (não Running), por ordem de chegada à fila
    std::vector<Occurrence> occurrences;      // o que aconteceu NESTE instante
};

// Resultado da leitura do arquivo (A3.3.6). 'errors' não vazio => nada carregado.
struct Config {
    std::string algorithm = "RM";             // padrões (A3.2)
    int quantum = 2;
    int cpuCount = 1;
    uint64_t seed = 1;
    std::vector<Task> tasks;                  // campos do arquivo; estado dinâmico no padrão
};
struct ParseResult {
    Config config;
    std::vector<std::string> errors;          // ex.: "Linha 4, campo 'cor': 'GG0000' não é hexadecimal (esperado RRGGBB)"
    std::vector<std::string> warnings;        // ex.: "Linha 6: tarefa 'A3' é aperiódica; ignorada no Projeto A"
    bool ok() const { return errors.empty(); }
};

// Edição manual (A1.5.2, A3.1, A3.4). Ver §9.
enum class EditKind { SetRemaining, Suspend, Resume, ForceFinish, Remove, ActivateNow,
                      SetArrival, SetDuration, SetPeriod, SetDeadline, SetColor,
                      SetAlgorithm, SetCpuCount, SetQuantum, SetSeed, AddTask };
struct Edit { EditKind kind = EditKind::SetRemaining; int taskIndex = -1; long long value = 0; std::string text; Task newTask; };
struct EditResult { bool ok = false; std::string message; };   // message explica a recusa (A3.4)
```

Funções-ajuda livres (inline em `Model.h` ou `Simulator.h`): `const Job* currentJob(const Task&)`, `bool isCandidate(const Task&)` (`Ready` ou `Running`, não `ignored`), `bool naturalIdLess(const std::string&, const std::string&)`.

---

## 6. Semântica da simulação (contrato do motor)

**Linha do tempo.** `history[0]` é o instante 0 já processado. Cada passo gera `history[k+1]` a partir de `history[k]`:

```cpp
SystemSnapshot makeNext(const SystemSnapshot& prev) {
    SystemSnapshot s = prev;  s.occurrences.clear();
    executeTick(s);           // 1. consome 1 tick: Running.remaining--, runTicks++, waitTicks++ (Ready),
                              //    CPU.ticksBusy/ticksOff, quantumUsed
    s.tick = prev.tick + 1;
    beginInstant(s);          // 2. conclusões -> 3. ativações -> 4. perdas de prazo
    scheduleAndDispatch(s);   // 5. escalonar -> 6. despachar -> 7. readyQueue
    return s;
}
// instante 0: s.tick=0; initState(config); beginInstant(s); scheduleAndDispatch(s);
```

**Ordem fixa dentro de um instante** (responde Q-A10):

1. **Conclusões.** Para cada tarefa com `jobs.front().remaining == 0`: ocorrência `Termination` (x = tick); remove o job; `completed++`; libera a CPU (`cpu.taskIndex=-1`, `task.cpu=-1`; `lastCpu` permanece). Se ainda há job pendente → `Ready`; senão, se `released == LIMIT` → `Terminated` + ocorrência `TaskFinished`; senão `Dormant`. Registrar `finishTime`.
2. **Ativações.** Para cada tarefa não `ignored` com `nextRelease == tick` e `released < LIMIT`: criar `Job{release=tick, absDeadline=tick+deadline, duration, remaining=duration}`, `released++`, `nextRelease += period`, ocorrência `Arrival` (nota "enfileirada" se já havia job). Estado: `NotArrived`/`Dormant` ⇒ `Ready`; `Ready`/`Running` ⇒ continua (backlog); `Suspended` ⇒ continua `Suspended`.
3. **Perdas de prazo.** Para cada job com `absDeadline == tick`, `remaining > 0` e `!missed`: `missed=true`, `deadlineMisses++`, ocorrência `DeadlineMiss`.
4. **Escalonamento.** O escalonador do `snapshot.algorithm` devolve os `PriorityGroups` dos candidatos (`isCandidate`); `chooseTasks` pega grupo a grupo até preencher `M = cpus.size()` CPUs e só sorteia quando um grupo empatado não cabe nas CPUs que sobram (§7). Se `candidatos ≤ M`, todos executam e não há sorteio.
5. **Despacho** (`chosen` em ordem de escolha):
   a. `Running` fora de `chosen` ⇒ `Ready`, libera CPU, `preemptions++`, `Preemption`.
   b. `chosen` já `Running` ⇒ permanece (mesma CPU).
   c. `chosen` não `Running` ⇒ CPU livre: preferir `lastCpu` se livre, senão a de menor índice; `state=Running`; `Dispatch`; se `lastCpu != -1 && lastCpu != cpu` ⇒ `Migration`, `migrations++`; `firstStart` se `-1`; `quantumUsed=0`; se a decisão teve `tieDraw` ⇒ `TieBreakDraw`.
   d. CPU sem tarefa ⇒ `on=false` (e `CpuOff` se estava ligada); CPU com tarefa e `on==false` ⇒ `on=true` + `CpuOn`.
6. `readyQueue` = tarefas `Ready` ordenadas por `(jobs.front().release, índice)`.

**Fim.** `finished(s)` ⇔ toda tarefa não `ignored` está `Terminated`. A última coluna desenhável é `tick-1`; o instante final só carrega ocorrências (ex.: o último `Termination`).

**Leitura do Gantt.** Coluna `k` ← `history[k]`: estado de cada tarefa e CPU em `[k,k+1)`; `occurrences` de `history[k]` são desenhadas em `x=k`.

**Utilização (informativa; prepara B3).** Painel com `U = Σ duracao/periodo` das tarefas simuladas; limite de Liu-Layland `n(2^(1/n) − 1)` para RM em 1 CPU; `U ≤ 1` para EDF em 1 CPU; para M CPUs `U ≤ M` como condição **necessária**. Só informa; no A não recusa tarefas.

---

## 7. Escalonadores e registro (A4.2)

**Política × mecanismo** (modelo do Arthur, adotado): o escalonador (**política**) só diz a **ordem de prioridade**; não altera o estado, não conhece CPUs, não sorteia. O `Simulator` (**mecanismo**) escolhe quem executa, faz o sorteio, despacha e cuida das filas. Assim um algoritmo novo não toca no motor (A4.2).

```cpp
// Índices de tasks agrupados por prioridade, do grupo mais prioritário ao menos.
// Tarefas do mesmo grupo continuam empatadas depois dos critérios (1)–(4) do A4.3.
using PriorityGroups = std::vector<std::vector<int>>;
using SchedulerFn    = PriorityGroups (*)(const SystemSnapshot&);
using KeyFn          = Tick (*)(const Task&);  // menor = mais prioritário

PriorityGroups scheduleRM (const SystemSnapshot&);
PriorityGroups scheduleEDF(const SystemSnapshot&);
void        registerScheduler(const std::string& name, SchedulerFn fn);  // chave em MAIÚSCULAS
SchedulerFn findScheduler(const std::string& name);                     // sem diferenciar caixa; nullptr se não existe
std::vector<std::string> schedulerNames();                              // alimenta o parser e a GUI
uint64_t    tieHash(uint64_t seed, Tick now, int taskIndex);            // splitmix64 (D14)
```

- **RM:** chave = `period`. **EDF:** chave = `jobs.front().absDeadline`.
- `compareTasks(a, b, key)` devolve -1/0/1: chave → (1) estava `Running` → (2) menor prazo **absoluto** da ocorrência corrente (Q11) → (3) menor `arrival` → (4) menor `duration`. `orderAndGroup` ordena os candidatos com `std::stable_sort` e junta em grupos os que dão 0. **O sorteio nunca fica no comparador** (comparador aleatório = comportamento indefinido no `sort`).
- `chooseTasks` (no `Simulator`): percorre os grupos; um grupo que cabe inteiro nas CPUs livres entra todo (sem sorteio); o primeiro que não cabe é ordenado por `tieHash` e só os `slotsLeft` primeiros entram, marcados com `tieDraw` (ícone de sorteio, A4.3). Isso cobre o atalho "candidatos ≤ M ⇒ todos executam".
- O registro é um mapa estático local criado na primeira chamada já com RM e EDF (sem problema de ordem de inicialização estática). `registerScheduler` permite incluir algoritmos sem editar `Scheduler.cpp`.
- **Extensibilidade provada por teste:** `tests.cpp` registra um escalonador FIFO e roda uma simulação sem tocar no motor. (Opcional, só no fim: carregar `SchedulerFn` de uma DLL com `QLibrary`.)
- Adicionar um algoritmo = escrever 1 função-chave + 1 linha no registro (ou 1 chamada a `registerScheduler`).

---

## 8. Parser (A3.3.x)

`parser::parseConfig(const std::string& text)` e `parser::serializeConfig(const Config&)` (ida e volta), no `namespace parser`. Limites próprios do parser em `Parser.h`: `MAX_QUANTUM` (10⁶) e `MAX_FIELD_VALUE` (10⁹).

**Pré-processamento:** remover BOM UTF-8; normalizar CRLF/CR → LF; separar linhas **guardando o número original**; descartar linhas vazias ou só com espaços; 1ª linha útil = sistema; demais = tarefas. Em cada linha: dividir por `;`, preservando campos vazios; remover `;` final; `trim` em cada campo; comparar textos sem diferenciar caixa.

**Linha do sistema:** `algoritmo;quantum;qtde_cpus`. Campos ausentes ⇒ padrão (aviso informativo). Algoritmo desconhecido ⇒ **erro** listando `schedulerNames()`. `quantum` inteiro ≥ 1. `qtde_cpus` em `1..MAX_CPUS`.

**Linha de tarefa:** `id;cor;ingresso;duracao;periodo;prazo;lista_eventos` — `lista_eventos` = tudo depois do 6º `;` (D30). Obrigatórios: `id`, `ingresso`, `duracao`, `periodo`. Opcionais: `cor` (vazia ⇒ paleta automática: 12 cores fixas bem distintas e, depois, uma sequência que percorre as 2²⁴ cores sem repetir; nunca escolhe cor quase preta, quase branca ou já usada), `prazo` (vazio/0 ⇒ `periodo`), `lista_eventos`.

**Erros fatais** (mensagem: linha, campo, valor, motivo, o que seria válido): menos de 2 linhas úteis; algoritmo desconhecido; `quantum`/`qtde_cpus` inválidos; `id` vazio ou duplicado (sem caixa); `cor` fora de `RRGGBB`; `ingresso < 0`; `duracao ≤ 0`; `periodo` não inteiro; **`periodo < 0` (Q10: erro no A; esporádica só no B)**; `prazo < 0`; número não inteiro ou > 10⁹; campos obrigatórios ausentes; nenhuma tarefa simulável (todas ignoradas ou nenhuma).

**Avisos:** tarefa aperiódica ignorada (D7); cor repetida ou preta (a tarefa recebe outra cor, D17); `prazo > periodo`; `duracao > periodo`; `lista_eventos` não vazia (guardada); campos opcionais ausentes (padrão aplicado). IDs com prefixo de servidor **não** geram aviso nem tratamento especial no A (Q12).

**Tabela de mensagens** (ideia do Arthur, mantida): cada mensagem tem um código (`MsgCode`) ligado à severidade (erro/aviso) e a um modelo de texto com `{}` (`MESSAGES`); `addMessage` monta o texto. Assim a severidade de cada caso fica num único lugar, fácil de mudar se o professor responder diferente. Todos os campos de uma linha são validados, para o usuário ver **todos** os erros de uma vez.

Inteiros aceitam sinal `+` (`+5`). **Robustez:** nunca lançar exceção para o chamador (`parseConfig` protege a leitura com `try/catch` e converte uma falha interna no erro `InternalError`); arquivo vazio, binário, linha gigante e 100 000 tarefas devem retornar resultado sensato. Todo arquivo que obedeça ao formato **deve** carregar (A3.3.7).

---

## 9. Histórico, navegação e edição (A1.5.x, A3.1, A3.4)

```cpp
class Simulator {
public:
    Simulator();                                    // history com 1 snapshot vazio (o registro de escalonadores é automático, §7)
    bool load(const Config&);                       // zera e gera history[0]
    void reset();
    bool stepForward();                             // cursor++ se já existe; senão calcula; false se terminou
    bool stepBack();                                // cursor-- (O(1)); false no instante 0
    bool jumpTo(Tick tick);
    RunSummary runToEnd(Tick maxTicks = DEFAULT_MAX_TICKS);   // A1.5.3; para por limite de ticks/memória, com mensagem
    // RunSummary { bool finished; Tick ticks; std::string message; } — DEFAULT_MAX_TICKS = 2 000 000
    EditResult applyEdit(const Edit&);              // valida; se ok, trunca o futuro e recalcula (D15, D28)
    bool isFinished() const;                        // finished(current())
    const SystemSnapshot& current() const;          // history[cursor]
    const std::vector<SystemSnapshot>& history() const;
    size_t cursor() const;
};
```

- O relógio exibido é sempre `current().tick` (retroceder/avançar altera o relógio global — A1.5.2).
- **Memória:** ~10 tarefas × 1 000 ticks ≈ poucos MB; 100 × 10 000 pode passar de 200 MB. `runToEnd` estima o uso (`sizeof(Task)·n·ticks`) e para com mensagem clara acima de ~1 GiB ou `DEFAULT_MAX_TICKS = 2 000 000`. Otimização opcional na Fase 7: snapshots-chave a cada 64 ticks + re-simulação (possível por causa do determinismo).
- **Mecânica da edição em `k`:** `s = history[k-1]` → `executeTick` → `beginInstant` → **validar e aplicar a mutação** → `scheduleAndDispatch` → acrescentar `ManualEdit` → `history.resize(k)`; `push(s)`. Para `k == 0`: partir de `initState`. A validação ocorre numa cópia; se recusada, **nada muda** e `EditResult.message` explica o motivo.

| Edição | Regra de validação (mensagem de recusa explica o motivo) |
|---|---|
| `SetRemaining` | `1 ≤ valor ≤ duration` da ocorrência corrente; exige job pendente. Para concluir, usar `ForceFinish` |
| `Suspend` | Só `Ready`, `Running` ou `Dormant`. `NotArrived`/`Terminated`: recusa. Libera CPU; `blockReason="manual"` |
| `Resume` | Só `Suspended`; volta a `Ready` (tem job) ou `Dormant` |
| `ForceFinish` / `Remove` | Qualquer estado não terminal; vira `Terminated` (índices nunca são removidos). `Terminated` não volta (recusa) |
| `ActivateNow` | Só `Dormant`; cria job em `tick` |
| `SetArrival` | Só `NotArrived`; novo valor ≥ `tick` |
| `SetDuration`/`SetPeriod`/`SetDeadline` | Inteiros > 0; valem para ocorrências futuras; no prazo, o da ocorrência corrente é recalculado |
| `SetColor` | Hex válido; aviso se repetida ou preta |
| `SetAlgorithm` | Qualquer do registro; vale a partir do instante atual |
| `SetCpuCount` | `1..MAX_CPUS`; ao reduzir, quem estava nas CPUs removidas volta a `Ready` com `Preemption` |
| `SetQuantum`/`SetSeed` | Inteiros válidos |
| `AddTask` | ID único; `arrival ≥ tick`; mesmas validações do parser; entra no fim de `tasks` |
| Estado `Running`/`Ready` direto | **Sempre recusado**: "o estado Executando é decidido pelo escalonador; suspenda a tarefa ou altere parâmetros" |

Estado `Running` na GUI aparece como somente leitura; os botões oferecidos são Suspender, Retomar, Forçar término, Ativar agora, Editar parâmetros.

---

## 10. Visualização (A2.x, RG3)

**Consultar a seção 6.4 do livro do Maziero** e imitar as convenções (A2.5). O que o enunciado já fixa:

**Layout.** Eixo X = tempo (rótulo a cada tick; a cada *n* quando o zoom é pequeno). **Painel superior: uma linha por tarefa**, ID decrescente para cima (**menor ID colado no eixo X**, ordem natural), com amostra de cor + ID. **Painel inferior: uma linha por CPU** (CPU1 no topo), mesma escala, mostrando a tarefa que ocupou a CPU e os períodos desligados. Tarefas `ignored` não têm linha (aparecem cinzas na tabela). Legenda sempre visível (inclusive nas imagens). Título com algoritmo, nº de CPUs, semente e tick final.

**Células (tarefa × tick):** `Running` = sólido na cor + texto `CPUk` (cor do texto por luminância); `Ready` = só contorno; `Suspended` = preto pontilhado; `Dormant`/`NotArrived` = só linha-base. CPU desligada = hachura cinza diagonal.

**Ícones** (formas desenhadas com `QPainterPath`, sem fontes/emoji) — cada tipo distinto, todos na legenda:

| `OccurrenceType` | Ícone | Posição |
|---|---|---|
| `Arrival` | seta ↑ verde | `x=k`, linha da tarefa (a **cada ativação**, Q5) |
| `Termination` | seta ↓ azul | `x=k`, linha da tarefa (**cada ocorrência** concluída, Q5) |
| `TaskFinished` | seta ↓ azul com barra | idem (10ª ocorrência) |
| `DeadlineMiss` | X vermelho + linha tracejada | `x=k` (prazo absoluto) |
| `Preemption` | triângulo laranja | `x=k`, onde a tarefa saiu da CPU |
| `Migration` | seta dupla roxa | `x=k` |
| `TieBreakDraw` | dado (quadrado com pontos) | `x=k`, tarefa sorteada que (re)começa (A4.3) |
| `CpuOff` / `CpuOn` | tomada cortada / ligada | painel de CPUs |
| `ManualEdit` | losango magenta | `x=k` |
| `Dispatch` | (sem ícone; só log) | — |

**Interação.** Zoom 4–64 px/tick; rolagem H/V; tooltip (tarefa, CPU, tick, ocorrência, prazo); no passo a passo, redesenha a cada passo e rola até o tick atual; no modo completo mostra só o resultado final (A1.5.3). Painel textual com intervalos desligados por CPU e totais (ticks e %) (A1.2).

**Renderizador único:**

```cpp
struct GanttOptions { int tickWidth = 24, rowHeight = 28; bool showCpuPanel = true, showLegend = true;
                      Tick fromTick = 0, toTick = -1; QString title; };
class GanttRenderer {
public:
    static QSize contentSize(const std::vector<SystemSnapshot>&, size_t upto, const GanttOptions&);
    static void  paint(QPainter&, const QRectF& target, const std::vector<SystemSnapshot>&, size_t upto, const GanttOptions&);
    static bool  exportPng(const QString& path, const std::vector<SystemSnapshot>&, const GanttOptions&, QString* err);
    static bool  exportSvg(const QString& path, const std::vector<SystemSnapshot>&, const GanttOptions&, QString* err);
};
```

`paint` desenha só a faixa visível (rápido com 100 000 ticks). **Exportação (A2.4):** gera automaticamente **PNG + SVG da simulação inteira** (tarefas + CPUs + legenda + título) ao fim da simulação, nos dois modos; também há "Exportar imagem" no menu a qualquer momento. SVG é sempre completo (sem teto). PNG: `tickWidth` parte de 24 e diminui até 4 para caber em ~30 000 px de largura; se ainda não couber, divide em `parte_N.png`. Nome: `gantt_<config>_<aaaammdd-hhmmss>.png/.svg`, na pasta escolhida (padrão: a do arquivo de configuração; se sem permissão, `Documentos`). **Nunca** capturar a janela.

---

## 11. Interface (telas) e PDF de entrega

Numerar cada elemento nos comentários: `// [TELA 1 / E1.3] origem: history[cursor].tick`. Manter uma tabela viva Tela → Elemento → Origem da informação; ela vira o `telas_projeto_a.pdf` (regra de entrega).

| Tela | Conteúdo |
|---|---|
| **1. Principal** | Menu (Arquivo: abrir, salvar configuração, exportar imagem, sair; Simulação; Ajuda). Barra: abrir, reiniciar, retroceder, avançar, play/pausa, executar até o fim, ir para tick N, velocidade, modo (passo a passo / completo). Centro: Gantt de tarefas + CPUs + legenda. Dock direito: estado do sistema (relógio, algoritmo, quantum, semente, CPUs ligadas/desligadas e o que cada uma executa, filas Prontos/Dormentes/Suspensos/Novos/Terminados, utilização), **tabela de TCBs** (todas as colunas), detalhe da tarefa selecionada com botões de edição. Dock inferior: log de ocorrências, avisos e erros |
| **2. Resultado do carregamento** | Erros fatais ou avisos (linha, campo, motivo), tarefas carregadas e ignoradas |
| **3. Editar tarefa** | Campos da tabela do §9, ações (suspender, retomar, forçar término, ativar agora); recusa mostra o motivo |
| **4. Nova tarefa** | Mesmos campos, com padrões |
| **5. Configurações do sistema** | Algoritmo, quantum, CPUs, semente, zoom, pasta de exportação |
| **6. Fim da simulação** | Resumo (ticks, perdas de prazo, tempo desligado por CPU) e caminhos das imagens geradas |

Requisitos de usabilidade (A5): botões habilitados só quando fazem sentido; toda recusa e todo erro em português claro com o motivo; nunca travar a janela (limite em `runToEnd`).

---

## 12. Convenções de código

- Cabeçalho de **cada arquivo**: propósito, requisitos atendidos (`A1.2`, `RG6`…), decisões usadas (`D10`…).
- Cada função/struct: **o quê e o porquê** (RG6). Comentar decisões não óbvias com a justificativa técnica.
- `-Wall -Wextra -Wpedantic` sem warnings; sem `new/delete` crus; sem globais mutáveis; sem Qt em `Model/Parser/Scheduler/Simulator`; só STL.
- Perguntas abertas: `// PERGUNTA EM ABERTO Q-xx: <padrão adotado e onde trocar>`.
- Nomes: tipos `PascalCase`; funções e variáveis `camelCase`; constantes `UPPER_SNAKE`.
- Um commit por passo lógico, mensagem no imperativo, ex.: `Parser: valida cor RRGGBB com mensagem por linha`.

---

## 13. Estratégia de testes

Resumo; o detalhe (cenários, entradas e saídas esperadas) está em `testes.md`: cenários dourados **G1–G6** calculados à mão, casos do parser **P1–P14**, invariantes verificadas a cada tick em fuzz (conservação de trabalho: nunca há tarefa `Ready` com CPU desligada), determinismo, ida e volta do histórico, teste de extensibilidade (escalonador FIFO) e checklist manual da GUI.

---

## 14. Ganchos já previstos para o Projeto B

- `TaskKind`/`ServerType` já existem no modelo; no A o prefixo `BS/PS/DS/SS` não é interpretado (Q12) e `ignored` (aperiódicas) é só uma flag. No B: reconhecer o prefixo no parser e `periodo < 0` como esporádica.
- `Task.rawEvents` guarda `lista_eventos`; o B acrescenta `std::vector<TaskEvent>` sem limite de tamanho.
- `Suspended` com `blockReason` e `effPriority` separada de `basePriority` ⇒ mutex, herança/teto de prioridade, inversão de prioridades, E/S, troca de dados.
- Fila global de aperiódicas acessada pelos servidores; servidor = `Task` periódica com `serverBudget` (reposição no B). O escalonador recebe o `SystemSnapshot` inteiro, então campos novos não quebram RM/EDF.
- Renderizador com mapa **estado → estilo** (pen/brush/padrão) para os preenchimentos de servidor e aperiódica (B4). Tipo de ocorrência desconhecido ⇒ ícone genérico + entrada na legenda.
- Teste de escalonabilidade (B3): função pura `admit(snapshot, task)` que aceita ou recusa com motivo; o painel de utilização do A é a base.
- A API de edição (`AddTask`) já serve para criar aperiódicas/esporádicas durante a execução.
- Orçamento de arquivos: o A usa 18 de 20; planejar fusões no B (Q-A1).

---

## 15. Defesa: perguntas prováveis (todos devem saber responder)

1. **Por que fila única global?** A1.2 exige; evita ociosidade; custo: migração entre CPUs, mitigado por afinidade (D10).
2. **Por que `Dormant` separada de `Suspended`?** Semântica distinta (espera de período × impedimento); Q3; D1.
3. **E se o período chega antes de a ocorrência terminar?** Enfileira (backlog); a tarefa continua e a perda de prazo é marcada (D5, D9).
4. **Como o sorteio fica reproduzível ao retroceder?** Hash sem estado de `(semente, tick, índice)` (D14).
5. **Como incluir um escalonador novo sem tocar no motor?** Função + registro (A4.2); mostrar o teste FIFO.
6. **Como funciona retroceder e qual o custo?** Snapshot por tick; retroceder é mover o cursor; memória estimada e limitada (D16).
7. **Por que `MAX_CPUS = 64`?** D12.
8. **Por que o escalonador roda a cada tick? Complexidade?** Preemptivo; O(M·n) por tick, irrelevante em simulação.
9. **Onde está o TCB e o que guarda?** `Task` em `Model.h` (A1.3).
10. **Como sabem que RM/EDF estão corretos?** Cenários G1/G2 calculados à mão + invariantes em fuzz.
11. **Como o quantum entraria num algoritmo por fatia de tempo?** `CPU.quantumUsed` + `SystemSnapshot.quantum`, lidos pela função do escalonador (D13).
12. **Por que Qt e como garantem o standalone?** D26 + resposta do professor; mostrar a pasta `dist/` rodando sem Qt instalado.
13. **Por que o Gantt exportado não é print?** O mesmo `GanttRenderer::paint` desenha em `QImage`/`QSvgGenerator`.
14. **O que acontece com arquivo inválido? E com arquivo válido incomum?** D18; testes P1–P14.

Roteiro dos 10 minutos: 0–2 compilar ao vivo (`cmake -S . -B build`, `cmake --build build`) e abrir; 2–3 carregar config de pasta qualquer + arquivo inválido; 3–5 passo a passo, retroceder, edição válida e edição recusada; 5–6 G2 (RM com perda de prazo × EDF sem perda); 6–7 multi-CPU, CPU desligada, sorteio; 7–8 modo completo + imagem exportada; 8–10 código e perguntas. Ter tudo pré-configurado; levar `dist/` pré-compilado como plano B.

---

## 16. Questões em aberto

Ver `enunciado.md` §5 (Q-A1 … Q-B3). Cada uma tem um **padrão adotado**; implementar o padrão em um único ponto de decisão e comentar `// PERGUNTA EM ABERTO Q-xx`.
