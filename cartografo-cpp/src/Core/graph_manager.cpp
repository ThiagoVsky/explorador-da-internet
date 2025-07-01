#include "graph_manager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFileInfo>

GraphManager::GraphManager(QObject *parent) : QObject(parent)
{
    // Inicializações, se houver
    m_currentGraph.fileVersion = CURRENT_FILE_VERSION;
    m_currentGraph.timestamp = QDateTime::currentDateTime();
}

bool GraphManager::loadGraph(const QString& filePath)
{
    QFile loadFile(filePath);
    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Não foi possível abrir o arquivo de grafo: %s", qPrintable(filePath));
        emit errorOccurred(tr("Não foi possível abrir o arquivo: ") + loadFile.errorString());
        return false;
    }

    emit progressUpdate(tr("Lendo arquivo..."), 0);

    QByteArray saveData = loadFile.readAll();
    loadFile.close();

    QJsonParseError parseError;
    QJsonDocument loadDoc = QJsonDocument::fromJson(saveData, &parseError);

    if (loadDoc.isNull()) {
        qWarning("Erro ao parsear o arquivo JSON: %s", qPrintable(parseError.errorString()));
        emit errorOccurred(tr("Erro ao parsear o arquivo JSON: ") + parseError.errorString());
        return false;
    }

    if (!loadDoc.isObject()) {
        qWarning("Documento JSON não é um objeto.");
        emit errorOccurred(tr("Formato de arquivo inválido: o documento JSON não é um objeto."));
        return false;
    }

    QJsonObject rootObj = loadDoc.object();

    // Validação da versão do arquivo (RF02)
    if (!rootObj.contains("file_version")) {
        emit errorOccurred(tr("Formato de arquivo inválido: campo 'file_version' ausente."));
        return false;
    }
    QString fileVersion = rootObj["file_version"].toString();
    // Adicionar lógica mais robusta de comparação de versão se necessário (ex: semver)
    // Por enquanto, uma verificação simples:
    if (fileVersion < "0.6") { // Exemplo: aceita 0.6, 0.6.0, 0.6.1, etc. mas não 0.5.x
        emit errorOccurred(tr("Versão do arquivo (%1) não suportada. Requer v0.6+.").arg(fileVersion));
        return false;
    }

    emit progressUpdate(tr("Processando dados do grafo..."), 50);

    QMutexLocker locker(&m_mutex);
    m_currentGraph = GraphData::fromJson(rootObj);
    locker.unlock(); // Liberar mutex antes de emitir sinal

    // Se o arquivo não tiver timestamp, usamos o atual
    if (!m_currentGraph.timestamp.isValid()) {
        m_currentGraph.timestamp = QDateTime::currentDateTime();
    }
    // Garante que a versão do arquivo no GraphManager seja a lida ou a padrão se não presente
    m_currentGraph.fileVersion = fileVersion.isEmpty() ? CURRENT_FILE_VERSION : fileVersion;


    emit progressUpdate(tr("Grafo carregado."), 100);
    emit graphChanged();
    return true;
}

bool GraphManager::saveGraph(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    m_currentGraph.fileVersion = CURRENT_FILE_VERSION; // Garante que estamos salvando com a versão atual
    m_currentGraph.timestamp = QDateTime::currentDateTime(); // Atualiza o timestamp no momento de salvar

    QJsonObject rootObj = m_currentGraph.toJson();
    locker.unlock();

    QJsonDocument saveDoc(rootObj);

    QFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("Não foi possível abrir o arquivo para salvar: %s", qPrintable(filePath));
        emit errorOccurred(tr("Não foi possível salvar o arquivo: ") + saveFile.errorString());
        return false;
    }

    emit progressUpdate(tr("Salvando grafo..."), 0);
    qint64 bytesWritten = saveFile.write(saveDoc.toJson());
    saveFile.close();

    if (bytesWritten == -1) {
        emit errorOccurred(tr("Erro ao escrever dados no arquivo."));
        return false;
    }

    emit progressUpdate(tr("Grafo salvo com sucesso em %1.").arg(QFileInfo(filePath).fileName()), 100);
    return true;
}

GraphData GraphManager::getCurrentGraphData() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGraph;
}

void GraphManager::clearGraph()
{
    QMutexLocker locker(&m_mutex);
    m_currentGraph.clear();
    m_currentGraph.fileVersion = CURRENT_FILE_VERSION;
    m_currentGraph.timestamp = QDateTime::currentDateTime();
    locker.unlock();
    emit graphChanged();
}

bool GraphManager::addNode(const Node& node)
{
    if (node.id.isEmpty()) {
        qWarning("Tentativa de adicionar nó com ID vazio.");
        return false;
    }
    QMutexLocker locker(&m_mutex);
    bool exists = m_currentGraph.nodes.contains(node.id);
    m_currentGraph.nodes[node.id] = node; // Adiciona ou sobrescreve
    locker.unlock();

    if (!exists) { // Emitir graphChanged apenas se for uma adição real ou uma mudança significativa
        emit graphChanged();
    }
    return true;
}

