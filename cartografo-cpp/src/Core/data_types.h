#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariantMap>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

// Enum para tipos de nós, facilitando a estilização e lógica
enum class NodeType {
    Unknown,
    Explorer,
    Target,
    Hop,
    Ghost
};

// Informações de geolocalização
struct GeoInfo {
    QString query; // IP ou domínio consultado
    QString status;
    QString continent;
    QString continentCode;
    QString country;
    QString countryCode;
    QString region;
    QString regionName;
    QString city;
    QString district;
    QString zip;
    double lat = 0.0;
    double lon = 0.0;
    QString timezone;
    int offset = 0;
    QString currency;
    QString isp;
    QString org;
    QString as;
    QString asname;
    bool mobile = false;
    bool proxy = false;
    bool hosting = false;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["query"] = query;
        obj["status"] = status;
        obj["continent"] = continent;
        obj["continentCode"] = continentCode;
        obj["country"] = country;
        obj["countryCode"] = countryCode;
        obj["region"] = region;
        obj["regionName"] = regionName;
        obj["city"] = city;
        obj["district"] = district;
        obj["zip"] = zip;
        obj["lat"] = lat;
        obj["lon"] = lon;
        obj["timezone"] = timezone;
        obj["offset"] = offset;
        obj["currency"] = currency;
        obj["isp"] = isp;
        obj["org"] = org;
        obj["as"] = as;
        obj["asname"] = asname;
        obj["mobile"] = mobile;
        obj["proxy"] = proxy;
        obj["hosting"] = hosting;
        return obj;
    }

    static GeoInfo fromJson(const QJsonObject& obj) {
        GeoInfo geo;
        geo.query = obj["query"].toString();
        geo.status = obj["status"].toString();
        geo.continent = obj["continent"].toString();
        geo.continentCode = obj["continentCode"].toString();
        geo.country = obj["country"].toString();
        geo.countryCode = obj["countryCode"].toString();
        geo.region = obj["region"].toString();
        geo.regionName = obj["regionName"].toString();
        geo.city = obj["city"].toString();
        geo.district = obj["district"].toString();
        geo.zip = obj["zip"].toString();
        geo.lat = obj["lat"].toDouble();
        geo.lon = obj["lon"].toDouble();
        geo.timezone = obj["timezone"].toString();
        geo.offset = obj["offset"].toInt();
        geo.currency = obj["currency"].toString();
        geo.isp = obj["isp"].toString();
        geo.org = obj["org"].toString();
        geo.as = obj["as"].toString();
        geo.asname = obj["asname"].toString();
        geo.mobile = obj["mobile"].toBool();
        geo.proxy = obj["proxy"].toBool();
        geo.hosting = obj["hosting"].toBool();
        return geo;
    }
};

// Estrutura para um Nó no grafo
struct Node {
    QString id;         // Identificador único do nó (geralmente o IP)
    QString label;      // Rótulo para exibição (pode ser IP ou FQDN)
    NodeType type = NodeType::Unknown;
    QString group;      // Grupo do nó (explorer, target, hop, ghost) - para compatibilidade JSON
    GeoInfo geoInfo;
    QList<QString> fqdns; // Nomes de domínio totalmente qualificados associados
    QVariantMap customData; // Para dados adicionais específicos da aplicação

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["label"] = label;

        QString groupStr;
        switch (type) {
            case NodeType::Explorer: groupStr = "explorer"; break;
            case NodeType::Target:   groupStr = "target";   break;
            case NodeType::Hop:      groupStr = "hop";      break;
            case NodeType::Ghost:    groupStr = "ghost";    break;
            default:                 groupStr = "unknown";  break;
        }
        obj["group"] = groupStr;

        if (!geoInfo.query.isEmpty() || geoInfo.lat != 0.0 || geoInfo.lon != 0.0) { // Só adiciona se tiver dados
             obj["geo_info"] = geoInfo.toJson();
        }

        QJsonArray fqdnArray;
        for(const QString& fqdn : fqdns) {
            fqdnArray.append(fqdn);
        }
        obj["fqdns"] = fqdnArray;
        // customData não é serializado por padrão no formato .graph, mas pode ser usado internamente
        return obj;
    }

    static Node fromJson(const QJsonObject& obj) {
        Node node;
        node.id = obj["id"].toString();
        node.label = obj["label"].toString();
        node.group = obj["group"].toString();

        if (node.group == "explorer") node.type = NodeType::Explorer;
        else if (node.group == "target") node.type = NodeType::Target;
        else if (node.group == "hop") node.type = NodeType::Hop;
        else if (node.group == "ghost") node.type = NodeType::Ghost;
        else node.type = NodeType::Unknown;

        if (obj.contains("geo_info") && obj["geo_info"].isObject()) {
            node.geoInfo = GeoInfo::fromJson(obj["geo_info"].toObject());
        }

        if (obj.contains("fqdns") && obj["fqdns"].isArray()) {
            QJsonArray fqdnArray = obj["fqdns"].toArray();
            for (const QJsonValue& val : fqdnArray) {
                node.fqdns.append(val.toString());
            }
        }
        return node;
    }
};

