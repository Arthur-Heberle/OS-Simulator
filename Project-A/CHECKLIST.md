# Projeto A — Checklist de requisitos

> **Fonte:** "Simulação de um Sistema Operacional Multitarefa de Tempo Compartilhado", versão 0.7 (15/10/26), Prof. Dr. Marco Aurélio Wehrmeister.
>
> **Regra:** marcar `[x]` somente quando o item estiver **implementado e testado**.
>
> Os códigos são `RGn` para requisitos gerais e `An.n` para o Projeto A, seguindo a numeração do enunciado.

---

## Requisitos gerais (obrigatórios em A e em B)

### RG1 — Tipo de sistema
- [ ] Simula a execução de tarefas num SO multitarefa **preemptivo de tempo compartilhado**.

### RG2 — Processadores
- [ ] O computador simulado tem mais de um processador capaz de executar as tarefas.
- [ ] A quantidade de processadores é configurada pelo usuário.
- [ ] Mínimo de 1 e máximo de N processadores.
- [ ] O valor de N está definido e documentado.

### RG3 — Visualização gráfica
- [ ] Mostra a execução das tarefas ao longo do tempo, para cada processador.
- [ ] Todos os eventos importantes/relevantes aparecem claramente.
- [ ] Cada tipo de evento tem um elemento gráfico distinto.
- [ ] A visualização tem uma legenda desses elementos.
- [ ] Indica claramente em qual processador cada tarefa está executando.

### RG4 — Controle e configuração
- [ ] O software é configurável.
- [ ] O usuário controla a simulação.
- [ ] O usuário controla as características do sistema simulado.
- [ ] É possível avançar a simulação.
- [ ] É possível retroceder a simulação.
- [ ] O estado atual de cada tarefa pode ser modificado manualmente em qualquer passo/estágio.

### RG5 — Execução sem instalação
- [ ] A linguagem foi escolhida (C++17 / Qt 6).
- [ ] Executa sem instalar biblioteca extra, software, runtime ou equivalente.
- [ ] Testado num computador limpo, sem nada instalado.

### RG6 — Comentários
- [ ] Todo o código-fonte está comentado.
- [ ] Os comentários explicam o que cada parte/função/componente faz.
- [ ] Os comentários explicam as razões/justificativas de todas as decisões de implementação.

---

## Projeto A

### A1 — Simulação

**A1.1 — Relógio global**
- [ ] O sistema tem um relógio global.
- [ ] A passagem do tempo é simulada em ticks.
- [ ] A cada tick, verifica se há ação ou evento a realizar.
- [ ] O tempo da simulação não depende do tempo real.

**A1.2 — Múltiplos processadores e fila global**
- [ ] O sistema tem múltiplos processadores/CPUs/cores.
- [ ] O escalonador minimiza a ociosidade: nunca há tarefa na fila de prontos com CPU sem uso.
- [ ] A fila de prontos é **global**, não uma por CPU.
- [ ] Quando não há tarefa para um processador, ele é **desligado**.
- [ ] O período em que cada processador fica desligado é apresentado ao usuário.
- [ ] O período em que cada processador fica desligado aparece no gráfico.

**A1.3 — TCB**
- [ ] Todas as informações de cada tarefa (antes, durante e depois da simulação) ficam numa **única estrutura de dados** (TCB).

**A1.4 — Características de tempo**
- [ ] A simulação segue as características de tempo de cada tarefa: ingresso, duração, instante dos eventos etc.

**A1.5 — Modos de execução**
- [ ] (a) Modo passo a passo.
- [ ] (b) Modo de execução completa, sem intervenção humana.

**A1.5.1 — Passo a passo como depurador**
- [ ] Permite examinar o estado atual e as informações correntes do sistema.
- [ ] Permite examinar o estado de cada tarefa individualmente.

**A1.5.2 — Avançar, retroceder e editar**
- [ ] O usuário avança e retrocede a simulação quando quiser.
- [ ] O histórico do estado do sistema, a cada passo, fica armazenado em memória.
- [ ] Retroceder e avançar usam esse histórico.
- [ ] Retroceder e avançar alteram o relógio global.
- [ ] O usuário modifica o estado atual ou as características de qualquer tarefa em qualquer passo.

