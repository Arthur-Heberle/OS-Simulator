// =============================================================================
// Dialogs.h — Telas 2 a 6 da interface (design.md §11): resultado do
// carregamento, editar tarefa, nova tarefa, configurações do sistema e fim da
// simulação.
//
// O quê: cada tela é uma QDialog pequena. As telas de edição (3, 4 e 5) NÃO
// mudam a simulação: só montam os pedidos (Edit) a partir do que o usuário
// alterou. Quem aplica é a MainWindow, com Simulator::applyEdit, que valida e
// explica a recusa (A3.4).
//
// Por quê assim: toda regra de validação fica no núcleo (testada); a tela só
// coleta valores e mostra o resultado. Implementação no próprio cabeçalho
// (classes pequenas, sem .cpp) para economizar arquivos na entrega (D23, Q8).
//
// Comentários "[TELA x / Ey] origem: ..." numeram cada elemento visual e dizem
// de onde vem a informação mostrada: são a base do PDF de telas da entrega.
//
// Requisitos: A1.5.2, A3.1, A3.2, A3.3.6, A3.4, A4.4, A5, A1.2.
// Decisões: D7, D8, D15, D17, D18, design.md §9 e §11.
// =============================================================================
#pragma once

#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <vector>

#include "Model.h"
#include "Parser.h"     // MAX_FIELD_VALUE, MAX_QUANTUM
#include "Scheduler.h"  // schedulerNames

namespace dialogs {

// Campo numérico para valores de tempo (0 .. 10^9, mesma faixa do arquivo).
inline QSpinBox* tickBox(long long value, long long minimum = 0) {
    auto* box = new QSpinBox;
    box->setRange(static_cast<int>(minimum), static_cast<int>(MAX_FIELD_VALUE));
    box->setValue(static_cast<int>(value));
    return box;
}

// Campo de cor RRGGBB com botão que abre o seletor de cores do sistema.
inline QWidget* colorField(QLineEdit* edit) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* pick = new QPushButton(QStringLiteral("Escolher…"));
    layout->addWidget(edit, 1);
    layout->addWidget(pick);
    QObject::connect(pick, &QPushButton::clicked, edit, [edit]() {
        const QColor initial(QStringLiteral("#") + edit->text());
        const QColor chosen = QColorDialog::getColor(initial.isValid() ? initial : QColor(Qt::red), edit,
                                                     QStringLiteral("Cor da tarefa"));
        if (chosen.isValid()) edit->setText(chosen.name().mid(1).toUpper());
    });
    return row;
}

inline QString qs(const std::string& text) { return QString::fromStdString(text); }

