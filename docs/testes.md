# testes.md — Contrato de testes (o que "pronto" significa)

> **Para o Claude Code:** implemente estes testes em `simulador/tests.cpp` (executável `sim_tests`, sem Qt, sem GoogleTest). Os resultados dos cenários dourados foram **calculados à mão** a partir das definições de RM/EDF e da semântica de `design.md` §6. Se o seu motor discordar de um cenário, **não ajuste o teste para passar**: reexamine o cálculo, a semântica e avise o usuário com o tick exato da divergência.
> Notação: `tick:id` = tarefa que executa em `[tick, tick+1)` naquela CPU; `-` = CPU desligada. Ocorrências usam `x=k` conforme D3 (`Arrival`/`DeadlineMiss`/`Preemption`/`TieBreakDraw` em `x=k`; `Termination` em `x=k` no snapshot do instante em que a ocorrência conclui).

---

## 1. Harness

```cpp
// Macro mínima: não aborta no primeiro erro; conta falhas; main retorna != 0 se houver falha.
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fails; std::cerr << __FILE__ << ":" << __LINE__ << "  CHECK falhou: " #cond "\n"; } } while (0)
#define CHECK_EQ(a,b) CHECK((a) == (b))
```

- `tests.cpp` também contém `printGrid(const std::vector<SystemSnapshot>&, int maxTicks)`: imprime uma linha por tick (`tick | CPU1 CPU2 ...` com o ID da tarefa ou `-`) e a lista de ocorrências do instante. Use-a para depurar e para a Fase 3 (sem GUI).
- `runConfig(text)`: faz `parseConfig` → `Simulator::load` → `runToEnd` e devolve o histórico. As configs de teste são **strings embutidas** no arquivo (não criar arquivos extras: orçamento de 18).
- Helpers: `taskOn(history, tick, cpu)` (ID ou "-"), `countOcc(history, type, taskId)`, `occAt(history, tick, type)`.
- Rodar: `cmake --build build --target sim_tests && ./build/sim_tests` (a cada commit). Saída final: `N checks, M falhas`.

---

## 2. Cenários dourados (G1–G6)

Todas as tarefas: `ingresso`, `duracao`, `periodo`, `prazo` conforme a string. Quantum = 2 (ignorado). Semente = 1.

### G1 — RM, 1 CPU, folgado (também valida CPU desligada)
```
RM;2;1
1;FF0000;0;1;4;4
2;0000FF;0;2;6;6
```
Grade dos 12 primeiros ticks (CPU1): `0:1  1:2  2:2  3:-  4:1  5:-  6:2  7:2  8:1  9:-  10:-  11:-`
- Nenhum `DeadlineMiss` em toda a simulação. Nenhuma `Preemption`.
- Ocorrências: `Arrival` T1 em 0,4,8,12…; `Arrival` T2 em 0,6,12…; `Termination` T1 em 1,5,9; T2 em 3,8.
- CPU1: `CpuOff` em 3, `CpuOn` em 4, `CpuOff` em 5, `CpuOn` em 6, `CpuOff` em 9. `ticksOff` de CPU1 em [0,12) = 5 (ticks 3, 5, 9, 10 e 11).
- Cada tarefa tem exatamente 10 `Arrival`, 10 `Termination` e 1 `TaskFinished`.
- Utilização informativa: `U = 1/4 + 2/6 ≈ 0,583`.

### G2 — RM × EDF, 1 CPU, utilização alta (perda de prazo no RM)
Tarefas: `1;FF0000;0;2;5;5` e `2;0000FF;0;4;7;7` (U ≈ 0,971).

**RM** (`RM;2;1`) — primeiros 14 ticks: `0:1 1:1 2:2 3:2 4:2 5:1 6:1 7:2 8:2 9:2 10:1 11:1 12:2 13:2`
- T1 preempta T2 no instante 5: `Preemption` de T2 em 5.
- **`DeadlineMiss` de T2 em 7** (prazo absoluto 7, `remaining = 1`); `Termination` de T2 (1ª ocorrência) em **8**.
- `Arrival` de T2 em 7 com nota "enfileirada" (a ocorrência 1 ainda pendente); a ocorrência 2 só começa em 8.
- Nenhuma perda de prazo de T1 nos 14 primeiros ticks. T2 termina a 2ª ocorrência em 14 (prazo 14 ⇒ **não** perde).

