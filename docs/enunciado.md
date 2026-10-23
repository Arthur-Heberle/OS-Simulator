# enunciado.md — Fonte da verdade sobre O QUE o trabalho exige

> **Para o Claude Code:** este arquivo é a especificação do que o professor exige. Ele vem ANTES de qualquer decisão de design.
> Ordem de autoridade em caso de conflito: **(1) enunciado v0.7 (seção 2) → (2) respostas do professor (seção 4) → (3) decisões da equipe em `design.md`**.
> Nunca invente respostas do professor. Onde algo está em aberto (seção 5), use o **valor padrão** indicado, isole a escolha em UM ponto do código (constante/função) e avise o usuário.
> Os requisitos são citados pelo número do enunciado: `RGn` = requisito geral n; `An.n` = Projeto A; `Bn.n` = Projeto B. Use esses códigos nos comentários e nos testes.

---

## 1. Contexto

- Disciplina: Sistemas Operacionais, Engenharia de Computação, UTFPR. Professor: Prof. Dr. Marco Aurélio Wehrmeister.
- Livro adotado rigorosamente: MAZIERO, C. *Sistemas Operacionais: Conceitos e Mecanismos*. Ed. UFPR, 2019. O enunciado cita a **seção 6.4** (figuras de diagramas de tempo/Gantt) e o **capítulo "Escalonamento de Tempo-Real"** (no Moodle; seção 2.8 trata dos servidores).
- O projeto tem duas partes **incrementais**: o Projeto B complementa o A. Por isso o A deve ser feito já preparando o B.
- Equipe: 2 alunos. Linguagem: C++17. Interface: Qt 6 Widgets. Repositório: `OS-Simulator`.
- **Datas:** defesa do A em **09/11/26**; defesa do B em **07/12/26**. Data de submissão no Moodle: ver Moodle (não consta nos arquivos; entrega fora do prazo = nota ZERO).
- Data de referência deste material: 23/10/2026.

---

## 2. Transcrição integral do enunciado (versão 0.7 – 15/10/26)

> Transcrição fiel. Erros do original estão mantidos e marcados com **[sic]**; comentários da equipe estão em blocos `**[NOTA]**`.

### Simulação de um Sistema Operacional Multitarefa de Tempo Compartilhado
Versão 0.7 – 15/10/26 — Autor: Prof. Dr. Marco Aurélio Wehrmeister

### Objetivos
1. Aprofundar o estudo do gerenciamento de tarefas através de implementação de escalonadores, a execução de múltiplas tarefas e do escalador **[sic: escalonador]**, e a apresentação gráfica dessa execução do sistema ao longo do tempo.
2. Aprimorar o conhecimento sobre interação entre tarefas usando serviços de exclusão mútua comumente encontrados em sistemas operacionais multi-tarefa;
3. Compreender e resolver os problemas de escalonamento, p.ex., inanição, inversão de prioridades, impasse, etc.

### Requisitos Gerais
Todos os requisitos abaixo devem ser obrigatoriamente contemplados em **AMBOS** os projetos:

- **RG1.** O projeto deve implementar um software que simula a execução das tarefas em um sistema operacional multitarefa preemptivo de tempo compartilhado;
- **RG2.** O computador possui mais de um processador capaz de executar as tarefas do sistema. A quantidade de processadores é configurada pelo usuário, sendo, no mínimo 01 (um) e no máximo N processadores.
- **RG3.** O software deve permitir a visualização gráfica da execução das tarefas dentro do sistema operacional ao longo do tempo para cada processador. Todos os eventos importantes/relevantes que acontecem ao longo da execução das tarefas devem aparecer claramente na visualização gráfica na forma de um elemento gráfico distinto para cada tipo de evento. Uma legenda destes elementos gráficos deve ser mostrada na visualização. A visualização deve indicar, claramente, em qual processador cada tarefa está executando.
- **RG4.** O software deve ser configurável e permitir que o usuário controle a simulação do sistema, bem como as características do sistema que está sendo simulado. Deve ser possível avançar e retroceder a simulação, sendo que o estado atual de cada tarefa em execução pode ser modificado manualmente em qualquer passo/estágio da simulação.
- **RG5.** A linguagem a ser utilizada para implementação do projeto é de livre escolha, no entanto, é necessário que o software criado possa ser executado **sem a necessidade de instalar nenhuma biblioteca extra, ou qualquer outro software/runtime, ou equivalente**.
- **RG6.** Todo o código fonte deve ser comentado de forma que qualquer outra pessoa consiga compreender o que cada parte/função/componente de código faz e as razões/justificativas de todas as decisões/escolhas de implementação;

