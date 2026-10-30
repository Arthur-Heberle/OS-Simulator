// =============================================================================
// GanttRenderer.h — Gráfico de Gantt: um ÚNICO desenhista para a tela, o PNG
// e o SVG, e o widget que o mostra na janela.
//
// O quê: GanttRenderer desenha, com QPainter, o histórico da simulação
// (std::vector<SystemSnapshot>): uma linha por tarefa (menor ID colado no eixo
// X), uma linha por CPU, eixo do tempo, ícones de cada ocorrência no instante
// exato e a legenda. GanttWidget põe esse desenho numa área com rolagem, zoom,
// tooltip e destaque do instante atual.
//
// Por quê:
//   - A2.4 proíbe que a imagem exportada seja um print da janela: aqui o MESMO
//     paint() desenha num widget, numa QImage (PNG) e num QSvgGenerator (SVG).
//   - O desenho segue as figuras da seção 6.4 do livro do Maziero (A2.5):
//     barras contínuas com contorno preto, cor sólida = executando, branco =
//     pronta, nada fora do sistema, linhas pontilhadas a cada instante, eixos
//     com seta e "t". Os ícones de chegada/término/perda de prazo etc. são
//     exigência do enunciado (A2.2, RG3) e ficam por cima desse estilo.
//   - Coluna k = history[k] (D3): o snapshot do instante k diz quem executa em
//     [k, k+1); as ocorrências dele são desenhadas em x = k.
//
// Requisitos: RG3, A1.2 (CPU desligada visível), A2.1–A2.5, A4.3 (ícone de
// sorteio). Decisões: D2, D3, D4, D11, D14, D17; design.md §10.
// =============================================================================
#pragma once

#include <QPainter>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstddef>
#include <vector>

#include "Model.h"

class QScrollArea;

// Opções de desenho (zoom, faixa de tempo, título). Valores iniciais = padrão (A3.2).
struct GanttOptions {
    int tickWidth = 24;       // largura de 1 tick em pixels (zoom: 4 a 64)
    int rowHeight = 28;       // altura de cada linha de tarefa/CPU
    bool showCpuPanel = true; // painel com uma linha por CPU (A1.2: CPU desligada visível)
    bool showLegend = true;   // legenda dentro do desenho (imagens); o widget a mostra fixa
    Tick fromTick = 0;        // faixa de tempo desenhada: [fromTick, toTick)
    Tick toTick = -1;         // -1 = até o fim do histórico
    Tick currentTick = -1;    // instante destacado no passo a passo (-1 = sem destaque)
    QString title;            // nome da configuração (vai no título)
};

// Geometria do gráfico, calculada a partir do histórico e das opções. O mesmo
// cálculo serve para desenhar e para o tooltip, então os dois nunca discordam.
struct GanttLayout {
    std::vector<int> taskRows;  // índices em tasks, de cima para baixo (menor ID embaixo, A2.5)
    int cpuCount = 0;           // maior nº de CPUs no trecho (pode mudar por edição)
    Tick from = 0, to = 1;      // instantes desenhados: [from, to]
    int tickWidth = 24, rowHeight = 28;
    double left = 0;            // x do instante 'from' (depois da coluna de rótulos)
    double tasksTop = 0;        // y do topo da 1ª linha de tarefa
    double axisY = 0;           // y do eixo do tempo do painel de tarefas
    double cpuTop = 0;          // y do topo da 1ª linha de CPU
    double cpuAxisY = 0;        // y do eixo do painel de CPUs
    double legendTop = 0;       // y do início da legenda
    double width = 0, height = 0;
    Tick labelStep = 1;         // rótulos/linhas pontilhadas a cada 'labelStep' instantes

    double xOf(Tick t) const { return left + static_cast<double>(t - from) * tickWidth; }
    Tick tickAt(double x) const;
    int taskRowAt(double y) const;  // índice em taskRows, ou -1
    int cpuRowAt(double y) const;   // índice da CPU, ou -1
};

class GanttRenderer {
public:
    // Geometria e tamanho total do desenho (para o widget e as imagens).
    static GanttLayout layout(const std::vector<SystemSnapshot>& history, std::size_t upto,
                              const GanttOptions& options);
    static QSize contentSize(const std::vector<SystemSnapshot>& history, std::size_t upto,
                             const GanttOptions& options);

    // Desenha history[0 .. upto-1] em 'painter' (coordenadas do conteúdo).
    // Só a faixa 'visible' é desenhada em detalhe: rápido mesmo com 100 000 ticks.
    static void paint(QPainter& painter, const QRectF& visible,
                      const std::vector<SystemSnapshot>& history, std::size_t upto,
                      const GanttOptions& options);

    // Legenda (RG3): cada estado e cada ícone com o seu significado.
    static double legendHeight(double width);
    static void paintLegend(QPainter& painter, const QRectF& area);

    // Texto do tooltip para o ponto 'pos' do desenho (tarefa/CPU, instante, estado, ocorrências).
    static QString describe(const std::vector<SystemSnapshot>& history, std::size_t upto,
                            const GanttOptions& options, const QPointF& pos);

    // Exportação da simulação INTEIRA, sem teto de tempo e sem capturar a tela (A2.4).
    // SVG: sempre um arquivo só. PNG: reduz o tick de 24 até 4 px para caber em
    // ~30 000 px de largura; se ainda não couber, divide em <nome>_parte_N.png.
    // 'files' recebe os caminhos gravados; em erro, 'error' explica em português.
    static bool exportSvg(const QString& path, const std::vector<SystemSnapshot>& history,
                          const GanttOptions& options, QString* error);
    static bool exportPng(const QString& path, const std::vector<SystemSnapshot>& history,
                          const GanttOptions& options, QString* error, QStringList* files = nullptr);

    // Nome padrão dos arquivos: gantt_<configuração>_<aaaammdd-hhmmss> (sem extensão).
    static QString defaultBaseName(const QString& configName);

    // Nome de cada ocorrência em português (tooltip, registro de eventos da janela).
    static QString occurrenceText(OccurrenceType type);
};

// Widget do Gantt na janela: área com rolagem (H/V), zoom (Ctrl+roda ou
// zoomIn/zoomOut), tooltip, destaque e rolagem automática até o instante atual
// (A2.3), e a legenda sempre visível embaixo.
class GanttWidget : public QWidget {
public:
    explicit GanttWidget(QWidget* parent = nullptr);

    // Mostra history[0 .. upto-1]. 'history' é do Simulator (não é copiado: com
    // 100 000 snapshots a cópia a cada passo custaria caro); deve continuar
    // válido enquanto o widget o mostra. currentTick = instante a destacar.
    void setHistory(const std::vector<SystemSnapshot>* history, std::size_t upto, Tick currentTick);
    void setTitle(const QString& title);
    void setTickWidth(int pixels);  // zoom: 4 a 64 px por tick
    int tickWidth() const { return options_.tickWidth; }
    void zoomIn();
    void zoomOut();
    const GanttOptions& options() const { return options_; }

private:
    void updateCanvas(bool scrollToCurrent);

    QWidget* canvas_ = nullptr;       // o desenho (tamanho = conteúdo inteiro)
    QScrollArea* scroll_ = nullptr;
    QWidget* legend_ = nullptr;       // legenda fixa, fora da rolagem
    GanttOptions options_;
    const std::vector<SystemSnapshot>* history_ = nullptr;
    std::size_t upto_ = 0;

    friend class GanttCanvas;
};
