# planejamento.md — Plano de execução por fases (do estado atual do repositório até a entrega do Projeto A)

> **Para o Claude Code:** cada fase abaixo é um prompt autocontido. Execute **uma fase por vez**, na ordem. Antes de cada fase leia `CLAUDE.md`, `docs/enunciado.md` (o que se exige) e `docs/design.md` (como se decidiu); `docs/testes.md` define "pronto".
> **Nunca avance** com testes falhando. Ao terminar cada fase: rode a verificação, faça o commit indicado e entregue ao usuário um **relatório curto** (o que fez, o que testou, o que ficou em aberto, perguntas).
> **Pare e pergunte** quando: uma decisão de `design.md` parecer errada ou conflitar com o enunciado; faltar uma ferramenta (Qt, CMake, compilador) — **não instale nada sem perguntar**; a fase exigir mais de 18 arquivos em `simulador/`; surgir uma questão em aberto nova (`enunciado.md` §5).

## 0. Como usar (para o usuário)

Cole no Claude Code, uma fase por vez:

> *"Leia CLAUDE.md, docs/enunciado.md, docs/design.md e docs/testes.md. Execute a **Fase N** de docs/planejamento.md. Siga os critérios de aceitação, rode a verificação e me entregue o relatório."*

**Divisão entre os dois integrantes** (sugestão: quem criou o repositório fica com N): **N** = núcleo (Fases 2, 3, 4) e **G** = interface (Fase 5; Fase 0.5). Fases 0, 1, 6, 7, 8, 9 são conjuntas. Branches: `feat/core` (N) e `feat/gui` (G); integração por pull request com revisão cruzada; **ninguém altera `Model.h` sem avisar o outro** (é o contrato).

## 1. Calendário (hoje = 23/10/2026; defesa do A = 09/11/2026)

| Fase | Quem | Datas | Resultado |
|---|---|---|---|
| 0 Migração do repo + ambiente + risco do Qt | N + G | 23–24/10 | CMake funcionando, repo achatado, Qt standalone provado |
| 1 `Model.h` e contratos | N + G | 24–25/10 | Headers congelados + fixture para a GUI |
| 2 Parser | N | 25–27/10 | P1–P14 verdes |
| 3 Escalonadores e motor de ticks | N | 27–30/10 | G1–G4, invariantes e fuzz verdes |
| 4 Histórico, navegação e edição | N | 30/10–01/11 | G5–G6 verdes |
| 5 Renderizador de Gantt e exportação | G | 25/10–02/11 (em paralelo; usa o fixture até a Fase 3 terminar) | Imagem correta de G1–G3 e export de 100 000 ticks |
| 6 Janela principal e integração | N + G | 01–04/11 | Fluxo completo com os exemplos |
| 7 Endurecimento | N + G | 04–05/11 | Matriz de requisitos 100% |
| 8 Documentação, PDF das telas e pacote | N + G | 05–06/11 | Pasta `simulador/` final + tag `entrega-A` |
| 9 Ensaio da defesa | N + G | 07–08/11 | Roteiro de 10 min cronometrado |

Congelamento interno: **06/11**. O prazo real do Moodle não consta nos arquivos: o usuário deve informá-lo e as datas devem ser ajustadas. Entrega fora do prazo = ZERO; submeter com ≥ 24 h de folga.

## 2. Regras que valem em todas as fases

1. Núcleo (`Model`, `Parser`, `Scheduler`, `Simulator`) **sem Qt**, só STL; compila e testa sem janela.
2. Comentários em português explicando **o quê e o porquê** (RG6) em tudo que for escrito, desde a primeira linha; cabeçalho em cada arquivo com requisitos (`A1.2`…) e decisões (`D10`…).
3. `-Wall -Wextra -Wpedantic` sem warnings. Sem `new/delete` crus, sem globais mutáveis, sem ponteiros entre structs.
4. `simulador/` plano, **≤ 18 arquivos**, sem subpastas, includes planos (`#include "Model.h"`).
5. Mensagens ao usuário em português claro, com o motivo (A3.3.6, A3.4, A5).
6. Questões em aberto: usar o padrão, isolar em um ponto, comentar `// PERGUNTA EM ABERTO Q-xx`.
7. Commits pequenos, mensagem no imperativo, branch correta.