---

### PROJETO A – Implementação Inicial do Simulador

**A1.** O projeto deve implementar um software que simula a execução de tarefas em um sistema operacional multitarefa preemptivo de tempo compartilhado.

- **A1.1.** O sistema simulado deve incluir um relógio global do sistema, sendo que a passagem do tempo deve ser simulada na forma de ticks de relógio. A simulação deve verificar se existe a necessidade de realizar alguma ação ou evento (ou algo relevante para a simulação) a cada tick do relógio. A passagem do tempo na simulação não precisa estar atrelada ao tempo no mundo real, ou seja, o tempo de simulação pode ser incrementado/decrementado sem esperar a mesma quantidade de tempo passar no mundo real.
- **A1.2.** O sistema possui múltiplos processadores/CPUs/cores para executar as tarefas. O escalonador deve minimizar a ociosidade destes processadores, ou seja, **não devem existir tarefas na fila de prontos e CPUs sem uso**. A fila de prontos é **global** do sistema, ou seja, não é individual para cada CPU. Caso não seja possível atribuir uma tarefa para executar em um processador, **o processador deve ser desligado**. A informação sobre o período de tempo que um processador está desligamento **[sic: desligado]** deve ser apresentada para o usuário e também na visualização gráfica da execução das tarefas.
- **A1.3.** As informações de cada tarefa (antes, durante, depois da simulação) devem ser armazenadas durante a simulação em uma **única estrutura de dados**, p.ex., Task Control Block (TCB);
- **A1.4.** A simulação da execução das tarefas deve seguir as características de tempo informadas para cada tarefa, p.ex., instante de ingresso, duração, instante da ocorrência de eventos, etc.
- **A1.5.** Dois modos de execução da simulação devem ser implementados: (a) execução passo-a-passo; (b) execução completa sem intervenção humana.
  - **A1.5.1.** No caso execução passo-a-passo (opção a), deve ser possível examinar o estado atual e as informações correntes do sistema e também de cada tarefa individualmente. A ideia é que esse modo de execução forneça um debugger do sistema.
  - **A1.5.2.** No caso execução passo-a-passo (opção a), o simulador deve permitir que o usuário avance e retroceda a simulação conforme seu desejo. Dessa forma, o histórico do estado do sistema, a cada passo da execução da simulação, deve ser armazenado na memória e deve ser usado no processo de retroceder/avançar a simulação. Retroceder/avançar a simulação tem impacto no relógio global do sistema. Além disso, simulador deve permitir que o usuário modifique o estado atual ou as características de qualquer tarefa do sistema em qualquer passo/estágio da simulação.
  - **A1.5.3.** No caso execução completa (opção b), o software mostra apenas o resultado final da simulação do sistema. Não é necessário mostrar os passos intermediários da simulação.

**A2.** O software deve permitir a visualização gráfica da execução das tarefas dentro do sistema operacional ao longo do tempo.

