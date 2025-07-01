#include "edge_item.h"
#include "node_item.h" // Para acessar posições dos nós
#include <QPen>
#include <QPainter>
#include <QToolTip>
#include <QDebug>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem> // Para paint
#include <cmath> // Para atan2, sin, cos (desenho de seta)

EdgeItem::EdgeItem(const Edge& edgeData, NodeItem *sourceItem, NodeItem *destItem, QGraphicsItem *parent)
    : QGraphicsLineItem(parent),
      m_edgeData(edgeData),
      m_sourceNodeItem(sourceItem),
      m_destNodeItem(destItem),
      m_isSelected(false),
      m_isHovered(false)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable);
    // Arestas não são tipicamente móveis por si mesmas, elas seguem os nós
    setZValue(-1); // Desenhar arestas abaixo dos nós

    updateAppearance();
    updatePosition(); // Define a posição inicial da linha
}

EdgeItem::~EdgeItem()
{
    // Os NodeItems não são propriedade do EdgeItem, não os deleta.
}

const Edge& EdgeItem::getEdgeData() const
{
    return m_edgeData;
}

void EdgeItem::setEdgeData(const Edge& edgeData)
{
    m_edgeData = edgeData;
    updateAppearance();
}

NodeItem* EdgeItem::getSourceNodeItem() const
{
    return m_sourceNodeItem;
}

NodeItem* EdgeItem::getDestNodeItem() const
{
    return m_destNodeItem;
}

void EdgeItem::updatePosition()
{
    if (!m_sourceNodeItem || !m_destNodeItem) return;

    // QLineF line(mapFromItem(m_sourceNodeItem, 0, 0), mapFromItem(m_destNodeItem, 0, 0));
    // Usar as posições centrais dos NodeItems. NodeItem é uma elipse, então seu centro é (rect.width/2, rect.height/2) em suas coordenadas locais.
    // Precisamos mapear o centro do NodeItem para as coordenadas da cena.
    QPointF sourceCenter = m_sourceNodeItem->sceneBoundingRect().center();
    QPointF destCenter = m_destNodeItem->sceneBoundingRect().center();

    setLine(QLineF(sourceCenter, destCenter));
    update(); // Força redesenho
}


void EdgeItem::setDefaultStyle()
{
    m_lineColor = Qt::black;
    m_lineWidth = 1;
    m_lineStyle = Qt::SolidLine;

    // Estilo pode depender da latência, tipo de nós conectados, etc.
    if (m_edgeData.averageLatency < 0) { // Indicador de timeout/fantasma
        m_lineColor = Qt::gray;
        m_lineStyle = Qt::DashLine;
        m_lineWidth = 1;
    } else if (m_edgeData.averageLatency > 100) { // Exemplo: latência alta
        m_lineColor = Qt::red;
        m_lineWidth = 2;
    } else if (m_edgeData.averageLatency > 50) { // Exemplo: latência média
        m_lineColor = Qt::yellow;
        m_lineWidth = 1;
    } else { // Latência baixa
        m_lineColor = Qt::green;
        m_lineWidth = 1;
    }
}

void EdgeItem::updateAppearance()
{
    setDefaultStyle();

    if (m_isHovered) {
        m_lineWidth += 2;
        m_lineColor = m_lineColor.lighter(130);
    }
    if (isSelected()) { // QGraphicsItem::isSelected()
        m_lineColor = Qt::cyan; // Cor especial para seleção
        m_lineWidth = qMax(m_lineWidth, 2) + 1;
        m_lineStyle = Qt::SolidLine;
    }

    QPen customPen;
    customPen.setColor(m_lineColor);
    customPen.setWidth(m_lineWidth);
    customPen.setStyle(m_lineStyle);
    setPen(customPen);

    setToolTip(getHoverText());
    update(); // Força o redesenho
}


QString EdgeItem::getHoverText() const
{
    QStringList info;
    info << tr("Aresta: %1 -> %2").arg(m_edgeData.from).arg(m_edgeData.to);
    if (m_edgeData.averageLatency >= 0) {
        info << tr("Latência Média: %1 ms").arg(QString::number(m_edgeData.averageLatency, 'f', 2));
        info << tr("Latências: [%1]").arg([this]() {
            QStringList lats;
            for (double l : m_edgeData.latencies) lats << QString::number(l, 'f', 2);
            if (lats.size() > 5) { // Limita a exibição
                lats = lats.mid(0, 5);
                lats << "...";
            }
            return lats.join(", ");
        }());
    } else {
        info << tr("Latência: N/A (Timeout/Fantasma)");
    }
    return info.join("<br>");
}


void EdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_isHovered = true;
    updateAppearance();
    QGraphicsLineItem::hoverEnterEvent(event);
}

void EdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_isHovered = false;
    updateAppearance();
    QGraphicsLineItem::hoverLeaveEvent(event);
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Chama a pintura da classe base para desenhar a linha principal
    QGraphicsLineItem::paint(painter, option, widget);

    // Desenho de seta (opcional)
    // if (!m_sourceNodeItem || !m_destNodeItem) return;
    // QLineF currentLine = line();
    // if (currentLine.length() == 0) return;

    // double angle = std::atan2(-currentLine.dy(), currentLine.dx());
    // qreal arrowSize = 10;

    // QPointF arrowP1 = currentLine.p2() - QPointF(sin(angle + M_PI / 3) * arrowSize,
    //                                             cos(angle + M_PI / 3) * arrowSize);
    // QPointF arrowP2 = currentLine.p2() - QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize,
    //                                             cos(angle + M_PI - M_PI / 3) * arrowSize);
    // QPolygonF arrowHead;
    // arrowHead << currentLine.p2() << arrowP1 << arrowP2;

    // painter->save();
    // painter->setBrush(pen().color()); // Usa a cor da linha para a seta
    // painter->setPen(Qt::NoPen);
    // painter->drawPolygon(arrowHead);
    // painter->restore();


    // Desenhar texto de latência no meio da aresta (opcional)
    // if (m_edgeData.averageLatency >= 0 && (m_isHovered || isSelected())) {
    //     painter->save();
    //     painter->setPen(Qt::black); // Cor do texto
    //     QPointF center = currentLine.pointAt(0.5);
    //     QString latText = QString::number(m_edgeData.averageLatency, 'f', 1) + " ms";
    //     // Adicionar um pequeno background para o texto para legibilidade
    //     QFontMetrics fm(painter->font());
    //     QRectF textRect = fm.boundingRect(latText);
    //     textRect.moveCenter(center);
    //     painter->fillRect(textRect.adjusted(-2, -1, 2, 1), QColor(255, 255, 255, 180)); // Background semi-transparente
    //     painter->drawText(textRect, Qt::AlignCenter, latText);
    //     painter->restore();
    // }
}
