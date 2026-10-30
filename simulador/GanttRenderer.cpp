// =============================================================================
// GanttRenderer.cpp — Implementação do desenho do Gantt, da exportação PNG/SVG
// e do widget. Ver o cabeçalho de GanttRenderer.h (o quê, por quê, requisitos).
// =============================================================================

#include "GanttRenderer.h"
#include "Simulator.h"  // finished()

#include <QDateTime>
#include <QEvent>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QImage>
#include <QPaintEvent>
#include <QPolygonF>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSvgGenerator>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

// ---------------------------------------------------------------------------
// Constantes visuais (pixels). Ficam juntas para o estilo ser ajustado num só lugar.
// ---------------------------------------------------------------------------
const double MARGIN = 10;          // borda em volta do desenho
const double TITLE_HEIGHT = 28;    // faixa do título
const double AXIS_LABELS = 22;     // números embaixo de cada eixo
const double PANEL_GAP = 36;       // espaço entre o painel de tarefas e o de CPUs (título "Processadores")
const double MIN_WIDTH = 760;      // largura mínima (a legenda precisa de espaço)
const double LEGEND_ROW = 24;      // altura de cada linha da legenda
const double LEGEND_SAMPLE = 30;   // largura da amostra (barra/ícone) na legenda
const int MAX_PNG_WIDTH = 30000;   // largura máxima de um PNG (design.md §10)

// Cores dos ícones: uma por tipo, todas distintas e diferentes do preto/branco
// usados nos estados (RG3: elemento gráfico distinto para cada evento).
const QColor ARRIVAL_COLOR(0, 150, 0);
const QColor TERMINATION_COLOR(0, 80, 210);
const QColor MISS_COLOR(220, 0, 0);
const QColor PREEMPTION_COLOR(240, 140, 0);
const QColor MIGRATION_COLOR(140, 0, 180);
const QColor EDIT_COLOR(220, 0, 160);
const QColor CPU_ON_COLOR(0, 150, 0);
const QColor CPU_OFF_COLOR(120, 120, 120);
const QColor GRID_COLOR(170, 170, 170);
const QColor HIGHLIGHT_COLOR(255, 220, 0, 70);  // instante atual (translúcido)

// Fontes com tamanho em PIXELS: o mesmo tamanho na tela, no PNG e no SVG
// (pontos dependem da resolução de cada dispositivo).
QFont chartFont() {
    QFont font;
    font.setPixelSize(12);
    return font;
}
QFont smallFont() {
    QFont font;
    font.setPixelSize(10);
    return font;
}

// Estado de uma célula (tarefa x instante) para o desenho (D2 + figuras do livro).
enum class Cell { None, Ready, Running, Suspended };

Cell cellOf(const Task& task) {
    switch (task.state) {
        case TaskState::Ready:     return Cell::Ready;
        case TaskState::Running:   return Cell::Running;
        case TaskState::Suspended: return Cell::Suspended;
        default:                   return Cell::None;  // fora do sistema: nada (como no livro)
    }
}

// Cor da tarefa (RRGGBB do arquivo, A2.1).
QColor taskColor(const Task& task) {
    QColor color(QStringLiteral("#") + QString::fromStdString(task.color));
    return color.isValid() ? color : QColor(Qt::gray);
}

// Texto preto ou branco conforme a luminância do fundo (legível em qualquer cor).
QColor textColorOn(const QColor& background) {
    const double luminance = 0.299 * background.red() + 0.587 * background.green() +
                             0.114 * background.blue();
    return luminance < 140 ? Qt::white : Qt::black;
}

QString qs(const std::string& text) { return QString::fromStdString(text); }

// Seta de 'from' até 'to' com ponta cheia.
void drawArrow(QPainter& p, QPointF from, QPointF to, const QColor& color, double width = 1.8) {
    p.setPen(QPen(color, width));
    p.drawLine(from, to);
    const QPointF d = to - from;
    const double length = std::hypot(d.x(), d.y());
    if (length <= 0) return;
    const QPointF u = d / length;           // direção
    const QPointF n(-u.y(), u.x());          // perpendicular
    QPolygonF head;
    head << to << (to - u * 5.5 + n * 3.2) << (to - u * 5.5 - n * 3.2);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(head);
}

// Símbolo de "liga/desliga" (círculo aberto + traço), para CpuOn/CpuOff.
void drawPower(QPainter& p, QPointF center, double r, const QColor& color, bool crossed) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 1.6));
    const QRectF circle(center.x() - r, center.y() - r, 2 * r, 2 * r);
    p.drawArc(circle, 120 * 16, 300 * 16);  // abre em cima
    p.drawLine(QPointF(center.x(), center.y() - r - 1), QPointF(center.x(), center.y()));
    if (crossed) {
        p.setPen(QPen(MISS_COLOR, 1.6));
        p.drawLine(QPointF(center.x() - r - 1, center.y() + r + 1),
                   QPointF(center.x() + r + 1, center.y() - r - 1));
    }
}

