#include "geoip_client.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray> // Se a API retornar array em algum caso
#include <QUrlQuery> // Para adicionar parâmetros à URL de GeoIP
#include <QDebug>

GeoIPClient::GeoIPClient(QObject *parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

GeoIPClient::~GeoIPClient()
{
    // m_networkManager é filho de GeoIPClient, será deletado automaticamente.
}

void GeoIPClient::fetchPublicIp()
{
    QNetworkRequest request(QUrl(m_publicIpServiceUrl));
    // Alguns serviços podem requerer User-Agent
    // request.setHeader(QNetworkRequest::UserAgentHeader, "CartografoCpp/0.1");

    qDebug() << "Buscando IP público de" << m_publicIpServiceUrl;
    QNetworkReply *reply = m_networkManager->get(request);

    // Conectar o sinal finished do reply a um slot específico para esta requisição
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onPublicIpReplyFinished(reply);
    });
}

void GeoIPClient::onPublicIpReplyFinished(QNetworkReply *reply)
{
    if (!reply) {
        emit errorOccurred("publicIp", tr("Resposta da rede inválida (nula)."));
        return;
    }

    // Garante que o reply seja deletado após o uso
    // Isso é crucial se o connect foi feito com lambda que captura reply.
    // Se o connect usou this, &GeoIPClient::slotName, o reply é passado como argumento e pode ser deletado.
    // QScopedPointer ou reply->deleteLater() são boas práticas.
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> replyGuard(reply);


    if (reply->error() == QNetworkReply::NoError) {
        QString publicIp = QString::fromUtf8(reply->readAll()).trimmed();
        if (publicIp.isEmpty()) {
            qWarning() << "Serviço de IP público retornou resposta vazia.";
            emit errorOccurred("publicIp", tr("Serviço de IP público retornou resposta vazia."));
        } else {
            qDebug() << "IP Público detectado:" << publicIp;
            emit publicIpDetected(publicIp);
        }
    } else {
        QString errorMsg = tr("Erro ao buscar IP público: %1").arg(reply->errorString());
        qWarning() << errorMsg;
        emit errorOccurred("publicIp", errorMsg);
    }
    // reply é deletado automaticamente pelo QScopedPointer ao sair do escopo
}


void GeoIPClient::fetchGeoInfoForIp(const QString& ipAddress)
{
    if (ipAddress.isEmpty()) {
        emit errorOccurred("geoInfo", tr("Endereço IP para busca de GeoIP não pode ser vazio."));
        return;
    }

    QUrl url(m_geoIpServiceUrlBase + ipAddress);
    QUrlQuery query;
    // Campos que queremos da API ip-api.com. Personalize conforme necessário.
    // A lista completa está em http://ip-api.com/docs/api:json
    query.addQueryItem("fields", "status,message,continent,continentCode,country,countryCode,region,regionName,city,district,zip,lat,lon,timezone,offset,currency,isp,org,as,asname,mobile,proxy,hosting,query");
    url.setQuery(query);

    QNetworkRequest request(url);
    // request.setHeader(QNetworkRequest::UserAgentHeader, "CartografoCpp/0.1");

    qDebug() << "Buscando GeoInfo para" << ipAddress << "de" << url.toString();
    QNetworkReply *reply = m_networkManager->get(request);

    // Usar um lambda para conectar e capturar o ipAddress original
    connect(reply, &QNetworkReply::finished, this, [this, reply, ipAddress]() {
        // É importante usar um QScopedPointer ou deleteLater para o reply aqui também
        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> scopedReply(reply);
        onGeoInfoReplyFinished(scopedReply.data()); // Passa o ponteiro bruto
    });
}

void GeoIPClient::onGeoInfoReplyFinished(QNetworkReply *reply)
{
    if (!reply) {
        // O IP original não está disponível aqui diretamente sem captura,
        // então usamos um identificador genérico.
        emit errorOccurred("geoInfo", tr("Resposta da rede inválida (nula) para GeoIP."));
        return;
    }

    // O ipAddress original pode ser obtido da URL da requisição
    QString originalIp = reply->request().url().path().remove(0,1); // Remove o "/" inicial do path "/json/IP_ADDRESS"

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            QString errorMsg = tr("Erro ao parsear JSON da GeoIP para %1: %2")
                                   .arg(originalIp, parseError.errorString());
            qWarning() << errorMsg;
            emit errorOccurred("geoInfo_" + originalIp, errorMsg);
            return;
        }

        if (!jsonDoc.isObject()) {
            QString errorMsg = tr("Resposta JSON da GeoIP para %1 não é um objeto.").arg(originalIp);
            qWarning() << errorMsg;
            emit errorOccurred("geoInfo_" + originalIp, errorMsg);
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        QString status = jsonObj.value("status").toString();

        if (status == "success") {
            GeoInfo geoInfo = GeoInfo::fromJson(jsonObj); // Usa a função estática de data_types.h
            qDebug() << "GeoInfo recebido para" << originalIp << ":" << geoInfo.city << geoInfo.country;
            emit geoInfoReceived(originalIp, geoInfo);
        } else {
            QString message = jsonObj.value("message").toString();
            QString errorMsg = tr("API GeoIP para %1 retornou status '%2': %3")
                                   .arg(originalIp, status, message);
            qWarning() << errorMsg;
            emit errorOccurred("geoInfo_" + originalIp, errorMsg);
        }
    } else {
        QString errorMsg = tr("Erro de rede ao buscar GeoIP para %1: %2")
                               .arg(originalIp, reply->errorString());
        qWarning() << errorMsg;
        emit errorOccurred("geoInfo_" + originalIp, errorMsg);
    }
}
