# CLAUDE.md — Simulador de SO multitarefa (UTFPR, Sistemas Operacionais)

Simulador de escalonamento de tempo real (RM/EDF) com N CPUs, Gantt gráfico, passo a passo com retroceder/avançar e edição manual. Entrega em 2 projetos incrementais: **A** (defesa 09/11/2026) e **B** (defesa 07/12/2026), mesma base de código. Equipe de 2 alunos; **a nota é individual**: o código precisa ser compreendido e defendido por ambos.

## Leia nesta ordem antes de qualquer tarefa
1. `docs/enunciado.md` — o que o professor exige (enunciado v0.7 transcrito, regras de entrega/defesa, respostas às dúvidas, perguntas em aberto). **Vence qualquer outro documento.**
2. `docs/design.md` — decisões fechadas (D1–D30), arquitetura, `Model.h`, semântica do motor, parser, edição, Gantt, telas, defesa.
3. `docs/planejamento.md` — fases executáveis (0 a 9) e roteiro do B. Execute **uma fase por vez**.
4. `docs/testes.md` — cenários dourados calculados à mão, casos do parser, invariantes: define "pronto".
5. `docs/checklist-A.md` e `docs/checklist-B.md` — rastreabilidade por requisito; marcar `[x]` só se **implementado e testado**.
6. `docs/fluxo-git.md` — branches, merges, conflitos e correções de bug.

## Stack
C++17 · Qt 6 (Widgets + Svg) só na interface · CMake · Qt dinâmico + deploy em `dist/` (o professor confirmou que pasta com `.exe` + DLLs atende, desde que não exija instalar o Qt). Núcleo (`Model`, `Parser`, `Scheduler`, `Simulator`) é C++ puro, **sem Qt**, e compila como biblioteca `simcore`.

## Regras inegociáveis
- Entrega: **`simulador/` plano, ≤ 18 arquivos, sem subpastas, sem zip** (zip = nota ZERO; Moodle limita a 20 anexos). Includes planos (`#include "Model.h"`).
- **Todo** arquivo e função comentados, explicando o **quê e o porquê** (RG6); cabeçalho em cada arquivo com requisitos (`A1.2`…) e decisões (`D10`…). Identificadores em inglês. Comentários em **português** é o padrão da dupla; trechos já escritos em inglês (código do Arthur) podem ficar em inglês: o que importa é explicar.
- **Determinismo total:** sem relógio real, `rand()`, ponteiros como chave. Sorteio de desempate = hash sem estado (D14).
- Todo estado mutável em `SystemSnapshot` (copiável, **sem ponteiros**, índices). Regras = funções livres. **Não usar `std::deque`** nos `Job`s.
- O Gantt exportado vem do **mesmo renderizador** (`QPainter` em `QImage`/`QSvgGenerator`), **nunca** de captura de tela.
- **Nenhum arquivo de configuração válido pode falhar ao carregar** (A3.3.7 = falha grave). Na dúvida, aceitar com aviso.
- Mensagens ao usuário em português claro, sempre com o motivo.
- `-Wall -Wextra -Wpedantic` sem warnings.

## Como trabalhar
- Uma fase por vez; **não avance com testes falhando**. Ao fim: verificação, commit, relatório curto (feito / testado / em aberto / perguntas).
- Resultados dourados de `docs/testes.md` foram calculados à mão: se o motor divergir, **reexamine antes de mexer no teste** e relate o tick exato.
- Perguntas em aberto (`enunciado.md` §5): use o **padrão** indicado, isole em um ponto único e comente `// PERGUNTA EM ABERTO Q-xx`. **Nunca invente respostas do professor.**
- **Não instale ferramentas** nem altere o ambiente sem perguntar. Se Qt/CMake/compilador faltar, pare e pergunte.
- Se uma decisão do `design.md` parecer errada ou conflitar com o enunciado, **pare e pergunte** em vez de mudar por conta própria.
- **Git: seguir `docs/fluxo-git.md`.** Uma branch por fase (`fase-N-tema`), `fix/tema` para bug que já está na `main`; merge na `main` só com build sem warnings e `sim_tests` verde; conflitos resolvidos na branch da fase, nunca na `main`. Cada um integra a própria branch (PR opcional, sem aprovação obrigatória) e avisa o colega. Ninguém altera `Model.h` sem avisar a dupla. Commits pequenos, no imperativo, em português. Tag `entrega-A` marca o que vai ao Moodle.
- Preferências pessoais de cada integrante para o Claude Code (ex.: modo de ensino passo a passo, assinatura de commits) ficam no `CLAUDE.local.md` de cada um (não versionado), não neste arquivo.
- Caminho local do usuário tem espaços (Windows): se o build reclamar, sugerir clonar em caminho sem espaços.

## Comandos (a partir de `simulador/`)
```
cmake -S . -B build                 # configurar (BUILD_GUI=ON se o Qt for encontrado)
cmake --build build                 # compilar
./build/sim_tests                   # testes do núcleo (sem Qt)
cmake --install build --prefix dist # empacotar a GUI com as DLLs do Qt (pasta standalone)
```
Os comandos de compilar devem ficar também no comentário de topo do `CMakeLists.txt` (a defesa compila ao vivo com CMake na frente do professor).

## Mapa do repositório (alvo)
`simulador/` (entrega) · `docs/` (estes documentos, `fluxo-git.md`, PDF do enunciado e dúvidas) · `.gitignore` (**sem `*.md`**) · `.gitattributes`.
