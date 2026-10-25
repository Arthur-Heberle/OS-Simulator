# CLAUDE.md — Simulador de Escalonamento (Sistemas Operacionais, UTFPR)

Este arquivo dá ao Claude Code o contexto completo do projeto: o que já foi decidido, **por quê**, o que ainda está pendente e como trabalhar comigo.

---

## 1. Quem sou eu e como quero trabalhar

- Sou o Arthur, estudante de Engenharia de Computação (6º período). Este é um trabalho em grupo da disciplina de Sistemas Operacionais.
- **Meu objetivo principal é APRENDER**, não terminar rápido. Prefiro entender cada decisão a receber código pronto.

### Regras de trabalho
1. **Antes de escrever código, explique o que vai fazer e por quê, e espere eu concordar.**
2. Trabalhe em passos pequenos: no máximo uma função (ou ~40 linhas) por vez, depois pare.
3. Quando eu pedir **"dica"**, dê dica ou pseudocódigo, não a solução pronta.
4. Quando eu pedir **"revisão"**, aponte problemas e explique; não reescreva meu código sem eu pedir.
5. Não crie, renomeie ou apague arquivos sem eu pedir. Não instale nada.
6. Não marque itens nos checklists; diga quais itens o trabalho atende e eu marco.
7. Depois de alterar código, **compile e teste** antes de dizer que está pronto. Nunca afirme que algo funciona sem ter verificado.
8. **Commits:** a cada alteração que estiver correta (compilada e testada), faça um **commit simples** só dela, com mensagem curta em inglês. Commits **sem** `Co-Authored-By`, sem assinatura e sem menção ao Claude: o autor é só o Arthur. Não use `--no-verify` e não faça `push` sem eu pedir.

### Como decidir e responder (postura de analista)
- **Não suponha nada.** Se algo for ambíguo (no meu pedido, no código ou no enunciado), pergunte antes de agir. Uma pergunta objetiva por vez.
- **Sempre dê sua opinião e recomendação**, com o motivo. Quando houver mais de uma opção válida, mostre as opções com prós e contras e diga qual você escolheria.
- **Se achar que estou errado, diga claramente por quê e ofereça uma alternativa.** Não concorde só para agradar.
- **Justifique com o enunciado**: cite o código do requisito (ex.: `[A4.3]`, `[RG5]`). A fonte da verdade é o PDF `simulador-escalonamento-v20261015.pdf` na raiz do repositório.
- Se perceber que uma resposta sua anterior estava errada ou contradiz outra, admita e corrija explicitamente.
- Prefira a solução **mais simples e legível** que atende os requisitos. Evite recursos avançados de C++ quando um `for` resolve (o código será lido pelo professor e pelo grupo).
- Respostas em **português**.

---

## 2. O projeto

Simulador de um SO multitarefa preemptivo de tempo compartilhado, com várias CPUs, escalonamento de tempo real (RM e EDF), visualização em gráfico de Gantt, execução passo a passo com avançar/voltar e edição de tarefas.

- **Projeto A** (atual): só tarefas periódicas, algoritmos RM e EDF.
- **Projeto B** (depois): tarefas aperiódicas/esporádicas e servidores (BS, PS, DS, SS). **Não implementar nada do B agora**, mas não tomar decisões que o impeçam.

### Estrutura do repositório
```
OS-Simulator/
├── CLAUDE.md
├── simulador-escalonamento-v20261015.pdf   ← enunciado (fonte da verdade)
├── Project-A/
│   ├── Makefile
│   ├── include/core/   CPU.h  Parser.h  Scheduler.h  Simulator.h  SystemSnapshot.h  Task.h
│   ├── include/ui/     (vazio por enquanto)
│   └── src/            main.cpp  core/*.cpp  ui/ (vazio)
└── Project-B/
```

---

## 3. Decisões tomadas (e o motivo de cada uma)

### Linguagem e convenções
- **C++17.** Nomes de código em **inglês**. Comentários em **inglês simples**, explicando o quê e o porquê `[RG6]`.
- `src/core/` usa **só a biblioteca padrão** (nada de Qt/ImGui). Motivo: testável no console e reaproveitável no Projeto B.
- Prefira **funções livres** a classes. Regra usada no projeto:
  - **struct** = só dados (`Task`, `CPU`, `SystemSnapshot`, `Occurrence`, `Config`, `ParseResult`);
  - **função** = saída depende só da entrada (parser, algoritmos de escalonamento);
  - **classe** = estado que precisa ficar coerente entre chamadas → **só o `Simulator`**.
