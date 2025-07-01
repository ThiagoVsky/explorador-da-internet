#ifndef LOGGING_H
#define LOGGING_H

#include <QLoggingCategory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDir>
#include <QStandardPaths>

// Declaração das categorias de logging
// Q_DECLARE_LOGGING_CATEGORY(lcCore)      // Lógica de negócio principal
// Q_DECLARE_LOGGING_CATEGORY(lcGui)       // Interface gráfica
// Q_DECLARE_LOGGING_CATEGORY(lcExploration) // Tarefas de Traceroute
// Q_DECLARE_LOGGING_CATEGORY(lcNetwork)   // Operações de rede (GeoIP, etc.)
// Q_DECLARE_LOGGING_CATEGORY(lcFileIO)    // Operações de arquivo (load/save graph)

// Para evitar problemas de linkagem com Q_DECLARE_LOGGING_CATEGORY em múltiplos TUs,
// é mais seguro definir e obter a categoria no .cpp e expor via inline functions ou macros.
// Ou, usar strings diretamente com QLoggingCategory("category.name")

// Wrapper para facilitar o uso e garantir que as categorias sejam definidas
namespace Logging {

// É importante que estas sejam inline ou que a definição da QLoggingCategory
// seja feita em um .cpp para evitar múltiplas definições.
// Usaremos a abordagem de definir no .cpp e fornecer getters aqui.

QLoggingCategory& core();
QLoggingCategory& gui();
QLoggingCategory& exploration();
QLoggingCategory& network();
QLoggingCategory& fileio();
QLoggingCategory& general(); // Categoria geral para mensagens não específicas

// Função para instalar o message handler
void installMessageHandler();
void cleanupMessageHandler(); // Para fechar o arquivo de log, se necessário

// Message Handler customizado
class MessageHandler : public QObject
{
    Q_OBJECT
public:
    explicit MessageHandler(QObject *parent = nullptr);
    ~MessageHandler();

    void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    static QFile m_logFile;
    static QTextStream m_logStream;
    static QMutex m_logMutex;
    static bool m_isHandlerInstalled;

    friend void installMessageHandler();
    friend void cleanupMessageHandler();
};

} // namespace Logging

// Macros para facilitar o uso (opcional, mas pode ser útil)
#define logCoreDebug()      qCDebug(Logging::core())
#define logCoreInfo()       qCInfo(Logging::core())
#define logCoreWarning()    qCWarning(Logging::core())
#define logCoreCritical()   qCCritical(Logging::core())

#define logGuiDebug()       qCDebug(Logging::gui())
#define logGuiInfo()        qCInfo(Logging::gui())
#define logGuiWarning()     qCWarning(Logging::gui())
#define logGuiCritical()    qCCritical(Logging::gui())

#define logExplorationDebug()   qCDebug(Logging::exploration())
#define logExplorationInfo()    qCInfo(Logging::exploration())
#define logExplorationWarning() qCWarning(Logging::exploration())
#define logExplorationCritical()qCCritical(Logging::exploration())

#define logNetworkDebug()   qCDebug(Logging::network())
#define logNetworkInfo()    qCInfo(Logging::network())
#define logNetworkWarning() qCWarning(Logging::network())
#define logNetworkCritical()qCCritical(Logging::network())

#define logFileIODebug()    qCDebug(Logging::fileio())
#define logFileIOInfo()     qCInfo(Logging::fileio())
#define logFileIOWarning()  qCWarning(Logging::fileio())
#define logFileIOCritical() qCCritical(Logging::fileio())

#define logGeneralDebug()   qCDebug(Logging::general())
#define logGeneralInfo()    qCInfo(Logging::general())
#define logGeneralWarning() qCWarning(Logging::general())
#define logGeneralCritical()qCCritical(Logging::general())


#endif // LOGGING_H
