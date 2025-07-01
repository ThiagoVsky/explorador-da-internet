#include "logging.h"
#include <iostream> // Para saída de console do message handler padrão

// Definição das categorias de logging
// O nome da categoria (ex: "app.core") é usado para filtragem.
Q_LOGGING_CATEGORY(lcCore, "app.core")
Q_LOGGING_CATEGORY(lcGui, "app.gui")
Q_LOGGING_CATEGORY(lcExploration, "app.exploration")
Q_LOGGING_CATEGORY(lcNetwork, "app.network")
Q_LOGGING_CATEGORY(lcFileIO, "app.fileio")
Q_LOGGING_CATEGORY(lcGeneral, "app.general")

namespace Logging {

// Getters para as categorias (para evitar problemas de linkagem com Q_DECLARE_LOGGING_CATEGORY)
QLoggingCategory& core() { return lcCore; }
QLoggingCategory& gui() { return lcGui; }
QLoggingCategory& exploration() { return lcExploration; }
QLoggingCategory& network() { return lcNetwork; }
QLoggingCategory& fileio() { return lcFileIO; }
QLoggingCategory& general() { return lcGeneral; }


// Implementação do MessageHandler
QFile MessageHandler::m_logFile;
QTextStream MessageHandler::m_logStream;
QMutex MessageHandler::m_logMutex;
bool MessageHandler::m_isHandlerInstalled = false;
static QtMessageHandler defaultMessageHandler = nullptr; // Armazena o handler padrão

MessageHandler::MessageHandler(QObject *parent) : QObject(parent) {}

MessageHandler::~MessageHandler() {
    QMutexLocker locker(&m_logMutex);
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
}

void MessageHandler::handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&m_logMutex);

    QString levelText;
    switch (type) {
    case QtDebugMsg:    levelText = "DEBUG"; break;
    case QtInfoMsg:     levelText = "INFO "; break;
    case QtWarningMsg:  levelText = "WARN "; break;
    case QtCriticalMsg: levelText = "CRIT "; break;
    case QtFatalMsg:    levelText = "FATAL"; break;
    }

    // Formato da mensagem: Data Hora [NÍVEL] [CATEGORIA] Mensagem (arquivo:linha, função)
    QString formattedMessage = QString("%1 [%2] [%3] %4 (%5:%6, %7)\n")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                                   .arg(levelText)
                                   .arg(context.category ? context.category : "default")
                                   .arg(msg)
                                   .arg(context.file ? QFileInfo(context.file).fileName() : "unknown_file")
                                   .arg(context.line)
                                   .arg(context.function ? context.function : "unknown_function");

    if (m_logFile.isOpen() && m_logStream.device()) {
        m_logStream << formattedMessage;
        m_logStream.flush(); // Garante que seja escrito imediatamente
    }

    // Opcional: também envia para o console (especialmente para debug builds)
    // Pode ser controlado por uma diretiva de compilação como #ifdef QT_DEBUG
    // Para evitar duplicação se o handler padrão já imprime, podemos chamar o handler padrão.
    if (defaultMessageHandler) {
         // Não passar o formattedMessage, mas os argumentos originais,
         // pois o handler padrão pode ter seu próprio formato.
         // No entanto, para console, podemos querer o nosso formato também.
         // std::cerr << formattedMessage.toStdString(); // Exemplo de envio direto
        defaultMessageHandler(type, context, msg); // Chama o handler anterior
    } else {
        // Fallback se não houver handler padrão (improvável)
        fprintf(stderr, "%s", formattedMessage.toLocal8Bit().constData());
        if (type == QtFatalMsg) abort();
    }

    // if (type == QtFatalMsg) {
    //     // Se quisermos fazer algo especial antes de abortar
    //     abort();
    // }
}


static MessageHandler globalMessageHandlerInstance; // Uma instância global

void installMessageHandler()
{
    if (MessageHandler::m_isHandlerInstalled) return;

    QMutexLocker locker(&MessageHandler::m_logMutex);
    if (MessageHandler::m_isHandlerInstalled) return; // Double check com lock

    // Define o local do arquivo de log
    // QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // AppDataLocation pode ser algo como ~/.local/share/CartografoCppOrg/CartografoCpp/
    // Ou um local mais simples como o diretório da aplicação ou Documentos.
    // Por simplicidade, vamos usar o diretório da aplicação por enquanto.
    QString logPath = QCoreApplication::applicationDirPath();
    QDir logDir(logPath);
    // if (!logDir.exists()) {
    //     logDir.mkpath("."); // Cria o diretório se não existir (para AppDataLocation)
    // }
    MessageHandler::m_logFile.setFileName(logDir.filePath("cartografo.log"));

    if (!MessageHandler::m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning("Não foi possível abrir o arquivo de log: %s", qPrintable(MessageHandler::m_logFile.errorString()));
        return;
    }
    MessageHandler::m_logStream.setDevice(&MessageHandler::m_logFile);
    MessageHandler::m_logStream.setEncoding(QStringConverter::Utf8);


    // Salva o handler padrão e instala o nosso
    defaultMessageHandler = qInstallMessageHandler(&MessageHandler::handleMessage);
    MessageHandler::m_isHandlerInstalled = true;

    qCInfo(lcGeneral()) << "Manipulador de Log instalado. Logando em:" << MessageHandler::m_logFile.fileName();
}

void cleanupMessageHandler() {
    QMutexLocker locker(&MessageHandler::m_logMutex);
    if (MessageHandler::m_logFile.isOpen()) {
        qCInfo(lcGeneral()) << "Fechando arquivo de log.";
        MessageHandler::m_logStream.flush();
        MessageHandler::m_logFile.close();
    }
    // Restaurar o handler padrão se necessário, mas geralmente não é feito no cleanup
    // qInstallMessageHandler(defaultMessageHandler);
    // defaultMessageHandler = nullptr;
    MessageHandler::m_isHandlerInstalled = false;
}

} // namespace Logging
