#ifndef EDGEITEM_H
#define EDGEITEM_H

#include <QGraphicsLineItem> // Ou QGraphicsPathItem para curvas
#include <QPen>
#include <Core/data_types.h> // Para Edge struct

class NodeItem; // Forward declaration

class EdgeItem : public QGraphicsLineItem
{
public:
    // explicit EdgeItem(const Edge& edgeData, NodeItem *sourceNode, NodeItem *destNode, QGraphicsItem *parent = nullptr);
    // Simplificando: a EdgeItem apenas conhece os dados da aresta e desenha uma linha.
    // O MapView será responsável por atualizar as posições das linhas quando os nós se moverem.
    // Ou, EdgeItem pode ter ponteiros para NodeItems e ajustar-se. A segunda é mais robusta.
    explicit EdgeItem(const Edge& edgeData, NodeItem *sourceItem, NodeItem *destItem, QGraphicsItem *parent = nullptr);
    ~EdgeItem();

    enum { Type = UserType + 2 }; // Tipo customizado para QGraphicsItem
    int type() const override { return Type; }

    const Edge& getEdgeData() const;
    void setEdgeData(const Edge& edgeData); // Para atualizações

    NodeItem* getSourceNodeItem() const;
    NodeItem* getDestNodeItem() const;

    void updatePosition(); // Chamado quando os nós de origem/destino se movem
    void updateAppearance();

    QString getHoverText() const;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    // Sobrescrever paint se precisarmos de mais do que uma linha simples (ex: setas, texto de latência)
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;


private:
    Edge m_edgeData;
    NodeItem *m_sourceNodeItem;
    NodeItem *m_destNodeItem;

    QColor m_lineColor;
    int m_lineWidth;
    Qt::PenStyle m_lineStyle;

    bool m_isSelected; // Controlado pela view/cena
    bool m_isHovered;

    void setDefaultStyle();
};

#endif // EDGEITEM_H