// Ícone de uma ocorrência na faixa [top, top+h] de uma linha, no instante x
// (A2.2: no momento exato). Cada tipo ocupa uma parte da faixa para que
// ícones do mesmo instante não se cubram.
void drawIcon(QPainter& p, OccurrenceType type, double x, double top, double h) {
    p.save();
    const double mid = top + h / 2;
    const double s = std::max(3.0, h * 0.14);  // tamanho básico
    switch (type) {
    case OccurrenceType::Arrival:       // seta para cima, verde, na metade de cima
        drawArrow(p, QPointF(x, mid), QPointF(x, top + 1), ARRIVAL_COLOR);
        break;
    case OccurrenceType::Termination:   // seta para baixo, azul, na metade de baixo
        drawArrow(p, QPointF(x, mid), QPointF(x, top + h - 1), TERMINATION_COLOR);
        break;
    case OccurrenceType::TaskFinished:  // seta para baixo com barra: fim da tarefa
        drawArrow(p, QPointF(x, mid), QPointF(x, top + h - 2), TERMINATION_COLOR);
        p.setPen(QPen(TERMINATION_COLOR, 2.2));
        p.drawLine(QPointF(x - s - 1, top + h - 1), QPointF(x + s + 1, top + h - 1));
        break;
    case OccurrenceType::DeadlineMiss: {  // linha tracejada + X vermelho
        p.setPen(QPen(MISS_COLOR, 1.5, Qt::DashLine));
        p.drawLine(QPointF(x, top), QPointF(x, top + h));
        const QPointF c(x + s + 2, top + s + 1);
        p.setPen(QPen(MISS_COLOR, 2.2));
        p.drawLine(c + QPointF(-s, -s), c + QPointF(s, s));
        p.drawLine(c + QPointF(-s, s), c + QPointF(s, -s));
        break;
    }
    case OccurrenceType::Preemption: {  // triângulo laranja para baixo, embaixo da barra
        QPolygonF tri;
        tri << QPointF(x - s - 1, top + h - 2 * s - 2) << QPointF(x + s + 1, top + h - 2 * s - 2)
            << QPointF(x, top + h);
        p.setPen(QPen(Qt::black, 0.8));
        p.setBrush(PREEMPTION_COLOR);
        p.drawPolygon(tri);
        break;
    }
    case OccurrenceType::Migration: {  // seta dupla roxa (troca de CPU)
        const double y = top + h - s;
        const double cx = x + s + 4;
        drawArrow(p, QPointF(cx, y), QPointF(cx + s + 2, y), MIGRATION_COLOR, 1.5);
        drawArrow(p, QPointF(cx, y), QPointF(cx - s - 2, y), MIGRATION_COLOR, 1.5);
        break;
    }
    case OccurrenceType::TieBreakDraw: {  // dado: quadrado com três pontos
        const double side = 2 * s + 2;
        const QRectF die(x - side - 2, top, side, side);
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(Qt::white);
        p.drawRect(die);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        for (int i = 0; i < 3; ++i) {
            const QPointF dot(die.left() + side * (0.25 + 0.25 * i), die.top() + side * (0.25 + 0.25 * i));
            p.drawEllipse(dot, 1.0, 1.0);
        }
        break;
    }
    case OccurrenceType::CpuOn:
        drawPower(p, QPointF(x + s + 3, top + s + 2), s, CPU_ON_COLOR, false);
        break;
    case OccurrenceType::CpuOff:
        drawPower(p, QPointF(x + s + 3, top + s + 2), s, CPU_OFF_COLOR, true);
        break;
    case OccurrenceType::ManualEdit: {  // losango magenta
        const QPointF c(x - s - 2, top + h - s - 1);
        QPolygonF diamond;
        diamond << c + QPointF(0, -s) << c + QPointF(s, 0) << c + QPointF(0, s) << c + QPointF(-s, 0);
        p.setPen(QPen(Qt::black, 0.8));
        p.setBrush(EDIT_COLOR);
        p.drawPolygon(diamond);
        break;
    }
    case OccurrenceType::Dispatch:  // só log: sem ícone (design.md §10)
        break;
    }
    p.restore();
}

// Preenchimento pontilhado (suspensa: "cor preta e preenchimento diferente",
// A2.1). Pontos desenhados um a um, e não por padrão de pincel, para sair
// igual no PNG e no SVG.
void drawDots(QPainter& p, const QRectF& r) {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (double y = r.top() + 2.5; y < r.bottom() - 1; y += 4) {
        for (double x = r.left() + 2.5; x < r.right() - 1; x += 4) p.drawEllipse(QPointF(x, y), 0.9, 0.9);
    }
}

