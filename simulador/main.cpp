// =============================================================================
// main.cpp — Ponto de entrada da interface gráfica (sim_gui).
//
// PROVISÓRIO (Fase 0): este arquivo só prova que o Qt funciona e que a pasta
// standalone (dist/) roda sem instalar o Qt. A janela real (MainWindow, Gantt,
// diálogos) substitui este conteúdo nas Fases 5 e 6.
//
// O quê: abre uma janela com o botão "Gerar imagens de teste", que grava um
// PNG (QImage + QPainter) e um SVG (QSvgGenerator) desenhados pela MESMA
// função drawTestPicture. Também aceita, sem abrir janela:
//     sim_gui.exe --gerar-imagens <pasta>
// usado para testar o dist/ automaticamente (com PATH mínimo, em outra pasta).
//
// Por quê:
//   - O empacotamento do Qt é o maior risco técnico e não depende do motor
//     (D25), então é testado logo no início.
//   - Uma única função de desenho para tela, PNG e SVG é a base do requisito
//     A2.4 (a imagem exportada NÃO pode ser um print da janela); aqui provamos
//     que QImage e QSvgGenerator aceitam o mesmo código de QPainter.
//   - O modo por linha de comando também carrega o plugin de plataforma
//     (platforms/qwindows.dll) e a DLL do Qt Svg; se o deploy esquecer algo,
//     ele falha do mesmo jeito que a janela falharia.
//
// Requisitos: RG5 (executar sem instalar nada), A2.4 (imagem gerada por
// desenho, não por captura). Decisões: D25, D26 (Qt dinâmico + deploy em pasta).
// =============================================================================

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardPaths>
#include <QSvgGenerator>
#include <QVBoxLayout>
#include <QWidget>

// Tamanho fixo das imagens de teste, em pixels.
static const QSize TEST_PICTURE_SIZE(640, 260);