- Structs sem construtores nem destrutores ("regra do zero"); **todo campo com valor padrão** (`int x = 0;`) para nunca haver lixo de memória.
- Dentro do estado, use **índices** (`int`) em vez de ponteiros: cópias do snapshot precisam ser independentes.
- `Task.id` é `std::string` (o Projeto B usa IDs como `PS01`). Tarefas ficam em `std::vector<Task>`; busca por ID é linear (poucas tarefas). Não usar `map` para as tarefas.

### Separação núcleo × interface
- O core recebe **dados** e devolve **resultados**. Quem lida com disco e tela é a interface (ou o `main.cpp` no console).
- O **parser recebe o TEXTO**, não o caminho do arquivo. Quem abre o arquivo (UI ou `main.cpp`) trata o erro de abertura.
  Motivo: no Windows `std::ifstream` pode falhar com caminhos acentuados; na UI Qt, o `QFile` resolve. `[A3.3.4]`

### Parser (`namespace parser`, função `parseConfig(text)` → `ParseResult`)
- Formato **posicional**: o significado do campo vem da posição, nunca do conteúdo. Campos vazios são mantidos (`splitFields`); posição inexistente = campo vazio (`fieldAt`).
- Conta **todas** as linhas (inclusive em branco) para as mensagens apontarem a linha real (começa em 1).
- Regras de texto: trim, ignora linhas vazias `[A3.3.6]`, `;` final opcional `[A3.3.3]`, comparação sem diferenciar maiúsculas `[A3.3.2]`. Algoritmo guardado em **MAIÚSCULAS** (`"RM"`, `"EDF"`), igual às chaves do registro do scheduler.
- **Erro** = não há padrão possível → bloqueia a simulação. **Aviso** = usa padrão ou ignora, a simulação roda.
  - Linha do sistema: tudo é aviso (algoritmo inexistente, quantum/CPU inválidos ou ausentes) e usa o padrão (`RM`, quantum `2`, `1` CPU).
  - Tarefa: id ausente/repetido, inteiros ausentes/inválidos (`arrival ≥ 0`, `duration ≥ 1`, `period ≥ 0`, `deadline ≥ 1`) → **erro**, a tarefa não entra. `period < 0` é erro por definição `[A3.3]`.
  - Cor inválida/vazia ou **repetida** → **aviso**, a tarefa entra com uma cor livre gerada (`nextFreeColor`), garantindo cores diferentes `[A2.1]`.
  - Tarefa **aperiódica** (`period == 0`) → **aviso**, validada mas **não adicionada** `[A4.4]`.
  - Não existe regra `deadline > arrival + duration`: o prazo é relativo à ativação, e prazo menor que a duração só causa perda de prazo, não erro.
- Mensagens centralizadas em `MESSAGES` (`MsgCode` → severidade + texto com `{}`), montadas por `addMessage`. Todos os campos da linha são validados para o usuário ver **todos** os erros de uma vez.
- `MAX_CPUS = 64` (escolha do grupo para o "N" do `[RG2]`).