---

## Fase 0 — Migração do repositório, ambiente e risco do Qt (N + G, 23–24/10)

**Leia antes:** `design.md` §2, §3 (estado atual, alvo, mapa de migração, problemas 1–11).

**Objetivo:** repositório reorganizado, build por CMake funcionando e Qt standalone **provado** (é o maior risco técnico e não depende do motor).

**Tarefas**
1. **Diagnosticar o ambiente** e relatar: SO, `cmake --version`, compilador (`g++`/`cl`), gerador (Ninja/Make), Qt (caminho, versão, `windeployqt`). Se faltar algo, **pergunte** ao usuário (não instale). Verificar se o caminho do repositório tem espaços; se der problema, sugerir clonar em caminho sem espaços.
2. **Corrigir `.gitignore`**: remover `*.md`; ignorar `build/`, `build-*/`, `dist/`, `*.exe`, `*.o`, `*.obj`, `CMakeLists.txt.user`, `.vs/`, `.vscode/`. Criar `.gitattributes` (`* text=auto`), normalizar fins de linha (um commit dedicado).
3. **Criar `docs/`** e colocar: `enunciado.md`, `design.md`, `planejamento.md`, `testes.md`, `checklist-A.md`, `checklist-B.md` (mover `Project-B/CHECKLIST.md`), `duvidas_projeto_a.txt` e o PDF do enunciado. `CLAUDE.md` na raiz.
4. **Criar `simulador/`** e mover (`git mv`) os fontes **achatando** o layout: `include/core/*.h` e `src/core/*.cpp` para `simulador/`; trocar `#include "core/X.h"` por `#include "X.h"`. Não alterar a lógica ainda (a fusão em `Model.h` é da Fase 1).
5. **Escrever `simulador/CMakeLists.txt`** (C++17, `-Wall -Wextra -Wpedantic`):
   - `simcore` (biblioteca estática: os `.cpp` do núcleo);
   - `sim_tests` (`tests.cpp`, liga só em `simcore`; criar `tests.cpp` com a macro `CHECK` e um teste trivial);
   - `sim_gui` (`main.cpp`, `MainWindow.*`, `GanttRenderer.*`, `Dialogs.h`; Qt6 `Widgets` e `Svg`; `AUTOMOC ON`), controlado por `option(BUILD_GUI ON)`; se Qt não for achado, **avisar e continuar** sem GUI (N não pode ficar bloqueado);
   - MinGW: `-static-libgcc -static-libstdc++`;
   - alvo de deploy: preferir `qt_generate_deploy_app_script(...)` + `install(SCRIPT ...)` (Qt ≥ 6.5), com `cmake --install build --prefix dist`; fallback `windeployqt`;
   - **comentário de topo com as instruções exatas de compilar** (a defesa usa isso): `cmake -S . -B build` e `cmake --build build`.
6. **G — teste do Qt:** `main.cpp` provisório com uma janela e um botão "Gerar imagens de teste" que grava um PNG (`QImage`+`QPainter`) e um SVG (`QSvgGenerator`) com o mesmo código de desenho. Gerar `dist/` e **provar o standalone**: copiar `dist/` para outro diretório, rodar com `PATH` mínimo (`set PATH=C:\Windows\System32` no Windows) e, se possível, em VM/Windows Sandbox sem Qt. Registrar o resultado no relatório.
7. Remover `Makefile`, `Project-A/`, `Project-B/` **somente depois** de o CMake compilar `simcore` e `sim_tests` com sucesso.

**Critérios de aceitação**
- [ ] `cmake -S simulador -B build && cmake --build build` compila o núcleo e roda `sim_tests` (trivial) sem warnings.
- [ ] `.md` aparecem no `git status` (não são mais ignorados).
- [ ] `dist/` roda fora da máquina de desenvolvimento sem instalar Qt (ou o relatório diz exatamente o que falta).
- [ ] PNG e SVG de teste gerados corretamente.
- [ ] `simulador/` plano; includes planos.

