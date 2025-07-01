#ifndef DETAILSPANEL_H
#define DETAILSPANEL_H

#include <QDockWidget>
#include <QVariantMap> // Para receber dados do item selecionado
#include "Core/data_types.h" // Para Node, Edge, GeoInfo

// Forward declarations
namespace Ui {
class DetailsPanel;
}
namespace QtCharts { // QtCharts namespace
class QChartView;
class QChart;
class QLineSeries;
}

class DetailsPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit DetailsPanel(QWidget *parent = nullptr);
    ~DetailsPanel();

public slots:
    // Slot para receber dados do item selecionado (nó ou aresta)
    // O QVariant pode ser um QJsonObject como definido no MapView::onSceneSelectionChanged
    void displayItemData(const QVariant& itemData);
    void clearPanel(); // Limpa o painel quando nada está selecionado

private:
    void displayNodeDetails(const Node& nodeData);
    void displayEdgeDetails(const Edge& edgeData);
    void setupLatencyChart();
    void updateLatencyChart(const QList<double>& latencies);

    Ui::DetailsPanel *ui;

    // Para o gráfico de latência (Qt Charts)
    QtCharts::QChartView *m_latencyChartView;
    QtCharts::QChart *m_latencyChart;
    QtCharts::QLineSeries *m_latencySeries;
};

#endif // DETAILSPANEL_H