**EDF** (`EDF;2;1`) — primeiros 14 ticks: `0:1 1:1 2:2 3:2 4:2 5:2 6:1 7:1 8:2 9:2 10:2 11:2 12:1 13:1`
- **Nenhuma** `DeadlineMiss` nos 14 primeiros ticks e **nenhuma** `Preemption` (em 5, T2 com prazo 7 é mais urgente que T1 com prazo 10; em 7 e 10 a tarefa em execução tem o menor prazo).
- `Termination` de T2 (1ª) em 6; de T1 (2ª) em 8; de T2 (2ª) em 12.

### G3 — EDF, 2 CPUs, empate total e sorteio determinístico
```
EDF;2;2
1;FF0000;0;2;4;4
2;00FF00;0;2;4;4
3;0000FF;0;2;4;4
```
- Instante 0: exatamente **2** tarefas `Running` (uma em cada CPU) e exatamente **2** `TieBreakDraw` (uma por tarefa escolhida). Quais duas depende de `tieHash`; o teste verifica só a estrutura.
- Ticks 2 e 3: só a terceira tarefa executa, na **CPU1** (menor índice livre; ambas ficam livres em 2); **CPU2 desligada** (`CpuOff` em 2). Sem `TieBreakDraw` em 2 (um único candidato).
- Em 4 todas ativam de novo: novo sorteio (2 ícones). O padrão se repete a cada 4 ticks. `ticksOff` de CPU2 em [0,8) = 4.
- **Determinismo:** mesma semente ⇒ histórico idêntico em duas execuções. Com semente 2 as invariantes estruturais acima continuam valendo.

### G4 — Desempate (1): quem está executando continua
```
EDF;2;1
1;FF0000;0;3;10;10
2;0000FF;1;3;10;9
```
T1 tem prazo absoluto 10; T2 chega em 1 com prazo relativo 9 ⇒ absoluto 10. Empate de chave no instante 1.
- Grade: `0:1 1:1 2:1 3:2 4:2 5:2` e CPU desligada em seguida (até o próximo período).
- **Nenhuma** `Preemption` em 1 e **nenhum** `TieBreakDraw` (critério 1 resolveu). T2 só começa em 3.

### G5 — Edição manual (suspender/retomar) sobre G1
Procedimento: carregar G1; `stepForward` até `cursor == 4`; `applyEdit(Suspend, T1)`; `stepForward` até `cursor == 9`; `applyEdit(Resume, T1)`; `stepForward` até o instante 12.
- Após a 1ª edição: `history.size() == 5`; `snapshot[4].tasks[T1].state == Suspended`; `ManualEdit` em 4; CPU desligada no tick 4.
- Grade resultante dos ticks 4–11: `4:-  5:-  6:2  7:2  8:-  9:1  10:1  11:-`.
- `DeadlineMiss` de T1 em **8** (ocorrência 2: ativada em 4, prazo absoluto 8); `Arrival` de T1 em 8 (ocorrência 3 enfileirada, T1 ainda suspensa).
- Após `Resume` em 9: T1 executa a ocorrência 2 em `[9,10)` (conclui em 10, já com prazo perdido) e a ocorrência 3 em `[10,11)` (prazo absoluto 12 ⇒ não perde). T2 não é afetada.
- **Recusas** (`ok == false`, `message` não vazia e em português): `Suspend` em tarefa `Terminated`; `SetArrival` em tarefa já ingressada; `SetRemaining` com valor 0 ou > duração; `SetCpuCount` com 0 ou `MAX_CPUS+1`; `AddTask` com ID duplicado. Em toda recusa, `history` e `cursor` ficam **inalterados**.