**Commits:** `Corrige .gitignore e normaliza fins de linha`; `Achata estrutura em simulador/ e adiciona docs/`; `Adiciona CMake (simcore, sim_tests, sim_gui) e deploy`.
**Pare e pergunte se:** o Qt não for encontrado; houver mais de uma versão do Qt; o standalone falhar por motivo que exija decisão (ex.: tentar build estático).

---

## Fase 1 — `Model.h` e contratos (N + G, 24–25/10)

**Leia antes:** `design.md` §5, §6, §7, §9; `enunciado.md` A1.3, A3.3.5.

**Objetivo:** congelar o contrato de dados que o motor produz e o Gantt consome.

**Tarefas**
1. Escrever `Model.h` conforme `design.md` §5, **fundindo** `Task.h`, `CPU.h` e `SystemSnapshot.h` e movendo `Config`/`ParseResult`. `Task` vira `struct` (sem membros privados). Manter os nomes `Task`, `CPU`, `SystemSnapshot`, `Occurrence`, `OccurrenceType`. Remover `std::mt19937` (usar `uint64_t seed`) e **não usar `std::deque`**.
2. Apagar `Task.h`, `CPU.h`, `SystemSnapshot.h`, `Task.cpp`.
3. Escrever **só as declarações** de `Parser.h` (`parseConfig`, `serializeConfig`), `Scheduler.h` (§7) e `Simulator.h` (§9), com comentários.
4. Em `tests.cpp`: (a) `serializeSnapshot(const SystemSnapshot&)` → texto determinístico (usado em G6); (b) `demoHistoryG1()`: histórico de G1 **montado à mão** (12 ticks, ocorrências incluídas, conforme `testes.md` G1) para a Pessoa G desenhar o Gantt antes de o motor existir. Declarar em `Simulator.h` e definir em `Simulator.cpp`, marcada `// TEMPORÁRIO: remover na Fase 7`; assim a GUI e os testes compartilham o mesmo fixture sem criar arquivo novo. Remover na Fase 7 (a entrega não pode ter código morto).
5. **Revisão cruzada obrigatória** do `SystemSnapshot` e do `Occurrence` pelos dois integrantes (é o que o Gantt lê).

**Critérios de aceitação**
- [ ] Tudo compila contra os mesmos headers; sem ponteiros entre structs; `SystemSnapshot` copiável por valor.
- [ ] Cada campo tem comentário (o quê/por quê) e cita o requisito/decisão.
- [ ] Os dois aprovaram o `Model.h` (registrar no relatório). Após isso, mudanças só com aviso.

**Commit:** `Model.h: unifica Task/CPU/SystemSnapshot e define contratos`.

---

## Fase 2 — Parser (N, 25–27/10)

**Leia antes:** `design.md` §8, D4, D7, D8, D17, D18, D29, D30; `enunciado.md` A3.2–A3.3.7; `testes.md` §3.

**Objetivo:** `parseConfig` e `serializeConfig` robustos. **Nenhum arquivo válido pode falhar** (A3.3.7).

**Tarefas**
1. Criar `Parser.cpp`. Implementar o pré-processamento (BOM, CRLF/CR, número de linha original, linhas em branco, `trim`, `;` final, comparação sem caixa).
2. Linha do sistema: padrões (algoritmo `RM`, quantum 2, 1 CPU, semente 1); validar contra `schedulerNames()` (se o registro ainda não existir, usar lista provisória `{RM, EDF}` isolada em uma função e trocar na Fase 3).
3. Linhas de tarefa: campos obrigatórios/opcionais; `lista_eventos` = tudo depois do 6º `;`; `prazo` padrão = período; classificar `kind`/`server`/`ignored` + `ignoredReason` (D7); cor normalizada; paleta automática HSL sem preto/branco e sem repetir cores do arquivo.
4. Erros fatais e avisos conforme §8, **com linha, campo, valor e motivo**, em português.
5. `serializeConfig` (ida e volta).
6. Implementar **P1–P14 + robustez** em `tests.cpp`.

**Critérios de aceitação**
- [ ] P1–P14 e casos de robustez verdes; nenhuma exceção escapa; 100 000 tarefas em tempo razoável.
- [ ] Mensagens de erro legíveis (mostrar 5 exemplos no relatório).
- [ ] `periodo = 0` não derruba o carregamento (aviso, tarefa ignorada); `periodo < 0` é erro com linha/campo/valor (Q10); IDs `BSxx…` com período > 0 carregam como periódicas comuns (Q12).