bool GraphManager::addEdge(const Edge& edge)
{
    if (edge.id.isEmpty() || edge.from.isEmpty() || edge.to.isEmpty()) {
        qWarning("Tentativa de adicionar aresta com ID, from ou to vazios.");
        return false;
    }
    QMutexLocker locker(&m_mutex);
    bool exists = m_currentGraph.edges.contains(edge.id);
    m_currentGraph.edges[edge.id] = edge; // Adiciona ou sobrescreve

    // Recalcular a latência média ao adicionar/atualizar
    if (!m_currentGraph.edges[edge.id].latencies.isEmpty()) {
        double sum = 0;
        for(double lat : m_currentGraph.edges[edge.id].latencies) {
            sum += lat;
        }
        m_currentGraph.edges[edge.id].averageLatency = sum / m_currentGraph.edges[edge.id].latencies.size();
    } else {
        m_currentGraph.edges[edge.id].averageLatency = 0.0;
    }
    locker.unlock();

    if (!exists) {
        emit graphChanged();
    }
    return true;
}

bool GraphManager::addOrUpdateExplorer(const ExplorerInfo& explorerInfo) {
    if (explorerInfo.id.isEmpty()) {
        qWarning("Tentativa de adicionar explorador com ID vazio.");
        return false;
    }
    QMutexLocker locker(&m_mutex);
    m_currentGraph.explorers[explorerInfo.id] = explorerInfo;
    locker.unlock();
    // Poderia emitir um sinal específico para mudança de explorador se necessário
    emit graphChanged(); // Ou um sinal mais específico
    return true;
}


Node GraphManager::getNode(const QString& nodeId) const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGraph.nodes.value(nodeId); // Retorna nó default se não encontrado
}

Edge GraphManager::getEdge(const QString& edgeId) const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGraph.edges.value(edgeId); // Retorna aresta default se não encontrada
}

QList<Node> GraphManager::getAllNodes() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGraph.nodes.values();
}

QList<Edge> GraphManager::getAllEdges() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGraph.edges.values();
}

QList<ExplorerInfo> GraphManager::getAllExplorers() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGraph.explorers.values();
}

void GraphManager::concatenateExplorationData(const GraphData& newData)
{
    QMutexLocker locker(&m_mutex);

    // Adicionar/Atualizar Nós
    for (const Node& newNode : newData.nodes.values()) {
        if (m_currentGraph.nodes.contains(newNode.id)) {
            // Nó existente: atualizar informações se necessário (ex: FQDNs, tipo se mudou)
            Node& existingNode = m_currentGraph.nodes[newNode.id];
            if (existingNode.label.isEmpty() && !newNode.label.isEmpty()) existingNode.label = newNode.label;
            if (existingNode.type == NodeType::Unknown && newNode.type != NodeType::Unknown) existingNode.type = newNode.type;
            if (existingNode.group.isEmpty() && !newNode.group.isEmpty()) existingNode.group = newNode.group; // Atualiza grupo se estava vazio

            // Concatenar FQDNs (evitando duplicatas)
            for (const QString& fqdn : newNode.fqdns) {
                if (!existingNode.fqdns.contains(fqdn)) {
                    existingNode.fqdns.append(fqdn);
                }
            }
            // Atualizar GeoInfo se a nova tiver mais dados (simplista, pode precisar de lógica mais fina)
            if (newNode.geoInfo.lat != 0.0 || newNode.geoInfo.lon != 0.0) { // Se a nova tem geo_info
                 if(existingNode.geoInfo.query.isEmpty() || existingNode.geoInfo.lat == 0.0 ) { // Se a existente não tem ou é incompleta
                    existingNode.geoInfo = newNode.geoInfo;
                 }
            }
        } else {
            // Novo nó
            m_currentGraph.nodes.insert(newNode.id, newNode);
        }
    }

    // Adicionar/Atualizar Arestas
    for (const Edge& newEdge : newData.edges.values()) {
        QString edgeId = newEdge.id;
        if (edgeId.isEmpty()) edgeId = newEdge.from + "_" + newEdge.to; // Garante ID

        if (m_currentGraph.edges.contains(edgeId)) {
            // Aresta existente: concatenar latências
            Edge& existingEdge = m_currentGraph.edges[edgeId];
            existingEdge.latencies.append(newEdge.latencies);
            // Recalcular média
            if (!existingEdge.latencies.isEmpty()) {
                double sum = 0;
                for(double lat : existingEdge.latencies) sum += lat;
                existingEdge.averageLatency = sum / existingEdge.latencies.size();
            }
        } else {
            // Nova aresta
            m_currentGraph.edges.insert(edgeId, newEdge);
            // Calcular média para a nova aresta
            if (!m_currentGraph.edges[edgeId].latencies.isEmpty()) {
                double sum = 0;
                for(double lat : m_currentGraph.edges[edgeId].latencies) sum += lat;
                m_currentGraph.edges[edgeId].averageLatency = sum / m_currentGraph.edges[edgeId].latencies.size();
            }
        }
    }

    // Adicionar/Atualizar Explorers
    for (const ExplorerInfo& newExplorer : newData.explorers.values()) {
        if (m_currentGraph.explorers.contains(newExplorer.id)) {
            // Atualizar se necessário (ex: GeoInfo)
            ExplorerInfo& existingExplorer = m_currentGraph.explorers[newExplorer.id];
            if (newExplorer.geoInfo.lat != 0.0 || newExplorer.geoInfo.lon != 0.0) {
                existingExplorer.geoInfo = newExplorer.geoInfo; // Sobrescreve geo_info
            }
            if (existingExplorer.label.isEmpty() && !newExplorer.label.isEmpty()){
                existingExplorer.label = newExplorer.label;
            }
        } else {
            m_currentGraph.explorers.insert(newExplorer.id, newExplorer);
        }
    }

    // Atualizar timestamp geral do grafo
    m_currentGraph.timestamp = QDateTime::currentDateTime();
    // A versão do arquivo é mantida, a menos que explicitamente alterada ao salvar.

    locker.unlock();
    emit graphChanged();
}