// Estrutura para uma Aresta no grafo
struct Edge {
    QString id;         // Identificador único da aresta (ex: "ip1_ip2")
    QString from;       // ID do nó de origem
    QString to;         // ID do nó de destino
    QList<double> latencies; // Lista de latências registradas para esta conexão
    double averageLatency = 0.0; // Média das latências, calculada

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["from"] = from;
        obj["to"] = to;
        QJsonArray latArray;
        for(double lat : latencies) {
            latArray.append(lat);
        }
        obj["latencies"] = latArray;
        // averageLatency não faz parte do formato .graph, mas é útil internamente
        return obj;
    }

    static Edge fromJson(const QJsonObject& obj) {
        Edge edge;
        edge.id = obj["id"].toString(); // O ID pode precisar ser gerado se não estiver no arquivo
        edge.from = obj["from"].toString();
        edge.to = obj["to"].toString();
        if (obj.contains("latencies") && obj["latencies"].isArray()) {
            QJsonArray latArray = obj["latencies"].toArray();
            double sum = 0;
            int count = 0;
            for (const QJsonValue& val : latArray) {
                double lat = val.toDouble();
                edge.latencies.append(lat);
                sum += lat;
                count++;
            }
            if (count > 0) {
                edge.averageLatency = sum / count;
            }
        }
        // Se o ID não estiver presente no JSON, pode ser gerado como from + "_" + to
        if (edge.id.isEmpty()) {
            edge.id = edge.from + "_" + edge.to;
        }
        return edge;
    }
};

// Informações sobre os exploradores (conforme estrutura do .graph)
struct ExplorerInfo {
    QString id; // IP do explorador
    QString label;
    GeoInfo geoInfo;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["label"] = label;
        if (!geoInfo.query.isEmpty()) {
             obj["geo_info"] = geoInfo.toJson();
        }
        return obj;
    }

    static ExplorerInfo fromJson(const QJsonObject& obj) {
        ExplorerInfo info;
        info.id = obj["id"].toString();
        info.label = obj["label"].toString();
        if (obj.contains("geo_info") && obj["geo_info"].isObject()) {
            info.geoInfo = GeoInfo::fromJson(obj["geo_info"].toObject());
        }
        return info;
    }
};

// Estrutura para conter todos os dados do grafo
struct GraphData {
    QString fileVersion;
    QDateTime timestamp;
    QMap<QString, Node> nodes; // Nós indexados por ID para acesso rápido
    QMap<QString, Edge> edges; // Arestas indexadas por ID
    QMap<QString, ExplorerInfo> explorers; // Informações dos exploradores, chave é o IP

    void clear() {
        fileVersion.clear();
        timestamp = QDateTime();
        nodes.clear();
        edges.clear();
        explorers.clear();
    }

    QJsonObject toJson() const {
        QJsonObject rootObj;
        rootObj["file_version"] = fileVersion;
        rootObj["timestamp"] = timestamp.isValid() ? timestamp.toString(Qt::ISODate) : QString();

        QJsonArray nodesArray;
        for(const Node& node : nodes.values()) {
            nodesArray.append(node.toJson());
        }
        rootObj["nodes"] = nodesArray;

        QJsonArray edgesArray;
        for(const Edge& edge : edges.values()) {
            edgesArray.append(edge.toJson());
        }
        rootObj["edges"] = edgesArray;

        QJsonObject explorersObj; // No formato original, explorers é um objeto, não um array
        for(const ExplorerInfo& explorer : explorers.values()) {
            explorersObj[explorer.id] = explorer.toJson();
        }
        rootObj["explorers"] = explorersObj;

        return rootObj;
    }

    static GraphData fromJson(const QJsonObject& rootObj) {
        GraphData data;
        data.fileVersion = rootObj["file_version"].toString();
        QString tsString = rootObj["timestamp"].toString();
        data.timestamp = QDateTime::fromString(tsString, Qt::ISODate);

        if (rootObj.contains("nodes") && rootObj["nodes"].isArray()) {
            QJsonArray nodesArray = rootObj["nodes"].toArray();
            for (const QJsonValue& val : nodesArray) {
                Node node = Node::fromJson(val.toObject());
                data.nodes[node.id] = node;
            }
        }

        if (rootObj.contains("edges") && rootObj["edges"].isArray()) {
            QJsonArray edgesArray = rootObj["edges"].toArray();
            for (const QJsonValue& val : edgesArray) {
                Edge edge = Edge::fromJson(val.toObject());
                data.edges[edge.id] = edge; // Usa o ID da aresta (pode ser from_to)
            }
        }

        if (rootObj.contains("explorers") && rootObj["explorers"].isObject()) {
            QJsonObject explorersObject = rootObj["explorers"].toObject();
            for (auto it = explorersObject.constBegin(); it != explorersObject.constEnd(); ++it) {
                ExplorerInfo explorer = ExplorerInfo::fromJson(it.value().toObject());
                // Garante que o ID do explorer seja a chave, se não estiver dentro do objeto.
                // No entanto, o formato do explorer.py parece ter o ID como chave do objeto.
                if (explorer.id.isEmpty()) explorer.id = it.key();
                data.explorers[explorer.id] = explorer;
            }
        }
        return data;
    }
};

#endif // DATATYPES_H
