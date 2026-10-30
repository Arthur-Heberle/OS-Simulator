# fluxo-git.md — Como a dupla usa branches, merges e correções

> Regra de ouro: **a `main` sempre compila sem warnings e passa no `sim_tests`.** Todo o resto existe para proteger isso.

## 1. Branches

| Branch | Para quê | Nasce de | Volta para |
|---|---|---|---|
| `main` | Versão estável, a que vai ao Moodle (tag `entrega-A` / `entrega-B`) | — | — |
| `fase-N-tema` | Uma fase do `planejamento.md` (ex.: `fase-1-model`, `fase-2-parser`, `fase-5-gantt`) | `main` | `main` |
| `fix/tema` | Bug que **já está na `main`** (ex.: `fix/parser-bom`) | `main` | `main` |

- Não existe branch `dev`: somos dois, as fases são curtas e cada branch já é testada antes de entrar. Uma branch a mais só traria mais merges.
- Fases em paralelo (ex.: núcleo e Gantt) = duas branches de fase ao mesmo tempo, cada uma com seu dono.
- Código entra na `main` só por branch. Correção pequena de texto em `docs/` pode ir direto.

## 2. Ciclo de uma fase

```
git switch main && git pull                  # 1. parte da main atualizada
git switch -c fase-2-parser                  # 2. cria a branch da fase
# ... commits pequenos ...
git push -u origin fase-2-parser             # 3. publica (backup + o colega acompanha)
git fetch && git merge origin/main           # 4. traz a main para a SUA branch (repita sempre que a main mudar)
cmake --build build && build\sim_tests.exe   # 5. compila sem warnings e testes verdes (completo, sem --rapido)
```

6. **Integrar:** quem fez a branch revisa e integra **sozinho**, sem esperar aprovação do colega (a `main` não tem proteção de branch, então o autor pode fazer o merge do próprio PR). O PR é opcional e serve de registro: `gh pr create --base main --fill` e, depois de conferir o diff, `gh pr merge --merge --delete-branch`. Avise o colega no grupo quando integrar, para ele ler as mudanças depois (a nota é individual: os dois precisam entender tudo). Sem PR, o merge local também serve:
   ```
   git switch main && git pull
   git merge --no-ff fase-2-parser   # --no-ff deixa um commit de merge: a fase aparece inteira no histórico
   cmake --build build && build\sim_tests.exe
   git push
   ```
7. Apagar a branch: `git branch -d fase-2-parser` e `git push origin --delete fase-2-parser`.

## 3. Conflitos

**Evitar:**
- Avisar no grupo antes de mexer em arquivo que é do outro. Donos: núcleo (`Model.h`, `Parser.*`, `Scheduler.*`, `Simulator.*`, `tests.cpp`) e interface (`GanttRenderer.*`, `MainWindow.*`, `Dialogs.h`, `main.cpp`).
- **`Model.h` é o contrato:** só muda com aviso, e de preferência num commit só dele.
- Fazer o passo 4 (merge da `main` na sua branch) com frequência: conflitos pequenos são fáceis.

**Resolver** (sempre na **sua branch**, nunca direto na `main`):
1. `git merge origin/main` → o Git lista os arquivos em conflito (`git status`).
2. Em cada arquivo, entre `<<<<<<<` e `>>>>>>>`, montar a versão que mantém **as duas intenções** (não só "a minha").
3. Compilar e rodar `sim_tests`; depois `git add <arquivos>` e `git commit`.
4. Na dúvida sobre o código do outro, pergunte antes de escolher. Para desistir: `git merge --abort`.

Em `docs/*.md`, quase sempre basta manter os dois textos.

**Nunca:** `git push --force` na `main`; `rebase` em branch já publicada; commitar com testes falhando.

## 4. Bugs

| Onde está o bug | O que fazer |
|---|---|
| Só na sua branch de fase | Corrige na própria branch |
| Já na `main` | `fix/tema` a partir da `main`, com um teste que reproduz o bug, merge rápido. Depois cada um faz `git merge origin/main` na sua branch |
| Achou na branch do colega | Avisa o colega (ou sugere no PR); não commita na branch dele sem combinar |

## 5. Commits e entrega

- Pequenos, um assunto por commit, mensagem no imperativo em português (`Parser: aceita BOM UTF-8`); o corpo explica **o porquê**.
- Entrega: na `main` final, `git tag entrega-A` e `git push origin entrega-A`. É dessa tag que saem os arquivos do Moodle.
