// =============================================================================
// main.cpp — Ponto de entrada da interface gráfica (sim_gui).
//
// PROVISÓRIO (Fase 5): demonstração do Gantt enquanto a janela principal
// (MainWindow, Fase 6) não existe. A Fase 6 substitui este conteúdo.
//
// O quê:
//   - sim_gui.exe --gantt-demo <pasta>
//       roda cenários de docs/testes.md (G1, G2 RM e EDF, G3, G5 com edições)
//       e uma simulação de ~100 000 ticks, e exporta PNG + SVG de cada um com
//       GanttRenderer (sem abrir janela). Serve para conferir as imagens e para
//       testar a pasta standalone (dist/) com PATH mínimo.
//   - sim_gui.exe (sem argumentos)
//       janela com o GanttWidget: escolher o cenário, avançar/voltar (o
//       gráfico redesenha e rola até o instante atual, A2.3), zoom e exportar.
//
// Por quê: o Gantt (Fase 5) é desenvolvido e conferido antes da janela
// principal (Fase 6), usando o motor real (Fases 3 e 4).
//
// Requisitos: RG3, RG5, A2.1–A2.5. Decisões: D25, D26, design.md §10.
// =============================================================================

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <string>
#include <vector>

#include "GanttRenderer.h"
#include "Parser.h"
#include "Simulator.h"