**Commit:** `Parser: leitura tolerante, erros por linha/campo e serializador`.

---

## Fase 3 — Escalonadores e motor de ticks (N, 27–30/10)

**Leia antes:** `design.md` §6, §7, D1, D3, D5, D9–D11, D14; `testes.md` §2 (G1–G4), §4, §5, §6.

**Objetivo:** motor determinístico, sem GUI, validado por cenários calculados à mão.

**Tarefas**
1. `Scheduler.cpp`: registro (`registerScheduler`, `findScheduler`, `schedulerNames`), `tieHash` (splitmix64), `compareTasks`/`orderAndGroup` (A4.3 critérios 1→4, grupos de empate), **RM** (chave = `period`) e **EDF** (chave = `jobs.front().absDeadline`); `chooseTasks` no `Simulator` faz o critério 5 (sorteio só quando o grupo não cabe). *(Já adiantado na `main`: conferir e completar.)*
2. `Simulator.cpp`, como **funções livres sobre `SystemSnapshot&`** (sem I/O, sem estado global): `initState(Config)`, `executeTick`, `beginInstant` (conclusões → ativações → perdas de prazo), `scheduleAndDispatch` (candidatos; atalho `candidatos ≤ M`; `M` chamadas ao escalonador; despacho com afinidade; CPU on/off; `readyQueue`), `makeNext`, `finished`. Ordem **exatamente** como em `design.md` §6.
3. Em `tests.cpp`: `printGrid`, `runConfig`, helpers e os testes **G1, G2 (RM e EDF), G3, G4**, o **teste FIFO** (A4.2), as **invariantes** (§4) e o **fuzz** (§5). Gerar o histórico completo até `finished` (sem a classe `Simulator` ainda: usar `makeNext` em laço).
4. Se algum resultado dourado divergir do motor, **não ajustar o teste**: reexaminar o cálculo e relatar.

**Critérios de aceitação**
- [ ] G1–G4, FIFO, invariantes 1–9 e fuzz de 10 000 sistemas verdes.
- [ ] `printGrid` de G1 e G2 anexado ao relatório.
- [ ] Núcleo sem nenhum include de Qt; sem warnings.

**Commit:** `Scheduler (RM, EDF, registro) e motor de ticks com testes dourados`.

---

## Fase 4 — Histórico, navegação e edição (N, 30/10–01/11)

**Leia antes:** `design.md` §9, D15, D16, D21, D28; `testes.md` G5, G6, invariante 10.

**Tarefas**
1. Classe `Simulator`: `load`, `reset`, `stepForward`, `stepBack`, `jumpTo`, `runToEnd` (limite de ticks e de memória estimada, com mensagem clara), `finished`, `current`, `history`, `cursor`. O estado vive nos snapshots; a classe só guarda `history` + `cursor`.
2. `applyEdit` para **todas** as `EditKind` (§9): validar numa cópia; recusar com mensagem em português que explica o motivo; aplicar no ponto D28 (depois de `beginInstant`, antes de `scheduleAndDispatch`); truncar o futuro; registrar `ManualEdit`; não alterar nada em caso de recusa.
3. Testes **G5 e G6**, recusas, invariante 10, e uma verificação do `runToEnd` contra o limite (config que geraria histórico gigante).

**Critérios de aceitação**
- [ ] G5 (incluindo todas as recusas) e G6 verdes; histórico idêntico ao de execução direta.
- [ ] Edição inválida nunca muda `history`/`cursor`.
- [ ] `runToEnd` nunca trava nem estoura memória.

**Commit:** `Simulator: histórico, retroceder/avançar e edição manual validada`.

---

## Fase 5 — Renderizador de Gantt e exportação (G, 25/10–02/11)

**Leia antes:** `design.md` §10, D2, D3, D4; `enunciado.md` RG3, A2.1–A2.5. **Pedir ao usuário** o PDF do livro do Maziero (ou capturas das figuras da seção 6.4) para imitar as convenções (A2.5); guardar fora do Git (ex.: `docs/ref/`, ignorado).

