# Projeto A — Checklist de requisitos (recuperado do histórico do Git, commit 8ba93a4, e atualizado)

> **Atualização (09/11/2026, fim da Fase 7):** a auditoria da Fase 7 achou falhas em A2.4, A3.3.1, A3.3.6, A3.3.7, A4.4 e A5, corrigidas na branch `fix/carregamento-e-navegacao` (testes em `testParserSystemAndIds`, `testParserRobustness`, `testEditKinds`, `testGoldenG6`, `testRunToEnd` e no teste roteirizado da janela). Outra partição (D:) e zoom/rolagem verificados.
>
> **Atualização (30/10/2026):** itens marcados `[x]` na Fase 7, com a evidência da tabela abaixo; os `[ ]` têm a nota do que falta.
>
> **Atualização anterior (23/10/2026):** itens "Pendências" já respondidos pelo professor foram marcados; os demais apontam o **padrão adotado** e o ID em `docs/enunciado.md` §5. Regra de uso: `[x]` só com item **implementado e testado**. Códigos de teste (G1–G6, P1–P14) estão em `docs/testes.md`.
> Observações de interpretação do professor: Q5 (ícone de chegada = a cada ativação/entrada na fila de prontos; término = a cada ocorrência concluída) vale para A2.2.

> **Fonte:** "Simulação de um Sistema Operacional Multitarefa de Tempo Compartilhado", versão 0.7 (15/10/26), Prof. Dr. Marco Aurélio Wehrmeister.
>
> **Regra:** marcar `[x]` somente quando o item estiver **implementado e testado**.
>
> Os códigos são `RGn` para requisitos gerais e `An.n` para o Projeto A, seguindo a numeração do enunciado.

---

## Evidências (de onde vem cada `[x]`)

