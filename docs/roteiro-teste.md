# roteiro-teste.md — Como compilar, testar e ver o simulador funcionando

> Para qualquer integrante que acabou de pegar a versão atual do projeto no Windows. Cobre as Fases 0 a 6: núcleo (parser, motor RM/EDF com várias CPUs, histórico com voltar/avançar, edição manual), testes automáticos, gráfico de Gantt no estilo do livro do Maziero e a janela com todas as telas. Tempo: 20 a 30 minutos.
>
> O que cada fase entregou está em `docs/planejamento.md` (seção "Estado"); as regras de branch estão em `docs/fluxo-git.md`.

---

## 1. Pegar a versão atual

No PowerShell, na pasta do repositório:

```powershell
git status          # precisa estar limpo; se não estiver, faça commit ou stash antes
git fetch
git switch main
git pull
```

Organização do repositório:

- **`simulador/`**: o código. É plano (sem subpastas) porque é exatamente o que vai ao Moodle.
- **`docs/`**: enunciado, design, planejamento, testes, checklists e este roteiro.
- **`CLAUDE.md`** e **`AGENTS.md`**: as mesmas diretrizes para os agentes (Claude Code e Codex). Preferências pessoais ficam fora do repositório: `CLAUDE.local.md` ou `~/.codex/AGENTS.md`.
- O build é por **CMake**; não há Makefile.

---

## 2. Instalar o necessário (só na primeira vez)

Precisa de: **Qt 6 (6.5 ou mais novo) com MinGW 64-bit**, **CMake** e **Ninja**, todos vindos do instalador do Qt.

1. Baixe o **Qt Online Installer** em <https://www.qt.io/download-qt-installer> (pede uma conta Qt gratuita).
2. Na seleção de componentes, marque:
   - **Qt → Qt 6.x → MinGW 64-bit**;
   - **Developer and Designer Tools → MinGW 64-bit** (o compilador), **CMake** e **Ninja**.
3. Confira se estas pastas existem. **Os números de versão podem ser outros na sua máquina**; nesse caso, troque nos comandos dos próximos passos.

   | Pasta | Para quê |
   |---|---|
   | `C:\Qt\6.12.0\mingw_64\lib\cmake\Qt6Svg` | Qt com o módulo de SVG (exportação do Gantt) |
   | `C:\Qt\Tools\mingw1310_64\bin` | compilador `g++` |
   | `C:\Qt\Tools\CMake_64\bin` | `cmake` |
   | `C:\Qt\Tools\Ninja` | `ninja` |

**Sem instalar nada:** peça a pasta `simulador\dist` pronta para quem já compilou (cerca de 60 MB, por pendrive ou drive) e pule para o **passo 6**. Ela não vai pelo Git, porque é gerada.

---

## 3. Compilar

No PowerShell, a partir da pasta `simulador`. A linha do `$env:PATH` vale só para essa janela do terminal:

```powershell
cd simulador
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
cmake --build build
```

O que cada comando faz:

- **`cmake -S . -B build ...`** configura o projeto (só na primeira vez ou depois de apagar `build`). O `-DCMAKE_PREFIX_PATH` diz onde está o Qt.
- **`cmake --build build`** compila três alvos:
  - `simcore`: biblioteca com o núcleo, sem Qt;
  - `sim_tests`: testes do núcleo;
  - `sim_gui`: a interface.

Resultado esperado:

- no configure aparece "Qt 6.x encontrado: a interface sim_gui será compilada";
- no build aparecem as linhas `Linking ... libsimcore.a`, `sim_tests.exe` e `sim_gui.exe`, **sem nenhum warning** (regra do projeto: `-Wall -Wextra -Wpedantic`).

Esses comandos também estão no comentário do topo do `simulador/CMakeLists.txt`; é por ele que compilamos ao vivo na defesa.

**Se o Qt não for encontrado,** o CMake avisa e compila só o núcleo e os testes. Confira o caminho do `-DCMAKE_PREFIX_PATH`.

---

## 4. Rodar os testes automáticos do núcleo

Os testes não usam Qt e não abrem janela.

```powershell
.\build\sim_tests.exe --rapido     # ~4 s: tudo, com o fuzz reduzido
.\build\sim_tests.exe              # ~35 s: completo (rodar antes de integrar na main)
.\build\sim_tests.exe --grid       # imprime as grades de G1, G2 (RM e EDF) e G3
```

O que é testado (detalhes em `docs/testes.md`):

| Grupo | O quê |
|---|---|
| P1–P14 + robustez | Leitura do arquivo: CRLF, BOM, `;` final, maiúsculas, erros com linha/campo/valor, 100 000 tarefas, arquivo binário |
| G1–G6, G4b | Cenários calculados à mão (RM, EDF, perda de prazo, várias CPUs, sorteio, edição, voltar/avançar) |
| Invariantes | Regras que valem em **todo** instante (ex.: nunca há tarefa pronta com CPU desligada) |
| Fuzz | 10 000 sistemas aleatórios até o fim, verificando as invariantes; 200 sistemas com edições aleatórias |
| FIFO | Um algoritmo novo registrado de fora do núcleo, sem mexer no motor (A4.2) |