- **A2.1.** Apresentar um gráfico de gantt que mostra as tarefas sendo executadas ao longo do tempo e o processador usado em cada período de execução. Quando uma tarefa está executando, tal informação deve ser indicada com uma cor (definida em um parâmetro de entrada) e cada tarefa deve ter uma cor diferente. Quando uma tarefa está na fila de prontas **[sic]** e ela não está sendo executada, tal informação deve ser indicada como a **ausência da cor**. Quando uma tarefa está suspensa, por qualquer motivo, tal informação deve ser indicada com a **cor preta e preenchimento diferente** daquele usado para indicar a execução da tarefa (p.ex. preenchimento com pontos ao invés de sólido).
- **A2.2.** Quando uma tarefa nova chega no sistema isso deve ser claramente mostrado no gráfico de gantt. Da mesma forma, quando a tarefa termina a sua execução, isso deve ser claramente mostrado no gráfico de gantt. Para isso utilize ícones, figuras, setas ou outro elemento gráfico que indique esses eventos. Esses eventos devem ser mostrados **no momento exato** em que eles acontecem.
- **A2.3.** A atualização do gráfico de gantt deve ser compatível com o modo de execução da simulação (conforme requisito 1.4 **[sic: o conteúdo corresponde ao 1.5]**). Na execução passo a passo, o gráfico de gantt atualizado deve ser mostrado na tela para o usuário a cada passo da simulação.
- **A2.4.** Ao final da simulação, o software deve gerar um arquivo do tipo imagem (p.ex. JPG, PNG, SVG, etc) contendo o gráfico de gantt correspondendo a situação final do sistema após a execução de todas as tarefas. A figura gerada **não pode ser um print da janela** que mostra a simulação, deve conter a imagem que representa **toda a simulação** e **não deve estar limitada a nenhum tempo máximo de simulação**.
- **A2.5.** A visualização do gráfico de gantt deve ser similar as figuras da seção 6.4 do livro do Prof. Maziero. A ordem em que as tarefas aparecem no eixo Y deve ser **decrescente em relação ao ID**, ou seja, a tarefa com ID menor é a mais próxima do eixo X, a tarefa com o segundo ID menor aparece em cima da primeira, e assim por diante.

**A3.** O software deve ser configurável e permitir que o usuário controle a simulação do sistema, bem como as características do sistema que está sendo simulado.

- **A3.1.** O software deve permitir a configuração/parametrização do sistema a ser simulado **antes e durante** a execução da simulação, incluindo, mas não limitado a: conjunto de tarefas, características individuais de cada tarefa, algoritmo de escalonamento, eventos como solicitação/liberação de um mutex, operação de entrada/saída, envio/recebimento de dados, entre outros. Caso alguma modificação não faça sentido ou não possa ser aplicada durante a execução, o usuário precisa ser informado claramente que a modificação não pode ser feita, justificando o motivo.
- **A3.2.** O software deve sugerir **valores padrão** para todos os parâmetros de configuração da simulação ou do software em si. Esses valores padrão devem ser sobrescritos por valores indicados pelo usuário, tanto na hora de carregar o arquivo de configuração como também nas telas que permite a modificação dos parâmetros da simulação.
- **A3.3.** A configuração/parametrização do sistema a ser simulado deve ser carregada a partir de um **arquivo texto, formato texto simples (plain text)**, e deve seguir o seguinte formato:

```
algoritmo_escalonamento;quantum;qtde_cpus
id;cor;ingresso;duracao;periodo;prazo;lista_eventos
```

  onde:
  - `algoritmo_escalonamento` indica o algoritmo de escalonamento a ser usado na simulação;
  - `quantum` indica o período máximo de tempo que uma tarefa pode executar;
  - `qtde_cpus` é a quantidade de processadores que o sistema possui para executar as tarefas (valores entre 1 e N);
  - `id` é o identificador único da tarefa pode executar **[sic]**;
  - `cor` é cor que identifica a execução da tarefa no formato RGB em Hexadecimal, p.ex. "F0E0D0" indica Red=F0, Green=E0, Blue=D0;
  - `ingresso` indica o instante de tempo que a tarefa foi criada;
  - `duracao` indica o tempo de execução da tarefa;
  - `periodo` indica o período no qual uma tarefa é ativada: valores maiores que zero indicam que a tarefa tem ativação periódica a cada "n" ticks de relógio; valor zero indica que a tarefa é aperiódica, ou seja, é ativada apenas uma vez no momento da sua criação; **valores menores que zero representam erros no arquivo**.
  - `prazo` indica o tempo máximo que uma ocorrência/execução da tarefa deve terminar a sua execução (deadline). O prazo começa a contar a partir do momento que a tarefa for ativada. Caso o tempo de execução da ocorrência/execução da tarefa ultrapasse o prazo (deadline), isso indica um erro e deve ser mostrado no gráfico de gantt. **A tarefa que perdeu o prazo continua sendo executada** e só termina quando o tempo de execução da tarefa for igual ou maior que a duração da tarefa.
  - `lista_eventos` indica uma lista de eventos que ocorre durante a execução da tarefa.

  - **A3.3.1.** A primeira linha do arquivo de configuração indica parâmetros gerais da simulação ou do sistema operacional. A partir da segunda linha, cada linha indica os parâmetros de uma tarefa do sistema. O arquivo de configuração deve ter **pelo menos duas linhas** (primeira linha: parâmetros do sistema; a partir da segunda linha: parâmetros de uma tarefa, uma por linha), sendo que **não há limite máximo** para o número de tarefas a serem executadas no sistema.
  - **A3.3.2.** Todas as strings contidas no arquivo de configurações devem ser tratas **[sic]** de forma igual para o caso possuírem letras maiúsculas, minúsculas, ou a mistura de ambos. Exemplo: "PRIOP", "priop" ou "PrioP" deve indicar a mesma configuração: escalonador preemptivo por prioridades.
  - **A3.3.3.** Qualquer linha do arquivo de configuração pode terminar com o caracter ";" ou não. Os dois casos devem ser tratados no simulador.
  - **A3.3.4.** O simulador deve conseguir carregar um arquivo de configuração **com qualquer nome, a partir de qualquer lugar no sistema de arquivos** do computador, incluindo, mas não limitado a: qualquer diretório da partição principal ou qualquer outra partição do disco do computador ou de um disco externo (ex. pendrive). **Não é permitido utilizar um arquivo fixo** como arquivo de configuração.
  - **A3.3.5.** A lista de eventos de cada tarefa vai ser tratada no projeto B. No entanto, deve-se atentar para o fato da possibilidade de ocorrência de **inúmeros eventos** ao longo da execução de uma tarefa.
  - **A3.3.6.** Caso existam erros de carregamento do arquivo de configuração, ou erros no próprio arquivo de configuração, eles devem ser **claramente mostrados ao usuário indicado claramente o motivo do problema**. **Espaços e linhas em branco não representam erros e devem ser ignorados.**
  - **A3.3.7.** Qualquer erro ao carregar o arquivo de configuração válido (ou seja, que obedeça ao formato estabelecido nos requisitos) ou que impossibilitem carregar qualquer arquivo de configuração válido indicam **falha grave** no projeto e **impedirão a realização da sua demonstração**.
- **A3.4.** O simulador deve permitir que o usuário modifique o estado atual de qualquer tarefa do sistema em qualquer passo/estágio da simulação. Caso a modificação solicitada seja semanticamente inválida ou não seja possível de ser aplicada, isso deve ser indicado claramente para o usuário.

**A4.** Os seguintes algoritmos de escalonamento devem ser implementados: **Rate Monotonic preemptivo** (string `RM` no arquivo de configurações), **Earliest Deadline First preemptivo** (string `EDF` no arquivo de configurações) — ver capítulo "Escalonamento de Tempo-Real" no Moodle.