// Hachura diagonal cinza (CPU desligada, A1.2/D11), recortada à mão no retângulo.
void drawHatch(QPainter& p, const QRectF& r) {
    p.setPen(QPen(CPU_OFF_COLOR, 1));
    const double h = r.height();
    for (double d = -h; d < r.width(); d += 6) {
        double x1 = r.left() + d, y1 = r.bottom();
        double x2 = r.left() + d + h, y2 = r.top();
        if (x1 < r.left()) { y1 -= (r.left() - x1); x1 = r.left(); }
        if (x2 > r.right()) { y2 += (x2 - r.right()); x2 = r.right(); }
        if (x2 > x1) p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
}

// Uma barra (trecho contínuo no mesmo estado), no estilo do livro: contorno
// preto; executando = cor sólida (+ "CPUk"); pronta = branco; suspensa = pontos.
void drawBar(QPainter& p, const QRectF& r, Cell cell, const QColor& color, const QString& label) {
    const QPen outline(Qt::black, 1.2);
    if (cell == Cell::Running) {
        p.setPen(outline);
        p.setBrush(color);
        p.drawRect(r);
        if (!label.isEmpty()) {
            p.setFont(smallFont());
            if (QFontMetricsF(smallFont()).horizontalAdvance(label) + 4 <= r.width()) {
                p.setPen(textColorOn(color));
                p.drawText(r, Qt::AlignCenter, label);
            }
            p.setFont(chartFont());
        }
    } else if (cell == Cell::Ready) {
        p.setPen(outline);
        p.setBrush(Qt::white);
        p.drawRect(r);
    } else if (cell == Cell::Suspended) {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRect(r);
        drawDots(p, r);
        p.setPen(outline);
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }
}

// Faixa vertical da linha 'row' e retângulo da barra dentro dela.
double rowTop(double panelTop, int row, int rowHeight) { return panelTop + row * rowHeight; }
QRectF barRect(double x0, double x1, double top, int rowHeight) {
    return QRectF(x0, top + rowHeight * 0.22, x1 - x0, rowHeight * 0.56);
}

// Nome de cada ocorrência em português (tooltip).
QString occurrenceName(OccurrenceType type) {
    switch (type) {
        case OccurrenceType::Arrival:      return QStringLiteral("chegada (ativação)");
        case OccurrenceType::Termination:  return QStringLiteral("fim de uma ocorrência");
        case OccurrenceType::TaskFinished: return QStringLiteral("fim da tarefa");
        case OccurrenceType::DeadlineMiss: return QStringLiteral("perda de prazo");
        case OccurrenceType::Dispatch:     return QStringLiteral("despacho");
        case OccurrenceType::Preemption:   return QStringLiteral("preempção");
        case OccurrenceType::Migration:    return QStringLiteral("migração de CPU");
        case OccurrenceType::TieBreakDraw: return QStringLiteral("escolhida por sorteio");
        case OccurrenceType::CpuOn:        return QStringLiteral("CPU ligada");
        case OccurrenceType::CpuOff:       return QStringLiteral("CPU desligada");
        case OccurrenceType::ManualEdit:   return QStringLiteral("edição manual");
    }
    return QString();
}

// Itens da legenda, na ordem em que aparecem.
struct LegendItem {
    int kind;  // 0..3 = estados (executando, pronta, suspensa, CPU desligada); 4 = ícone; 5 = instante atual
    OccurrenceType type;
    QString text;
};
const std::vector<LegendItem>& legendItems() {
    static const std::vector<LegendItem> items = {
        {0, OccurrenceType::Dispatch, QStringLiteral("Executando (cor da tarefa; CPUk = processador)")},
        {1, OccurrenceType::Dispatch, QStringLiteral("Pronta (sem cor)")},
        {2, OccurrenceType::Dispatch, QStringLiteral("Suspensa (preto pontilhado)")},
        {3, OccurrenceType::Dispatch, QStringLiteral("CPU desligada")},
        {4, OccurrenceType::Arrival, QStringLiteral("Chegada (ativação)")},
        {4, OccurrenceType::Termination, QStringLiteral("Fim de uma ocorrência")},
        {4, OccurrenceType::TaskFinished, QStringLiteral("Fim da tarefa (10ª ocorrência)")},
        {4, OccurrenceType::DeadlineMiss, QStringLiteral("Perda de prazo")},
        {4, OccurrenceType::Preemption, QStringLiteral("Preempção")},
        {4, OccurrenceType::Migration, QStringLiteral("Migração de CPU")},
        {4, OccurrenceType::TieBreakDraw, QStringLiteral("Sorteio (desempate)")},
        {4, OccurrenceType::CpuOn, QStringLiteral("CPU ligada")},
        {4, OccurrenceType::CpuOff, QStringLiteral("CPU desligada (evento)")},
        {4, OccurrenceType::ManualEdit, QStringLiteral("Edição manual")},
        {5, OccurrenceType::Dispatch, QStringLiteral("Instante atual")},
    };
    return items;
}

// Posição de cada item da legenda dentro de 'width' (quebra de linha automática).
std::vector<QRectF> legendBoxes(double width) {
    std::vector<QRectF> boxes;
    const QFontMetricsF fm(chartFont());
    double x = 0, y = 20;  // 20 = linha do título "Legenda"
    for (const LegendItem& item : legendItems()) {
        const double w = LEGEND_SAMPLE + 6 + fm.horizontalAdvance(item.text) + 18;
        if (x > 0 && x + w > width) {
            x = 0;
            y += LEGEND_ROW;
        }
        boxes.emplace_back(x, y, w, LEGEND_ROW);
        x += w;
    }
    return boxes;
}

// Passo dos rótulos do eixo: 1, 2, 5, 10, 20, 50... o menor que deixa espaço para o número.
Tick chooseLabelStep(Tick lastTick, int tickWidth) {
    const double needed = QFontMetricsF(chartFont()).horizontalAdvance(QString::number(lastTick)) + 10;
    Tick step = 1;
    while (true) {
        for (Tick m : {1, 2, 5}) {
            if (static_cast<double>(step * m) * tickWidth >= needed) return step * m;
        }
        step *= 10;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Geometria
// ---------------------------------------------------------------------------

Tick GanttLayout::tickAt(double x) const {
    return from + static_cast<Tick>(std::floor((x - left) / tickWidth));
}

int GanttLayout::taskRowAt(double y) const {
    if (y < tasksTop || y >= tasksTop + static_cast<double>(taskRows.size()) * rowHeight) return -1;
    return static_cast<int>((y - tasksTop) / rowHeight);
}

int GanttLayout::cpuRowAt(double y) const {
    if (cpuCount == 0 || y < cpuTop || y >= cpuTop + static_cast<double>(cpuCount) * rowHeight) return -1;
    return static_cast<int>((y - cpuTop) / rowHeight);
}

GanttLayout GanttRenderer::layout(const std::vector<SystemSnapshot>& history, std::size_t upto,
                                  const GanttOptions& options) {
    GanttLayout L;
    L.tickWidth = std::max(1, options.tickWidth);
    L.rowHeight = std::max(12, options.rowHeight);
    upto = std::min(upto, history.size());
    if (upto == 0) {
        L.width = MIN_WIDTH;
        L.height = 100;
        return L;
    }
    const SystemSnapshot& last = history[upto - 1];

    // Faixa de tempo: até o último instante. Se a simulação acabou, o último
    // instante só tem ocorrências (ex.: o último término), desenhadas na ponta do eixo.
    const Tick end = finished(last) ? std::max<Tick>(last.tick, 1) : last.tick + 1;
    L.from = std::max<Tick>(0, std::min(options.fromTick, end - 1));
    L.to = options.toTick >= 0 ? std::min(options.toTick, end) : end;
    if (L.to <= L.from) L.to = L.from + 1;

    // Linhas das tarefas simuladas: maior ID no topo, menor colado no eixo X (A2.5, D4).
    for (int i = 0; i < static_cast<int>(last.tasks.size()); ++i) {
        if (!last.tasks[i].ignored) L.taskRows.push_back(i);
    }
    std::sort(L.taskRows.begin(), L.taskRows.end(), [&last](int a, int b) {
        return naturalIdLess(last.tasks[b].id, last.tasks[a].id);  // decrescente
    });

    // Nº de CPUs: o maior no trecho (uma edição pode aumentar ou reduzir).
    for (Tick k = L.from; k <= L.to && k < static_cast<Tick>(upto); ++k) {
        L.cpuCount = std::max(L.cpuCount, static_cast<int>(history[k].cpus.size()));
    }

    // Coluna dos rótulos: cabe o maior ID (com a amostra de cor) e "CPU64".
    const QFontMetricsF fm(chartFont());
    double labelWidth = fm.horizontalAdvance(QStringLiteral("CPU64"));
    for (int i : L.taskRows) labelWidth = std::max(labelWidth, fm.horizontalAdvance(qs(last.tasks[i].id)) + 16);
    L.left = MARGIN + labelWidth + 16;

    L.tasksTop = TITLE_HEIGHT + 16;  // espaço para a seta do eixo Y
    const int rows = std::max<int>(1, static_cast<int>(L.taskRows.size()));
    L.axisY = L.tasksTop + rows * L.rowHeight + 4;
    double bottom = L.axisY + AXIS_LABELS;
    if (options.showCpuPanel && L.cpuCount > 0) {
        L.cpuTop = bottom + PANEL_GAP;
        L.cpuAxisY = L.cpuTop + L.cpuCount * L.rowHeight + 4;
        bottom = L.cpuAxisY + AXIS_LABELS;
    }
    L.width = std::max(MIN_WIDTH, L.xOf(L.to) + 40);
    L.labelStep = chooseLabelStep(L.to, L.tickWidth);
    if (options.showLegend) {
        L.legendTop = bottom + 12;
        L.height = L.legendTop + legendHeight(L.width - 2 * MARGIN) + MARGIN;
    } else {
        L.height = bottom + MARGIN;
    }
    return L;
}

QSize GanttRenderer::contentSize(const std::vector<SystemSnapshot>& history, std::size_t upto,
                                 const GanttOptions& options) {
    const GanttLayout L = layout(history, upto, options);
    return QSize(static_cast<int>(std::ceil(L.width)), static_cast<int>(std::ceil(L.height)));
}

// ---------------------------------------------------------------------------
// Desenho
// ---------------------------------------------------------------------------

double GanttRenderer::legendHeight(double width) {
    const std::vector<QRectF> boxes = legendBoxes(width);
    return boxes.empty() ? 0 : boxes.back().bottom();
}

void GanttRenderer::paintLegend(QPainter& p, const QRectF& area) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(chartFont());
    p.setPen(Qt::black);
    p.drawText(QRectF(area.left(), area.top(), area.width(), 18), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Legenda"));
    const std::vector<QRectF> boxes = legendBoxes(area.width());
    const std::vector<LegendItem>& items = legendItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        const QRectF box = boxes[i].translated(area.topLeft());
        const QRectF sample(box.left(), box.top() + 2, LEGEND_SAMPLE, LEGEND_ROW - 4);
        const QRectF bar(sample.left(), sample.top() + 4, sample.width(), sample.height() - 8);
        switch (items[i].kind) {
            case 0: drawBar(p, bar, Cell::Running, QColor(70, 120, 210), QString()); break;
            case 1: drawBar(p, bar, Cell::Ready, Qt::white, QString()); break;
            case 2: drawBar(p, bar, Cell::Suspended, Qt::black, QString()); break;
            case 3:
                p.setPen(QPen(CPU_OFF_COLOR, 1));
                p.setBrush(Qt::NoBrush);
                drawHatch(p, bar);
                p.setPen(QPen(CPU_OFF_COLOR, 1));
                p.drawRect(bar);
                break;
            case 4: drawIcon(p, items[i].type, sample.center().x(), sample.top(), sample.height()); break;
            case 5: p.fillRect(bar, HIGHLIGHT_COLOR); break;
            default: break;
        }
        p.setPen(Qt::black);
        p.drawText(QRectF(sample.right() + 6, box.top(), box.width() - LEGEND_SAMPLE - 6, LEGEND_ROW),
                   Qt::AlignLeft | Qt::AlignVCenter, items[i].text);
    }
    p.restore();
}

void GanttRenderer::paint(QPainter& p, const QRectF& visible, const std::vector<SystemSnapshot>& history,
                          std::size_t upto, const GanttOptions& options) {
    p.save();
    p.fillRect(visible, Qt::white);
    upto = std::min(upto, history.size());
    if (upto == 0) {
        p.restore();
        return;
    }
    const GanttLayout L = layout(history, upto, options);
    const SystemSnapshot& last = history[upto - 1];
    const Tick lastIndex = static_cast<Tick>(upto) - 1;
    const int rh = L.rowHeight;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(chartFont());

    // Só os instantes visíveis são desenhados em detalhe (100 000 ticks na tela).
    const Tick t0 = std::max(L.from, L.tickAt(visible.left()) - 1);
    const Tick t1 = std::min(L.to, L.tickAt(visible.right()) + 2);  // colunas [t0, t1)

    // --- Título: configuração, algoritmo, CPUs, semente, instante final.
    {
        QString title = options.title.isEmpty() ? QStringLiteral("Gráfico de Gantt") : options.title;
        title += QStringLiteral(" — %1, %2 CPU(s), semente %3, instante %4%5")
                     .arg(qs(last.algorithm))
                     .arg(last.cpus.size())
                     .arg(QString::number(last.seed))
                     .arg(last.tick)
                     .arg(finished(last) ? QStringLiteral(" (fim)") : QString());
        if (L.from > 0 || L.to < (finished(last) ? last.tick : last.tick + 1)) {
            title += QStringLiteral("  [instantes %1 a %2]").arg(L.from).arg(L.to);
        }
        QFont bold = chartFont();
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(Qt::black);
        p.drawText(QRectF(MARGIN, 0, L.width - 2 * MARGIN, TITLE_HEIGHT), Qt::AlignLeft | Qt::AlignVCenter, title);
        p.setFont(chartFont());
    }

    // --- Linhas pontilhadas verticais a cada 'labelStep' instantes (como no livro).
    p.setPen(QPen(GRID_COLOR, 1, Qt::DotLine));
    for (Tick k = ((t0 + L.labelStep - 1) / L.labelStep) * L.labelStep; k <= t1; k += L.labelStep) {
        const double x = L.xOf(k);
        p.drawLine(QPointF(x, L.tasksTop - 6), QPointF(x, L.axisY));
        if (L.cpuCount > 0 && options.showCpuPanel) p.drawLine(QPointF(x, L.cpuTop - 6), QPointF(x, L.cpuAxisY));
    }

    // --- Rótulos das linhas: amostra de cor + ID da tarefa; CPU1..CPUn.
    for (int r = 0; r < static_cast<int>(L.taskRows.size()); ++r) {
        const Task& t = last.tasks[L.taskRows[r]];
        const double top = rowTop(L.tasksTop, r, rh);
        p.setPen(QPen(Qt::black, 0.8));
        p.setBrush(taskColor(t));
        p.drawRect(QRectF(MARGIN, top + rh / 2.0 - 5, 10, 10));
        p.setPen(Qt::black);
        p.drawText(QRectF(MARGIN + 14, top, L.left - MARGIN - 22, rh), Qt::AlignRight | Qt::AlignVCenter, qs(t.id));
    }
    if (options.showCpuPanel && L.cpuCount > 0) {
        // Título do painel acima da seta do eixo Y (que sobe até cpuTop - 12).
        p.drawText(QRectF(MARGIN, L.cpuTop - PANEL_GAP, 300, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Processadores"));
        for (int c = 0; c < L.cpuCount; ++c) {
            p.drawText(QRectF(MARGIN, rowTop(L.cpuTop, c, rh), L.left - MARGIN - 8, rh),
                       Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("CPU%1").arg(c + 1));
        }
    }

    // --- Barras das tarefas: trechos contínuos no mesmo estado viram UM retângulo.
    for (int r = 0; r < static_cast<int>(L.taskRows.size()); ++r) {
        const int i = L.taskRows[r];
        const double top = rowTop(L.tasksTop, r, rh);
        Cell runCell = Cell::None;
        int runCpu = -1;
        std::string runColor;
        Tick runStart = t0;
        auto flush = [&](Tick end) {
            if (runCell != Cell::None && end > runStart) {
                const Task& at = history[runStart].tasks[i];
                drawBar(p, barRect(L.xOf(runStart), L.xOf(end), top, rh), runCell, taskColor(at),
                        runCell == Cell::Running ? QStringLiteral("CPU%1").arg(runCpu + 1) : QString());
            }
        };
        for (Tick k = t0; k < t1 && k <= lastIndex; ++k) {
            const SystemSnapshot& s = history[k];
            Cell cell = Cell::None;
            int cpu = -1;
            std::string color;
            if (i < static_cast<int>(s.tasks.size())) {
                cell = cellOf(s.tasks[i]);
                cpu = s.tasks[i].cpu;
                color = s.tasks[i].color;
            }
            if (cell != runCell || cpu != runCpu || color != runColor) {
                flush(k);
                runCell = cell;
                runCpu = cpu;
                runColor = color;
                runStart = k;
            }
        }
        flush(std::min(t1, lastIndex + 1));
    }

    // --- Painel de CPUs: tarefa que ocupou cada CPU (cor + ID) ou hachura se desligada.
    if (options.showCpuPanel) {
        for (int c = 0; c < L.cpuCount; ++c) {
            const double top = rowTop(L.cpuTop, c, rh);
            int runKind = 0;  // 0 = não existe, 1 = desligada, 2 = executando
            int runTask = -1;
            Tick runStart = t0;
            auto flush = [&](Tick end) {
                if (runKind == 0 || end <= runStart) return;
                const QRectF r = barRect(L.xOf(runStart), L.xOf(end), top, rh);
                if (runKind == 1) {
                    drawHatch(p, r);
                    p.setPen(QPen(CPU_OFF_COLOR, 1));
                    p.setBrush(Qt::NoBrush);
                    p.drawRect(r);
                } else {
                    const Task& t = history[runStart].tasks[runTask];
                    drawBar(p, r, Cell::Running, taskColor(t), qs(t.id));
                }
            };
            for (Tick k = t0; k < t1 && k <= lastIndex; ++k) {
                const SystemSnapshot& s = history[k];
                int kind = 0, task = -1;
                if (c < static_cast<int>(s.cpus.size())) {
                    task = s.cpus[c].taskIndex;
                    kind = task >= 0 ? 2 : 1;
                }
                if (kind != runKind || task != runTask) {
                    flush(k);
                    runKind = kind;
                    runTask = task;
                    runStart = k;
                }
            }
            flush(std::min(t1, lastIndex + 1));
        }
    }

    // --- Destaque do instante atual (passo a passo, A2.3).
    if (options.currentTick >= L.from && options.currentTick < L.to) {
        const double x = L.xOf(options.currentTick);
        p.fillRect(QRectF(x, L.tasksTop - 6, L.tickWidth, L.axisY - L.tasksTop + 6), HIGHLIGHT_COLOR);
        if (options.showCpuPanel && L.cpuCount > 0) {
            p.fillRect(QRectF(x, L.cpuTop - 6, L.tickWidth, L.cpuAxisY - L.cpuTop + 6), HIGHLIGHT_COLOR);
        }
    }

    // --- Eixos: Y com seta para cima, X com seta para a direita e "t" (como no livro).
    auto drawAxes = [&](double axisY, double panelTop) {
        const double xEnd = L.xOf(L.to) + 18;
        p.setPen(QPen(Qt::black, 1.2));
        drawArrow(p, QPointF(L.left, axisY), QPointF(L.left, panelTop - 12), Qt::black, 1.2);
        drawArrow(p, QPointF(L.left, axisY), QPointF(xEnd, axisY), Qt::black, 1.2);
        p.setPen(Qt::black);
        p.drawText(QRectF(xEnd - 4, axisY - 20, 20, 16), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("t"));
        for (Tick k = ((t0 + L.labelStep - 1) / L.labelStep) * L.labelStep; k <= t1 && k <= L.to; k += L.labelStep) {
            const double x = L.xOf(k);
            p.setPen(QPen(Qt::black, 1));
            p.drawLine(QPointF(x, axisY), QPointF(x, axisY + 4));
            p.drawText(QRectF(x - 40, axisY + 4, 80, AXIS_LABELS - 4), Qt::AlignHCenter | Qt::AlignTop,
                       QString::number(k));
        }
    };
    drawAxes(L.axisY, L.tasksTop);
    if (options.showCpuPanel && L.cpuCount > 0) drawAxes(L.cpuAxisY, L.cpuTop);

    // --- Ícones das ocorrências no instante exato x = k (A2.2, RG3).
    std::vector<int> rowOfTask(last.tasks.size(), -1);
    for (int r = 0; r < static_cast<int>(L.taskRows.size()); ++r) rowOfTask[L.taskRows[r]] = r;
    const Tick o0 = std::max(L.from, t0 - 1);
    const Tick o1 = std::min({L.to, t1 + 1, lastIndex});
    for (Tick k = o0; k <= o1; ++k) {
        const double x = L.xOf(k);
        for (const Occurrence& o : history[k].occurrences) {
            if (o.type == OccurrenceType::Dispatch) continue;
            if (o.type == OccurrenceType::CpuOn || o.type == OccurrenceType::CpuOff) {
                if (options.showCpuPanel && o.cpuIndex >= 0 && o.cpuIndex < L.cpuCount) {
                    drawIcon(p, o.type, x, rowTop(L.cpuTop, o.cpuIndex, rh), rh);
                }
            } else if (o.taskIndex >= 0 && o.taskIndex < static_cast<int>(rowOfTask.size()) &&
                       rowOfTask[o.taskIndex] >= 0) {
                drawIcon(p, o.type, x, rowTop(L.tasksTop, rowOfTask[o.taskIndex], rh), rh);
            } else if (o.type == OccurrenceType::ManualEdit) {
                drawIcon(p, o.type, x + 7, L.axisY - rh / 2.0 - 2, rh / 2.0);  // edição do sistema: sobre o eixo
            }
        }
    }

    // --- Legenda (nas imagens; o widget mostra a sua, fixa).
    if (options.showLegend) {
        paintLegend(p, QRectF(MARGIN, L.legendTop, L.width - 2 * MARGIN, legendHeight(L.width - 2 * MARGIN)));
    }
    p.restore();
}

// ---------------------------------------------------------------------------
// Tooltip
// ---------------------------------------------------------------------------

QString GanttRenderer::describe(const std::vector<SystemSnapshot>& history, std::size_t upto,
                                const GanttOptions& options, const QPointF& pos) {
    upto = std::min(upto, history.size());
    if (upto == 0) return QString();
    const GanttLayout L = layout(history, upto, options);
    const Tick k = L.tickAt(pos.x());
    if (k < L.from || k > L.to || k >= static_cast<Tick>(upto)) return QString();
    const SystemSnapshot& s = history[k];

    const int row = L.taskRowAt(pos.y());
    if (row >= 0) {
        const int i = L.taskRows[row];
        if (i >= static_cast<int>(s.tasks.size())) return QString();
        const Task& t = s.tasks[i];
        QString state;
        switch (t.state) {
            case TaskState::NotArrived: state = QStringLiteral("ainda não chegou"); break;
            case TaskState::Ready:      state = QStringLiteral("pronta (aguardando CPU)"); break;
            case TaskState::Running:    state = QStringLiteral("executando na CPU%1").arg(t.cpu + 1); break;
            case TaskState::Dormant:    state = QStringLiteral("dormente (aguardando a próxima ativação)"); break;
            case TaskState::Suspended:  state = QStringLiteral("suspensa (%1)").arg(qs(t.blockReason)); break;
            case TaskState::Terminated: state = QStringLiteral("terminada"); break;
        }
        QString text = QStringLiteral("Tarefa %1 — instante %2\nEstado: %3").arg(qs(t.id)).arg(k).arg(state);
        if (const Job* job = currentJob(t)) {
            text += QStringLiteral("\nOcorrência %1: restam %2 de %3; prazo absoluto %4%5")
                        .arg(t.completed + 1).arg(job->remaining).arg(job->duration).arg(job->absDeadline)
                        .arg(job->missed ? QStringLiteral(" (perdido)") : QString());
        }
        text += QStringLiteral("\nOcorrências concluídas: %1 de %2; perdas de prazo: %3")
                    .arg(t.completed).arg(PERIODIC_ACTIVATION_LIMIT).arg(t.deadlineMisses);
        QStringList events;
        for (const Occurrence& o : s.occurrences) {
            if (o.taskIndex == i && o.type != OccurrenceType::Dispatch) {
                events << occurrenceName(o.type) + (o.note.empty() ? QString() : QStringLiteral(" (%1)").arg(qs(o.note)));
            }
        }
        if (!events.isEmpty()) text += QStringLiteral("\nNeste instante: ") + events.join(QStringLiteral(", "));
        return text;
    }

    const int cpu = L.cpuRowAt(pos.y());
    if (options.showCpuPanel && cpu >= 0) {
        if (cpu >= static_cast<int>(s.cpus.size())) return QStringLiteral("CPU%1 — instante %2: não existe neste instante").arg(cpu + 1).arg(k);
        const CPU& c = s.cpus[cpu];
        QString text = QStringLiteral("CPU%1 — instante %2: ").arg(cpu + 1).arg(k);
        text += c.taskIndex >= 0 ? QStringLiteral("executando a tarefa %1").arg(qs(s.tasks[c.taskIndex].id))
                                 : QStringLiteral("desligada");
        text += QStringLiteral("\nAté aqui: %1 tick(s) ocupada, %2 desligada").arg(c.ticksBusy).arg(c.ticksOff);
        return text;
    }

    // Faixa do eixo: edições do sistema neste instante.
    QStringList edits;
    for (const Occurrence& o : s.occurrences) {
        if (o.type == OccurrenceType::ManualEdit && o.taskIndex < 0) edits << qs(o.note);
    }
    if (!edits.isEmpty() && std::abs(pos.y() - L.axisY) < L.rowHeight) {
        return QStringLiteral("Instante %1 — edição manual: %2").arg(k).arg(edits.join(QStringLiteral("; ")));
    }
    return QString();
}

// ---------------------------------------------------------------------------
// Exportação (A2.4): desenho direto em SVG/QImage, nunca captura de tela.
// ---------------------------------------------------------------------------

namespace {
// Opções para exportar a simulação inteira: sem destaque de instante, com
// legenda, e o tick reduzido de 24 até 4 px enquanto a largura passar de
// ~30 000 px. Vale para o PNG (limite de memória da imagem) e para o SVG (com
// tick menor os rótulos ficam mais espaçados e o arquivo, bem menor; o SVG
// continua sendo um arquivo só, com a simulação inteira).
GanttOptions exportOptions(const GanttOptions& options, const std::vector<SystemSnapshot>& history) {
    GanttOptions o = options;
    o.fromTick = 0;
    o.toTick = -1;
    o.currentTick = -1;
    o.showLegend = true;
    o.tickWidth = std::max(4, std::min(o.tickWidth, 24));
    while (o.tickWidth > 4 && GanttRenderer::contentSize(history, history.size(), o).width() > MAX_PNG_WIDTH) {
        --o.tickWidth;
    }
    return o;
}
}  // namespace

bool GanttRenderer::exportSvg(const QString& path, const std::vector<SystemSnapshot>& history,
                              const GanttOptions& options, QString* error) {
    if (history.empty()) {
        if (error) *error = QStringLiteral("Não há simulação para exportar.");
        return false;
    }
    const GanttOptions o = exportOptions(options, history);
    const QSize size = contentSize(history, history.size(), o);
    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setSize(size);
    generator.setViewBox(QRect(QPoint(0, 0), size));
    generator.setTitle(QStringLiteral("Gráfico de Gantt"));
    generator.setDescription(o.title);
    QPainter painter;
    if (!painter.begin(&generator)) {
        if (error) *error = QStringLiteral("Não foi possível gravar \"%1\" (pasta inexistente ou sem permissão de escrita).").arg(path);
        return false;
    }
    paint(painter, QRectF(QPointF(0, 0), size), history, history.size(), o);
    painter.end();
    return true;
}

bool GanttRenderer::exportPng(const QString& path, const std::vector<SystemSnapshot>& history,
                              const GanttOptions& options, QString* error, QStringList* files) {
    if (history.empty()) {
        if (error) *error = QStringLiteral("Não há simulação para exportar.");
        return false;
    }
    // 1) Tick reduzido (24 -> 4 px) para a simulação inteira caber em ~30 000 px.
    const GanttOptions o = exportOptions(options, history);
    const GanttLayout full = layout(history, history.size(), o);
    const QSize fullSize = contentSize(history, history.size(), o);

    // 2) Se ainda não couber (ou a imagem passaria de ~100 milhões de pixels),
    //    divide o tempo em partes, cada uma com título, eixos e legenda.
    const int maxWidth = std::min(MAX_PNG_WIDTH, std::max(1000, static_cast<int>(100000000LL / std::max(1, fullSize.height()))));
    const Tick total = full.to - full.from;
    Tick perPart = total;
    if (fullSize.width() > maxWidth) {
        const double overhead = fullSize.width() - static_cast<double>(total) * o.tickWidth;
        perPart = std::max<Tick>(1, static_cast<Tick>((maxWidth - overhead) / o.tickWidth));
    }
    const int parts = static_cast<int>((total + perPart - 1) / perPart);
    QString base = path;
    if (base.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) base.chop(4);

    for (int part = 0; part < parts; ++part) {
        GanttOptions po = o;
        po.fromTick = full.from + part * perPart;
        po.toTick = std::min(full.to, po.fromTick + perPart);
        const QSize size = contentSize(history, history.size(), po);
        QImage image(size, QImage::Format_RGB32);
        if (image.isNull()) {
            if (error) *error = QStringLiteral("Memória insuficiente para criar uma imagem de %1 x %2 pixels.").arg(size.width()).arg(size.height());
            return false;
        }
        image.fill(Qt::white);
        {
            QPainter painter(&image);
            paint(painter, QRectF(QPointF(0, 0), size), history, history.size(), po);
        }
        const QString file = parts == 1 ? base + QStringLiteral(".png")
                                        : base + QStringLiteral("_parte_%1.png").arg(part + 1);
        if (!image.save(file, "PNG")) {
            if (error) *error = QStringLiteral("Não foi possível gravar \"%1\" (pasta inexistente ou sem permissão de escrita).").arg(file);
            return false;
        }
        if (files) files->append(file);
    }
    return true;
}

QString GanttRenderer::defaultBaseName(const QString& configName) {
    QString name = configName.isEmpty() ? QStringLiteral("simulacao") : configName;
    return QStringLiteral("gantt_%1_%2").arg(name, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
}

// ---------------------------------------------------------------------------
// Widget
// ---------------------------------------------------------------------------

// Área de desenho: do tamanho do conteúdo inteiro, dentro de uma QScrollArea;
// pinta só a faixa exposta (event->rect()).
class GanttCanvas : public QWidget {
public:
    explicit GanttCanvas(GanttWidget* owner) : owner_(owner) { setAttribute(Qt::WA_OpaquePaintEvent); }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter p(this);
        if (owner_->history_ == nullptr) {
            p.fillRect(event->rect(), Qt::white);
            return;
        }
        GanttRenderer::paint(p, event->rect(), *owner_->history_, owner_->upto_, owner_->options_);
    }
    bool event(QEvent* event) override {
        if (event->type() == QEvent::ToolTip && owner_->history_ != nullptr) {
            const auto* help = static_cast<QHelpEvent*>(event);
            const QString text = GanttRenderer::describe(*owner_->history_, owner_->upto_, owner_->options_, help->pos());
            if (text.isEmpty()) QToolTip::hideText();
            else QToolTip::showText(help->globalPos(), text, this);
            return true;
        }
        return QWidget::event(event);
    }
    void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {  // Ctrl + roda = zoom
            if (event->angleDelta().y() > 0) owner_->zoomIn();
            else owner_->zoomOut();
            event->accept();
        } else {
            event->ignore();  // rolagem normal fica com a QScrollArea
        }
    }

private:
    GanttWidget* owner_;
};

namespace {
// Legenda fixa embaixo do gráfico (sempre visível, RG3); a altura acompanha a largura.
class LegendBar : public QWidget {
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        GanttRenderer::paintLegend(p, QRectF(MARGIN, 4, width() - 2 * MARGIN, height() - 8));
    }
    void resizeEvent(QResizeEvent* event) override {
        const int h = static_cast<int>(std::ceil(GanttRenderer::legendHeight(event->size().width() - 2 * MARGIN))) + 10;
        if (h != height()) setFixedHeight(h);
        QWidget::resizeEvent(event);
    }
};
}  // namespace

GanttWidget::GanttWidget(QWidget* parent) : QWidget(parent) {
    options_.showLegend = false;  // a legenda fica fixa, fora da rolagem
    canvas_ = new GanttCanvas(this);
    scroll_ = new QScrollArea(this);
    scroll_->setWidget(canvas_);
    scroll_->setWidgetResizable(false);
    scroll_->setBackgroundRole(QPalette::Base);
    legend_ = new LegendBar;
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll_, 1);
    layout->addWidget(legend_);
    updateCanvas(false);
}

void GanttWidget::setHistory(const std::vector<SystemSnapshot>* history, std::size_t upto, Tick currentTick) {
    history_ = history;
    upto_ = upto;
    options_.currentTick = currentTick;
    updateCanvas(true);
}

void GanttWidget::setTitle(const QString& title) {
    options_.title = title;
    updateCanvas(false);
}

void GanttWidget::setTickWidth(int pixels) {
    options_.tickWidth = std::max(4, std::min(64, pixels));
    updateCanvas(true);
}

void GanttWidget::zoomIn() { setTickWidth(options_.tickWidth < 16 ? options_.tickWidth + 2 : options_.tickWidth + 8); }
void GanttWidget::zoomOut() { setTickWidth(options_.tickWidth <= 16 ? options_.tickWidth - 2 : options_.tickWidth - 8); }

void GanttWidget::updateCanvas(bool scrollToCurrent) {
    if (history_ == nullptr || history_->empty() || upto_ == 0) {
        canvas_->resize(400, 100);
        canvas_->update();
        return;
    }
    // Um QWidget não passa de 16 777 215 px: em simulações enormes o zoom é
    // reduzido o necessário (o PNG/SVG não têm esse limite: são exportados à parte).
    QSize size = GanttRenderer::contentSize(*history_, upto_, options_);
    while (size.width() > QWIDGETSIZE_MAX && options_.tickWidth > 1) {
        options_.tickWidth = std::max(1, options_.tickWidth / 2);
        size = GanttRenderer::contentSize(*history_, upto_, options_);
    }
    canvas_->resize(size);
    canvas_->update();
    if (scrollToCurrent && options_.currentTick >= 0) {
        const GanttLayout L = GanttRenderer::layout(*history_, upto_, options_);
        const int x = static_cast<int>(L.xOf(options_.currentTick));
        const int y = scroll_->verticalScrollBar()->value() + scroll_->viewport()->height() / 2;
        scroll_->ensureVisible(x, y, 120, 0);
    }
}