**Resultado esperado:** a última linha é `N checks, 0 falhas`. Se houver falha, a linha do `tests.cpp` e a condição aparecem antes do placar.

No `--grid`, cada linha é um instante: quem ocupa cada CPU em `[k, k+1)` e as ocorrências daquele instante. Compare com as grades de `docs/testes.md`. Exemplo: no G2 RM a tarefa 2 perde o prazo em t = 7; no G2 EDF ninguém perde.

---

## 5. Gerar a pasta que roda sem instalar nada (RG5)

```powershell
cmake --install build --prefix dist
```

Isso cria `simulador\dist\` com o `sim_gui.exe`, as DLLs do Qt, os plugins e as DLLs do compilador. **É sempre desta pasta que se abre o programa.** O `build\sim_gui.exe` sozinho reclama de "DLL faltando", porque procura o Qt no PATH.

**Depois de mudar o código:** feche o `sim_gui.exe` (com ele aberto a cópia falha) e rode `cmake --build build` e `cmake --install build --prefix dist` de novo. A `dist/` é ignorada pelo Git.

---

## 6. Exportar o Gantt sem abrir a janela (plano B da defesa)

A partir da pasta `simulador`:

```powershell
Start-Process .\dist\sim_gui.exe -ArgumentList '--exportar','exemplo_rm.txt','saida' -Wait
dir saida
```

O programa carrega o arquivo, executa até o fim e grava `gantt_exemplo_rm_<data>.png` e `.svg` em `saida\`. Abra o PNG: é o gráfico da **simulação inteira**, desenhado (não é print de tela, A2.4).

As mensagens do `--exportar` não aparecem no terminal, porque o programa é de janela. Confira o resultado pela pasta. As imagens `gantt_*` são ignoradas pelo Git.

---

## 7. Usar a janela

Dê dois cliques em **`simulador\dist\sim_gui.exe`**. Outra forma: arrastar um `.txt` de configuração em cima do `sim_gui.exe` já abre com ele carregado.

### 7.1 O que tem na janela (Tela 1)

| Região | Conteúdo |
|---|---|
| Menu e barra (2 linhas) | Abrir, Reiniciar, Retroceder, Avançar, Play/Pausa, Executar até o fim · Ir para o instante, Velocidade, Modo (passo a passo / completo), Zoom, Exportar |
| Centro | Gantt: uma linha por tarefa (menor ID colado no eixo X), painel de CPUs embaixo e legenda fixa |
| Direita, em cima | **Estado do sistema:** relógio, algoritmo, o que cada CPU executa, tempo e intervalos desligada, filas, utilização |
| Direita, embaixo | **Tabela de TCBs** (todas as informações de cada tarefa), detalhe da tarefa selecionada e botões Suspender, Retomar, Ativar agora, Forçar término, Remover, Editar parâmetros |
| Embaixo | **Registro:** ocorrências de cada instante, edições, avisos e erros |
| Rodapé | Relógio global |

Como ler o Gantt (o mesmo estilo das figuras 6.1–6.10 do Maziero):

- **cor sólida + "CPUk"** = executando naquela CPU;
- **retângulo branco** = pronta, esperando CPU;
- **preto pontilhado** = suspensa;
- **nada** = fora do sistema (ainda não chegou, esperando o próximo período, ou terminada);
- **hachura cinza** no painel de CPUs = CPU desligada;
- **coluna amarela** = instante atual.

Cada ícone (chegada, término, perda de prazo, preempção, migração, sorteio, CPU ligada/desligada, edição) está na legenda.

Atalhos: Ctrl+O abrir, Ctrl+←/→ voltar/avançar, F6 play, F5 até o fim, Ctrl+N nova tarefa, Ctrl+E exportar, Ctrl +/- zoom.

### 7.2 Roteiro de conferência

| # | Faça | Deve acontecer | Requisito |
|---|---|---|---|
| 1 | **Arquivo → Abrir configuração** → `simulador\exemplo_rm.txt` | A Tela 2 avisa que a tarefa 4 é aperiódica e será ignorada; na tabela ela aparece cinza, como "Ignorada" | A3.3.6, A4.4 |
| 2 | **Avançar** várias vezes; **Retroceder**; "Ir para o instante" 10 + **Ir** | O Gantt cresce a cada passo, a coluna amarela marca o instante, o relógio acompanha (inclusive ao voltar) | A1.5.2, A2.3 |
| 3 | Observe o Gantt e o painel de CPUs | Menor ID embaixo; "CPU1/CPU2" dentro das barras; hachura quando a CPU desliga | A2.1, A2.5, A1.2 |
| 4 | Sobre o Gantt: **Ctrl + roda do mouse**; roda normal e a barra de rolagem | Zoom de 4 a 64 px por instante; rolagem | A2 |
| 5 | Pare o mouse sobre uma barra | Dica com o estado, a CPU, o tempo restante e o prazo absoluto | A1.5.1 |
| 6 | Leia **Estado do sistema** e a tabela | Filas, CPUs ligadas/desligadas com os intervalos, todos os campos do TCB | A1.3, A1.5.1, A1.2 |
| 7 | Selecione a tarefa 1 → **Suspender** → avance alguns passos → **Retomar** | Barra preta pontilhada enquanto suspensa e losango magenta; no registro, "o futuro foi recalculado" | A3.4, A2.1 |
| 8 | **Editar parâmetros…** (Tela 3): mude o prazo e dê OK; repita com a cor `000000` | O prazo muda a partir do instante atual; a cor é **recusada com o motivo** (o preto indica suspensa) | A3.1, A3.4 |
| 9 | **Simulação → Configurações do sistema** (Tela 5): EDF e 3 CPUs | Vale a partir do instante atual; o futuro é recalculado | A3.1 |
| 10 | **Simulação → Nova tarefa** (Tela 4) com o ID `1` | Recusa: "o id '1' já é usado" | A3.4 |
| 11 | Em **Modo**, escolha **Completo** (ou aperte F5) | Executa até o fim, **grava PNG + SVG** na pasta do arquivo e abre a Tela 6 (resumo, perdas de prazo, tempo desligado por CPU); botão "Abrir pasta das imagens" | A1.5.3, A2.4 |
| 12 | Abra `exemplo_edf.txt` e dê **Play** | Três tarefas empatadas: ícones de dado (sorteio do desempate). Feche a janela no meio do play: fecha sem travar | A4.3 |
| 13 | Copie `exemplo_rm.txt` para a Área de Trabalho, troque a 1ª linha por `XYZ;0;1` e abra a cópia | A Tela 2 lista os erros com linha, campo e motivo; **nada é carregado** | A3.3.6, A3.3.4 |
| 14 | Selecione uma tarefa → **Suspender** e não retome → **Executar até o fim** | Para com a mensagem "só restam tarefas suspensas… retome-as ou force o término" (não fica rodando à toa) | A5 |

**Sobre a exportação:** as imagens vão para a pasta do arquivo de configuração; se ela não aceitar escrita, vão para Documentos. A pasta pode ser trocada na Tela 5.

---

## 8. Teste do "roda sem instalar nada" (RG5)

No **cmd** (não no PowerShell), a partir da pasta `simulador`:

```bat
set PATH=C:\Windows\System32
dist\sim_gui.exe
```

A janela tem que abrir mesmo sem o Qt no PATH. O teste definitivo, numa máquina sem Qt (Windows Sandbox ou outro PC), fica para a Fase 7.

---

## 9. Onde está cada coisa no código

| Arquivo | O quê |
|---|---|
| `Model.h` | Contrato de dados: `Task` (o TCB), `Job` (ocorrência), `CPU`, `Occurrence`, `SystemSnapshot` (estado completo de um instante), `Config`, `Edit` |
| `Parser.h/.cpp` | Leitura do arquivo: todos os erros e avisos (tabela `MESSAGES`), padrões, ida e volta (`serializeConfig`) |
| `Scheduler.h/.cpp` | RM, EDF, desempate do A4.3 (grupos de empate), registro de algoritmos, `tieHash` (sorteio) |
| `Simulator.h/.cpp` | Motor de ticks (ordem fixa do instante), histórico, voltar/avançar, `runToEnd`, edição manual |
| `GanttRenderer.h/.cpp` | Desenho único do Gantt para tela, PNG e SVG; widget com rolagem, zoom e tooltip |
| `MainWindow.h/.cpp` | Tela 1 |
| `Dialogs.h` | Telas 2 a 6 |
| `main.cpp` | Entrada do programa e modo `--exportar` |
| `tests.cpp` | Todos os testes automáticos |
| `exemplo_rm.txt`, `exemplo_edf.txt` | Configurações de demonstração |

Para entender o **porquê** de cada decisão (o que o professor vai perguntar na defesa): `docs/design.md`, decisões D1–D30 e §15 "Defesa: perguntas prováveis".

---

## 10. Achou um problema?

1. Anote o passo deste roteiro, o que esperava e o que apareceu (um print ajuda).
2. Avise a dupla.
3. Para corrigir: branch `fix/<tema>` a partir da `main`, com um teste que reproduz o problema (`docs/fluxo-git.md`). Integre na `main` só com o build sem warnings e o `sim_tests` completo verde.
