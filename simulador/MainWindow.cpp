// =============================================================================
// MainWindow.cpp — Implementação da Tela 1 (janela principal). Ver o cabeçalho
// de MainWindow.h. Comentários "[TELA 1 / Ex] origem: ..." numeram cada
// elemento visual e dizem de onde vem a informação (base do PDF de telas).
// =============================================================================

#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "Dialogs.h"
#include "Parser.h"

namespace {

QString qs(const std::string& text) { return QString::fromStdString(text); }

// Estado da tarefa em português (tabela, detalhe).
QString stateName(TaskState state) {
    switch (state) {
        case TaskState::NotArrived: return QStringLiteral("Ainda não chegou");
        case TaskState::Ready:      return QStringLiteral("Pronta");
        case TaskState::Running:    return QStringLiteral("Executando");
        case TaskState::Dormant:    return QStringLiteral("Dormente");
        case TaskState::Suspended:  return QStringLiteral("Suspensa");
        case TaskState::Terminated: return QStringLiteral("Terminada");
    }
    return QString();
}

// Lista de IDs de tarefas, em ordem natural (D4).
QString idList(const SystemSnapshot& s, std::vector<int> indices) {
    if (indices.empty()) return QStringLiteral("—");
    QStringList ids;
    for (int i : indices) ids << qs(s.tasks[i].id);
    return ids.join(QStringLiteral(", "));
}

}  // namespace

