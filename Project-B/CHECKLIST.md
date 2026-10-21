# Projeto B — Checklist de requisitos

> **Fonte:** "Simulação de um Sistema Operacional Multitarefa de Tempo Compartilhado", versão 0.7 (15/10/26), Prof. Dr. Marco Aurélio Wehrmeister.
>
> **Regra:** marcar `[x]` somente quando o item estiver **implementado e testado**.
>
> Os códigos são `RGn` para requisitos gerais e `Bn.n` para o Projeto B, seguindo a numeração do enunciado.
>
> O Projeto B é um *refinamento* do A. A confirmar com o professor: todos os requisitos do A continuam valendo?

---

## Requisitos gerais (obrigatórios em A e em B)

### RG1 — Tipo de sistema
- [ ] Simula a execução de tarefas num SO multitarefa **preemptivo de tempo compartilhado**.

### RG2 — Processadores
- [ ] O computador simulado tem mais de um processador capaz de executar as tarefas.
- [ ] A quantidade de processadores é configurada pelo usuário.
- [ ] Mínimo de 1 e máximo de N processadores.

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
- [ ] Executa sem instalar biblioteca extra, software, runtime ou equivalente.
- [ ] Testado num computador limpo, sem nada instalado.

### RG6 — Comentários
- [ ] Todo o código-fonte está comentado.
- [ ] Os comentários explicam o que cada parte/função/componente faz.
- [ ] Os comentários explicam as razões/justificativas de todas as decisões de implementação.

---

## Projeto B

### B1 — Novos tipos de tarefa
- [ ] Criação e execução de tarefas **aperiódicas**.
- [ ] Criação e execução de tarefas **esporádicas**.

**B1.1 — Não interferência**
- [ ] Tarefas aperiódicas e esporádicas não interferem no escalonamento das periódicas. O enunciado diz "periódicas ou esporádicas", mas provavelmente quer dizer "aperiódicas ou esporádicas" (confirmar).
- [ ] Essas tarefas são executadas por um dos servidores do capítulo "Escalonamento de Tempo-Real".
- [ ] O usuário escolhe o servidor.

**B1.2 — Tarefa aperiódica**
- [ ] É indicada no arquivo por `periodo = 0`.
- [ ] O valor fica guardado na TCB da tarefa (o enunciado escreve "TCP").

**B1.3 — Tarefa esporádica**
- [ ] É indicada no arquivo por `periodo < 0`.
- [ ] O valor absoluto de `periodo` é o tempo mínimo entre duas ocorrências consecutivas.
- [ ] As ocorrências são lidas de `lista_eventos` no formato `instante01,instante02,...`.
- [ ] Cada instante é relativo à criação da tarefa (ex.: criada em 10, o instante 0 fica pronto em 10 e o 5 fica pronto em 15).
- [ ] Ao ler a linha, o simulador verifica a propriedade de tempo mínimo entre execuções consecutivas.
- [ ] Violação dessa propriedade é reportada ao usuário com o motivo.

### B2 — Servidores
- [ ] O usuário indica um ou mais servidores como tarefas periódicas.
- [ ] O tipo do servidor vem do prefixo do ID:
  - [ ] `BSxx` — Background Server.
  - [ ] `PSxx` — Polling Server (o enunciado escreve "Pooling").
  - [ ] `DSxx` — Deferrable Server.
  - [ ] `SSxx` — Sporadic Server.
- [ ] O `xx` (número inteiro) diferencia servidores do mesmo tipo.
- [ ] O prefixo é reconhecido sem diferenciar maiúsculas e minúsculas (herdado do A3.3.2).

**B2.1 — Campos dos servidores**
- [ ] Os demais campos da linha mantêm a mesma semântica.
- [ ] Esses campos definem o comportamento de cada servidor, conforme a seção 2.8 do capítulo "Escalonamento de Tempo-Real".

**B2.2 — Fila compartilhada**
- [ ] Todos os servidores acessam a fila de tarefas aperiódicas/esporádicas prontas.
- [ ] Cada servidor escolhe uma tarefa conforme a sua política de escalonamento.
- [ ] A tarefa escolhida sai da fila e fica inacessível para os outros servidores.

### B3 — Teste de escalonabilidade
O enunciado escreve "escalabilidade".
- [ ] O teste roda antes de qualquer tarefa ser aceita e incluída no sistema.
- [ ] O teste também avalia tarefas criadas durante a execução.
- [ ] O teste considera tarefas periódicas.
- [ ] O teste considera tarefas aperiódicas.
- [ ] O teste considera tarefas esporádicas.
- [ ] O teste considera os servidores.
- [ ] Uma tarefa recusada pelo teste é reportada ao usuário com o motivo.

### B4 — Visualização
- [ ] O Gantt mostra claramente quando um servidor está executando.
- [ ] O Gantt mostra claramente quando uma tarefa aperiódica/esporádica está executando.
- [ ] Usa um elemento visual distinto, como preenchimento não sólido ou borda de espessura/padrão diferente (sugestão do enunciado).
- [ ] A legenda inclui esses novos elementos (herdado do RG3).

---

## Pendências a confirmar com o professor

- [ ] Todos os requisitos do Projeto A continuam valendo no B?
- [ ] No B1.1, a não interferência é das tarefas "aperiódicas ou esporádicas"?
- [ ] Os objetivos 2 e 3 do enunciado citam mutex, inversão de prioridade e impasse, mas o B não detalha requisitos para eles. Precisam ser implementados?
- [ ] Os eventos da `lista_eventos` além das ocorrências esporádicas (mutex, E/S, envio/recebimento) entram no B? Em que formato?