### Escalonamento: política × mecanismo
- **Política** (o que muda entre algoritmos) fica no **scheduler**: só decide a **ordem de prioridade**. Não altera o estado, não conhece CPUs, não mexe em filas.
- **Mecanismo** (igual para todos) fica no **Simulator**: liberar ativações, prazos, filas, despachar, executar, quantum, sorteio.
- Motivo: `[A4.2]` exige incluir algoritmo novo sem mexer no código da simulação.
- Scheduler = **função livre** registrada num mapa `schedulerRegistry()` (nome → ponteiro de função `SchedulerFn`). O `Simulator` guarda o ponteiro escolhido no construtor.
- **RM:** menor `period` = maior prioridade (fixa). **EDF:** menor `absoluteDeadline` da ativação atual = maior prioridade (dinâmica).
- Desempate `[A4.3]`, nesta ordem, para todos os algoritmos: (1) estava executando, (2) prazo, (3) menor ingresso, (4) menor duração, (5) sorteio.
- **O sorteio NUNCA fica dentro do comparador do `std::sort`** (comparador aleatório = comportamento indefinido).
- **Sorteio = grupos de empate (decidido):** `SchedulerFn` devolve `PriorityGroups` (`std::vector<std::vector<int>>`, índices de `tasks`, do grupo mais prioritário ao menos). Tarefas do mesmo grupo continuam empatadas depois dos critérios (1) a (4). O scheduler **não sorteia** e não usa o `rng` (fica `const`); o `Simulator::chooseTasks` pega grupo a grupo e **só sorteia quando um grupo empatado não cabe nas CPUs livres**, registrando a occurrence de sorteio.
- **Estrutura do `Scheduler.cpp`:** cada algoritmo é só uma *chave* (`rmKey` = `period`, `edfKey` = `absoluteDeadline`; menor = mais prioritário). `compareTasks(a, b, key)` devolve -1/0/1 (chave + critérios 1 a 4) e serve para ordenar (`< 0`) e agrupar (`== 0`). `orderAndGroup` ordena com `std::stable_sort` e agrupa. Algoritmo novo `[A4.2]` = uma função chave + uma linha em `schedulerRegistry()`.
- **Definição de prazo (professor, `[A3.3]`):** o prazo (deadline) é o tempo máximo para uma ocorrência/execução da tarefa terminar, contado a partir do momento em que a tarefa é ativada. Se a execução ultrapassar o prazo é um erro e deve aparecer no Gantt; a tarefa que perdeu o prazo continua executando e só termina quando o tempo executado for igual ou maior que a duração.

### Simulator (a única classe do core)
- Histórico = `std::vector<SystemSnapshot>` + `currentIndex_`. O estado atual é **sempre** `history_[currentIndex_]` (uma única fonte da verdade; não duplicar tarefas/CPUs como membros).
- `step(bool forward)`: voltar só decrementa o índice; avançar reaproveita o snapshot se existir, senão chama `computeNext` e guarda.
- Editar uma tarefa (futuro, `[A3.4]`): valida, aplica no snapshot atual e **apaga os snapshots futuros**.
- O loop "executar até o fim" fica **fora** do Simulator: modo completo = `while (sim.step(true)) {}`.
- **Convenção de tempo:** `history_[k]` = estado no instante `k`; o tick `t` é o intervalo `[t, t+1)`. Chegadas acontecem no início (`t`), términos no fim (`t+1`).
- Ordem fixa dentro de `computeNext`:
  1. copiar o atual e **limpar `occurrences`**;
  2. `releaseActivations` — ativações com `tick == nextRelease` entram como prontas; registra chegada;
  3. `checkDeadlines` — ativação com `absoluteDeadline == tick` e `remaining > 0` → perda de prazo; **a tarefa continua** `[A3.3]`;
  4. scheduler (ordem) → escolha das que executam → `dispatch` (despachante: quem já está numa CPU fica nela; escolhidas novas vão para CPUs livres; não escolhidas saem; CPU sem tarefa fica desligada `[A1.2]`);
  5. `runOneTick` — cada CPU com tarefa: `remaining--`, `quantumUsed++`; CPU sem tarefa: `ticksOff++`;
  6. `finishActivations` — `remaining == 0` → `activationsDone++`, libera a CPU, registra término; 10 ativações concluídas → `Terminated` `[A4.4]`, senão `Waiting`;
  7. `tick++`.
- O escalonador é chamado **todo tick** (simples e atende `[A1.1]`; o critério (1) evita trocas desnecessárias).
- O "contexto" da tarefa já é a própria `Task` dentro do snapshot; o despachante não precisa "salvar contexto" à parte.
- Estados da tarefa: `New` (antes da 1ª chegada), `Ready`, `Executing`, `Suspended` (só por edição manual no A; preto pontilhado no Gantt), `Waiting` (entre ativações; nada desenhado), `Terminated`.
- **Occurrence** = fato marcante do tick que o Gantt desenha com ícone (chegada, término, perda de prazo, sorteio). Fica no snapshot do tick em que aconteceu (por isso não tem campo `tick`).

### Interface e entrega
- O professor respondeu: **pasta com `.exe` + DLLs é aceita**, desde que não seja preciso instalar Qt (nem nada) para rodar `[RG5]`. Testar sempre em máquina limpa.
- Desenvolvimento atual no **Windows** (não mais no WSL).

---

## 4. Pendências (NÃO decida sozinho — pergunte quando chegar nelas)