namespace {

// Cenários de demonstração (configurações de docs/testes.md).
struct Scenario {
    const char* name;
    const char* config;
    bool g5Edits;  // aplica as edições do G5 (suspender T1 em 4, retomar em 9)
};

const std::vector<Scenario>& scenarios() {
    static const std::vector<Scenario> list = {
        {"G1_RM", "RM;2;1\n1;FF0000;0;1;4;4\n2;0000FF;0;2;6;6\n", false},
        {"G2_RM", "RM;2;1\n1;FF0000;0;2;5;5\n2;0000FF;0;4;7;7\n", false},
        {"G2_EDF", "EDF;2;1\n1;FF0000;0;2;5;5\n2;0000FF;0;4;7;7\n", false},
        {"G3_EDF_2CPUs", "EDF;2;2\n1;FF0000;0;2;4;4\n2;00FF00;0;2;4;4\n3;0000FF;0;2;4;4\n", false},
        {"G5_edicao", "RM;2;1\n1;FF0000;0;1;4;4\n2;0000FF;0;2;6;6\n", true},
        // ~100 000 ticks: 10 ativações de período ~10 000 (teste de desempenho).
        {"longa_100k", "RM;2;2\n1;;0;3000;10000;\n2;;0;2500;10000;\n3;;7;1500;9000;\n", false},
    };
    return list;
}

// Carrega o cenário num Simulator e o executa até o fim (com as edições do G5).
bool runScenario(const Scenario& sc, Simulator& sim, QString* error) {
    const ParseResult parsed = parser::parseConfig(sc.config);
    if (!parsed.ok() || !sim.load(parsed.config)) {
        *error = QStringLiteral("configuração inválida");
        return false;
    }
    if (sc.g5Edits) {
        Edit edit;
        edit.taskIndex = 0;
        sim.jumpTo(4);
        edit.kind = EditKind::Suspend;
        sim.applyEdit(edit);
        sim.jumpTo(9);
        edit.kind = EditKind::Resume;
        sim.applyEdit(edit);
    }
    const RunSummary summary = sim.runToEnd();
    if (!summary.finished) {
        *error = QString::fromStdString(summary.message);
        return false;
    }
    return true;
}

// --gantt-demo <pasta>: exporta PNG + SVG de todos os cenários.
int exportDemo(const QString& dir) {
    QDir().mkpath(dir);
    int failures = 0;
    for (const Scenario& sc : scenarios()) {
        QElapsedTimer timer;
        timer.start();
        Simulator sim;
        QString error;
        if (!runScenario(sc, sim, &error)) {
            std::printf("%s: ERRO na simulação: %s\n", sc.name, error.toLocal8Bit().constData());
            ++failures;
            continue;
        }
        const qint64 simMs = timer.restart();
        GanttOptions options;
        options.title = QString::fromLatin1(sc.name);
        const QString base = QDir(dir).filePath(QString::fromLatin1(sc.name));
        QStringList files;
        const bool ok = GanttRenderer::exportPng(base + QStringLiteral(".png"), sim.history(), options, &error, &files) &&
                        GanttRenderer::exportSvg(base + QStringLiteral(".svg"), sim.history(), options, &error);
        std::printf("%s: %zu instantes, simulação %lld ms, exportação %lld ms, %s (%d PNG)\n", sc.name,
                    sim.history().size(), static_cast<long long>(simMs), static_cast<long long>(timer.elapsed()),
                    ok ? "ok" : error.toLocal8Bit().constData(), static_cast<int>(files.size()));
        if (!ok) ++failures;
    }
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // QApplication::arguments() preserva acentos e espaços nos caminhos (Windows).
    const QStringList args = QApplication::arguments();
    if (args.size() >= 3 && args.at(1) == QStringLiteral("--gantt-demo")) {
        return exportDemo(args.at(2));
    }

    // Janela de demonstração do GanttWidget.
    QWidget window;
    window.setWindowTitle(QStringLiteral("Simulador de SO — demonstração do Gantt (provisório, Fase 5)"));
    auto* scenarioBox = new QComboBox;
    for (const Scenario& sc : scenarios()) scenarioBox->addItem(QString::fromLatin1(sc.name));
    auto* back = new QPushButton(QStringLiteral("◀ Voltar"));
    auto* forward = new QPushButton(QStringLiteral("Avançar ▶"));
    auto* zoomOut = new QPushButton(QStringLiteral("Zoom −"));
    auto* zoomIn = new QPushButton(QStringLiteral("Zoom +"));
    auto* exportButton = new QPushButton(QStringLiteral("Exportar PNG + SVG"));
    auto* clock = new QLabel;
    auto* gantt = new GanttWidget;

    auto* bar = new QHBoxLayout;
    bar->addWidget(scenarioBox);
    bar->addWidget(back);
    bar->addWidget(forward);
    bar->addWidget(zoomOut);
    bar->addWidget(zoomIn);
    bar->addWidget(exportButton);
    bar->addWidget(clock, 1);
    auto* layout = new QVBoxLayout(&window);
    layout->addLayout(bar);
    layout->addWidget(gantt, 1);

    // O Simulator vive aqui; o widget só aponta para o histórico dele.
    Simulator sim;
    auto refresh = [&]() {
        gantt->setHistory(&sim.history(), sim.cursor() + 1, sim.current().tick);
        clock->setText(QStringLiteral("Relógio global: instante %1").arg(sim.current().tick));
    };
    auto load = [&](int index) {
        const Scenario& sc = scenarios()[static_cast<std::size_t>(index)];
        QString error;
        if (!runScenario(sc, sim, &error)) {
            QMessageBox::critical(&window, QStringLiteral("Erro"), error);
            return;
        }
        sim.jumpTo(0);  // começa no instante 0, como no passo a passo
        gantt->setTitle(QString::fromLatin1(sc.name));
        refresh();
    };
    QObject::connect(scenarioBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &window, load);
    QObject::connect(forward, &QPushButton::clicked, &window, [&]() { sim.stepForward(); refresh(); });
    QObject::connect(back, &QPushButton::clicked, &window, [&]() { sim.stepBack(); refresh(); });
    QObject::connect(zoomIn, &QPushButton::clicked, gantt, [gantt]() { gantt->zoomIn(); });
    QObject::connect(zoomOut, &QPushButton::clicked, gantt, [gantt]() { gantt->zoomOut(); });
    QObject::connect(exportButton, &QPushButton::clicked, &window, [&]() {
        const QString dir = QFileDialog::getExistingDirectory(
            &window, QStringLiteral("Pasta para as imagens"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (dir.isEmpty()) return;
        const QString base = QDir(dir).filePath(GanttRenderer::defaultBaseName(scenarioBox->currentText()));
        QString error;
        QStringList files;
        if (GanttRenderer::exportPng(base + QStringLiteral(".png"), sim.history(), gantt->options(), &error, &files) &&
            GanttRenderer::exportSvg(base + QStringLiteral(".svg"), sim.history(), gantt->options(), &error)) {
            files << base + QStringLiteral(".svg");
            QMessageBox::information(&window, QStringLiteral("Imagens geradas"), files.join(QStringLiteral("\n")));
        } else {
            QMessageBox::critical(&window, QStringLiteral("Erro ao exportar"), error);
        }
    });

    load(0);
    window.resize(1100, 600);
    window.show();
    return app.exec();
}
