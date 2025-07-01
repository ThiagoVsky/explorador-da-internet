#ifndef GRAPHMANAGER_H
#define GRAPHMANAGER_H

#include "data_types.h"
#include <QObject>
#include <QString>
#include <QMutex> // Para thread-safety se necessário no futuro

class GraphManager : public QObject
{
    Q_OBJECT

public:
    explicit GraphManager(QObject *parent = nullptr);

    // Carrega um grafo de um arquivo .graph (JSON)
    Q_INVOKABLE bool loadGraph(const QString& filePath);

    // Salva o grafo atual em um arquivo .graph (JSON)
    Q_INVOKABLE bool saveGraph(const QString& filePath);

    // Retorna uma cópia dos dados do grafo atual
    // (Considerar const correctness e talvez retornar const&)
    GraphData getCurrentGraphData() const;

    // Limpa o grafo atual
    void clearGraph();

    // Adiciona um nó ao grafo
    // Retorna true se adicionado, false se já existe e foi atualizado ou erro
    bool addNode(const Node& node);

    // Adiciona uma aresta ao grafo
    // Retorna true se adicionada, false se já existe e foi atualizada ou erro
    bool addEdge(const Edge& edge);

    // Adiciona ou atualiza informações de um explorador
    bool addOrUpdateExplorer(const ExplorerInfo& explorerInfo);

    // Concatena dados de uma nova exploração ao grafo existente
    // Esta é uma operação complexa que pode envolver:
    // - Adicionar novos nós e arestas
    // - Atualizar latências de arestas existentes
    // - Atualizar informações de nós (ex: FQDNs, tipo)
    void concatenateExplorationData(const GraphData& newData);

    // Retorna o nó com o ID especificado, se existir
    Node getNode(const QString& nodeId) const;

    // Retorna a aresta com o ID especificado, se existir
    Edge getEdge(const QString& edgeId) const;

    // Retorna todos os nós
    QList<Node> getAllNodes() const;

    // Retorna todas as arestas
    QList<Edge> getAllEdges() const;

    // Retorna informações de todos os exploradores
    QList<ExplorerInfo> getAllExplorers() const;


signals:
    // Sinal emitido quando o grafo é modificado (carregado, nós/arestas adicionados, etc.)
    void graphChanged();

    // Sinal emitido durante operações longas (carregar/salvar)
    // Parâmetros: mensagem, progresso (0-100 ou -1 para indeterminado)
    void progressUpdate(const QString& message, int value);

    // Sinal emitido em caso de erro durante operações
    void errorOccurred(const QString& errorMessage);

private:
    GraphData m_currentGraph;
    mutable QMutex m_mutex; // Para proteger acesso a m_currentGraph se usado por múltiplas threads

    const QString CURRENT_FILE_VERSION = "0.6.0"; // Ou superior, conforme RF02
};

#endif // GRAPHMANAGER_H