// -----------------------------------------------------------------------------
// Construção da janela
// -----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    timer_ = new QTimer(this);  // play: um passo a cada 'velocidade' ms (filho da janela: para junto com ela)
    connect(timer_, &QTimer::timeout, this, [this]() { stepForward(); });
    buildUi();
    refreshAll();
    resize(1400, 860);
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Simulador de SO multitarefa — RM / EDF"));

    // [TELA 1 / E1.4] Gráfico de Gantt (tarefas + CPUs + legenda fixa).
    // origem: Simulator::history()[0 .. cursor] desenhado por GanttRenderer (A2.1–A2.5).
    gantt_ = new GanttWidget;
    setCentralWidget(gantt_);

    // --- Ações (menus, barra e atalhos) ------------------------------------
    auto action = [this](const QString& text, const QKeySequence& key, auto slot) {
        auto* a = new QAction(text, this);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    openAct_ = action(QStringLiteral("Abrir configuração…"), QKeySequence::Open, [this]() { openDialog(); });
    saveAct_ = action(QStringLiteral("Salvar configuração…"), QKeySequence::Save, [this]() { saveConfigDialog(); });
    exportAct_ = action(QStringLiteral("Exportar imagem (PNG + SVG)…"), QKeySequence(QStringLiteral("Ctrl+E")), [this]() { exportDialog(); });
    auto* quitAct = action(QStringLiteral("Sair"), QKeySequence::Quit, [this]() { close(); });
    resetAct_ = action(QStringLiteral("Reiniciar"), QKeySequence(QStringLiteral("Ctrl+R")), [this]() { resetSimulation(); });
    // Só texto nos botões: símbolos como ◀ ▶ dependem da fonte instalada.
    backAct_ = action(QStringLiteral("Retroceder"), QKeySequence(QStringLiteral("Ctrl+Left")), [this]() { stepBack(); });
    forwardAct_ = action(QStringLiteral("Avançar"), QKeySequence(QStringLiteral("Ctrl+Right")), [this]() { stepForward(); });
    playAct_ = action(QStringLiteral("Play"), QKeySequence(QStringLiteral("F6")), [this]() { togglePlay(); });
    endAct_ = action(QStringLiteral("Executar até o fim"), QKeySequence(QStringLiteral("F5")), [this]() { runToEnd(); });
    goAct_ = action(QStringLiteral("Ir"), QKeySequence(), [this]() { jumpToTick(goTo_->value()); });
    newTaskAct_ = action(QStringLiteral("Nova tarefa…"), QKeySequence::New, [this]() { newTaskDialog(); });
    systemAct_ = action(QStringLiteral("Configurações do sistema…"), QKeySequence(), [this]() { systemDialog(); });
    zoomInAct_ = action(QStringLiteral("Zoom +"), QKeySequence::ZoomIn, [this]() { gantt_->zoomIn(); });
    zoomOutAct_ = action(QStringLiteral("Zoom -"), QKeySequence::ZoomOut, [this]() { gantt_->zoomOut(); });

    // [TELA 1 / E1.1] Menus Arquivo, Simulação e Ajuda.
    QMenu* file = menuBar()->addMenu(QStringLiteral("&Arquivo"));
    file->addAction(openAct_);
    file->addAction(saveAct_);
    file->addAction(exportAct_);
    file->addSeparator();
    file->addAction(quitAct);
    QMenu* simulation = menuBar()->addMenu(QStringLiteral("&Simulação"));
    for (QAction* a : {resetAct_, backAct_, forwardAct_, playAct_, endAct_}) simulation->addAction(a);
    simulation->addSeparator();
    simulation->addAction(newTaskAct_);
    simulation->addAction(systemAct_);
    QMenu* help = menuBar()->addMenu(QStringLiteral("A&juda"));
    help->addAction(QStringLiteral("Sobre"), this, [this]() {
        showMessage(false, QStringLiteral("Sobre"),
                    QStringLiteral("Simulador de SO multitarefa preemptivo de tempo compartilhado (Projeto A).\n"
                                   "Escalonadores RM e EDF, N CPUs com fila global, Gantt, passo a passo com "
                                   "retroceder/avançar e edição manual.\nUTFPR — Sistemas Operacionais."));
    });

    // [TELA 1 / E1.2] Barra de controle da simulação.
    auto* bar = addToolBar(QStringLiteral("Simulação"));
    bar->setMovable(false);
    bar->addAction(openAct_);
    bar->addSeparator();
    bar->addAction(resetAct_);
    bar->addAction(backAct_);
    bar->addAction(forwardAct_);
    bar->addAction(playAct_);
    bar->addAction(endAct_);
    // Segunda linha da barra: ajustes (não cabem numa linha em telas de ~1400 px).
    addToolBarBreak();
    bar = addToolBar(QStringLiteral("Ajustes"));
    bar->setMovable(false);
    // [TELA 1 / E1.2a] Ir para o instante N (A1.5.2). origem: valor digitado.
    bar->addWidget(new QLabel(QStringLiteral(" Ir para o instante: ")));
    goTo_ = new QSpinBox;
    goTo_->setRange(0, 2000000000);
    bar->addWidget(goTo_);
    bar->addAction(goAct_);
    bar->addSeparator();
    // [TELA 1 / E1.2b] Velocidade do play (ms por passo). origem: valor escolhido.
    bar->addWidget(new QLabel(QStringLiteral(" Velocidade: ")));
    speed_ = new QSpinBox;
    speed_->setRange(20, 5000);
    speed_->setValue(300);
    speed_->setSuffix(QStringLiteral(" ms/passo"));
    connect(speed_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int ms) { timer_->setInterval(ms); });
    bar->addWidget(speed_);
    bar->addSeparator();
    // [TELA 1 / E1.2c] Modo de execução (A1.5): passo a passo ou completo.
    bar->addWidget(new QLabel(QStringLiteral(" Modo: ")));
    mode_ = new QComboBox;
    mode_->addItems({QStringLiteral("Passo a passo"), QStringLiteral("Completo (só o resultado final)")});
    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) { setCompleteMode(i == 1); });
    bar->addWidget(mode_);
    bar->addSeparator();
    bar->addAction(zoomOutAct_);
    bar->addAction(zoomInAct_);
    bar->addAction(exportAct_);

    // [TELA 1 / E1.3] Relógio global na barra de status (A1.1, A1.5.2). origem: Simulator::current().tick.
    clock_ = new QLabel;
    statusBar()->addPermanentWidget(clock_);

    // [TELA 1 / E1.6] Estado do sistema (dock à direita).
    state_ = new QTextBrowser;
    auto* stateDock = new QDockWidget(QStringLiteral("Estado do sistema"), this);
    stateDock->setWidget(state_);
    stateDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, stateDock);

    // [TELA 1 / E1.7] Tabela de TCBs (A1.3, A1.5.1) + [E1.8] detalhe + [E1.9] ações da tarefa.
    auto* tasksPanel = new QWidget;
    auto* tasksLayout = new QVBoxLayout(tasksPanel);
    table_ = new QTableWidget;
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() { refreshDetail(); refreshActions(); });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editTaskDialog(); });
    tasksLayout->addWidget(table_, 3);
    detail_ = new QLabel(QStringLiteral("Selecione uma tarefa na tabela."));
    detail_->setWordWrap(true);
    detail_->setTextFormat(Qt::RichText);
    detail_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    tasksLayout->addWidget(detail_, 1);
    auto* buttons = new QHBoxLayout;
    auto button = [&](const QString& text, auto slot) {
        auto* b = new QPushButton(text);
        connect(b, &QPushButton::clicked, this, slot);
        buttons->addWidget(b);
        return b;
    };
    suspendBtn_ = button(QStringLiteral("Suspender"), [this]() { taskAction(EditKind::Suspend); });
    resumeBtn_ = button(QStringLiteral("Retomar"), [this]() { taskAction(EditKind::Resume); });
    activateBtn_ = button(QStringLiteral("Ativar agora"), [this]() { taskAction(EditKind::ActivateNow); });
    finishBtn_ = button(QStringLiteral("Forçar término"), [this]() { taskAction(EditKind::ForceFinish); });
    removeBtn_ = button(QStringLiteral("Remover"), [this]() { taskAction(EditKind::Remove); });
    editBtn_ = button(QStringLiteral("Editar parâmetros…"), [this]() { editTaskDialog(); });
    tasksLayout->addLayout(buttons);
    auto* tasksDock = new QDockWidget(QStringLiteral("Tarefas (TCB)"), this);
    tasksDock->setWidget(tasksPanel);
    tasksDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, tasksDock);
    splitDockWidget(stateDock, tasksDock, Qt::Vertical);

    // [TELA 1 / E1.10] Registro de eventos, avisos e erros (dock inferior).
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(5000);  // sem crescer para sempre em simulações longas
    auto* logDock = new QDockWidget(QStringLiteral("Registro (ocorrências, edições, avisos e erros)"), this);
    logDock->setWidget(log_);
    logDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    resizeDocks({stateDock, tasksDock}, {300, 420}, Qt::Vertical);
    resizeDocks({stateDock}, {560}, Qt::Horizontal);
    resizeDocks({logDock}, {140}, Qt::Vertical);
}