### G6 — Retroceder/avançar e edição no passado
- Avançar 30, voltar 30, avançar 30 ⇒ cada `history[k]` é **idêntico** ao de uma execução direta (comparar via serialização textual do `SystemSnapshot`).
- `jumpTo(k)` para trás e para frente mantém `current().tick == k`.
- Voltar a `k = 10`, aplicar uma edição válida ⇒ `history.size() == 11` (futuro descartado) e a nova linha do tempo é determinística (rodar duas vezes dá o mesmo).

---

## 3. Parser (P1–P14)

Cada caso passa a **string** a `parseConfig` e verifica `errors`/`warnings`/campos. Usar `\n` e `\r\n` conforme indicado. Nenhum caso pode lançar exceção.

| Cód. | Entrada (resumo) | Esperado |
|---|---|---|
| P1 | Caso feliz: `RM;2;2` + 3 tarefas periódicas válidas | `ok()`, 3 tarefas, algoritmo `RM`, quantum 2, 2 CPUs, cores normalizadas em maiúsculas |
| P2 | Mesmo arquivo com `\r\n` | Idêntico ao P1 |
| P3 | Mesmo arquivo com BOM UTF-8 no início | Idêntico ao P1 |
| P4 | Linhas com `;` final e sem `;` final misturadas (inclusive na linha do sistema) | Todas aceitas, mesmo resultado |
| P5 | `rM;2;1` / `edf;1;1`; cor `f0e0d0`; ID `a1` e `A1` na mesma tarefa | Algoritmo normalizado (`RM`, `EDF`); cor `F0E0D0`; (e `a1`/`A1` em tarefas diferentes ⇒ erro de duplicado, sem diferenciar caixa) |
| P6 | Linhas em branco, linhas só com espaços, espaços ao redor dos campos e de `;` | Ignoradas/aparadas; **nenhum erro** (A3.3.6) |
| P7 | Só 1 linha útil (apenas o sistema) | Erro: "o arquivo precisa ter ao menos 2 linhas…" |
| P8 | Algoritmo `XYZ` | Erro com a linha e a lista de algoritmos disponíveis |
| P9 | `qtde_cpus` = `0`, `65` (`MAX_CPUS+1`), `abc`, `1.5` | Erro em cada caso; `1` e `64` válidos |
| P10 | ID duplicado (inclusive `a1` × `A1`) | Erro citando as duas linhas |
| P11 | Cor `GG0000`, `FFF`, `1234567`; cor repetida; cor `000000`; cor `FFFFFF` | Erro "esperado RRGGBB" nos três primeiros; `#FF0000` tolerado; cor vazia ⇒ paleta automática (sem erro); repetida, quase preta ou quase branca ⇒ **aviso** e a tarefa recebe outra cor livre (D17) |
| P12 | Número inválido em cada campo (`ingresso=-1`, `duracao=0`, `periodo=x`, `periodo=-5`, `prazo=-2`, `duracao=1.5`) | Erro apontando **linha, campo e valor**; `periodo=-5` é erro no A (Q10) |
| P13 | `periodo=0` (aperiódica), ID `BS01` com período > 0, + 1 tarefa periódica válida | `ok()`; a aperiódica fica `ignored` com `ignoredReason` e **aviso** claro (D7); `BS01` carrega como **periódica comum**, sem aviso (Q12); a válida carrega |
| P14 | Ida e volta: `parseConfig` → `serializeConfig` → `parseConfig` | Configs iguais; `lista_eventos` com `;` internos preservada; prazo vazio ⇒ preenchido com o período |

Casos extras de robustez: arquivo vazio; só espaços; 100 000 tarefas (tempo razoável); linha de 1 MB; bytes binários; nenhuma tarefa simulável (todas ignoradas) ⇒ erro claro.

---

## 4. Invariantes (verificadas em **cada** snapshot, no fuzz e nos cenários)

