#ifndef GEOIPCLIENT_H
#define GEOIPCLIENT_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "data_types.h" // Para GeoInfo

class GeoIPClient : public QObject
{
    Q_OBJECT

public:
    explicit GeoIPClient(QObject *parent = nullptr);
    ~GeoIPClient();

    // Busca o IP público atual do explorador
    // O resultado é emitido através do sinal publicIpDetected ou errorOccurred
    Q_INVOKABLE void fetchPublicIp();

    // Busca informações de GeoIP para um dado IP
    // O resultado é emitido através do sinal geoInfoReceived ou errorOccurred
    Q_INVOKABLE void fetchGeoInfoForIp(const QString& ipAddress);

signals:
    // Emitido quando o IP público é detectado com sucesso
    void publicIpDetected(const QString& publicIp);

    // Emitido quando as informações de GeoIP para um IP são recebidas com sucesso
    void geoInfoReceived(const QString& ipAddress, const GeoInfo& geoInfo);

    // Emitido em caso de erro de rede ou parsing
    void errorOccurred(const QString& requestType, const QString& errorString); // requestType pode ser "publicIp" ou "geoInfo"

private slots:
    void onPublicIpReplyFinished(QNetworkReply *reply);
    void onGeoInfoReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    const QString m_publicIpServiceUrl = "https://ifconfig.me/ip";
    // Documentação da API: http://ip-api.com/docs/api:json
    const QString m_geoIpServiceUrlBase = "http://ip-api.com/json/";
};

#endif // GEOIPCLIENT_H
