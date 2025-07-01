#include "map_view.h"
#include "node_item.h"
#include "edge_item.h"
#include "Core/graph_manager.h" // Apenas para referência de tipo, não usado diretamente aqui ainda

#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QGraphicsItem>
#include <cmath> // Para M_PI, sin, cos no layout circular
#include <QRandomGenerator> // Para layout aleatório
#include <QScrollBar> // Para obter o centro da view

MapView::MapView(QWidget *parent)
    : QGraphicsView(parent),
      m_scene(new QGraphicsScene(this)),
      m_graphManager(nullptr),
      m_panning(false),
      m_currentScale(1.0),
      m_minScale(0.1),
      m_maxScale(10.0)
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag); // Pan customizado, seleção de itens padrão
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    // Conectar o sinal selectionChanged da cena ao slot local
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &MapView::onSceneSelectionChanged);

    // Estilo da cena
    m_scene->setBackgroundBrush(Qt::white); // Fundo branco
}

MapView::~MapView()
{
    // m_scene é filho de MapView, será deletado automaticamente.
    // NodeItems e EdgeItems são filhos da cena, também deletados.
    clearMap(); // Garante que os mapas sejam limpos
}

void MapView::setGraphManager(GraphManager* manager)
{
    m_graphManager = manager;
    // Se um grafo já estiver carregado no manager, exibi-lo
    // if (m_graphManager && (!m_graphManager->getCurrentGraphData().nodes.isEmpty() || !m_graphManager->getCurrentGraphData().edges.isEmpty())) {
    //     updateGraph(m_graphManager->getCurrentGraphData());
    // }
}

void MapView::clearMap()
{
    m_scene->clear(); // Remove todos os itens da cena
    m_nodeItems.clear();
    m_edgeItems.clear();
}

void MapView::updateGraph(const GraphData& graphData)
{
    clearMap();
    if (graphData.nodes.isEmpty()) {
        return;
    }
    populateScene(graphData);
    applyBasicLayout(graphData); // Aplica um layout inicial
    // fitToScreen(); // Ajusta à tela após popular
}


void MapView::populateScene(const GraphData& graphData)
{
    // 1. Adicionar Nós
    for (const Node& nodeData : graphData.nodes.values()) {
        // Tamanho padrão para os nós
        qreal nodeWidth = 15;
        qreal nodeHeight = 15;
        if (nodeData.type == NodeType::Explorer || nodeData.type == NodeType::Target) {
            nodeWidth = 20; nodeHeight = 20;
        }

        NodeItem *nodeItem = new NodeItem(nodeData, 0, 0, nodeWidth, nodeHeight); // Posição inicial (0,0), layout depois
        m_scene->addItem(nodeItem);
        m_nodeItems.insert(nodeData.id, nodeItem);
    }

    // 2. Adicionar Arestas
    for (const Edge& edgeData : graphData.edges.values()) {
        NodeItem *sourceNodeItem = m_nodeItems.value(edgeData.from);
        NodeItem *destNodeItem = m_nodeItems.value(edgeData.to);

        if (sourceNodeItem && destNodeItem) {
            EdgeItem *edgeItem = new EdgeItem(edgeData, sourceNodeItem, destNodeItem);
            m_scene->addItem(edgeItem);
            m_edgeItems.insert(edgeData.id, edgeItem);
        } else {
            qWarning() << "Não foi possível criar aresta:" << edgeData.id << "- nó(s) não encontrado(s). De:" << edgeData.from << "Para:" << edgeData.to;
        }
    }
    m_scene->update(); // Atualiza a cena
}