- **A4.1.** O usuário deve indicar, antes da simulação, qual é o algoritmo de escalonamento a ser usado;
- **A4.2.** O mecanismo de implementação do algoritmo de escalonamento deve ser flexível/configurável/parametrizável de modo que **novos algoritmos possam ser facilmente incluídos no simulador sem a necessidade de modificar o código da simulação**. Idealmente, o escalonador é apenas uma função que retorna qual é a próxima tarefa a ser executada, que pode, inclusive, estar contida em uma biblioteca dinâmica fora do arquivo binário do executável do simulador;
- **A4.3.** Em caso de empates na escolha da próxima tarefa (para todos os algoritmos), o desempate deve considerar os seguintes critérios, **nessa ordem**: (1) a tarefa selecionada é a que está executando imediatamente antes do escalonamento ser acionado (para evitar uma troca de contexto para a própria tarefa); (2) o prazo da tarefa; (3) o instante de ingresso da tarefa: quem chegou antes é escolhida; (4) duração da tarefa: duração menor deve se escolhida; (5) sorteio (quando isso acontecer, **um elemento gráfico deve ser mostrado no gráfico de gantt no momento em que a tarefa escolhida (re)começar a execução**).
- **A4.4.** No projeto A, a simulação deve suportar **somente tarefas periódicas**, sendo que as tarefas aperiódicas serão abordadas no projeto B. Caso exista a indicação de tarefas aperiódicas no projeto A, elas devem ser **ignoradas e essa situação deve ser reportada ao usuário de forma clara**. Por definição, as tarefas periódicas nunca terminam. No entanto, com objetivo de terminar a simulação, assume-se que **uma tarefa periódica termina após ela ter executado 10 vezes**.

**A5.** A interface de interação do usuário com o simulador deve ser intuitiva, fácil de entender e usar. Qualquer erro que ocorrer durante o uso do software do simulador deve ser apresentado de forma clara para o usuário indicado o motivo da mensagem de erro ser apresentada.

---

### PROJETO B – Refinamento da Simulação do Sistema Operacional Multitarefa
(O B é incremental sobre o A. Implementar só depois do A, mas **preparar o terreno desde o A**.)

**B1.** Ampliar os tipos de tarefas suportados na simulação, incluindo a criação/execução de tarefas **aperiódicas e esporádicas** (ver capítulo "Escalonamento de Tempo-Real").
- **B1.1.** A tarefas periódicas ou esporádicas **[sic: provavelmente "aperiódicas ou esporádicas"]** não devem interferir no escalonamento das tarefas periódicas, ou seja, a sua execução deve ser feita usando algum dos **servidores** apresentados no capítulo "Escalonamento de Tempo-Real" a escolha do usuário.
- **B1.2.** No arquivo de configuração, uma tarefa aperiódica é indicada como tendo o valor do `periodo` igual 0. Este valor deve ser guardado na TCP **[sic: TCB]** da tarefa.
- **B1.3.** No arquivo de configuração, uma tarefa esporádica é indicada como tendo um **valor negativo** no campo `periodo`. O valor absoluto do campo `periodo` indica o período mínimo entre duas ocorrências consecutivas da tarefa. Cada ocorrência da tarefa esporádica é informada no campo `lista_eventos` da seguinte forma: `instante01,instante02,...` onde cada instante é um tempo **relativo ao instante de criação da tarefa**, p.ex. para uma tarefa criada no tempo 10, o valor zero indica que ela já está pronta para executar e o valor 5 indica que ela só fica pronta para executar no tempo 15. Ao ler a linha de configuração, deve-se realizar uma **verificação da propriedade de tempo mínimo** entre duas execuções consecutivas da tarefa.