| Itens | Evidência |
|---|---|
| RG1, A1.1–A1.4 (relógio, CPUs, fila global, CPU desligada, TCB) | G1–G4b, invariantes 1–9 e fuzz de 10 000 sistemas em `sim_tests` |
| RG2 (1 a 64 CPUs) | P9, recusa de `SetCpuCount` (G5), `MAX_CPUS = 64` (D12) |
| RG3, A2.1–A2.5 (Gantt, legenda, ícones, eixo Y, imagem) | `GanttRenderer`; checklist manual de `docs/testes.md` §7 (Fase 6); exportação de 100 000 ticks |
| RG4, A1.5.x, A3.4 (avançar, retroceder, editar) | G5 (com as recusas), G6, `testEditKinds`, `testEditFuzz`, `testRunToEnd` |
| A3.1, A3.2 (parametrizar antes e durante, padrões) | 16 `EditKind`; Telas 3, 4 e 5; padrões no parser |
| A3.3.x (formato, caixa, `;`, erros, falha grave) | P1–P14 e robustez (100 000 tarefas, BOM, CRLF, binário) |
| A4, A4.1–A4.4 (RM, EDF, desempate, sorteio, 10 vezes, aperiódicas) | G1–G4b, teste FIFO (A4.2), P13, invariante 6 |
| A5 (erros claros) | mensagens das Telas 2 a 6, verificadas na Fase 6 |
| Desempenho | Núcleo: `testStepSpeed`, passo com 100 tarefas ≈ 0,05 ms. Janela (passo + redesenho, 100 tarefas): ≈ 14 ms, abaixo do limite de 16 ms, depois de a tabela de TCBs passar a ser atualizada no lugar (antes ≈ 33 ms) |
| A3.3.4 (outra partição) | `sim_gui --exportar` com arquivo `minha config.cfg` em `D:\teste simulador ção (apagar)\`, saída gravada no próprio D: (Fase 7) |
| Zoom e rolagem | Ctrl+roda e rolagem horizontal simuladas: pior caso 25 ms (64 CPUs, 200 tarefas) e 6 ms (93 000 instantes) |

---

## Requisitos gerais (obrigatórios em A e em B)

### RG1 — Tipo de sistema
- [x] Simula a execução de tarefas num SO multitarefa **preemptivo de tempo compartilhado**.

### RG2 — Processadores
- [x] O computador simulado tem mais de um processador capaz de executar as tarefas.
- [x] A quantidade de processadores é configurada pelo usuário.
- [x] Mínimo de 1 e máximo de N processadores.
- [x] O valor de N está definido e documentado (`MAX_CPUS = 64`, D12, com justificativa técnica: Q6).

### RG3 — Visualização gráfica
- [x] Mostra a execução das tarefas ao longo do tempo, para cada processador.
- [x] Todos os eventos importantes/relevantes aparecem claramente.
- [x] Cada tipo de evento tem um elemento gráfico distinto.
- [x] A visualização tem uma legenda desses elementos.
- [x] Indica claramente em qual processador cada tarefa está executando.

### RG4 — Controle e configuração
- [x] O software é configurável.
- [x] O usuário controla a simulação.
- [x] O usuário controla as características do sistema simulado.
- [x] É possível avançar a simulação.
- [x] É possível retroceder a simulação.
- [x] O estado atual de cada tarefa pode ser modificado manualmente em qualquer passo/estágio.

### RG5 — Execução sem instalação
- [x] A linguagem foi escolhida (C++17 / Qt 6).
- [ ] Executa sem instalar biblioteca extra, software, runtime ou equivalente. *(falta provar numa máquina sem Qt; com PATH mínimo já funciona)*
- [ ] Testado num computador limpo, sem nada instalado. *(pendente: VM ou outro PC sem Qt)*

### RG6 — Comentários
- [x] Todo o código-fonte está comentado. *(varredura de funções sem comentário, 30/10)*
- [x] Os comentários explicam o que cada parte/função/componente faz. *(idem)*
- [ ] Os comentários explicam as razões/justificativas de todas as decisões de implementação. *(pendente: leitura da dupla; as decisões estão citadas como D1–D30 e Q-xx)*

---

## Projeto A

### A1 — Simulação

**A1.1 — Relógio global**
- [x] O sistema tem um relógio global.
- [x] A passagem do tempo é simulada em ticks.
- [x] A cada tick, verifica se há ação ou evento a realizar.
- [x] O tempo da simulação não depende do tempo real.

**A1.2 — Múltiplos processadores e fila global**
- [x] O sistema tem múltiplos processadores/CPUs/cores.
- [x] O escalonador minimiza a ociosidade: nunca há tarefa na fila de prontos com CPU sem uso.
- [x] A fila de prontos é **global**, não uma por CPU.
- [x] Quando não há tarefa para um processador, ele é **desligado**.
- [x] O período em que cada processador fica desligado é apresentado ao usuário.
- [x] O período em que cada processador fica desligado aparece no gráfico.

**A1.3 — TCB**
- [x] Todas as informações de cada tarefa (antes, durante e depois da simulação) ficam numa **única estrutura de dados** (TCB).

**A1.4 — Características de tempo**
- [x] A simulação segue as características de tempo de cada tarefa: ingresso, duração, instante dos eventos etc.

**A1.5 — Modos de execução**
- [x] (a) Modo passo a passo.
- [x] (b) Modo de execução completa, sem intervenção humana.

**A1.5.1 — Passo a passo como depurador**
- [x] Permite examinar o estado atual e as informações correntes do sistema.
- [x] Permite examinar o estado de cada tarefa individualmente.

**A1.5.2 — Avançar, retroceder e editar**
- [x] O usuário avança e retrocede a simulação quando quiser.
- [x] O histórico do estado do sistema, a cada passo, fica armazenado em memória.
- [x] Retroceder e avançar usam esse histórico.
- [x] Retroceder e avançar alteram o relógio global.
- [x] O usuário modifica o estado atual ou as características de qualquer tarefa em qualquer passo.

**A1.5.3 — Execução completa**
- [x] Mostra apenas o resultado final, sem os passos intermediários.

### A2 — Visualização (gráfico de Gantt)

**A2.1 — Representação dos estados**
- [x] O Gantt mostra as tarefas executando ao longo do tempo.
- [x] O Gantt mostra o processador usado em cada período de execução.
- [x] Tarefa executando é desenhada com a cor definida no parâmetro de entrada.
- [x] Cada tarefa tem uma cor diferente (comportamento para cores repetidas definido).
- [x] Tarefa na fila de prontos, sem executar, aparece como **ausência de cor**.
- [x] Tarefa suspensa, por qualquer motivo, aparece em **preto** com preenchimento diferente do de execução (ex.: pontilhado).

**A2.2 — Chegada e término**
- [x] A chegada de uma tarefa nova aparece claramente (ícone, figura, seta etc.).
- [x] O término de uma tarefa aparece claramente.
- [x] Esses eventos aparecem no instante exato em que acontecem.

**A2.3 — Atualização do gráfico**
- [x] A atualização é compatível com o modo de execução. O enunciado cita o requisito 1.4, mas o conteúdo corresponde ao 1.5.
- [x] No passo a passo, o Gantt atualizado aparece na tela a cada passo.

**A2.4 — Exportação da imagem**
- [x] Ao final da simulação, gera um arquivo de imagem (JPG, PNG, SVG etc.). *(também quando o fim vem de uma edição: corrigido na Fase 7)*
- [x] A imagem corresponde à situação final, depois de todas as tarefas executarem.
- [x] A imagem **não** é um print da janela.
- [x] A imagem contém a simulação inteira.
- [x] A imagem não está limitada a nenhum tempo máximo de simulação. *(sem limite de instantes desde a Fase 7; o único teto é a memória do histórico, metade da memória livre, porque o A1.5.2 exige o histórico em memória: design.md §9)*

**A2.5 — Estilo e ordem**
- [x] O Gantt é similar às figuras da seção 6.4 do livro do Prof. Maziero.
- [x] O eixo Y está em ordem decrescente de ID: a tarefa de ID menor fica mais perto do eixo X.

### A3 — Configuração

**A3.1 — Parametrização antes e durante a simulação**
- [x] Conjunto de tarefas.
- [x] Características individuais de cada tarefa.
- [x] Algoritmo de escalonamento.
- [ ] Eventos: solicitação/liberação de mutex, E/S, envio/recebimento de dados etc. (escopo no A a confirmar com o professor). *(Q-A9: o arquivo só guarda o texto; os eventos são do Projeto B)*
- [x] Modificação sem sentido ou impossível durante a execução é recusada com aviso claro e justificativa.

**A3.2 — Valores padrão**
- [x] Há valores padrão sugeridos para todos os parâmetros da simulação e do software.
- [x] Os padrões são sobrescritos pelos valores do arquivo de configuração.
- [x] Os padrões são sobrescritos pelos valores digitados nas telas de parâmetros.

**A3.3 — Formato do arquivo**
- [x] O arquivo é texto simples (plain text).
- [x] Linha 1: `algoritmo_escalonamento;quantum;qtde_cpus`.
- [x] Linhas seguintes: `id;cor;ingresso;duracao;periodo;prazo;lista_eventos`.
- [x] `algoritmo_escalonamento` define o algoritmo usado.
- [x] `quantum` é o tempo máximo que uma tarefa pode executar. *(lido, guardado e exibido; 0 a 10⁶; RM e EDF não usam, Q4)*
- [x] `qtde_cpus` aceita valores entre 1 e N.
- [x] `id` é um identificador único.
- [x] `cor` está em RGB hexadecimal (ex.: `F0E0D0`).
- [x] `ingresso` é o instante de criação da tarefa.
- [x] `duracao` é o tempo de execução da tarefa.
- [x] `periodo > 0`: tarefa periódica, ativada a cada `n` ticks.
- [x] `periodo = 0`: tarefa aperiódica.
- [x] `periodo < 0` é **erro do arquivo** no A, mostrado com linha, campo e valor. **Respondido (Q10)**; no B passa a indicar esporádica (B1.3).
- [x] `prazo` (deadline) conta a partir da ativação.
- [x] Prazo ultrapassado é um erro e aparece no Gantt.
- [x] A tarefa que perdeu o prazo continua executando até o tempo executado ser ≥ à duração.
- [x] `lista_eventos` é lida e armazenada.

**A3.3.1 — Estrutura do arquivo**
- [x] A linha 1 tem os parâmetros gerais. Da linha 2 em diante, uma tarefa por linha.
- [x] O arquivo tem no mínimo 2 linhas.
- [x] Não há limite máximo de tarefas. *(o parser aceita 100 000 tarefas; a simulação só para se o histórico não couber na memória livre, com mensagem clara)*

**A3.3.2 — Maiúsculas e minúsculas**
- [x] Todas as strings são tratadas sem diferenciar maiúsculas e minúsculas (ex.: `PRIOP` = `priop` = `PrioP`).

**A3.3.3 — Ponto e vírgula final**
- [x] Linhas terminadas com `;` são aceitas.
- [x] Linhas terminadas sem `;` são aceitas.

**A3.3.4 — Local do arquivo**
- [x] Carrega arquivo com **qualquer nome**.
- [ ] Carrega de **qualquer local**: qualquer diretório, outra partição ou disco externo (pendrive). *(pasta com espaço e acento e outra partição (D:) verificados; falta o pendrive, teste físico)*
- [x] **Não** usa arquivo fixo.

**A3.3.5 — Eventos (preparação para o B)**
- [x] A estrutura suporta inúmeros eventos ao longo da execução de uma tarefa.

**A3.3.6 — Erros no arquivo**
- [x] Erros de carregamento são mostrados ao usuário com o motivo. *(UTF-16 e ANSI convertidos com aviso; erros de arquivo em português: Fase 7)*
- [x] Erros no conteúdo do arquivo são mostrados ao usuário com o motivo.
- [x] Espaços e linhas em branco **não** são erros e são ignorados.

**A3.3.7 — Falha grave**
- [x] Nenhum arquivo válido, que obedeça ao formato, gera erro de carregamento (testado com uma bateria de arquivos). *(quantum 0 e arquivo só com aperiódicas carregam desde a Fase 7)*

**A3.4 — Modificação de tarefas**
- [x] O usuário modifica o estado atual de qualquer tarefa em qualquer passo/estágio.
- [x] Modificação semanticamente inválida ou impossível é indicada claramente ao usuário.

### A4 — Algoritmos de escalonamento

- [x] Rate Monotonic preemptivo (string `RM`).
- [x] Earliest Deadline First preemptivo (string `EDF`).

**A4.1 — Escolha do algoritmo**
- [x] O usuário indica o algoritmo antes da simulação.

**A4.2 — Mecanismo extensível**
- [x] Novos algoritmos são incluídos sem modificar o código da simulação.
- [x] O escalonador é uma função que retorna a próxima tarefa a executar.
- [ ] (Ideal, opcional) O escalonador fica numa biblioteca dinâmica fora do executável. *(decidido não fazer: o registro registerScheduler e o teste FIFO atendem A4.2)*

**A4.3 — Desempate (para todos os algoritmos, nesta ordem)**
- [x] (1) A tarefa que estava executando imediatamente antes do escalonamento.
- [x] (2) O prazo da tarefa.
- [x] (3) O instante de ingresso: a que chegou antes.
- [x] (4) A duração: a menor.
- [x] (5) Sorteio.
- [x] Quando há sorteio, um elemento gráfico aparece no Gantt no instante em que a tarefa escolhida (re)começa.

**A4.4 — Tipos de tarefa no Projeto A**
- [x] A simulação suporta somente tarefas periódicas.
- [x] Tarefas aperiódicas são ignoradas. *(inclusive quando todas são aperiódicas: carrega com aviso e termina no instante 0)*
- [x] A presença de tarefas aperiódicas é reportada claramente ao usuário.
- [x] Cada tarefa periódica termina após executar 10 vezes.

### A5 — Interface
- [ ] A interface é intuitiva e fácil de entender e usar. *(subjetivo: avaliar na defesa)*
- [x] Qualquer erro durante o uso é apresentado claramente, com o motivo. *("Ir para o instante" e "avançar" respeitam o teto de memória com mensagem: Fase 7)*

---

## Pendências a confirmar com o professor

- [x] Uma pasta portátil com `.exe` + DLLs atende o RG5? **RESPONDIDO (Q9):** atende desde que não seja preciso instalar o Qt antes (D26).
- [x] Qual o papel do `quantum` no RM e no EDF? **RESPONDIDO (Q4):** tem que ser lido do arquivo; usar ou não é específico de cada escalonador (RM/EDF ignoram).
- [x] No desempate (A4.3), o "prazo" é o relativo ou o absoluto? **RESPONDIDO (Q11):** prazo absoluto da ocorrência atual (D14).
- [x] IDs `BSxx/PSxx/DSxx/SSxx` no A? **RESPONDIDO (Q12):** com período > 0 são periódicas comuns; não estarão nos testes da defesa A (D7).
- [ ] A pasta `dist/` (Q9) continua valendo depois da Q13 ("tudo tem que estar empacotado")? *(aberta Q-A11; padrão: pasta `dist/`, D26)*
- [ ] Qual a ordem dos eventos dentro de um tick? *(aberta Q-A10; padrão em design.md §6)*
- [ ] O `id` é sempre numérico (afeta a ordem do eixo Y)? *(aberta Q-A6; padrão: string, ordem natural, D4)*
- [ ] Cores repetidas no arquivo geram erro ou aviso? *(aberta Q-A7; padrão: aviso, D17)*
- [ ] "Executar 10 vezes" significa 10 ativações concluídas? *(aberta Q-A8; padrão: D6)*
- [ ] Qual o escopo dos eventos do A3.1 (mutex, E/S) no Projeto A? *(aberta Q-A9; padrão: guardar texto cru, sem simular)*