// -----------------------------------------------------------------------------
// Arquivo de configuração (A3.3.4, A3.3.6, A3.3.7)
// -----------------------------------------------------------------------------

bool MainWindow::readConfigFile(const QString& path, std::string& text, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Não foi possível abrir \"%1\": %2.").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();  // bytes crus: o parser trata BOM e CRLF/CR (D18)
    text.assign(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    return true;
}

void MainWindow::openDialog() {
    // Qualquer arquivo, de qualquer pasta, partição ou pendrive (A3.3.4).
    const QString start = configPath_.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                                : QFileInfo(configPath_).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Abrir arquivo de configuração"), start,
                                                      QStringLiteral("Arquivos de texto (*.txt);;Todos os arquivos (*)"));
    if (!path.isEmpty()) openConfigFile(path);
}

bool MainWindow::openConfigFile(const QString& path) {
    stopPlay();
    std::string text;
    QString error;
    if (!readConfigFile(path, text, &error)) {
        showMessage(true, QStringLiteral("Erro ao abrir o arquivo"), error);
        return false;
    }
    const ParseResult result = parser::parseConfig(text);

    // [TELA 2] Erros (nada é carregado) ou avisos (carrega e informa): D18.
    if (!result.ok() || !result.warnings.empty()) {
        auto* dialog = new dialogs::LoadResultDialog(QDir::toNativeSeparators(path), result, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();  // não bloqueia a janela principal
    }
    for (const std::string& e : result.errors) log(qs(e));
    for (const std::string& w : result.warnings) log(qs(w));
    if (!result.ok()) {
        log(QStringLiteral("Arquivo \"%1\" NÃO foi carregado (%2 erro(s)).").arg(QDir::toNativeSeparators(path)).arg(result.errors.size()));
        return false;
    }
    sim_.load(result.config);
    loaded_ = true;
    configPath_ = path;
    ++timeline_;
    gantt_->setTitle(QFileInfo(path).completeBaseName());
    log(QStringLiteral("Arquivo \"%1\" carregado: %2 tarefa(s), algoritmo %3, %4 CPU(s).")
            .arg(QDir::toNativeSeparators(path)).arg(result.config.tasks.size())
            .arg(qs(result.config.algorithm)).arg(result.config.cpuCount));
    logInstant(sim_.current());
    if (completeMode_) {
        runToEnd();  // A1.5.3: no modo completo, só o resultado final
    } else {
        refreshAll();
    }
    return true;
}

void MainWindow::saveConfigDialog() {
    if (!loaded_) return;
    // A configuração ATUAL (com as edições) no formato do arquivo (A3.3).
    const SystemSnapshot& s = sim_.current();
    Config config;
    config.algorithm = s.algorithm;
    config.quantum = s.quantum;
    config.cpuCount = static_cast<int>(s.cpus.size());
    config.seed = s.seed;
    config.tasks = s.tasks;
    const QString suggestion = QFileInfo(configPath_).absolutePath() + QStringLiteral("/") +
                               QFileInfo(configPath_).completeBaseName() + QStringLiteral("_editada.txt");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar configuração"), suggestion,
                                                      QStringLiteral("Arquivos de texto (*.txt)"));
    if (path.isEmpty()) return;
    QFile file(path);
    const std::string text = parser::serializeConfig(config);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size())) {
        showMessage(true, QStringLiteral("Erro ao salvar"),
                    QStringLiteral("Não foi possível gravar \"%1\": %2.").arg(QDir::toNativeSeparators(path), file.errorString()));
        return;
    }
    log(QStringLiteral("Configuração salva em \"%1\" (a semente do sorteio não faz parte do formato).").arg(QDir::toNativeSeparators(path)));
}

// -----------------------------------------------------------------------------
// Controle da simulação (A1.5)
// -----------------------------------------------------------------------------

void MainWindow::stepForward() {
    if (!loaded_) return;
    if (!sim_.stepForward()) {
        stopPlay();
        statusBar()->showMessage(QStringLiteral("A simulação já terminou."), 4000);
        return;
    }
    logInstant(sim_.current());
    refreshAll();
    checkEnd();
}

void MainWindow::stepBack() {
    if (!loaded_ || !sim_.stepBack()) return;
    log(QStringLiteral("Voltou para o instante %1.").arg(sim_.current().tick));
    refreshAll();
}

void MainWindow::runToEnd() {
    if (!loaded_) return;
    stopPlay();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const RunSummary summary = sim_.runToEnd();
    QApplication::restoreOverrideCursor();
    log(qs(summary.message));
    refreshAll();
    if (!summary.finished) {
        showMessage(false, QStringLiteral("Execução interrompida"), qs(summary.message));
    }
    checkEnd();
}

void MainWindow::jumpToTick(Tick tick) {
    if (!loaded_) return;
    stopPlay();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool reached = sim_.jumpTo(tick);
    QApplication::restoreOverrideCursor();
    if (!reached) {
        showMessage(false, QStringLiteral("Instante inexistente"),
                    QStringLiteral("A simulação termina no instante %1; não existe o instante %2. "
                                   "O relógio foi para o último instante.").arg(sim_.current().tick).arg(tick));
    }
    log(QStringLiteral("Foi para o instante %1.").arg(sim_.current().tick));
    refreshAll();
    checkEnd();
}

