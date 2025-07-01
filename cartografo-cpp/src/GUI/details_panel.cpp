#include "details_panel.h"
#include "ui_details_panel.h" // Gerado pelo UIC

#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QTextBrowser> // Para exibir múltiplos FQDNs ou GeoInfo formatado
#include <QVBoxLayout>
#include <QScrollArea>

// Includes para Qt Charts
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegend>

// Usar o namespace para QtCharts
QT_CHARTS_USE_NAMESPACE

DetailsPanel::DetailsPanel(QWidget *parent) :
    QDockWidget(tr("Painel de Detalhes"), parent), // Título do DockWidget
    ui(new Ui::DetailsPanel),
    m_latencyChartView(nullptr),
    m_latencyChart(nullptr),
    m_latencySeries(nullptr)
{
    ui->setupUi(this); // Configura a UI a partir do .ui (que pode ser simples)
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // O widget principal do DetailsPanel será um QScrollArea para acomodar conteúdo variável
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    QWidget *mainDisplayWidget = new QWidget(); // Widget que vai dentro do scrollArea
    QVBoxLayout *mainLayout = new QVBoxLayout(mainDisplayWidget);
    mainLayout->setAlignment(Qt::AlignTop); // Alinha conteúdo ao topo

    // Adiciona um QLabel para título/tipo de item (será preenchido dinamicamente)
    QLabel *titleLabel = new QLabel(tr("Nenhum item selecionado."), mainDisplayWidget);
    titleLabel->setObjectName("titleLabel"); // Para acesso posterior
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14pt;");
    mainLayout->addWidget(titleLabel);

    // Adiciona um QTextBrowser para detalhes formatados
    QTextBrowser *detailsBrowser = new QTextBrowser(mainDisplayWidget);
    detailsBrowser->setObjectName("detailsBrowser");
    detailsBrowser->setOpenExternalLinks(true); // Para links em GeoInfo, se houver
    detailsBrowser->setMinimumHeight(150); // Altura mínima para os detalhes
    mainLayout->addWidget(detailsBrowser);

    // Configura o gráfico de latência
    setupLatencyChart();
    if (m_latencyChartView) {
        m_latencyChartView->setMinimumHeight(200); // Altura mínima para o gráfico
        mainLayout->addWidget(m_latencyChartView);
        m_latencyChartView->hide(); // Esconde inicialmente, mostra apenas para arestas
    }

    scrollArea->setWidget(mainDisplayWidget);
    setWidget(scrollArea); // Define o scrollArea como o widget principal do QDockWidget

    clearPanel(); // Estado inicial
}

DetailsPanel::~DetailsPanel()
{
    delete ui;
    // m_latencyChartView, m_latencyChart, m_latencySeries são gerenciados pelo Qt Charts
    // ou são filhos e serão deletados.
}

void DetailsPanel::setupLatencyChart()
{
    m_latencyChart = new QChart();
    m_latencyChart->setTitle(tr("Gráfico de Latência da Aresta"));
    m_latencyChart->legend()->hide(); // Esconder legenda por padrão

    m_latencySeries = new QLineSeries();
    m_latencySeries->setName(tr("Latências"));
    m_latencyChart->addSeries(m_latencySeries);

    QValueAxis *axisX = new QValueAxis;
    axisX->setTitleText(tr("Medição nº"));
    axisX->setLabelFormat("%i"); // Inteiros para o número da medição
    axisX->setTickCount(5); // Sugestão, pode ajustar
    m_latencyChart->addAxis(axisX, Qt::AlignBottom);
    m_latencySeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis;
    axisY->setTitleText(tr("Latência (ms)"));
    axisY->setLabelFormat("%.1f"); // Um decimal para latência
    m_latencyChart->addAxis(axisY, Qt::AlignLeft);
    m_latencySeries->attachAxis(axisY);

    m_latencyChartView = new QChartView(m_latencyChart, this); // 'this' como pai
    m_latencyChartView->setRenderHint(QPainter::Antialiasing);
}

