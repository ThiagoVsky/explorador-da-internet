#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QMap> // Para mapear IDs de nós para NodeItem*
#include "Core/data_types.h" // Para GraphData

class NodeItem; // Forward declaration
class EdgeItem; // Forward declaration
class GraphManager; // Forward declaration

class MapView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MapView(QWidget *parent = nullptr);
    ~MapView();

    void setGraphManager(GraphManager* manager); // Para obter dados do grafo
    void updateGraph(const GraphData& graphData); // Para (re)desenhar o grafo
    void clearMap();

public slots:
    void zoomIn();
    void zoomOut();
    void fitToScreen();
    void начальная компоновка(); // "initial layout" ou "reorganize layout"

signals:
    // Sinal emitido quando um nó ou aresta é selecionado (clicado)
    void itemSelected(const QVariant& itemData); // QVariant pode conter Node ou Edge

protected:
    // Eventos de mouse para Pan e Zoom
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private slots:
    void onSceneSelectionChanged(); // Conectado ao QGraphicsScene::selectionChanged

private:
    void populateScene(const GraphData& graphData);
    void applyBasicLayout(const GraphData& graphData); // Um layout muito simples

    QGraphicsScene *m_scene;
    GraphManager *m_graphManager; // Referência, não possui

    // Mapeamento para acesso rápido aos itens gráficos
    QMap<QString, NodeItem*> m_nodeItems; // map NodeID to NodeItem*
    QMap<QString, EdgeItem*> m_edgeItems; // map EdgeID to EdgeItem*

    // Para Pan
    bool m_panning;
    QPoint m_panStartPos;

    // Para Zoom
    qreal m_currentScale;
    qreal m_minScale;
    qreal m_maxScale;

    // Para garantir que os itens sejam selecionáveis
    void setupSceneInteraction();
};

#endif // MAPVIEW_H