// -----------------------------------------------------------------------------
// Desenha um "mini Gantt" de exemplo com os elementos que o Gantt real terá:
// bloco sólido colorido com rótulo da CPU (executando), bloco só com contorno
// (pronta = ausência de cor, A2.1), bloco preto pontilhado (suspensa, A2.1) e
// setas de chegada/término (A2.2). É chamada tanto para o PNG quanto para o SVG:
// o QPainter não sabe (nem precisa saber) qual é o destino do desenho.
// -----------------------------------------------------------------------------
static void drawTestPicture(QPainter& painter, const QSize& size) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(QRect(QPoint(0, 0), size), Qt::white);

    const int left = 60;       // espaço para os rótulos das tarefas
    const int tickWidth = 50;  // largura de 1 tick em pixels
    const int rowHeight = 50;  // altura de cada linha de tarefa
    const int axisY = 210;     // posição do eixo X (tempo)

    // Título.
    painter.setPen(Qt::black);
    painter.drawText(QRect(0, 5, size.width(), 25), Qt::AlignCenter,
                     QStringLiteral("Teste do Qt (Fase 0): mesmo desenho em PNG e SVG"));

    // Eixo do tempo com marcação a cada tick.
    painter.drawLine(left, axisY, left + 10 * tickWidth, axisY);
    for (int t = 0; t <= 10; ++t) {
        const int x = left + t * tickWidth;
        painter.drawLine(x, axisY, x, axisY + 5);
        painter.drawText(QRect(x - 15, axisY + 6, 30, 18), Qt::AlignCenter, QString::number(t));
    }

    // Duas tarefas; a de menor ID fica mais perto do eixo X (A2.5).
    const int yTask1 = axisY - rowHeight;      // linha da tarefa 1
    const int yTask2 = axisY - 2 * rowHeight;  // linha da tarefa 2
    painter.drawText(QRect(0, yTask1 + 10, left - 8, 30), Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("T1"));
    painter.drawText(QRect(0, yTask2 + 10, left - 8, 30), Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("T2"));

    // Retângulo de uma célula: tarefa na linha 'rowTop', ticks [from, to).
    auto cell = [&](int rowTop, int from, int to) {
        return QRect(left + from * tickWidth, rowTop + 12, (to - from) * tickWidth, rowHeight - 20);
    };

    // T1 executando em [0,3) na CPU1: sólido na cor da tarefa + rótulo.
    painter.setPen(Qt::black);
    painter.setBrush(QColor(0xFF, 0x40, 0x40));
    painter.drawRect(cell(yTask1, 0, 3));
    painter.drawText(cell(yTask1, 0, 3), Qt::AlignCenter, QStringLiteral("CPU1"));

    // T2 pronta em [0,3): só contorno (ausência de cor).
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(cell(yTask2, 0, 3));

    // T2 executando em [3,6).
    painter.setBrush(QColor(0x40, 0x60, 0xFF));
    painter.drawRect(cell(yTask2, 3, 6));
    painter.setPen(Qt::white);
    painter.drawText(cell(yTask2, 3, 6), Qt::AlignCenter, QStringLiteral("CPU1"));

    // T1 suspensa em [6,9): preto com preenchimento pontilhado.
    painter.setPen(Qt::black);
    painter.setBrush(QBrush(Qt::black, Qt::Dense5Pattern));
    painter.drawRect(cell(yTask1, 6, 9));

    // Seta para cima (chegada, verde) e para baixo (término, azul), em x exato.
    auto arrow = [&](int x, int yTop, int yBottom, bool up, const QColor& color) {
        QPainterPath path;
        path.moveTo(x, yTop);
        path.lineTo(x, yBottom);
        const int tipY = up ? yTop : yBottom;
        const int dir = up ? 1 : -1;
        path.moveTo(x - 5, tipY + dir * 8);
        path.lineTo(x, tipY);
        path.lineTo(x + 5, tipY + dir * 8);
        painter.setPen(QPen(color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    };
    arrow(left + 0 * tickWidth, yTask1 + 2, yTask1 + rowHeight - 2, true, QColor(0, 150, 0));
    arrow(left + 3 * tickWidth, yTask1 + 2, yTask1 + rowHeight - 2, false, QColor(0, 0, 200));
    arrow(left + 0 * tickWidth + 4, yTask2 + 2, yTask2 + rowHeight - 2, true, QColor(0, 150, 0));
    arrow(left + 6 * tickWidth, yTask2 + 2, yTask2 + rowHeight - 2, false, QColor(0, 0, 200));
}

// -----------------------------------------------------------------------------
// Grava teste_qt.png e teste_qt.svg na pasta 'dir'. Devolve true se os dois
// arquivos foram gravados; em 'message' explica o resultado em português
// (caminhos gravados ou o motivo da falha), para mostrar ao usuário (A5).
// -----------------------------------------------------------------------------
static bool generateTestImages(const QString& dir, QString* message) {
    const QString pngPath = QDir(dir).filePath(QStringLiteral("teste_qt.png"));
    const QString svgPath = QDir(dir).filePath(QStringLiteral("teste_qt.svg"));

    // PNG: desenha numa imagem em memória (QImage) e salva. Nada vem da tela.
    QImage image(TEST_PICTURE_SIZE, QImage::Format_ARGB32_Premultiplied);
    {
        QPainter painter(&image);
        drawTestPicture(painter, TEST_PICTURE_SIZE);
    }  // o QPainter precisa terminar antes de salvar
    if (!image.save(pngPath, "PNG")) {
        *message = QStringLiteral("Não foi possível gravar o PNG em \"%1\" "
                                  "(pasta inexistente ou sem permissão de escrita).").arg(pngPath);
        return false;
    }

    // SVG: o mesmo desenho, agora com o QSvgGenerator como destino.
    QSvgGenerator generator;
    generator.setFileName(svgPath);
    generator.setSize(TEST_PICTURE_SIZE);
    generator.setViewBox(QRect(QPoint(0, 0), TEST_PICTURE_SIZE));
    generator.setTitle(QStringLiteral("Teste do Qt (Fase 0)"));
    generator.setDescription(QStringLiteral("Mesmo desenho do PNG, gerado por QSvgGenerator"));
    QPainter svgPainter;
    if (!svgPainter.begin(&generator)) {
        *message = QStringLiteral("Não foi possível gravar o SVG em \"%1\" "
                                  "(pasta inexistente ou sem permissão de escrita).").arg(svgPath);
        return false;
    }
    drawTestPicture(svgPainter, TEST_PICTURE_SIZE);
    svgPainter.end();

    *message = QStringLiteral("Imagens gravadas:\n%1\n%2")
                   .arg(QDir::toNativeSeparators(pngPath), QDir::toNativeSeparators(svgPath));
    return true;
}

// -----------------------------------------------------------------------------
// main: modo por linha de comando (--gerar-imagens <pasta>) ou janela de teste.
// -----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // QApplication::arguments() devolve os argumentos em Unicode, o que
    // preserva pastas com espaços e acentos no Windows (argv seria ANSI).
    const QStringList args = QApplication::arguments();
    if (args.size() >= 3 && args.at(1) == QStringLiteral("--gerar-imagens")) {
        QString message;
        return generateTestImages(args.at(2), &message) ? 0 : 1;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("Simulador de SO — teste do Qt (Fase 0)"));

    auto* info = new QLabel(QStringLiteral(
        "Janela provisória para provar que o Qt funciona sem instalação.\n"
        "O botão grava um PNG e um SVG com o mesmo código de desenho."));
    auto* button = new QPushButton(QStringLiteral("Gerar imagens de teste"));
    auto* status = new QLabel;

    // Os widgets são filhos de 'window' (pelo layout): o Qt os libera junto com ela.
    auto* layout = new QVBoxLayout(&window);
    layout->addWidget(info);
    layout->addWidget(button);
    layout->addWidget(status);

    QObject::connect(button, &QPushButton::clicked, &window, [&window, status]() {
        // O usuário escolhe a pasta: nenhum caminho fixo (mesmo espírito do A3.3.4).
        const QString dir = QFileDialog::getExistingDirectory(
            &window, QStringLiteral("Escolha a pasta onde gravar as imagens"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (dir.isEmpty()) {
            return;  // usuário cancelou
        }
        QString message;
        if (generateTestImages(dir, &message)) {
            status->setText(message);
            QMessageBox::information(&window, QStringLiteral("Imagens geradas"), message);
        } else {
            status->setText(message);
            QMessageBox::critical(&window, QStringLiteral("Erro ao gerar imagens"), message);
        }
    });

    window.resize(480, 180);
    window.show();
    return app.exec();
}