void MainWindow::resetSimulation() {
    if (!loaded_) return;
    stopPlay();
    sim_.reset();
    log(QStringLiteral("Simulação reiniciada no instante 0."));
    refreshAll();
}

void MainWindow::togglePlay() {
    if (timer_->isActive()) {
        stopPlay();
        return;
    }
    if (!loaded_) return;
    if (sim_.isFinished() && sim_.cursor() + 1 == sim_.history().size()) return;
    timer_->start(speed_->value());
    playAct_->setText(QStringLiteral("Pausa"));
    refreshActions();
}

void MainWindow::stopPlay() {
    if (timer_ != nullptr && timer_->isActive()) timer_->stop();
    if (playAct_ != nullptr) playAct_->setText(QStringLiteral("Play"));
    refreshActions();
}

void MainWindow::setCompleteMode(bool complete) {
    completeMode_ = complete;
    // Sincroniza a combo sem disparar o sinal de novo (senão runToEnd rodaria duas vezes).
    if (mode_->currentIndex() != (complete ? 1 : 0)) {
        const QSignalBlocker block(mode_);
        mode_->setCurrentIndex(complete ? 1 : 0);
    }
    if (complete && loaded_) runToEnd();
}

// -----------------------------------------------------------------------------
// Edição manual (A1.5.2, A3.1, A3.4)
// -----------------------------------------------------------------------------

bool MainWindow::applyEdits(const std::vector<Edit>& edits, const QString& what) {
    if (!loaded_ || edits.empty()) return false;
    stopPlay();
    int applied = 0;
    for (const Edit& edit : edits) {
        const EditResult result = sim_.applyEdit(edit);
        if (!result.ok) {
            QString text = QStringLiteral("A edição \"%1\" foi recusada no instante %2:\n\n%3")
                               .arg(what).arg(sim_.current().tick).arg(qs(result.message));
            if (applied > 0) text += QStringLiteral("\n\n(%1 alteração(ões) anterior(es) desta edição já foram aplicadas.)").arg(applied);
            log(QStringLiteral("Edição recusada (%1): %2").arg(what, qs(result.message)));
            if (applied > 0) {
                ++timeline_;
                refreshAll();
            }
            showMessage(true, QStringLiteral("Edição recusada"), text);
            return false;
        }
        ++applied;
        if (!result.message.empty()) log(QStringLiteral("Aviso: ") + qs(result.message));
    }
    ++timeline_;  // o futuro foi descartado (D15): a próxima chegada ao fim exporta de novo
    log(QStringLiteral("Edição aplicada no instante %1: %2 (o futuro foi recalculado).").arg(sim_.current().tick).arg(what));
    refreshAll();
    return true;
}

void MainWindow::taskAction(EditKind kind) {
    const int index = selectedTask();
    if (index < 0) return;
    Edit edit;
    edit.kind = kind;
    edit.taskIndex = index;
    const QString id = qs(sim_.current().tasks[index].id);
    QString what;
    switch (kind) {
        case EditKind::Suspend:     what = QStringLiteral("suspender %1").arg(id); break;
        case EditKind::Resume:      what = QStringLiteral("retomar %1").arg(id); break;
        case EditKind::ActivateNow: what = QStringLiteral("ativar %1 agora").arg(id); break;
        case EditKind::ForceFinish: what = QStringLiteral("forçar o término de %1").arg(id); break;
        case EditKind::Remove:      what = QStringLiteral("remover %1").arg(id); break;
        default:                    what = id; break;
    }
    applyEdits({edit}, what);
}

void MainWindow::editTaskDialog() {
    const int index = selectedTask();
    if (index < 0 || !loaded_) return;
    stopPlay();
    const Task& task = sim_.current().tasks[index];
    if (task.ignored) {
        showMessage(true, QStringLiteral("Edição não permitida"),
                    QStringLiteral("A tarefa %1 não é simulada no Projeto A (%2).").arg(qs(task.id), qs(task.ignoredReason)));
        return;
    }
    dialogs::EditTaskDialog dialog(task, index, sim_.current().tick, this);
    if (dialog.exec() == QDialog::Accepted) {
        const std::vector<Edit> edits = dialog.edits();
        if (edits.empty()) return;
        applyEdits(edits, QStringLiteral("parâmetros de %1").arg(qs(task.id)));
    }
}

void MainWindow::newTaskDialog() {
    if (!loaded_) return;
    stopPlay();
    dialogs::NewTaskDialog dialog(sim_.current().tick, this);
    if (dialog.exec() == QDialog::Accepted) {
        const Edit edit = dialog.edit();
        applyEdits({edit}, QStringLiteral("nova tarefa %1").arg(qs(edit.newTask.id)));
    }
}