**B2.** O usuário pode indicar um ou mais servidores como sendo tarefas periódicas no sistema. O tipo do servidor é indicado no ID da tarefa como: (a) `BSxx` indica o Background Server; (b) `PSxx` indica o Pooling **[sic: Polling]** Server; (c) `DSxx` indica o Deferrable Server; e (d) `SSxx` indica o Sporadic Server; "xx" indica um numero inteiro para diferenciar os servidores de um mesmo tipo.
- **B2.1.** Os demais campos da linha de configuração permanecem com a mesma semântica e definem como cada servidor vai se comportar (ver seção 2.8 do capítulo "Escalonamento de Tempo-Real").
- **B2.2.** Todos os servidores têm acesso à fila de tarefas aperiódicas ou esporádicas prontas para serem executadas e pode escolher uma delas para executar (conforme a sua política de escalonamento), retirando-a da fila e tornando-a inacessível para outros servidores.

**B3.** Executar o **teste de escalabilidade [sic: escalonabilidade]** do sistema **antes de qualquer tarefa ser aceita** e incluída para execução no sistema, incluindo a avaliação de todos os tipos de tarefas que são criadas durante a execução do sistema, p.ex., tarefas periódicas, aperiódicas, esporádicas e servidores.

**B4.** A execução das tarefas aperiódicas/esporádicas e dos servidores deve ser apresentada de forma que se possa visualizar claramente no gráfico de gantt quando o servidor está executando e quando as tarefas aperiódicas/esporádicas estão executando. Sugestão: usar um padrão de preenchimento diferente de sólido, ou usar uma borda diferente em espessura ou padrão de desenho.

> **[NOTA]** Os objetivos 2 e 3 (mutex, inversão de prioridades, impasse) não têm requisitos detalhados no Projeto B. Tratar como pergunta em aberto (seção 5, Q-B1) e **deixar ganchos** no modelo (ver `design.md` §14).

---

## 3. Regras de avaliação, defesa e entrega (informadas pelo aluno)

### Projetos + Defesa
O enunciado é objeto de avaliação por **arguição oral, avaliação do código-fonte e avaliação da execução**. Durante a defesa a equipe deverá:
- **Compilar o projeto na presença do professor** usando o comando Make, CMake ou outro sistema de automação de compilação;
- Mostrar os arquivos de código-fonte criados;
- Responder perguntas sobre o funcionamento do projeto e sobre a implementação;
- Cada equipe tem **no máximo 10 minutos** para a defesa.

**Datas das defesas:** 09/11/26 — Projeto A; 07/12/26 — Projeto B.

**Observações:**
- Equipes de **DOIS** alunos. As defesas são em equipe, mas **a nota é INDIVIDUAL**. Cada aluno precisa saber explicar **todas** as partes.
- Trabalho feito parcialmente → nota proporcional ao que foi feito.
- Submeter via link do Moodle **apenas os arquivos solicitados**.
- **Não é aceita entrega de arquivos compactados (zip, rar e afins): nota ZERO.**
- **Não é aceita entrega fora do prazo do Moodle: nota ZERO.**
- Há detecção de plágio (comparação com a turma e com turmas anteriores da base do professor e do prof. Maziero). Plágio em qualquer arquivo → ZERO na defesa de todos os envolvidos, mesmo depois da defesa. Regras em "Regras das atividades de laboratório".

### Entrega do Projeto A (e do B, regras idênticas)
Regras gerais: (1) pode ser individual ou em dupla; (2) entrega via Moodle até a data/hora indicadas, atraso não é aceito; (3) arquivos compactados → nota ZERO; (4) software de similaridade, com penalidade.

Regras específicas — Simulador de SO:
1. Entregar **todos os arquivos de código e configuração** criados, desenvolvidos ou modificados;
2. **Todos os arquivos** devem ter comentários explicando o que acontece em cada parte relevante do código;
3. Enviar **um arquivo PDF** contendo **todas as telas criadas**, com a explicação de **cada elemento visual** incluído na tela e **de onde vem a informação** mostrada.

---

## 4. Respostas do professor às dúvidas (verbatim organizado)

> Prof. = Prof. Marco Wehrmeister. Itens de Q1 a Q8 vêm de `duvidas_projeto_a.txt`; Q9 foi respondida depois.