void MapView::applyBasicLayout(const GraphData& graphData)
{
    if (m_nodeItems.isEmpty()) return;

    // Layout Circular Simples
    const int numNodes = m_nodeItems.size();
    const qreal radiusBase = qMax(100.0, qreal(numNodes * 10)); // Raio aumenta com o número de nós
    const qreal sceneWidth = viewport()->width();
    const qreal sceneHeight = viewport()->height();
    const qreal viewCenterX = sceneWidth / 2.0;
    const qreal viewCenterY = sceneHeight / 2.0;

    // Tentar estimar um raio que caiba na view inicial, se possível
    qreal radius = qMin(radiusBase, qMin(sceneWidth, sceneHeight) * 0.4);


    int i = 0;
    for (NodeItem *nodeItem : m_nodeItems.values()) {
        if (numNodes == 1) {
            nodeItem->setPos(viewCenterX, viewCenterY);
        } else {
            qreal angle = 2 * M_PI * i / numNodes;
            qreal x = viewCenterX + radius * cos(angle);
            qreal y = viewCenterY + radius * sin(angle);
            nodeItem->setPos(x, y);
        }
        i++;
    }

    // Atualizar posições das arestas
    for (EdgeItem *edgeItem : m_edgeItems.values()) {
        edgeItem->updatePosition();
    }
    m_scene->setSceneRect(m_scene->itemsBoundingRect()); // Ajusta o retângulo da cena
    fitToScreen(); // Ajusta a visualização após o layout
}


void MapView::zoomIn()
{
    scale(1.2, 1.2);
    m_currentScale *= 1.2;
    m_currentScale = qBound(m_minScale, m_currentScale, m_maxScale);
}

void MapView::zoomOut()
{
    scale(1 / 1.2, 1 / 1.2);
    m_currentScale /= 1.2;
    m_currentScale = qBound(m_minScale, m_currentScale, m_maxScale);
}

void MapView::fitToScreen()
{
    if (m_scene->items().isEmpty()) return;

    // Reseta a transformação para calcular o fit corretamente
    resetTransform();
    m_currentScale = 1.0;

    QRectF itemsBound = m_scene->itemsBoundingRect();
    if (itemsBound.isEmpty() || !itemsBound.isValid()){
         m_scene->setSceneRect(-200, -150, 400, 300); // Default scene rect
         itemsBound = m_scene->sceneRect();
    }

    fitInView(itemsBound, Qt::KeepAspectRatio);

    // Atualiza m_currentScale após fitInView
    // A transformação atual contém a escala aplicada por fitInView
    m_currentScale = transform().m11(); // m11 é o fator de escala horizontal
    m_currentScale = qBound(m_minScale, m_currentScale, m_maxScale); // Garante que está dentro dos limites
}

void MapView::начальная компоновка() // "reorganize layout"
{
    if (m_graphManager) { // Se tivermos um graph manager, podemos pegar os dados atuais
        // applyBasicLayout(m_graphManager->getCurrentGraphData());
        // Ou, se MapView armazena seus próprios dados (o que não faz atualmente):
        GraphData gd; // Precisa de uma forma de obter os dados atuais
        QList<Node> nodes;
        for(NodeItem* ni : m_nodeItems.values()){
            nodes.append(ni->getNodeData());
            gd.nodes.insert(ni->getNodeData().id, ni->getNodeData());
        }
         QList<Edge> edges;
        for(EdgeItem* ei : m_edgeItems.values()){
            edges.append(ei->getEdgeData());
            gd.edges.insert(ei->getEdgeData().id, ei->getEdgeData());
        }
        // gd.nodes = QMap<QString, Node>::from připrav (m_nodeItems.cbegin(), m_nodeItems.cend(), [](NodeItem* ni){ return qMakePair(ni->getNodeData().id, ni->getNodeData()); });
        applyBasicLayout(gd);

    } else if (!m_nodeItems.isEmpty()) {
        // Se não houver graph manager, mas houver itens, tenta um layout com os itens existentes
        // Isso requer que os NodeItems tenham os dados ou que construamos um GraphData temporário
        GraphData currentSceneData;
        for(NodeItem* ni : qAsConst(m_nodeItems)) {
            currentSceneData.nodes.insert(ni->getNodeData().id, ni->getNodeData());
        }
        // Arestas não são necessárias para o layout de nós, mas para consistência:
        for(EdgeItem* ei : qAsConst(m_edgeItems)) {
            currentSceneData.edges.insert(ei->getEdgeData().id, ei->getEdgeData());
        }
        applyBasicLayout(currentSceneData);
    }
    m_scene->update();
}


