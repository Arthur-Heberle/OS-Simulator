// =============================================================================
// MainWindow.h — Tela 1: janela principal do simulador (design.md §11).
//
// O quê: liga a interface ao núcleo. Abre o arquivo de configuração (de
// qualquer lugar, A3.3.4), mostra o Gantt (GanttWidget), o estado do sistema,
// a tabela de TCBs, o detalhe da tarefa com os botões de edição e o registro
// de eventos; controla a simulação (avançar, retroceder, play/pausa, ir para
// o instante N, executar até o fim, modo passo a passo ou completo) e, no
// fim, exporta PNG + SVG e mostra o resumo (Tela 6).
//
// Por quê assim: a janela não tem regra de simulação; ela só chama o
// Simulator (estado no histórico, D21) e mostra history[cursor]. Toda recusa
// vem do núcleo com o motivo em português (A3.4, A5). Os botões só ficam
// habilitados quando a ação faz sentido (A5).
//
// Requisitos: RG1–RG4, A1.2, A1.5 (A1.5.1–A1.5.3), A2.3, A2.4, A3.1, A3.3.4,
// A3.3.6, A3.4, A5. Decisões: D15, D16, D21, D26, D28, design.md §9–§11.
// =============================================================================
#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <string>
#include <vector>

#include "GanttRenderer.h"
#include "Simulator.h"

class QAction;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextBrowser;
class QTimer;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Lê um arquivo de qualquer caminho (QFile: pendrive, acentos, espaços;
    // A3.3.4). Em erro de leitura, 'error' explica o motivo. Usado também pelo
    // modo sem janela (--exportar) do main.cpp.
    static bool readConfigFile(const QString& path, std::string& text, QString* error);

    // Abre e carrega um arquivo de configuração: mostra a Tela 2 se houver
    // erros ou avisos; com erro, nada é carregado (D18). true se carregou.
    bool openConfigFile(const QString& path);

    // Ações da simulação (botões, menus e atalhos chamam estas funções).
    void stepForward();
    void stepBack();
    void runToEnd();
    void jumpToTick(Tick tick);
    void resetSimulation();
    void togglePlay();
    void setCompleteMode(bool complete);

    // Aplica edições na ordem; para na primeira recusa e mostra o motivo (A3.4).
    bool applyEdits(const std::vector<Edit>& edits, const QString& what);

    const Simulator& simulator() const { return sim_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void refreshAll();          // tudo o que depende de history[cursor]
    void refreshState();        // [E1.6] estado do sistema
    void refreshTable();        // [E1.7] tabela de TCBs
    void refreshDetail();       // [E1.8–E1.9] detalhe da tarefa selecionada
    void refreshActions();      // habilita só o que faz sentido (A5)
    void logInstant(const SystemSnapshot& s);
    void log(const QString& line);
    void showMessage(bool isError, const QString& title, const QString& text);
    void stopPlay();
    void checkEnd();            // fim da simulação: exporta e mostra a Tela 6
    QString exportFolder() const;
    int selectedTask() const;   // índice em tasks da linha selecionada, ou -1
    void taskAction(EditKind kind);

    // Arquivo > ...
    void openDialog();
    void saveConfigDialog();
    void exportDialog();
    void editTaskDialog();
    void newTaskDialog();
    void systemDialog();

    Simulator sim_;
    bool loaded_ = false;
    QString configPath_;            // arquivo aberto (para o título e a pasta padrão)
    QString exportDir_;             // pasta das imagens (Tela 5); vazio = padrão
    int timeline_ = 0;              // muda a cada carga/edição: a exportação é por linha do tempo
    int exportedTimeline_ = -1;     // última linha do tempo já exportada (não repete)
    bool completeMode_ = false;     // A1.5: false = passo a passo, true = completo

    GanttWidget* gantt_ = nullptr;
    QTextBrowser* state_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* detail_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QLabel* clock_ = nullptr;
    QComboBox* mode_ = nullptr;
    QSpinBox* speed_ = nullptr;
    QSpinBox* goTo_ = nullptr;
    QTimer* timer_ = nullptr;
    std::vector<int> tableTasks_;   // linha da tabela -> índice em tasks

    QAction* openAct_ = nullptr;
    QAction* saveAct_ = nullptr;
    QAction* exportAct_ = nullptr;
    QAction* resetAct_ = nullptr;
    QAction* backAct_ = nullptr;
    QAction* forwardAct_ = nullptr;
    QAction* playAct_ = nullptr;
    QAction* endAct_ = nullptr;
    QAction* goAct_ = nullptr;
    QAction* newTaskAct_ = nullptr;
    QAction* systemAct_ = nullptr;
    QAction* zoomInAct_ = nullptr;
    QAction* zoomOutAct_ = nullptr;
    QPushButton* suspendBtn_ = nullptr;
    QPushButton* resumeBtn_ = nullptr;
    QPushButton* activateBtn_ = nullptr;
    QPushButton* finishBtn_ = nullptr;
    QPushButton* removeBtn_ = nullptr;
    QPushButton* editBtn_ = nullptr;
};
