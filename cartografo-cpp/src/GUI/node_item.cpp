#include "node_item.h"
#include <QPainter>
#include <QToolTip>
#include <QDebug>
#include <QGraphicsSceneHoverEvent>
#include <QApplication> // Para setOverrideCursor

NodeItem::NodeItem(const Node& nodeData, qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent)
    // : QGraphicsEllipseItem(parent), m_nodeData(nodeData), m_isSelected(false), m_isHovered(false)
    : QGraphicsEllipseItem(x - width / 2, y - height / 2, width, height, parent), // Centraliza a elipse em x,y
      m_nodeData(nodeData),
      m_borderWidth(1),
      m_isSelected(false),
      m_isHovered(false)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable); // Permitir seleção pela cena/view
    setFlag(QGraphicsItem::ItemIsMovable);    // Permitir movimento (pode ser desabilitado se o layout for fixo)
    setFlag(QGraphicsItem::ItemSendsGeometryChanges); // Para itemChange, se usado

    // Define o retângulo delimitador com base nos parâmetros x, y, width, height
    // O construtor de QGraphicsEllipseItem já faz isso.
    // setRect(-width/2, -height/2, width, height); // Define o retângulo localmente em torno de (0,0)
    // setPos(x,y); // E então posiciona o item na cena

    updateAppearance();
}

const Node& NodeItem::getNodeData() const
{
    return m_nodeData;
}

void NodeItem::setNodeData(const Node& nodeData)
{
    m_nodeData = nodeData;
    updateAppearance();
    // Tooltip pode precisar ser atualizado se visível
}

QString NodeItem::getHoverText() const
{
    QStringList info;
    info << tr("ID: %1").arg(m_nodeData.id);
    if (!m_nodeData.label.isEmpty() && m_nodeData.label != m_nodeData.id) {
        info << tr("Rótulo: %1").arg(m_nodeData.label);
    }
    info << tr("Tipo: %1").arg(m_nodeData.group); // Usar m_nodeData.group para consistência com JSON

    if (m_nodeData.geoInfo.lat != 0 || m_nodeData.geoInfo.lon != 0) {
        if (!m_nodeData.geoInfo.city.isEmpty()) info << tr("Cidade: %1").arg(m_nodeData.geoInfo.city);
        if (!m_nodeData.geoInfo.country.isEmpty()) info << tr("País: %1").arg(m_nodeData.geoInfo.country);
    }
    if (!m_nodeData.fqdns.isEmpty()) {
        info << tr("FQDNs: %1").arg(m_nodeData.fqdns.join(", "));
    }
    return info.join("<br>");
}

void NodeItem::setDefaultStyle()
{
    m_fillColor = Qt::lightGray;
    m_borderColor = Qt::black;
    m_borderWidth = 1;
}

void NodeItem::applyStyleForType(NodeType type)
{
    // Cores e formas distintas para cada tipo de nó (RF03)
    switch (type) {
    case NodeType::Explorer:
        m_fillColor = QColor(Qt::blue).lighter(150); // Azul claro
        m_borderColor = Qt::blue;
        m_borderWidth = 2;
        // setRect(-10, -10, 20, 20); // Tamanho maior para explorador, por exemplo
        break;
    case NodeType::Target:
        m_fillColor = QColor(Qt::red).lighter(150); // Vermelho claro
        m_borderColor = Qt::darkRed;
        m_borderWidth = 2;
        // setRect(-8, -8, 16, 16); // Tamanho um pouco maior
        break;
    case NodeType::Hop:
        m_fillColor = Qt::yellow;
        m_borderColor = Qt::darkYellow;
        m_borderWidth = 1;
        // setRect(-6, -6, 12, 12); // Tamanho padrão
        break;
    case NodeType::Ghost:
        m_fillColor = Qt::gray;
        m_borderColor = Qt::darkGray;
        // setRect(-5, -5, 10, 10); // Tamanho menor
        // Poderia usar um QGraphicsPathItem para uma forma de fantasma
        setPen(QPen(m_borderColor, m_borderWidth, Qt::DashLine));
        break;
    case NodeType::Unknown:
    default:
        m_fillColor = Qt::magenta; // Cor de fallback para tipos desconhecidos
        m_borderColor = Qt::darkMagenta;
        m_borderWidth = 1;
        // setRect(-6, -6, 12, 12);
        break;
    }
}

void NodeItem::updateAppearance()
{
    setDefaultStyle();
    applyStyleForType(m_nodeData.type);

    if (m_isHovered) {
        m_fillColor = m_fillColor.lighter(120); // Mais claro ao passar o mouse
        m_borderWidth += 1;
    }
    if (isSelected()) { // QGraphicsItem::isSelected()
        m_borderColor = Qt::cyan; // Cor de borda especial para seleção
        m_borderWidth = 3;
    }

    setPen(QPen(m_borderColor, m_borderWidth));
    setBrush(QBrush(m_fillColor));

    // Tooltip
    setToolTip(getHoverText());

    update(); // Força o redesenho
}


void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_isHovered = true;
    // QApplication::setOverrideCursor(Qt::PointingHandCursor); // Mudar cursor
    updateAppearance();
    QGraphicsEllipseItem::hoverEnterEvent(event);
    //qDebug() << "Hover enter" << m_nodeData.id;
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_isHovered = false;
    // QApplication::restoreOverrideCursor(); // Restaurar cursor
    updateAppearance();
    QGraphicsEllipseItem::hoverLeaveEvent(event);
    //qDebug() << "Hover leave" << m_nodeData.id;
}

/*
// Se precisarmos de pintura customizada além do que QGraphicsEllipseItem oferece
// (ex: desenhar texto dentro do nó, formas mais complexas)
void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Primeiro, chame a pintura da classe base se estiver usando suas funcionalidades de desenho
    QGraphicsEllipseItem::paint(painter, option, widget);

    // Exemplo: desenhar o label do nó no centro
    // painter->setPen(Qt::black);
    // QString label = m_nodeData.label.isEmpty() ? m_nodeData.id : m_nodeData.label;
    // painter->drawText(boundingRect(), Qt::AlignCenter, label);
}
*/

/*
// Exemplo de como reagir a mudanças, como seleção:
QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        m_isSelected = value.toBool();
        qDebug() << "Node" << m_nodeData.id << "selection changed to" << m_isSelected;
        updateAppearance();
    }
    return QGraphicsEllipseItem::itemChange(change, value);
}
*/

// Eventos de mouse podem ser tratados aqui se quisermos comportamento customizado
// além da seleção/movimentação padrão da cena.
// void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
// {
//     qDebug() << "Node" << m_nodeData.id << "pressed";
//     QGraphicsEllipseItem::mousePressEvent(event); // Chama implementação base para seleção etc.
// }

// void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
// {
//     QGraphicsEllipseItem::mouseReleaseEvent(event);
// }