**Q1 — Bibliotecas externas (ex.: SFML) são permitidas?**
Prof.: "Não há nada no enunciado do trabalho que diga que não se pode usar biblioteca externa. O que está escrito é '… é necessário que o software criado possa ser executado SEM a necessidade de INSTALAR nenhuma biblioteca extra', ou seja, o executável gerado precisa ser 'standalone'." (referência dada: diferença entre standalone e installed executable).

**Q2 — A linguagem precisa ser compilada ou pode ser interpretada?**
Prof.: "Qualquer uma das opções. Mas… para executar o software, não se deve exigir instalar nada extra no SO para qual esse simulador foi desenvolvido."

**Q3 — Para qual fila vai uma tarefa periódica que completou seu tempo de computação antes do período acabar?**
Prof.: "Toda fila tem um motivo, uma semântica. Por que uma tarefa entra na fila de suspensão? Pode haver outras filas, com outras semântica? Qual? Como? Por que?"
Réplica do aluno: precisa haver um lugar para tarefas periódicas que já completaram a computação do período; não podem voltar à fila de prontos senão seriam escolhidas antes do próximo período.
Prof.: "Isso é um dos problemas a serem resolvidos. A equipe decide como fazer e depois, na defesa, justifica a decisão."
→ **Obrigação:** decidir e **justificar tecnicamente** (ver D1 em `design.md`).

**Q4 — O `quantum` é obrigatório no cabeçalho, mas RM/EDF não usam. O que fazer?**
Prof. (primeiro): "Não confundir 'quantum' de uma tarefa com o 'relógio' do sistema…" (pediu para citar o requisito). O aluno citou o exemplo do item 3.3 (`algoritmo_escalonamento; quantum;` / `qtde_cpus`).
Prof.: "Ah sim, **tem que ler do arquivo de configuração**. Se vai precisar usar ou não, aí é **específico de cada escalonador**."
Aluno: nos algoritmos a implementar não há quantum; vai "passar reto". (Prof. não objetou.)
→ **Obrigação:** ler, validar, guardar e exibir o quantum; RM e EDF o ignoram; o mecanismo de escalonador deve permitir que outro algoritmo o use.

**Q5 — Ícones de chegada/saída (A2.2): uma vez por tarefa ou a cada ocorrência?**
Prof.: "O texto tem realmente um problema. A indicação da criação da tarefa, na verdade, deveria ser o **momento que ela entra na fila de prontos**. No caso das tarefas periódicas, quando passou o período e ela está pronta para executar novamente. No caso das aperiódicas (defesa B), quando ela foi criada no sistema. O término, por sua vez, é a indicação que ela **terminou uma ocorrência da execução** (para tarefas periódicas) ou terminou e será excluída (para o caso das aperiódicas)."
→ **Obrigação:** ícone de "chegada" **a cada ativação** (entrada na fila de prontos) e ícone de "término" **a cada ocorrência concluída**.

**Q6 — Quantos processadores (N)?**
Prof.: "Não há um limite para o N. No entanto, vocês podem estabelecer um limite máximo mas, se isso for perguntado, vocês vão precisar defender essa decisão de projeto. Respostas como 'não sei', 'achei melhor', 'decidimos assim', etc. sem uma justificativa técnica plausível impactarão negativamente na avaliação do trabalho."
→ **Obrigação:** se houver limite, justificativa técnica (ver D12).

**Q7 — O projeto passou de 20 arquivos (limite de anexos do Moodle). E agora?**
Prof.: "Como assim 20+ 'arquivos essenciais'? Por quê? Qual linguagem? Não precisa mandar o executável. Se não há outra forma de refatorar o código para caber dentro do limite de 20 arquivos, vocês podem mandar todos os arquivos do projeto dentro de um arquivo zip. No dia da defesa vocês defendem/explicam as decisões de projeto que levaram à criação dessa quantidade de arquivos. Mas quem puder, evite essa abordagem."
→ **ATENÇÃO — conflito:** as regras gerais dizem zip = ZERO. Estratégia da equipe: **no máximo 18 arquivos soltos**, sem zip (ver D23). Confirmar por escrito (Q-A1, seção 5).