void DetailsPanel::displayItemData(const QVariant& itemData)
{
    if (!itemData.isValid() || itemData.isNull() || itemData.typeId() != QMetaType::QJsonObject) {
        clearPanel();
        return;
    }

    QJsonObject data = itemData.toJsonObject();
    QString itemType = data.value("item_type").toString();

    QLabel* titleLabel = this->widget()->findChild<QLabel*>("titleLabel"); // Encontra o label do título
    QTextBrowser* detailsBrowser = this->widget()->findChild<QTextBrowser*>("detailsBrowser");

    if (!titleLabel || !detailsBrowser) {
        qWarning() << "Labels do DetailsPanel não encontrados!";
        return;
    }

    detailsBrowser->clear(); // Limpa conteúdo anterior
    m_latencyChartView->hide(); // Esconde gráfico por padrão

    if (itemType == "node") {
        Node node = Node::fromJson(data); // Usa o parser de Node
        if (titleLabel) titleLabel->setText(tr("Detalhes do Nó: %1").arg(node.id));
        displayNodeDetails(node);
    } else if (itemType == "edge") {
        Edge edge = Edge::fromJson(data); // Usa o parser de Edge
        if (titleLabel) titleLabel->setText(tr("Detalhes da Aresta: %1").arg(edge.id));
        displayEdgeDetails(edge);
        if (!edge.latencies.isEmpty() && edge.latencies.first() >= 0) { // Não mostra gráfico para fantasma/timeout
            updateLatencyChart(edge.latencies);
            m_latencyChartView->show();
        }
    } else {
        clearPanel();
    }
}

void DetailsPanel::displayNodeDetails(const Node& nodeData)
{
    QTextBrowser* detailsBrowser = this->widget()->findChild<QTextBrowser*>("detailsBrowser");
    if (!detailsBrowser) return;

    QString html;
    html += QString("<b>ID:</b> %1<br>").arg(nodeData.id);
    html += QString("<b>Rótulo:</b> %1<br>").arg(nodeData.label.isEmpty() ? nodeData.id : nodeData.label);
    html += QString("<b>Tipo:</b> %1<br>").arg(nodeData.group);

    if (!nodeData.fqdns.isEmpty()) {
        html += QString("<b>FQDNs:</b><ul>");
        for (const QString& fqdn : nodeData.fqdns) {
            html += QString("<li>%1</li>").arg(fqdn);
        }
        html += QString("</ul>");
    }

    const GeoInfo& geo = nodeData.geoInfo;
    if (geo.lat != 0 || geo.lon != 0 || !geo.country.isEmpty() || !geo.city.isEmpty()) {
        html += "<hr><b>Informações Geográficas (GeoIP):</b><br>";
        if (!geo.query.isEmpty()) html += QString("Consulta: %1<br>").arg(geo.query);
        if (!geo.country.isEmpty()) html += QString("País: %1 (%2)<br>").arg(geo.country, geo.countryCode);
        if (!geo.regionName.isEmpty()) html += QString("Região: %1<br>").arg(geo.regionName);
        if (!geo.city.isEmpty()) html += QString("Cidade: %1<br>").arg(geo.city);
        if (geo.lat != 0 || geo.lon != 0) {
            html += QString("Lat/Lon: %1, %2").arg(QString::number(geo.lat, 'f', 4))
                                             .arg(QString::number(geo.lon, 'f', 4));
            // Adicionar link para OpenStreetMap ou Google Maps
            html += QString(" (<a href=\"https://www.openstreetmap.org/?mlat=%1&mlon=%2&zoom=12\">Mapa</a>)<br>")
                        .arg(QString::number(geo.lat, 'f', 4))
                        .arg(QString::number(geo.lon, 'f', 4));
        }
        if (!geo.isp.isEmpty()) html += QString("ISP: %1<br>").arg(geo.isp);
        if (!geo.org.isEmpty()) html += QString("Organização: %1<br>").arg(geo.org);
        if (!geo.asname.isEmpty()) html += QString("AS: %1 (%2)<br>").arg(geo.as, geo.asname);
    }
    // Adicionar mais campos de GeoInfo conforme necessário

    detailsBrowser->setHtml(html);
}