**A1.5.3 — Execução completa**
- [ ] Mostra apenas o resultado final, sem os passos intermediários.

### A2 — Visualização (gráfico de Gantt)

**A2.1 — Representação dos estados**
- [ ] O Gantt mostra as tarefas executando ao longo do tempo.
- [ ] O Gantt mostra o processador usado em cada período de execução.
- [ ] Tarefa executando é desenhada com a cor definida no parâmetro de entrada.
- [ ] Cada tarefa tem uma cor diferente (comportamento para cores repetidas definido).
- [ ] Tarefa na fila de prontos, sem executar, aparece como **ausência de cor**.
- [ ] Tarefa suspensa, por qualquer motivo, aparece em **preto** com preenchimento diferente do de execução (ex.: pontilhado).

**A2.2 — Chegada e término**
- [ ] A chegada de uma tarefa nova aparece claramente (ícone, figura, seta etc.).
- [ ] O término de uma tarefa aparece claramente.
- [ ] Esses eventos aparecem no instante exato em que acontecem.

**A2.3 — Atualização do gráfico**
- [ ] A atualização é compatível com o modo de execução. O enunciado cita o requisito 1.4, mas o conteúdo corresponde ao 1.5.
- [ ] No passo a passo, o Gantt atualizado aparece na tela a cada passo.

**A2.4 — Exportação da imagem**
- [ ] Ao final da simulação, gera um arquivo de imagem (JPG, PNG, SVG etc.).
- [ ] A imagem corresponde à situação final, depois de todas as tarefas executarem.
- [ ] A imagem **não** é um print da janela.
- [ ] A imagem contém a simulação inteira.
- [ ] A imagem não está limitada a nenhum tempo máximo de simulação.

**A2.5 — Estilo e ordem**
- [ ] O Gantt é similar às figuras da seção 6.4 do livro do Prof. Maziero.
- [ ] O eixo Y está em ordem decrescente de ID: a tarefa de ID menor fica mais perto do eixo X.

### A3 — Configuração

**A3.1 — Parametrização antes e durante a simulação**
- [ ] Conjunto de tarefas.
- [ ] Características individuais de cada tarefa.
- [ ] Algoritmo de escalonamento.
- [ ] Eventos: solicitação/liberação de mutex, E/S, envio/recebimento de dados etc. (escopo no A a confirmar com o professor).
- [ ] Modificação sem sentido ou impossível durante a execução é recusada com aviso claro e justificativa.

**A3.2 — Valores padrão**
- [ ] Há valores padrão sugeridos para todos os parâmetros da simulação e do software.
- [ ] Os padrões são sobrescritos pelos valores do arquivo de configuração.
- [ ] Os padrões são sobrescritos pelos valores digitados nas telas de parâmetros.

**A3.3 — Formato do arquivo**
- [ ] O arquivo é texto simples (plain text).
- [ ] Linha 1: `algoritmo_escalonamento;quantum;qtde_cpus`.
- [ ] Linhas seguintes: `id;cor;ingresso;duracao;periodo;prazo;lista_eventos`.
- [ ] `algoritmo_escalonamento` define o algoritmo usado.
- [ ] `quantum` é o tempo máximo que uma tarefa pode executar.
- [ ] `qtde_cpus` aceita valores entre 1 e N.
- [ ] `id` é um identificador único.
- [ ] `cor` está em RGB hexadecimal (ex.: `F0E0D0`).
- [ ] `ingresso` é o instante de criação da tarefa.
- [ ] `duracao` é o tempo de execução da tarefa.
- [ ] `periodo > 0`: tarefa periódica, ativada a cada `n` ticks.
- [ ] `periodo = 0`: tarefa aperiódica.
- [ ] `periodo < 0`: tratado como **erro** no arquivo.
- [ ] `prazo` (deadline) conta a partir da ativação.
- [ ] Prazo ultrapassado é um erro e aparece no Gantt.
- [ ] A tarefa que perdeu o prazo continua executando até o tempo executado ser ≥ à duração.
- [ ] `lista_eventos` é lida e armazenada.