**Q8 — Classes pequenas implementadas só no `.hpp` para caber no limite: desfazer e usar zip, ou manter?**
Prof.: "Vocês decidem. O importante é vocês demonstrarem que sabem do que estão falando na hora da defesa; que têm conhecimento dos 'como's' e 'por que's' da solução de vocês."
→ Cabeçalhos com implementação inline são aceitáveis.

**Q9 — Qt com pasta (.exe + DLLs, sem instalador) atende o RG5?**
Prof.: "**Atende desde que não seja preciso instalar o Qt antes de conseguir usar o software de vocês.**"
→ **Decisão fechada:** Qt dinâmico + deploy (pasta com `.exe` + DLLs + plugins) é aceito. Ver D26. Mesmo assim: testar a pasta em máquina/VM limpa, sem Qt.

---

## 5. Erros, ambiguidades e perguntas em aberto

| ID | Assunto | Situação | Padrão adotado até resposta (isolar em UM ponto do código) |
|---|---|---|---|
| Q-A1 | Zip: regras gerais dizem ZERO; fórum aceitou zip se >20 arquivos | **Aberta** | Entregar ≤ 18 arquivos soltos, sem zip. Pedir confirmação por escrito |
| Q-A2 | `periodo < 0` no A: "erro do arquivo" (texto do A) vs "esporádica" (texto do B) | **Aberta** | Carregar com **aviso**, marcar como esporádica, **ignorar** na simulação (D7). Não é erro fatal (A3.3.7) |
| Q-A3 | Desempate (2) "prazo da tarefa": relativo (campo `prazo`) ou absoluto da ocorrência? | **Aberta** | Prazo **absoluto** da ocorrência corrente (D14) |
| Q-A4 | IDs `BSxx/PSxx/DSxx/SSxx` no A | **Aberta** | Tratar como servidor, **ignorar** com aviso (D7) |
| Q-A5 | A2.3 cita "requisito 1.4" mas é o 1.5 | Erro de redação | Implementar o 1.5 (D19) |
| Q-A6 | `id` sempre numérico? (afeta ordem do eixo Y) | Aberta | `id` string; ordem natural (D4) |
| Q-A7 | Cores repetidas: erro ou aviso? | Aberta | Aviso (D17) |
| Q-A8 | "Executar 10 vezes" = 10 ativações concluídas? | Aberta | Libera 10 ocorrências e termina quando as 10 concluem (D6) |
| Q-A9 | Escopo de `lista_eventos` (mutex, E/S) no A | Aberta | Guardar texto cru, sem simular (A3.3.5 diz que é do B) |
| Q-A10 | Ordem dos eventos dentro de um tick | Aberta | Ordem fixa definida em `design.md` §6 |
| Q-B1 | Mutex, inversão de prioridade, impasse (objetivos 2 e 3) sem requisito no B | Aberta | Ganchos no modelo; decidir no B |
| Q-B2 | Todos os requisitos do A continuam valendo no B? | Aberta | Sim (B é incremental) |
| Q-B3 | B1.1: "periódicas ou esporádicas" = "aperiódicas ou esporádicas"? | Aberta | Assumir aperiódicas/esporádicas |
| — | Qt standalone (pasta com DLLs) | **Respondida (Q9)** | Opção A (D26) |
| — | Papel do quantum | **Respondida (Q4)** | Ler/guardar; RM/EDF ignoram |

**Regra para o Claude Code:** ao encontrar uma pergunta "Aberta", não bloqueie; use o padrão, deixe um comentário `// PERGUNTA EM ABERTO Q-xx: ...` no ponto único de decisão e liste no relatório da fase.
