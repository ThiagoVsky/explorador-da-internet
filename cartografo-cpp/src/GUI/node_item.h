#ifndef NODEITEM_H
#define NODEITEM_H

#include <QGraphicsEllipseItem> // Ou QGraphicsRectItem, QGraphicsPathItem, etc.
#include <QPen>
#include <QBrush>
#include <QString>
#include <Core/data_types.h> // Para NodeType e Node struct

class NodeItem : public QGraphicsEllipseItem // Começar com elipse, pode ser mais complexo
{
public:
    // Usar QGraphicsEllipseItem como tipo base para simplificar
    // Para formas mais complexas, QGraphicsObject ou QGraphicsPathItem seriam melhores
    // explicit NodeItem(const Node& nodeData, QGraphicsItem *parent = nullptr);
    explicit NodeItem(const Node& nodeData, qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent = nullptr);


    // Tipo do nó para facilitar acesso e estilização
    enum { Type = UserType + 1 }; // Tipo customizado para QGraphicsItem
    int type() const override { return Type; }

    const Node& getNodeData() const;
    void setNodeData(const Node& nodeData); // Para atualizações

    // Para tooltip ou informações rápidas
    QString getHoverText() const;

    // Atualiza a aparência baseada nos dados do nó (ex: tipo, seleção)
    void updateAppearance();

protected:
    // Eventos para interatividade (hover, clique - se não tratados pela view)
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    // QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    // void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    // void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    // Para desenhar texto dentro ou perto do nó, se necessário (QGraphicsSimpleTextItem pode ser filho)
    // void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;


private:
    Node m_nodeData;
    QColor m_fillColor;
    QColor m_borderColor;
    int m_borderWidth;
    bool m_isSelected; // Controlado pela view/cena
    bool m_isHovered;

    void setDefaultStyle();
    void applyStyleForType(NodeType type);
};

#endif // NODEITEM_H