**Objetivo:** um único `GanttRenderer` que desenha na tela, em PNG e em SVG, a partir de `std::vector<SystemSnapshot>`.

**Tarefas**
1. Layout: margens, coluna de rótulos, eixo X com marcação adaptativa, painel de tarefas (**ID decrescente para cima, menor ID no eixo X, ordem natural**) e painel de CPUs, legenda, título.
2. Células conforme D2 (sólido+`CPUk`, contorno, preto pontilhado, linha-base, hachura de CPU desligada); contraste do texto por luminância.
3. Ícones por `QPainterPath` para **cada** `OccurrenceType` com ícone (tabela do §10), todos na legenda; posição `x=k`.
4. `paint` só da faixa visível; `contentSize`; opções de zoom.
5. `GanttWidget` (rolagem H/V, zoom 4–64, tooltip, auto-rolagem para o tick atual).
6. `exportPng`/`exportSvg`: simulação inteira, sem teto; `tickWidth` adaptativo e divisão em partes quando exceder ~30 000 px; nome com carimbo de data/hora; **nunca capturar a janela**.
7. Trabalhar com o fixture de G1; ao terminar a Fase 3, trocar para histórico real de G1–G3. Gerar um histórico sintético de 100 000 ticks para testar desempenho e exportação.

**Critérios de aceitação**
- [ ] Imagens de G1, G2 (RM com perda de prazo) e G3 (sorteio, CPU desligada) corretas e conferidas contra `testes.md`.
- [ ] Legenda com todos os ícones; CPU de cada execução legível.
- [ ] Export de 100 000 ticks conclui sem travar; PNG/SVG abrem corretamente.
- [ ] Visual coerente com as figuras da seção 6.4.

**Commit:** `GanttRenderer: tela, PNG e SVG com legenda e ícones`.

---

## Fase 6 — Janela principal e integração (N + G, 01–04/11)

**Leia antes:** `design.md` §11, §9; `testes.md` §7.

**Tarefas**
1. `MainWindow`: menu, barra (abrir, reiniciar, retroceder, avançar, play/pausa com `QTimer`, executar até o fim, ir para tick N, velocidade, modo passo a passo/completo), docks (estado do sistema, tabela de TCBs completa, detalhe da tarefa, log).
2. **Abrir arquivo:** `QFileDialog` + `QFile` (qualquer caminho, inclusive pendrive e nomes com acento) → `parseConfig`. **Tela 2** com erros fatais ou avisos; nunca travar.
3. Modos: passo a passo (Gantt a cada passo) e completo (só o resultado final).
4. `Dialogs.h`: **Tela 3** (editar tarefa), **Tela 4** (nova tarefa), **Tela 5** (configurações) chamando `applyEdit` e exibindo recusas com o motivo.
5. Ao final da simulação: exportação automática PNG + SVG e **Tela 6** (resumo; tempo desligado por CPU).
6. Comentar cada widget com `// [TELA x / Ey] origem: ...` (base do PDF de entrega).
7. Criar `exemplo_rm.txt` e `exemplo_edf.txt` (aproveitar G1/G2/G3 e uma tarefa aperiódica para exercitar o aviso).

**Critérios de aceitação**
- [ ] Fluxo completo com os dois exemplos: abrir → passo a passo → retroceder → editar → completo → imagem.
- [ ] Todas as telas mostram erros e recusas em português com o motivo.
- [ ] Checklist manual de `testes.md` §7 (itens de GUI) executado.

**Commit:** `GUI: janela principal, docks, diálogos de edição e exportação automática`.

---

## Fase 7 — Endurecimento (N + G, 04–05/11)

1. Executar **todo** o checklist de `testes.md` §7 e §8 e a matriz de requisitos (`checklist-A.md`): marcar `[x]` **só** o que está implementado **e** testado; qualquer item sem teste ganha teste.
2. Casos feios: arquivos válidos incomuns (campos vazios, espaços, CRLF, BOM, maiúsculas), inválidos, enormes; 1 e 64 CPUs; 200 tarefas; 100 000 ticks.
3. Desempenho: passo < 16 ms com 100 tarefas; estimativa de memória; (opcional) snapshots-chave a cada 64 ticks.
4. Revisar **todas** as mensagens ao usuário; revisar comentários (RG6) arquivo por arquivo: cabeçalho, "o quê/por quê", decisões `Dx`.
5. Resolver ou documentar cada `PERGUNTA EM ABERTO`.
6. **Remover código temporário**: `demoHistoryG1()` (marcada `// TEMPORÁRIO`), prints de depuração e código morto (a entrega não pode ter).

