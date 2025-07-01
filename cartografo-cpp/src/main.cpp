#include <QApplication>
#include "GUI/main_window.h"
#include "Core/logging.h" // Inclui o sistema de logging
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Configurações da Aplicação (nome da organização para QSettings, etc.)
    // Movido para o construtor da MainWindow para que QSettings funcione corretamente lá.
    // QCoreApplication::setOrganizationName("CartografoCppOrg"); // Já definido na MainWindow
    // QCoreApplication::setApplicationName("CartografoCpp");   // Já definido na MainWindow
    QApplication::setApplicationVersion("0.1.0"); // Pode ficar aqui ou na MainWindow

    // Instala o manipulador de log
    Logging::installMessageHandler();
    qCInfo(Logging::general()) << "Aplicação Cartógrafo de Rede iniciando...";
    logGeneralInfo() << "Versão da Aplicação:" << QApplication::applicationVersion();


    MainWindow mainWindow;
    mainWindow.show();

    int result = app.exec();

    // Limpa o manipulador de log (ex: fecha o arquivo)
    Logging::cleanupMessageHandler();

    return result;
}