1. **Biblioteca da interface:** Qt (favorita depois da resposta do professor; exige CMake com `AUTOMOC`) ou ImGui. Se Qt: kit **MinGW** (o `windeployqt` resolve tudo) ou **MSVC** (copiar `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll` para a pasta; nunca entregar o instalador `vc_redist`). O `Makefile` atual foi feito para WSL e provavelmente será substituído por `CMakeLists.txt`.
2. **Gerador do sorteio:** recomendado `std::minstd_rand` **dentro do `SystemSnapshot`** (8 bytes por cópia; o `std::mt19937` atual ocupa ~2,5 KB por snapshot). Alternativa mínima: `rand()` com `srand(semente fixa)`. Nunca `srand(time(...))`: o mesmo arquivo daria Gantts diferentes.
3. ~~Onde fazer o sorteio~~ → **decidido** (grupos de empate; ver §3, "Escalonamento"). Falta implementar o `Simulator::chooseTasks`.
4. **Quantum no RM/EDF** (pergunta enviada ao professor). Proposta provisória: tarefa que esgotou o quantum perde o critério de desempate (1), gerando rodízio só entre tarefas de mesma prioridade. Manter isolado numa única condição.
5. **"Prazo" no desempate (2):** relativo (campo do arquivo) ou absoluto da ativação? **Provisório: relativo (`task.deadline`)**, numa única linha de `compareTasks` (`Scheduler.cpp`). Motivo: no EDF o absoluto seria redundante (a chave principal já é o prazo absoluto). O texto do professor define o prazo mas não diz qual usar no desempate; trocar quando houver resposta.
6. **Ativação que chega antes da anterior terminar** (após perda de prazo): proposta é enfileirar (a nova começa quando a anterior termina).
7. **Ordem do eixo Y por ID** `[A2.5]`: comparar numericamente quando o ID for número.

---

## 5. Estado atual do código e problemas conhecidos

Encontrados na última revisão (commit "finished parser and editing scheduler..."). Corrigir **comigo**, um de cada vez:

- **`.gitignore` contém `*.md`** → este `CLAUDE.md`, os checklists e o passo a passo não vão para o GitHub. Remover essa linha.
- **`Scheduler.h` / `Scheduler.cpp`:** RM e EDF implementados e testados (12 casos, g++ 13 no WSL, `-Wall -Wextra` sem avisos). Os problemas antigos (índice vs. id, sorteio no comparador, `ta.arrival < ta.arrival`, EDF vazio) foram corrigidos.
- **`Task.h`:** `duration`, `period`, `deadline`, `remaining` sem valor padrão (lixo). Já tem `absoluteDeadline` (adicionado para o EDF). Faltam os campos de execução: `nextRelease`, `activationsDone`, `cpuIndex` (-1 = nenhuma), `missedDeadline`, `rawEvents` (texto bruto da `lista_eventos`, para o Projeto B).
- **`Task.cpp`** ainda existe; com `Task` sendo struct só de dados, pode ser apagado.
- **`SystemSnapshot.h`:** usa `std::mt19937` (ver Pendência 2).
- **`Simulator`:** construtor ainda não cria as CPUs nem inicializa `nextRelease`; `releaseActivations`, `checkDeadlines`, `dispatch`, `finishActivations` são TODO; `runOneTick` implementado. Comentários ainda em português (padronizar para inglês).
- **`Parser.h`:** `AperiodicIgnored` está listado sob o comentário "Errors" no `enum`, mas a severidade no mapa já é `Warning` (correta). Só mover no `enum`.
- **`main.cpp`:** vazio. Próximo uso: teste de console que lê o arquivo (função `readFile` no próprio `main.cpp`), chama `parser::parseConfig`, roda `while (sim.step(true)) {}` e imprime um Gantt em texto.

### Próximos passos sugeridos (em ordem)
1. Corrigir `.gitignore` e `Task.h`.
2. ~~Corrigir `Scheduler.cpp`~~ (feito); decidir a Pendência 2.
3. Implementar as etapas do `Simulator` na ordem do `computeNext`.
4. `main.cpp` de console com Gantt em texto; validar com exemplos calculados à mão.

### Exemplo de validação (1 CPU, prazo = período)
T1: duração 2, período 5. T2: duração 4, período 7.
- **RM:** T1 T1 T2 T2 T2 **T1 T1** T2 … → T2 **perde o prazo em t = 7** e termina em t = 8.
- **EDF:** T1 T1 T2 T2 T2 **T2** T1 T1 … → ninguém perde prazo.
Use este caso para testar os dois algoritmos.