void DetailsPanel::displayEdgeDetails(const Edge& edgeData)
{
    QTextBrowser* detailsBrowser = this->widget()->findChild<QTextBrowser*>("detailsBrowser");
    if (!detailsBrowser) return;

    QString html;
    html += QString("<b>De:</b> %1<br>").arg(edgeData.from);
    html += QString("<b>Para:</b> %1<br>").arg(edgeData.to);

    if (edgeData.averageLatency >= 0) {
        html += QString("<b>Latência Média:</b> %1 ms<br>").arg(QString::number(edgeData.averageLatency, 'f', 2));
        if (!edgeData.latencies.isEmpty()) {
            html += QString("<b>Todas as Latências (ms):</b> ");
            QStringList latsStr;
            for(double l : edgeData.latencies) latsStr << QString::number(l, 'f', 2);
            html += latsStr.join(", ");
            html += "<br>";
        }
    } else {
         html += QString("<b>Latência:</b> N/A (Timeout/Fantasma)<br>");
    }

    detailsBrowser->setHtml(html);
}


void DetailsPanel::updateLatencyChart(const QList<double>& latencies)
{
    if (!m_latencySeries || !m_latencyChart) return;

    m_latencySeries->clear();
    if (latencies.isEmpty()) {
        m_latencyChart->setTitle(tr("Gráfico de Latência (Sem dados)"));
        // Ajustar eixos para estado vazio ou esconder
        QValueAxis *axisX = qobject_cast<QValueAxis*>(m_latencyChart->axes(Qt::Horizontal, m_latencySeries).first());
        QValueAxis *axisY = qobject_cast<QValueAxis*>(m_latencyChart->axes(Qt::Vertical, m_latencySeries).first());
        if(axisX) axisX->setRange(0,1);
        if(axisY) axisY->setRange(0,1);
        return;
    }

    m_latencyChart->setTitle(tr("Gráfico de Latência da Aresta"));

    qreal minLat = latencies.first();
    qreal maxLat = latencies.first();

    for (int i = 0; i < latencies.size(); ++i) {
        m_latencySeries->append(i, latencies.at(i));
        if (latencies.at(i) < minLat) minLat = latencies.at(i);
        if (latencies.at(i) > maxLat) maxLat = latencies.at(i);
    }

    QValueAxis *axisX = qobject_cast<QValueAxis*>(m_latencyChart->axes(Qt::Horizontal, m_latencySeries).first());
    if (axisX) {
        axisX->setRange(0, qMax(1, latencies.size() -1)); // Evita range 0-0 se só 1 ponto
        axisX->setTickCount(qMin(latencies.size() +1, 10)); // Ajusta número de ticks
    }

    QValueAxis *axisY = qobject_cast<QValueAxis*>(m_latencyChart->axes(Qt::Vertical, m_latencySeries).first());
    if (axisY) {
        qreal padding = (maxLat - minLat) * 0.1; // 10% de padding
        if (padding < 1.0) padding = 1.0; // Mínimo de 1ms de padding
        if (minLat == maxLat) { // Se todos os valores forem iguais
            axisY->setRange(minLat - padding, maxLat + padding);
        } else {
            axisY->setRange(qMax(0.0, minLat - padding), maxLat + padding);
        }
        axisY->setTickCount(5); // Ajusta número de ticks
    }
}


void DetailsPanel::clearPanel()
{
    QLabel* titleLabel = this->widget()->findChild<QLabel*>("titleLabel");
    QTextBrowser* detailsBrowser = this->widget()->findChild<QTextBrowser*>("detailsBrowser");

    if (titleLabel) titleLabel->setText(tr("Nenhum item selecionado."));
    if (detailsBrowser) detailsBrowser->clear();

    if (m_latencySeries) m_latencySeries->clear();
    if (m_latencyChart) m_latencyChart->setTitle(tr("Gráfico de Latência"));
    if (m_latencyChartView) m_latencyChartView->hide();
}
