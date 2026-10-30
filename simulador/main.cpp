// =============================================================================
// main.cpp — Ponto de entrada da interface gráfica (sim_gui).
//
// O quê:
//   - sim_gui.exe                      abre a janela principal (Tela 1);
//   - sim_gui.exe <arquivo>            abre a janela já com esse arquivo carregado
//                                      (ex.: arrastar o .txt sobre o .exe);
//   - sim_gui.exe --exportar <arquivo> <pasta>
//                                      sem janela: carrega, executa até o fim e
//                                      grava o Gantt em PNG + SVG na pasta.
//
// Por quê o modo sem janela: é o plano B da defesa (gera as imagens mesmo se
// algo der errado com a tela) e o teste automático da pasta standalone
// (dist/ com PATH mínimo, RG5), que precisa carregar o Qt e o plugin de SVG.
//
// Requisitos: RG5, A2.4, A3.3.4, A3.3.6. Decisões: D26, D29.
// =============================================================================

#include <QApplication>
#include <QDir>
#include <QFileInfo>

#include <cstdio>
#include <string>

#include "GanttRenderer.h"
#include "MainWindow.h"
#include "Parser.h"
#include "Simulator.h"

// --exportar <arquivo> <pasta>: imprime as mensagens em português e devolve
// 0 se gravou as imagens, 1 se houve erro (arquivo, configuração ou gravação).
static int exportWithoutWindow(const QString& configPath, const QString& dir) {
    std::string text;
    QString error;
    if (!MainWindow::readConfigFile(configPath, text, &error)) {
        std::printf("%s\n", error.toLocal8Bit().constData());
        return 1;
    }
    const ParseResult parsed = parser::parseConfig(text);
    for (const std::string& line : parsed.errors) std::printf("%s\n", line.c_str());
    for (const std::string& line : parsed.warnings) std::printf("%s\n", line.c_str());
    Simulator sim;
    if (!parsed.ok() || !sim.load(parsed.config)) return 1;
    const RunSummary summary = sim.runToEnd();
    std::printf("%s\n", summary.message.c_str());

    QDir().mkpath(dir);
    GanttOptions options;
    options.title = QFileInfo(configPath).completeBaseName();
    const QString base = QDir(dir).filePath(GanttRenderer::defaultBaseName(options.title));
    QStringList files;
    if (!GanttRenderer::exportPng(base + QStringLiteral(".png"), sim.history(), options, &error, &files) ||
        !GanttRenderer::exportSvg(base + QStringLiteral(".svg"), sim.history(), options, &error)) {
        std::printf("%s\n", error.toLocal8Bit().constData());
        return 1;
    }
    files << base + QStringLiteral(".svg");
    for (const QString& file : files) std::printf("Gravado: %s\n", QDir::toNativeSeparators(file).toLocal8Bit().constData());
    return 0;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Simulador de SO"));

    // QApplication::arguments() preserva acentos e espaços nos caminhos (Windows).
    const QStringList args = QApplication::arguments();
    if (args.size() >= 4 && args.at(1) == QStringLiteral("--exportar")) {
        return exportWithoutWindow(args.at(2), args.at(3));
    }

    MainWindow window;
    window.show();
    if (args.size() >= 2 && !args.at(1).startsWith(QStringLiteral("--"))) {
        window.openConfigFile(args.at(1));
    }
    return app.exec();
}