1. Nº de tarefas `Running` ≤ nº de CPUs; cada `Running` está em **exatamente uma** CPU e cada CPU tem no máximo uma tarefa; `task.cpu == c ⇔ cpus[c].taskIndex == task` (espelho consistente).
2. **Conservação de trabalho (A1.2):** nunca existe tarefa `Ready` enquanto há CPU com `on == false`; CPU `on == false` ⇔ `taskIndex == -1`.
3. `jobs.front().remaining` nunca é negativo; só tarefas `Running` consomem tempo.
4. **Propriedade de prioridade (RM/EDF):** o conjunto `Running` é o top-M por chave entre os candidatos; nenhuma `Ready` tem chave **estritamente** melhor que alguma `Running`.
5. `tick` cresce de 1 em 1; `history[k].tick == k`.
6. Ao final: cada tarefa simulada tem `released == completed == 10`, 10 `Arrival`, 10 `Termination`, 1 `TaskFinished`, e `runTicks == 10 · duration`.
7. `Σ (ticksBusy + ticksOff)` de cada CPU = nº de ticks executados.
8. Uma ocorrência com `missed == true` teve `DeadlineMiss` registrado **uma única vez**.
9. **Determinismo:** duas execuções da mesma configuração/semente produzem históricos idênticos.
10. **Ida e volta:** `stepBack`/`stepForward`/`jumpTo` nunca alteram o conteúdo de `history[k]`.

## 5. Fuzz

Gerador com semente fixa (reproduzível): 10 000 sistemas com 1–12 tarefas, `periodo ∈ [2,20]`, `duracao ∈ [1,periodo]`, `ingresso ∈ [0,10]`, `prazo ∈ [1, 2·periodo]`, 1–4 CPUs, RM ou EDF, semente aleatória. Rodar até o fim verificando as invariantes de §4 a cada snapshot. Se uma falhar, imprimir a configuração mínima (string) que reproduz.

## 6. Extensibilidade (A4.2)

`tests.cpp` registra um escalonador `FIFO` (menor `jobs.front().release` primeiro) via `registerScheduler`, roda uma simulação com `FIFO;2;1` e confere que (a) o parser o aceita, (b) o motor executa **sem nenhuma alteração** no código de `Simulator`.

## 7. Checklist manual da GUI (Fase 6/7)

- [ ] Abrir config de pendrive, de outra partição, de pasta com espaço e acento, com nome arbitrário (A3.3.4).
- [ ] Arquivo inválido: tela de erros com linha/campo/motivo; a janela não trava; nada é carregado.
- [ ] Arquivo com tarefas aperiódicas: aviso claro de "ignoradas" (A4.4).
- [ ] Passo a passo: avançar, retroceder, ir para tick N; o relógio acompanha (A1.5.2); o Gantt atualiza a cada passo (A2.3).
- [ ] Edição válida (suspender/retomar/alterar prazo) e edição recusada com motivo (A3.4).
- [ ] Modo completo mostra só o resultado final (A1.5.3); ao fim gera PNG + SVG da simulação inteira (A2.4); abrir o PNG e conferir que **não** é print da janela.
- [ ] Ordem do eixo Y: menor ID colado no eixo X (A2.5); cores do arquivo; ausência de cor = pronta; preto pontilhado = suspensa (A2.1).
- [ ] Legenda completa e todos os ícones distintos (RG3); CPU de cada execução visível (`CPUk`); CPUs desligadas visíveis e somadas no painel (A1.2).
- [ ] 1 CPU e 64 CPUs; 200 tarefas; simulação de 100 000 ticks exporta sem travar; zoom e rolagem fluidos.
- [ ] Fechar a janela durante o "play" não trava nem vaza.
- [ ] **Standalone (RG5):** gerar `dist/`, copiar para um caminho novo, rodar com `PATH` mínimo (Windows: `set PATH=C:\Windows\System32` e executar o `.exe`) e, idealmente, em VM/Windows Sandbox sem Qt instalado.

## 8. Qualidade de build

- Compila do zero em pasta limpa seguindo só os comentários do `CMakeLists.txt`, com `-Wall -Wextra -Wpedantic` **sem warnings**.
- `sim_tests` não depende de Qt (conferir: compila com `-DBUILD_GUI=OFF`).
- Contagem de arquivos de `simulador/` ≤ 18; nenhum `.zip`; nenhum caminho absoluto no código.