// -----------------------------------------------------------------------------
// [TELA 2] Resultado do carregamento do arquivo (A3.3.6, A4.4, D18).
// -----------------------------------------------------------------------------
class LoadResultDialog : public QDialog {
public:
    LoadResultDialog(const QString& path, const ParseResult& result, QWidget* parent) : QDialog(parent) {
        setWindowTitle(result.ok() ? QStringLiteral("Arquivo carregado") : QStringLiteral("Erro no arquivo de configuração"));
        auto* layout = new QVBoxLayout(this);

        // [TELA 2 / E2.1] origem: caminho escolhido no QFileDialog e ParseResult::ok().
        auto* header = new QLabel(result.ok()
            ? QStringLiteral("<b>%1</b><br>carregado com %2 aviso(s).").arg(path.toHtmlEscaped()).arg(result.warnings.size())
            : QStringLiteral("<b>%1</b><br><span style='color:#c00000'>O arquivo tem %2 erro(s). <b>Nada foi carregado</b>; "
                             "corrija o arquivo e abra de novo.</span>").arg(path.toHtmlEscaped()).arg(result.errors.size()));
        header->setWordWrap(true);
        layout->addWidget(header);

        // [TELA 2 / E2.2] origem: ParseResult::errors e ::warnings (linha, campo, valor e motivo).
        auto* messages = new QPlainTextEdit;
        messages->setReadOnly(true);
        QStringList lines;
        for (const std::string& e : result.errors) lines << qs(e);
        for (const std::string& w : result.warnings) lines << qs(w);
        if (lines.isEmpty()) lines << QStringLiteral("Nenhum erro ou aviso.");
        messages->setPlainText(lines.join(QStringLiteral("\n")));
        layout->addWidget(new QLabel(QStringLiteral("Erros e avisos:")));
        layout->addWidget(messages, 1);

        // [TELA 2 / E2.3] origem: Config::tasks (campos do arquivo já com os padrões, A3.2).
        if (result.ok()) {
            layout->addWidget(new QLabel(QStringLiteral("Tarefas lidas (algoritmo %1, quantum %2, %3 CPU(s)):")
                                             .arg(qs(result.config.algorithm)).arg(result.config.quantum)
                                             .arg(result.config.cpuCount)));
            auto* table = new QTableWidget(static_cast<int>(result.config.tasks.size()), 7);
            table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("Cor"), QStringLiteral("Ingresso"),
                                              QStringLiteral("Duração"), QStringLiteral("Período"), QStringLiteral("Prazo"),
                                              QStringLiteral("Situação")});
            for (int r = 0; r < table->rowCount(); ++r) {
                const Task& t = result.config.tasks[static_cast<std::size_t>(r)];
                const QStringList cells = {qs(t.id), qs(t.color), QString::number(t.arrival), QString::number(t.duration),
                                           QString::number(t.period), QString::number(t.deadline),
                                           t.ignored ? QStringLiteral("ignorada: ") + qs(t.ignoredReason)
                                                     : QStringLiteral("simulada (periódica)")};
                for (int c = 0; c < cells.size(); ++c) table->setItem(r, c, new QTableWidgetItem(cells[c]));
                table->item(r, 1)->setBackground(QColor(QStringLiteral("#") + qs(t.color)));
            }
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->horizontalHeader()->setStretchLastSection(true);
            layout->addWidget(table, 1);
        }
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        layout->addWidget(buttons);
        resize(760, 480);
    }
};