void MapView::wheelEvent(QWheelEvent *event)
{
    // Zoom com a roda do mouse
    // O AnchorUnderMouse já faz com que o zoom seja centrado no mouse
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else {
        zoomOut();
    }
    event->accept();
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    // Pan com o botão do meio ou direito do mouse
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_panning = true;
        m_panStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    // Se for o botão esquerdo, permitir que a view base lide com a seleção de itens
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        // Traduzir a cena
        // A quantidade de movimento é a diferença entre a posição atual e a anterior do mouse
        // Isso precisa ser transformado para as coordenadas da cena se houver rotação/cisalhamento,
        // mas para translação e escala simples, a diferença na view é suficiente se usada com scrollbars.
        // Uma forma mais robusta é usar mapToScene e setSceneRect ou translate.

        // Usando scrollbars para pan:
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (event->pos().x() - m_panStartPos.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() - (event->pos().y() - m_panStartPos.y()));
        m_panStartPos = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor); // Restaurar cursor padrão
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}


void MapView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect); // Chama a implementação base (ex: backgroundBrush)

    // Desenhar uma grade sutil (opcional)
    // qreal left = int(rect.left()) - (int(rect.left()) % 100);
    // qreal top = int(rect.top()) - (int(rect.top()) % 100);
    // QVarLengthArray<QLineF, 100> lines;

    // painter->setPen(QPen(Qt::lightGray, 0, Qt::DotLine));
    // for (qreal x = left; x < rect.right(); x += 100)
    //     lines.append(QLineF(x, rect.top(), x, rect.bottom()));
    // for (qreal y = top; y < rect.bottom(); y += 100)
    //     lines.append(QLineF(rect.left(), y, rect.right(), y));
    // painter->drawLines(lines.data(), lines.size());
}


void MapView::onSceneSelectionChanged()
{
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (!selectedItems.isEmpty()) {
        QGraphicsItem* firstSelectedItem = selectedItems.first();
        QVariant itemData;

        if (NodeItem *node = qgraphicsitem_cast<NodeItem*>(firstSelectedItem)) {
            // Empacotar Node em QVariant
            // itemData.setValue(node->getNodeData()); // Requer Q_DECLARE_METATYPE(Node) e qRegisterMetaType
            // Por simplicidade, vamos emitir o ID e o tipo por enquanto, ou um QJsonObject.
            QJsonObject nodeJson = node->getNodeData().toJson();
            nodeJson["item_type"] = "node";
            itemData = nodeJson;
            qDebug() << "Node selecionado:" << node->getNodeData().id;

        } else if (EdgeItem *edge = qgraphicsitem_cast<EdgeItem*>(firstSelectedItem)) {
            // Empacotar Edge em QVariant
            // itemData.setValue(edge->getEdgeData()); // Requer Q_DECLARE_METATYPE(Edge) e qRegisterMetaType
            QJsonObject edgeJson = edge->getEdgeData().toJson();
            edgeJson["item_type"] = "edge";
            itemData = edgeJson;
            qDebug() << "Edge selecionada:" << edge->getEdgeData().id;
        }

        if (itemData.isValid()){
            emit itemSelected(itemData);
        }

    } else {
        // Nenhum item selecionado, emitir QVariant nulo ou um sinal específico
        emit itemSelected(QVariant()); // Emite QVariant nulo para indicar deseleção
        qDebug() << "Seleção limpa.";
    }
}

void MapView::setupSceneInteraction()
{
    // A seleção já é tratada por QGraphicsView/QGraphicsScene se os itens forem selecionáveis.
    // QGraphicsView::RubberBandDrag permite seleção em área.
    setDragMode(QGraphicsView::RubberBandDrag); // Permite seleção de múltiplos itens
}