void MainWindow::systemDialog() {
    if (!loaded_) return;
    stopPlay();
    dialogs::SystemDialog dialog(sim_.current(), gantt_->tickWidth(), exportFolder(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    gantt_->setTickWidth(dialog.tickWidth());
    if (QDir(dialog.exportDir()).exists()) {
        exportDir_ = dialog.exportDir();
    } else if (!dialog.exportDir().isEmpty()) {
        showMessage(true, QStringLiteral("Pasta inválida"),
                    QStringLiteral("A pasta \"%1\" não existe; a pasta das imagens não foi alterada.").arg(dialog.exportDir()));
    }
    const std::vector<Edit> edits = dialog.edits();
    if (!edits.empty()) applyEdits(edits, QStringLiteral("configurações do sistema"));
}

// -----------------------------------------------------------------------------
// Fim da simulação e exportação (A2.4, A1.5.3)
// -----------------------------------------------------------------------------

QString MainWindow::exportFolder() const {
    // Pasta escolhida (Tela 5) -> pasta do arquivo de configuração -> Documentos.
    if (!exportDir_.isEmpty() && QFileInfo(exportDir_).isDir() && QFileInfo(exportDir_).isWritable()) return exportDir_;
    const QFileInfo configDir(QFileInfo(configPath_).absolutePath());
    if (!configPath_.isEmpty() && configDir.isDir() && configDir.isWritable()) return configDir.absoluteFilePath();
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::exportDialog() {
    if (!loaded_) return;
    stopPlay();
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Pasta para as imagens"), exportFolder());
    if (dir.isEmpty()) return;
    const QString base = QDir(dir).filePath(GanttRenderer::defaultBaseName(QFileInfo(configPath_).completeBaseName()));
    QStringList files;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = GanttRenderer::exportPng(base + QStringLiteral(".png"), sim_.history(), gantt_->options(), &error, &files) &&
                    GanttRenderer::exportSvg(base + QStringLiteral(".svg"), sim_.history(), gantt_->options(), &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        showMessage(true, QStringLiteral("Erro ao exportar"), error);
        return;
    }
    files << base + QStringLiteral(".svg");
    log(QStringLiteral("Imagens exportadas (instantes 0 a %1): %2").arg(sim_.history().back().tick).arg(files.join(QStringLiteral("; "))));
    showMessage(false, QStringLiteral("Imagens exportadas"), files.join(QStringLiteral("\n")));
}

void MainWindow::checkEnd() {
    if (!loaded_ || !sim_.isFinished() || sim_.cursor() + 1 != sim_.history().size()) return;
    if (exportedTimeline_ == timeline_) return;  // esta linha do tempo já foi exportada
    exportedTimeline_ = timeline_;
    stopPlay();

    // Exportação automática da simulação INTEIRA (A2.4), sem capturar a tela.
    statusBar()->showMessage(QStringLiteral("Exportando as imagens do gráfico de Gantt…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QString base = QDir(exportFolder()).filePath(GanttRenderer::defaultBaseName(QFileInfo(configPath_).completeBaseName()));
    QStringList files;
    QString error;
    const bool ok = GanttRenderer::exportPng(base + QStringLiteral(".png"), sim_.history(), gantt_->options(), &error, &files) &&
                    GanttRenderer::exportSvg(base + QStringLiteral(".svg"), sim_.history(), gantt_->options(), &error);
    QApplication::restoreOverrideCursor();
    statusBar()->clearMessage();
    if (ok) files << base + QStringLiteral(".svg");
    log(ok ? QStringLiteral("Imagens exportadas: ") + files.join(QStringLiteral("; "))
           : QStringLiteral("Erro ao exportar as imagens: ") + error);

    // [TELA 6] Resumo do fim da simulação. origem: último snapshot.
    const SystemSnapshot& s = sim_.current();
    QString html = QStringLiteral("<h3>Simulação concluída no instante %1</h3>").arg(s.tick);
    html += QStringLiteral("<p>Algoritmo %1, %2 CPU(s), semente %3.</p>").arg(qs(s.algorithm)).arg(s.cpus.size()).arg(QString::number(s.seed));
    int misses = 0;
    html += QStringLiteral("<table border='1' cellspacing='0' cellpadding='3'><tr><th>Tarefa</th><th>Ocorrências</th>"
                           "<th>Perdas de prazo</th><th>Preempções</th><th>Migrações</th><th>Executou</th><th>Esperou</th></tr>");
    for (const Task& t : s.tasks) {
        if (t.ignored) continue;
        misses += t.deadlineMisses;
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td></tr>")
                    .arg(qs(t.id).toHtmlEscaped()).arg(t.completed).arg(t.deadlineMisses).arg(t.preemptions)
                    .arg(t.migrations).arg(t.runTicks).arg(t.waitTicks);
    }
    html += QStringLiteral("</table><p><b>Total de perdas de prazo: %1</b></p>").arg(misses);
    html += QStringLiteral("<p><b>Tempo das CPUs</b> (A1.2):<br>");
    for (const CPU& c : s.cpus) {
        const double total = std::max<double>(1.0, static_cast<double>(c.ticksBusy + c.ticksOff));
        html += QStringLiteral("CPU%1: ocupada %2 tick(s), desligada %3 tick(s) (%4% do tempo)<br>")
                    .arg(c.id + 1).arg(c.ticksBusy).arg(c.ticksOff).arg(100.0 * c.ticksOff / total, 0, 'f', 1);
    }
    html += QStringLiteral("</p>");
    auto* dialog = new dialogs::EndDialog(html, ok ? files : QStringList(), ok ? QString() : error, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

// -----------------------------------------------------------------------------
// Atualização da tela a partir de history[cursor]
// -----------------------------------------------------------------------------

void MainWindow::refreshAll() {
    if (loaded_) {
        gantt_->setHistory(&sim_.history(), sim_.cursor() + 1, sim_.current().tick);
        clock_->setText(QStringLiteral("Relógio global: instante %1   ").arg(sim_.current().tick));
        goTo_->setValue(static_cast<int>(sim_.current().tick));
        setWindowTitle(QStringLiteral("Simulador de SO — %1").arg(QFileInfo(configPath_).fileName()));
    } else {
        clock_->setText(QStringLiteral("Nenhuma configuração carregada (Arquivo > Abrir configuração)   "));
    }
    refreshState();
    refreshTable();
    refreshDetail();
    refreshActions();
}

void MainWindow::refreshState() {
    if (!loaded_) {
        state_->setHtml(QStringLiteral("<p>Abra um arquivo de configuração (<b>Arquivo &gt; Abrir configuração</b>).</p>"));
        return;
    }
    const SystemSnapshot& s = sim_.current();
    QString html;
    // [TELA 1 / E1.6a] origem: SystemSnapshot::tick, algorithm, quantum, seed.
    html += QStringLiteral("<p><b>Relógio global:</b> instante %1%2<br><b>Algoritmo:</b> %3 &nbsp; <b>Quantum:</b> %4 "
                           "(não usado por RM/EDF) &nbsp; <b>Semente:</b> %5</p>")
                .arg(s.tick).arg(finished(s) ? QStringLiteral(" (fim)") : QString())
                .arg(qs(s.algorithm)).arg(s.quantum).arg(QString::number(s.seed));

    // [TELA 1 / E1.6b] CPUs: o que executa, tempo ocupada/desligada e os períodos desligada (A1.2).
    // origem: SystemSnapshot::cpus e history[0..cursor] (intervalos com on == false).
    html += QStringLiteral("<p><b>Processadores</b> (%1):<br>").arg(s.cpus.size());
    const auto& h = sim_.history();
    for (int c = 0; c < static_cast<int>(s.cpus.size()); ++c) {
        const CPU& cpu = s.cpus[c];
        QStringList intervals;
        Tick start = -1;
        for (std::size_t k = 0; k <= sim_.cursor(); ++k) {
            const bool off = c < static_cast<int>(h[k].cpus.size()) && !h[k].cpus[c].on;
            if (off && start < 0) start = static_cast<Tick>(k);
            if (!off && start >= 0) {
                intervals << QStringLiteral("[%1, %2)").arg(start).arg(k);
                start = -1;
            }
        }
        if (start >= 0) intervals << QStringLiteral("[%1, …)").arg(start);
        const int shown = 6;
        QString list = intervals.mid(std::max(0, static_cast<int>(intervals.size()) - shown)).join(QStringLiteral(" "));
        if (intervals.size() > shown) list = QStringLiteral("… ") + list;
        const double total = std::max<double>(1.0, static_cast<double>(cpu.ticksBusy + cpu.ticksOff));
        html += QStringLiteral("<b>CPU%1</b>: %2 &nbsp; ocupada %3, desligada %4 tick(s) (%5%)")
                    .arg(c + 1)
                    .arg(cpu.taskIndex >= 0 ? QStringLiteral("executando <b>%1</b>").arg(qs(s.tasks[cpu.taskIndex].id).toHtmlEscaped())
                                            : QStringLiteral("<span style='color:#808080'>desligada</span>"))
                    .arg(cpu.ticksBusy).arg(cpu.ticksOff).arg(100.0 * cpu.ticksOff / total, 0, 'f', 1);
        if (!intervals.isEmpty()) html += QStringLiteral("<br>&nbsp;&nbsp;desligada em: ") + list;
        html += QStringLiteral("<br>");
    }
    html += QStringLiteral("</p>");

    // [TELA 1 / E1.6c] Filas (D1). origem: SystemSnapshot::readyQueue (ordem da fila global) e Task::state.
    std::vector<int> running, dormant, suspended, notArrived, terminated, ignored;
    for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) {
        const Task& t = s.tasks[i];
        if (t.ignored) { ignored.push_back(i); continue; }
        switch (t.state) {
            case TaskState::Running:    running.push_back(i); break;
            case TaskState::Dormant:    dormant.push_back(i); break;
            case TaskState::Suspended:  suspended.push_back(i); break;
            case TaskState::NotArrived: notArrived.push_back(i); break;
            case TaskState::Terminated: terminated.push_back(i); break;
            default: break;
        }
    }
    auto byId = [&s](std::vector<int>& v) {
        std::sort(v.begin(), v.end(), [&s](int a, int b) { return naturalIdLess(s.tasks[a].id, s.tasks[b].id); });
    };
    for (auto* v : {&running, &dormant, &suspended, &notArrived, &terminated, &ignored}) byId(*v);
    html += QStringLiteral("<p><b>Fila de prontos (global, em ordem):</b> %1<br><b>Executando:</b> %2<br>"
                           "<b>Dormentes</b> (esperando a próxima ativação): %3<br><b>Suspensas:</b> %4<br>"
                           "<b>Ainda não chegaram:</b> %5<br><b>Terminadas:</b> %6")
                .arg(idList(s, s.readyQueue), idList(s, running), idList(s, dormant), idList(s, suspended),
                     idList(s, notArrived), idList(s, terminated));
    if (!ignored.empty()) html += QStringLiteral("<br><b>Ignoradas</b> (fora do Projeto A): %1").arg(idList(s, ignored));
    html += QStringLiteral("</p>");

    // [TELA 1 / E1.6d] Utilização (informativa, design.md §6). origem: duração/período das tarefas simuladas.
    double u = 0;
    int n = 0;
    for (const Task& t : s.tasks) {
        if (t.ignored || t.period <= 0) continue;
        u += static_cast<double>(t.duration) / static_cast<double>(t.period);
        ++n;
    }
    const double liuLayland = n > 0 ? n * (std::pow(2.0, 1.0 / n) - 1.0) : 1.0;
    html += QStringLiteral("<p><b>Utilização</b> U = Σ duração/período = %1<br>"
                           "RM, 1 CPU: escalonável se U ≤ %2 (limite de Liu-Layland para %3 tarefas)<br>"
                           "EDF, 1 CPU: escalonável se U ≤ 1 &nbsp;·&nbsp; %4 CPUs: U ≤ %4 é condição necessária<br>"
                           "<i>Informativo: no Projeto A nenhuma tarefa é recusada.</i></p>")
                .arg(u, 0, 'f', 3).arg(liuLayland, 0, 'f', 3).arg(n).arg(s.cpus.size());
    state_->setHtml(html);
}

void MainWindow::refreshTable() {
    const int keep = selectedTask();
    // [TELA 1 / E1.7] Uma linha por TCB (A1.3), todas as colunas. origem: SystemSnapshot::tasks de history[cursor].
    const QStringList headers = {QStringLiteral("ID"), QStringLiteral("Estado"), QStringLiteral("CPU"), QStringLiteral("Cor"),
                                 QStringLiteral("Ingresso"), QStringLiteral("Duração"), QStringLiteral("Período"),
                                 QStringLiteral("Prazo"), QStringLiteral("Restante"), QStringLiteral("Prazo absoluto"),
                                 QStringLiteral("Ativações (lib./concl.)"), QStringLiteral("Próx. ativação"),
                                 QStringLiteral("Perdas de prazo"), QStringLiteral("Preempções"), QStringLiteral("Migrações"),
                                 QStringLiteral("Executou"), QStringLiteral("Esperou"), QStringLiteral("1º início"),
                                 QStringLiteral("Término"), QStringLiteral("Eventos"), QStringLiteral("Observação")};
    table_->blockSignals(true);
    table_->clear();
    table_->setColumnCount(headers.size());
    table_->setHorizontalHeaderLabels(headers);
    tableTasks_.clear();
    if (loaded_) {
        const SystemSnapshot& s = sim_.current();
        for (int i = 0; i < static_cast<int>(s.tasks.size()); ++i) tableTasks_.push_back(i);
        std::sort(tableTasks_.begin(), tableTasks_.end(),
                  [&s](int a, int b) { return naturalIdLess(s.tasks[a].id, s.tasks[b].id); });
        table_->setRowCount(static_cast<int>(tableTasks_.size()));
        for (int r = 0; r < static_cast<int>(tableTasks_.size()); ++r) {
            const Task& t = s.tasks[tableTasks_[r]];
            const Job* job = currentJob(t);
            auto dash = [](Tick v) { return v < 0 ? QStringLiteral("—") : QString::number(v); };
            const QStringList cells = {
                qs(t.id), t.ignored ? QStringLiteral("Ignorada") : stateName(t.state), t.cpu >= 0 ? QStringLiteral("CPU%1").arg(t.cpu + 1) : QStringLiteral("—"),
                qs(t.color), QString::number(t.arrival), QString::number(t.duration), QString::number(t.period),
                QString::number(t.deadline), job ? QString::number(job->remaining) : QStringLiteral("—"),
                job ? QString::number(job->absDeadline) + (job->missed ? QStringLiteral(" (perdido)") : QString()) : QStringLiteral("—"),
                QStringLiteral("%1 / %2").arg(t.released).arg(t.completed),
                t.released < PERIODIC_ACTIVATION_LIMIT && t.state != TaskState::Terminated ? QString::number(t.nextRelease) : QStringLiteral("—"),
                QString::number(t.deadlineMisses), QString::number(t.preemptions), QString::number(t.migrations),
                QString::number(t.runTicks), QString::number(t.waitTicks), dash(t.firstStart), dash(t.finishTime),
                qs(t.rawEvents), t.ignored ? qs(t.ignoredReason) : qs(t.blockReason)};
            for (int c = 0; c < cells.size(); ++c) {
                auto* item = new QTableWidgetItem(cells[c]);
                if (t.ignored) item->setForeground(Qt::gray);  // fora do Projeto A (D7)
                table_->setItem(r, c, item);
            }
            table_->item(r, 3)->setBackground(QColor(QStringLiteral("#") + qs(t.color)));
            table_->item(r, 3)->setForeground(QColor(QStringLiteral("#") + qs(t.color)).lightness() < 128 ? Qt::white : Qt::black);
            if (tableTasks_[r] == keep) table_->selectRow(r);
        }
        table_->resizeColumnsToContents();
    }
    table_->blockSignals(false);
}

void MainWindow::refreshDetail() {
    const int index = selectedTask();
    if (!loaded_ || index < 0) {
        detail_->setText(QStringLiteral("Selecione uma tarefa na tabela para ver os detalhes e editar."));
        return;
    }
    // [TELA 1 / E1.8] Detalhe da tarefa selecionada (A1.5.1). origem: TCB em history[cursor].
    const Task& t = sim_.current().tasks[index];
    QString html = QStringLiteral("<b>Tarefa %1</b> — %2").arg(qs(t.id).toHtmlEscaped(), stateName(t.state));
    if (t.state == TaskState::Running) html += QStringLiteral(" na CPU%1").arg(t.cpu + 1);
    if (const Job* job = currentJob(t)) {
        html += QStringLiteral("<br>Ocorrência atual (nº %1): restam %2 de %3; prazo absoluto %4%5")
                    .arg(t.completed + 1).arg(job->remaining).arg(job->duration).arg(job->absDeadline)
                    .arg(job->missed ? QStringLiteral(" <span style='color:#c00000'>(perdido)</span>") : QString());
        if (t.jobs.size() > 1) html += QStringLiteral("<br>%1 ocorrência(s) enfileirada(s) depois desta").arg(t.jobs.size() - 1);
    }
    if (t.state == TaskState::Running) {
        html += QStringLiteral("<br><i>\"Executando\" é decidido pelo escalonador: para tirar a tarefa da CPU, use Suspender.</i>");
    }
    if (t.ignored) html += QStringLiteral("<br><i>Não simulada no Projeto A: %1</i>").arg(qs(t.ignoredReason));
    detail_->setText(html);
}

void MainWindow::refreshActions() {
    const bool playing = timer_ != nullptr && timer_->isActive();
    const bool atEnd = loaded_ && sim_.cursor() + 1 == sim_.history().size();
    const bool done = loaded_ && sim_.isFinished() && atEnd;
    for (QAction* a : {saveAct_, exportAct_, resetAct_, goAct_, newTaskAct_, systemAct_, zoomInAct_, zoomOutAct_}) {
        if (a) a->setEnabled(loaded_);
    }
    if (backAct_) backAct_->setEnabled(loaded_ && sim_.cursor() > 0);
    if (forwardAct_) forwardAct_->setEnabled(loaded_ && !done);
    if (endAct_) endAct_->setEnabled(loaded_ && !done);
    if (playAct_) playAct_->setEnabled(playing || (loaded_ && !done));
    if (goTo_) goTo_->setEnabled(loaded_);

    // [TELA 1 / E1.9] Ações da tarefa: só as que valem para o estado atual (design.md §9, A5).
    const int index = selectedTask();
    const Task* t = (loaded_ && index >= 0) ? &sim_.current().tasks[index] : nullptr;
    const bool simulated = t != nullptr && !t->ignored;
    const TaskState st = t ? t->state : TaskState::Terminated;
    if (suspendBtn_) {
        suspendBtn_->setEnabled(simulated && (st == TaskState::Ready || st == TaskState::Running || st == TaskState::Dormant));
        resumeBtn_->setEnabled(simulated && st == TaskState::Suspended);
        activateBtn_->setEnabled(simulated && st == TaskState::Dormant);
        finishBtn_->setEnabled(simulated && st != TaskState::Terminated);
        removeBtn_->setEnabled(simulated && st != TaskState::Terminated);
        editBtn_->setEnabled(simulated);
    }
}

// -----------------------------------------------------------------------------
// Auxiliares
// -----------------------------------------------------------------------------

int MainWindow::selectedTask() const {
    if (table_ == nullptr) return -1;
    const QList<QTableWidgetItem*> items = table_->selectedItems();
    if (items.isEmpty()) return -1;
    const int row = items.first()->row();
    return row >= 0 && row < static_cast<int>(tableTasks_.size()) ? tableTasks_[row] : -1;
}

void MainWindow::logInstant(const SystemSnapshot& s) {
    // [TELA 1 / E1.10] origem: SystemSnapshot::occurrences do instante (A2.2: o que aconteceu em x = tick).
    QStringList parts;
    for (const Occurrence& o : s.occurrences) {
        if (o.type == OccurrenceType::Dispatch) continue;  // despacho: visível no Gantt e na tabela
        QString text = GanttRenderer::occurrenceText(o.type);
        if (o.taskIndex >= 0 && o.taskIndex < static_cast<int>(s.tasks.size())) text = qs(s.tasks[o.taskIndex].id) + QStringLiteral(": ") + text;
        else if (o.cpuIndex >= 0) text = QStringLiteral("CPU%1: ").arg(o.cpuIndex + 1) + text;
        if (!o.note.empty()) text += QStringLiteral(" (") + qs(o.note) + QStringLiteral(")");
        parts << text;
    }
    log(QStringLiteral("t=%1  %2").arg(s.tick).arg(parts.isEmpty() ? QStringLiteral("sem eventos") : parts.join(QStringLiteral(" · "))));
}

void MainWindow::log(const QString& line) {
    if (log_) log_->appendPlainText(line);
}

void MainWindow::showMessage(bool isError, const QString& title, const QString& text) {
    // Não bloqueia: a janela continua respondendo (A5).
    auto* box = new QMessageBox(isError ? QMessageBox::Warning : QMessageBox::Information, title, text, QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    stopPlay();  // fechar durante o play não deixa o timer disparando
    event->accept();
}