**Critério:** matriz 100% marcada; zero warnings; `sim_tests` verde.
**Commit:** `Endurecimento: testes extras, mensagens e comentários`.

---

## Fase 8 — Documentação, PDF das telas e pacote de entrega (N + G, 05–06/11)

1. **PDF `telas_projeto_a.pdf`:** capturas reais das Telas 1–6 + legenda de **cada elemento visual** + **de onde vem a informação** (usar os comentários `[TELA x / Ey]`). Gerar a partir de Markdown/LibreOffice/pandoc (perguntar qual o usuário prefere). Um arquivo PDF só.
2. **Conferir a entrega** (executar e colar a saída no relatório): `ls simulador | wc -l` ≤ 18; nenhuma subpasta; nenhum `.zip`; nenhum caminho absoluto; todo arquivo com cabeçalho e comentários; sem código morto nem `printf` de depuração.
3. Build do zero em pasta limpa **seguindo só o comentário do `CMakeLists.txt`**; gerar `dist/` e repetir o teste de standalone (máquina limpa).
4. Tag `entrega-A`; atualizar `checklist-A.md`.
5. Guardar um `dist/` pré-compilado (pendrive) como plano B da defesa.
6. Lembrar o usuário: upload no Moodle **arquivo por arquivo, sem zip**, com ≥ 24 h de folga.

**Commit/tag:** `Entrega do Projeto A` + `git tag entrega-A`.

---

## Fase 9 — Ensaio da defesa (N + G, 07–08/11)

1. Gerar para o usuário um roteiro cronometrado (≤ 10 min) a partir de `design.md` §15 e um "cola" de arquivos/linhas para mostrar (onde está o TCB, o registro de escalonadores, o desempate, o histórico, o parser).
2. Simular perguntas (§15) e cobrar justificativas técnicas (D1, D10, D12, D14, D26).
3. Ensaiar a compilação ao vivo em máquina limpa; cada integrante responde sobre **todas** as partes (a nota é individual).

---

## Roteiro do Projeto B (defesa 07/12/2026) — alto nível

> Detalhar só depois da entrega do A. Reaproveitar os ganchos de `design.md` §14 e `docs/checklist-B.md`.

| Fase | Conteúdo | Observações |
|---|---|---|
| B0 | Reler enunciado B; resolver Q-B1…Q-B3 com o professor; reavaliar orçamento de arquivos (A usa 18 de 20) | Prever fusões ou autorização por escrito para passar de 20 |
| B1 | Aperiódicas (`periodo=0`) e esporádicas (`periodo<0`, `lista_eventos` = `instante01,instante02,…`, verificação do tempo mínimo entre ocorrências); fila global de aperiódicas | Desligar a flag `ignored` (D7); guardar `periodo` no TCB (B1.2) |
| B2 | Servidores `BSxx/PSxx/DSxx/SSxx` como tarefas periódicas; política de cada um (capítulo "Escalonamento de Tempo-Real", seção 2.8); fila compartilhada com retirada exclusiva | Aperiódicas/esporádicas **não** interferem nas periódicas (B1.1) |
| B3 | Teste de escalonabilidade `admit(snapshot, task)` antes de aceitar qualquer tarefa (inclusive criadas durante a execução; todos os tipos + servidores); recusa com motivo | Base: painel de utilização do A |
| B4 | Visual: padrão de preenchimento/borda distinto para servidor e para aperiódica/esporádica; legenda atualizada | Mapa estado → estilo no renderizador |
| B5 | (Se confirmado) eventos de mutex/E/S, herança de prioridade, inversão de prioridades, impasse | `Suspended` + `blockReason`, `effPriority` |
| B6 | Testes, PDF das telas do B, ensaio da defesa | Mesmas regras de entrega do A |