// -----------------------------------------------------------------------------
// [TELA 3] Editar parâmetros de uma tarefa (A1.5.2, A3.1, A3.4; design.md §9).
// As ações de estado (suspender, retomar, forçar término, remover, ativar
// agora) ficam em botões no painel da tarefa (Tela 1 / E1.9).
// -----------------------------------------------------------------------------
class EditTaskDialog : public QDialog {
public:
    EditTaskDialog(const Task& task, int taskIndex, Tick now, QWidget* parent)
        : QDialog(parent), task_(task), index_(taskIndex) {
        setWindowTitle(QStringLiteral("Editar tarefa %1").arg(qs(task.id)));
        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout;

        // [TELA 3 / E3.1] origem: Task::id e o instante do cursor (Simulator::current().tick).
        layout->addWidget(new QLabel(QStringLiteral("Tarefa <b>%1</b> — alterações valem a partir do instante %2 "
                                                    "(o futuro já calculado é descartado).").arg(qs(task.id).toHtmlEscaped()).arg(now)));
        // [TELA 3 / E3.2] origem: Task::arrival. Só pode mudar antes de a tarefa chegar.
        arrival_ = tickBox(task.arrival, now);
        arrival_->setEnabled(task.state == TaskState::NotArrived);
        form->addRow(QStringLiteral("Ingresso:"), arrival_);
        // [TELA 3 / E3.3] origem: Task::duration (vale para as próximas ocorrências).
        duration_ = tickBox(task.duration, 1);
        form->addRow(QStringLiteral("Duração:"), duration_);
        // [TELA 3 / E3.4] origem: Task::period.
        period_ = tickBox(task.period, 1);
        form->addRow(QStringLiteral("Período:"), period_);
        // [TELA 3 / E3.5] origem: Task::deadline (recalcula o prazo absoluto das ocorrências pendentes).
        deadline_ = tickBox(task.deadline, 1);
        form->addRow(QStringLiteral("Prazo:"), deadline_);
        // [TELA 3 / E3.6] origem: Task::color (RRGGBB).
        color_ = new QLineEdit(qs(task.color));
        form->addRow(QStringLiteral("Cor (RRGGBB):"), colorField(color_));
        // [TELA 3 / E3.7] origem: jobs.front().remaining (só com ocorrência pendente).
        const Job* job = currentJob(task);
        remaining_ = tickBox(job ? job->remaining : 1, 1);
        if (job) remaining_->setMaximum(static_cast<int>(job->duration));
        remaining_->setEnabled(job != nullptr);
        form->addRow(QStringLiteral("Tempo restante da ocorrência atual:"), remaining_);
        layout->addLayout(form);

        auto* note = new QLabel(QStringLiteral("O estado \"Executando\" é decidido pelo escalonador; para tirar a tarefa da CPU "
                                               "use Suspender no painel da tarefa."));
        note->setWordWrap(true);
        layout->addWidget(note);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    // Um Edit por campo alterado, na ordem da tela.
    std::vector<Edit> edits() const {
        std::vector<Edit> list;
        auto add = [&](EditKind kind, long long value, const std::string& text = std::string()) {
            Edit e;
            e.kind = kind;
            e.taskIndex = index_;
            e.value = value;
            e.text = text;
            list.push_back(e);
        };
        if (arrival_->isEnabled() && arrival_->value() != task_.arrival) add(EditKind::SetArrival, arrival_->value());
        if (duration_->value() != task_.duration) add(EditKind::SetDuration, duration_->value());
        if (period_->value() != task_.period) add(EditKind::SetPeriod, period_->value());
        if (deadline_->value() != task_.deadline) add(EditKind::SetDeadline, deadline_->value());
        if (color_->text().trimmed().toUpper().remove(QLatin1Char('#')) != qs(task_.color)) {
            add(EditKind::SetColor, 0, color_->text().trimmed().toStdString());
        }
        const Job* job = currentJob(task_);
        if (remaining_->isEnabled() && job && remaining_->value() != job->remaining) {
            add(EditKind::SetRemaining, remaining_->value());
        }
        return list;
    }

private:
    Task task_;
    int index_;
    QSpinBox* arrival_ = nullptr;
    QSpinBox* duration_ = nullptr;
    QSpinBox* period_ = nullptr;
    QSpinBox* deadline_ = nullptr;
    QSpinBox* remaining_ = nullptr;
    QLineEdit* color_ = nullptr;
};

// -----------------------------------------------------------------------------
// [TELA 4] Nova tarefa durante a simulação (A3.1; validada pelo parser, §9).
// -----------------------------------------------------------------------------
class NewTaskDialog : public QDialog {
public:
    NewTaskDialog(Tick now, QWidget* parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Nova tarefa"));
        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout;
        // [TELA 4 / E4.1] origem: digitado (único, sem diferenciar maiúsculas; validado pelo parser).
        id_ = new QLineEdit;
        form->addRow(QStringLiteral("ID:"), id_);
        // [TELA 4 / E4.2] origem: digitado ou seletor; vazio = cor automática (D17).
        color_ = new QLineEdit;
        color_->setPlaceholderText(QStringLiteral("vazio = automática"));
        form->addRow(QStringLiteral("Cor (RRGGBB):"), colorField(color_));
        // [TELA 4 / E4.3] padrão: instante atual (não pode ficar no passado).
        arrival_ = tickBox(now, now);
        form->addRow(QStringLiteral("Ingresso:"), arrival_);
        // [TELA 4 / E4.4–E4.6] padrões sugeridos (A3.2); prazo 0 = igual ao período (D8).
        duration_ = tickBox(1, 1);
        form->addRow(QStringLiteral("Duração:"), duration_);
        period_ = tickBox(10, 0);
        form->addRow(QStringLiteral("Período (0 = aperiódica, ignorada no Projeto A):"), period_);
        deadline_ = tickBox(0, 0);
        form->addRow(QStringLiteral("Prazo (0 = igual ao período):"), deadline_);
        // [TELA 4 / E4.7] lista de eventos crua (só o Projeto B a interpreta, D30).
        events_ = new QLineEdit;
        form->addRow(QStringLiteral("Lista de eventos (opcional):"), events_);
        layout->addLayout(form);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    Edit edit() const {
        Edit e;
        e.kind = EditKind::AddTask;
        e.newTask.id = id_->text().trimmed().toStdString();
        e.newTask.color = color_->text().trimmed().toStdString();
        e.newTask.arrival = arrival_->value();
        e.newTask.duration = duration_->value();
        e.newTask.period = period_->value();
        e.newTask.deadline = deadline_->value();
        e.newTask.rawEvents = events_->text().trimmed().toStdString();
        return e;
    }

private:
    QLineEdit* id_ = nullptr;
    QLineEdit* color_ = nullptr;
    QLineEdit* events_ = nullptr;
    QSpinBox* arrival_ = nullptr;
    QSpinBox* duration_ = nullptr;
    QSpinBox* period_ = nullptr;
    QSpinBox* deadline_ = nullptr;
};

// -----------------------------------------------------------------------------
// [TELA 5] Configurações do sistema (A3.1, A3.2): algoritmo, quantum, CPUs e
// semente viram edições; zoom e pasta de exportação são da interface.
// -----------------------------------------------------------------------------
class SystemDialog : public QDialog {
public:
    SystemDialog(const SystemSnapshot& s, int tickWidth, const QString& exportDir, QWidget* parent)
        : QDialog(parent), snapshot_(s) {
        setWindowTitle(QStringLiteral("Configurações do sistema"));
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(QStringLiteral("Alterações valem a partir do instante %1.").arg(s.tick)));
        auto* form = new QFormLayout;
        // [TELA 5 / E5.1] origem: schedulerNames() (registro, A4.2); atual = SystemSnapshot::algorithm.
        algorithm_ = new QComboBox;
        for (const std::string& name : schedulerNames()) algorithm_->addItem(qs(name));
        algorithm_->setCurrentText(qs(s.algorithm));
        form->addRow(QStringLiteral("Algoritmo:"), algorithm_);
        // [TELA 5 / E5.2] origem: SystemSnapshot::quantum (RM/EDF não usam, Q4).
        quantum_ = new QSpinBox;
        quantum_->setRange(1, MAX_QUANTUM);
        quantum_->setValue(s.quantum);
        form->addRow(QStringLiteral("Quantum:"), quantum_);
        // [TELA 5 / E5.3] origem: SystemSnapshot::cpus.size() (1..MAX_CPUS, D12).
        cpus_ = new QSpinBox;
        cpus_->setRange(1, MAX_CPUS);
        cpus_->setValue(static_cast<int>(s.cpus.size()));
        form->addRow(QStringLiteral("Nº de CPUs:"), cpus_);
        // [TELA 5 / E5.4] origem: SystemSnapshot::seed (sorteio do desempate, D14).
        seed_ = new QSpinBox;
        seed_->setRange(0, 2147483647);
        seed_->setValue(static_cast<int>(std::min<uint64_t>(s.seed, 2147483647ULL)));
        form->addRow(QStringLiteral("Semente do sorteio:"), seed_);
        // [TELA 5 / E5.5] origem: GanttOptions::tickWidth (zoom do gráfico).
        zoom_ = new QSpinBox;
        zoom_->setRange(4, 64);
        zoom_->setValue(tickWidth);
        zoom_->setSuffix(QStringLiteral(" px por instante"));
        form->addRow(QStringLiteral("Zoom do gráfico:"), zoom_);
        // [TELA 5 / E5.6] origem: pasta das imagens exportadas (padrão: a do arquivo de configuração).
        dir_ = new QLineEdit(exportDir);
        auto* browse = new QPushButton(QStringLiteral("Escolher…"));
        auto* dirRow = new QWidget;
        auto* dirLayout = new QHBoxLayout(dirRow);
        dirLayout->setContentsMargins(0, 0, 0, 0);
        dirLayout->addWidget(dir_, 1);
        dirLayout->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, [this]() {
            const QString chosen = QFileDialog::getExistingDirectory(this, QStringLiteral("Pasta das imagens"), dir_->text());
            if (!chosen.isEmpty()) dir_->setText(QDir::toNativeSeparators(chosen));
        });
        form->addRow(QStringLiteral("Pasta das imagens:"), dirRow);
        layout->addLayout(form);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    // Edições do sistema para os valores alterados.
    std::vector<Edit> edits() const {
        std::vector<Edit> list;
        auto add = [&](EditKind kind, long long value, const std::string& text = std::string()) {
            Edit e;
            e.kind = kind;
            e.value = value;
            e.text = text;
            list.push_back(e);
        };
        if (algorithm_->currentText() != qs(snapshot_.algorithm)) add(EditKind::SetAlgorithm, 0, algorithm_->currentText().toStdString());
        if (quantum_->value() != snapshot_.quantum) add(EditKind::SetQuantum, quantum_->value());
        if (cpus_->value() != static_cast<int>(snapshot_.cpus.size())) add(EditKind::SetCpuCount, cpus_->value());
        if (static_cast<uint64_t>(seed_->value()) != snapshot_.seed) add(EditKind::SetSeed, seed_->value());
        return list;
    }
    int tickWidth() const { return zoom_->value(); }
    QString exportDir() const { return dir_->text().trimmed(); }

private:
    SystemSnapshot snapshot_;
    QComboBox* algorithm_ = nullptr;
    QSpinBox* quantum_ = nullptr;
    QSpinBox* cpus_ = nullptr;
    QSpinBox* seed_ = nullptr;
    QSpinBox* zoom_ = nullptr;
    QLineEdit* dir_ = nullptr;
};

// -----------------------------------------------------------------------------
// [TELA 6] Fim da simulação (A1.5.3, A1.2, A2.4): resumo e imagens geradas.
// -----------------------------------------------------------------------------
class EndDialog : public QDialog {
public:
    EndDialog(const QString& summaryHtml, const QStringList& files, const QString& error, QWidget* parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Fim da simulação"));
        auto* layout = new QVBoxLayout(this);
        // [TELA 6 / E6.1] origem: último snapshot (tick, perdas de prazo, contadores das tarefas e das CPUs).
        auto* summary = new QLabel(summaryHtml);
        summary->setTextFormat(Qt::RichText);
        summary->setWordWrap(true);
        summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(summary);
        // [TELA 6 / E6.2] origem: caminhos gravados por GanttRenderer::exportPng/exportSvg (A2.4).
        auto* paths = new QPlainTextEdit;
        paths->setReadOnly(true);
        paths->setPlainText(error.isEmpty() ? QStringLiteral("Imagens do gráfico de Gantt (simulação inteira):\n") +
                                                  files.join(QStringLiteral("\n"))
                                            : QStringLiteral("As imagens NÃO foram geradas: ") + error);
        paths->setMaximumHeight(110);
        layout->addWidget(paths);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
        if (!files.isEmpty()) {
            // [TELA 6 / E6.3] abre a pasta das imagens no explorador de arquivos.
            auto* open = buttons->addButton(QStringLiteral("Abrir pasta das imagens"), QDialogButtonBox::ActionRole);
            const QString folder = QFileInfo(files.first()).absolutePath();
            connect(open, &QPushButton::clicked, this, [folder]() { QDesktopServices::openUrl(QUrl::fromLocalFile(folder)); });
        }
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        layout->addWidget(buttons);
        resize(620, 420);
    }
};

}  // namespace dialogs