**A3.3.1 — Estrutura do arquivo**
- [ ] A linha 1 tem os parâmetros gerais. Da linha 2 em diante, uma tarefa por linha.
- [ ] O arquivo tem no mínimo 2 linhas.
- [ ] Não há limite máximo de tarefas.

**A3.3.2 — Maiúsculas e minúsculas**
- [ ] Todas as strings são tratadas sem diferenciar maiúsculas e minúsculas (ex.: `PRIOP` = `priop` = `PrioP`).

**A3.3.3 — Ponto e vírgula final**
- [ ] Linhas terminadas com `;` são aceitas.
- [ ] Linhas terminadas sem `;` são aceitas.

**A3.3.4 — Local do arquivo**
- [ ] Carrega arquivo com **qualquer nome**.
- [ ] Carrega de **qualquer local**: qualquer diretório, outra partição ou disco externo (pendrive).
- [ ] **Não** usa arquivo fixo.

**A3.3.5 — Eventos (preparação para o B)**
- [ ] A estrutura suporta inúmeros eventos ao longo da execução de uma tarefa.

**A3.3.6 — Erros no arquivo**
- [ ] Erros de carregamento são mostrados ao usuário com o motivo.
- [ ] Erros no conteúdo do arquivo são mostrados ao usuário com o motivo.
- [ ] Espaços e linhas em branco **não** são erros e são ignorados.

**A3.3.7 — Falha grave**
- [ ] Nenhum arquivo válido, que obedeça ao formato, gera erro de carregamento (testado com uma bateria de arquivos).

**A3.4 — Modificação de tarefas**
- [ ] O usuário modifica o estado atual de qualquer tarefa em qualquer passo/estágio.
- [ ] Modificação semanticamente inválida ou impossível é indicada claramente ao usuário.

### A4 — Algoritmos de escalonamento

- [ ] Rate Monotonic preemptivo (string `RM`).
- [ ] Earliest Deadline First preemptivo (string `EDF`).

**A4.1 — Escolha do algoritmo**
- [ ] O usuário indica o algoritmo antes da simulação.

**A4.2 — Mecanismo extensível**
- [ ] Novos algoritmos são incluídos sem modificar o código da simulação.
- [ ] O escalonador é uma função que retorna a próxima tarefa a executar.
- [ ] (Ideal, opcional) O escalonador fica numa biblioteca dinâmica fora do executável.

**A4.3 — Desempate (para todos os algoritmos, nesta ordem)**
- [ ] (1) A tarefa que estava executando imediatamente antes do escalonamento.
- [ ] (2) O prazo da tarefa.
- [ ] (3) O instante de ingresso: a que chegou antes.
- [ ] (4) A duração: a menor.
- [ ] (5) Sorteio.
- [ ] Quando há sorteio, um elemento gráfico aparece no Gantt no instante em que a tarefa escolhida (re)começa.

**A4.4 — Tipos de tarefa no Projeto A**
- [ ] A simulação suporta somente tarefas periódicas.
- [ ] Tarefas aperiódicas são ignoradas.
- [ ] A presença de tarefas aperiódicas é reportada claramente ao usuário.
- [ ] Cada tarefa periódica termina após executar 10 vezes.

### A5 — Interface
- [ ] A interface é intuitiva e fácil de entender e usar.
- [ ] Qualquer erro durante o uso é apresentado claramente, com o motivo.

---

## Pendências a confirmar com o professor

- [ ] Uma pasta portátil com `.exe` + DLLs atende o RG5?
- [ ] Qual o papel do `quantum` no RM e no EDF?
- [ ] No desempate (A4.3), o "prazo" é o relativo ou o absoluto?
- [ ] Qual a ordem dos eventos dentro de um tick?
- [ ] O `id` é sempre numérico (afeta a ordem do eixo Y)?
- [ ] Cores repetidas no arquivo geram erro ou aviso?
- [ ] "Executar 10 vezes" significa 10 ativações concluídas?
- [ ] Qual o escopo dos eventos do A3.1 (mutex, E/S) no Projeto A?
