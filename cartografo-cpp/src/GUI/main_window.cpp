#include "main_window.h"
#include "ui_main_window.h"
#include "welcome_dialog.h"
#include "new_exploration_dialog.h"
#include "Core/graph_manager.h"
#include "Core/geoip_client.h"
#include "Core/traceroute_task.h"
#include "GUI/map_view.h"
#include "GUI/details_panel.h"

#include <QApplication>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QProgressDialog> // Incluído no .h
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QDebug>
#include <QSettings> // Para RNF02

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_graphManager(nullptr)
    , m_geoIPClient(nullptr)
    , m_mapView(nullptr)
    , m_detailsPanel(nullptr)
    , m_saveAction(nullptr)
    , m_saveAsAction(nullptr)
    , m_newExplorationAction(nullptr)
    , m_reExploreAction(nullptr)
    , m_zoomInButton(nullptr)
    , m_zoomOutButton(nullptr)
    , m_fitToScreenButton(nullptr)
    , m_reorganizeLayoutButton(nullptr)
    , m_totalTasksToRun(0)
    , m_tasksCompleted(0)
    , m_globalExplorationProgressDialog(nullptr)
{
    ui->setupUi(this);
    // Nome da organização e aplicação para QSettings
    QCoreApplication::setOrganizationName("CartografoCppOrg");
    QCoreApplication::setApplicationName("CartografoCpp");

    setWindowTitle(tr("Cartógrafo de Rede"));
    // setMinimumSize(800, 600); // Removido para permitir que QSettings restaure o tamanho

    setupCoreComponents();
    setupMenuBar();
    setupMainLayout();

    readSettings(); // RNF02: Ler configurações da UI

    statusBar()->showMessage(tr("Pronto."), 2000);
    setUiForExplorationState(false);

    QTimer::singleShot(0, this, &MainWindow::showWelcomeDialog);
}

MainWindow::~MainWindow()
{
    for (QThread* thread : qAsConst(m_explorationThreads)) {
        if (thread->isRunning()) {
            thread->quit();
            thread->wait(1000);
            if(thread->isRunning()) thread->terminate();
        }
        delete thread;
    }
    m_explorationThreads.clear();

    delete ui;
    if (m_globalExplorationProgressDialog) {
        delete m_globalExplorationProgressDialog;
        m_globalExplorationProgressDialog = nullptr;
    }
}

void MainWindow::readSettings() {
    QSettings settings;
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/windowState").toByteArray()); // Restaura estado de docks, toolbars

    // Restaurar visibilidade e tamanho do DetailsPanel (se ele for um QDockWidget)
    if (m_detailsPanel) {
        if (settings.contains("detailsPanel/visible")) {
            m_detailsPanel->setVisible(settings.value("detailsPanel/visible", true).toBool());
        }
        // O tamanho do dock widget é geralmente parte do estado da main window.
        // Se precisarmos de controle mais fino:
        // if (settings.contains("detailsPanel/size")) {
        //     m_detailsPanel->resize(settings.value("detailsPanel/size").toSize());
        // }
    }
    // Último diretório não é armazenado aqui, mas usado no QFileDialog
}

void MainWindow::writeSettings() {
    QSettings settings;
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/windowState", saveState()); // Salva estado de docks, toolbars

    if (m_detailsPanel) {
        settings.setValue("detailsPanel/visible", m_detailsPanel->isVisible());
        // settings.setValue("detailsPanel/size", m_detailsPanel->size());
    }
    // Último diretório é salvo pelo QFileDialog se usarmos os métodos estáticos getOpenFileName/getSaveFileName
    // com um argumento de diretório que é atualizado.
}


void MainWindow::setupMainLayout()
{
    m_mapView = new MapView(this);
    setCentralWidget(m_mapView);

    if (m_graphManager) {
        m_mapView->setGraphManager(m_graphManager);
    }
    connect(m_mapView, &MapView::itemSelected, this, &MainWindow::onMapViewItemSelected);

    m_detailsPanel = new DetailsPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_detailsPanel);
    m_detailsPanel->setObjectName("detailsPanelDock"); // Necessário para saveState/restoreState funcionar bem
    m_detailsPanel->setMinimumWidth(250);

    setupOverlayButtons();
}


void MainWindow::setupOverlayButtons()
{
    m_zoomInButton = new QPushButton("+", this);
    m_zoomInButton->setToolTip(tr("Zoom In"));
    m_zoomInButton->setFixedSize(30, 30);
    connect(m_zoomInButton, &QPushButton::clicked, this, &MainWindow::onZoomInClicked);

    m_zoomOutButton = new QPushButton("-", this);
    m_zoomOutButton->setToolTip(tr("Zoom Out"));
    m_zoomOutButton->setFixedSize(30, 30);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &MainWindow::onZoomOutClicked);

    m_fitToScreenButton = new QPushButton("[]", this);
    m_fitToScreenButton->setToolTip(tr("Ajustar à Tela"));
    m_fitToScreenButton->setFixedSize(30, 30);
    connect(m_fitToScreenButton, &QPushButton::clicked, this, &MainWindow::onFitToScreenClicked);

    m_reorganizeLayoutButton = new QPushButton("R", this);
    m_reorganizeLayoutButton->setToolTip(tr("Reorganizar Layout"));
    m_reorganizeLayoutButton->setFixedSize(30, 30);
    connect(m_reorganizeLayoutButton, &QPushButton::clicked, this, &MainWindow::onReorganizeLayoutClicked);

    positionOverlayButtons();

    m_zoomInButton->show();
    m_zoomOutButton->show();
    m_fitToScreenButton->show();
    m_reorganizeLayoutButton->show();

    m_zoomInButton->raise();
    m_zoomOutButton->raise();
    m_fitToScreenButton->raise();
    m_reorganizeLayoutButton->raise();
}

void MainWindow::positionOverlayButtons()
{
    if (!m_mapView || !m_zoomInButton) return;

    int margin = 10;
    int buttonHeight = 30;
    int currentY = margin;

    int xPos = width() - margin - m_zoomInButton->width();
    if (m_detailsPanel && m_detailsPanel->isVisible() && !m_detailsPanel->isFloating()) {
         QList<QDockWidget *> dockWidgets = findChildren<QDockWidget *>();
         for(QDockWidget* dock : dockWidgets) {
            if (dockWidgetArea(dock) == Qt::RightDockWidgetArea && dock->isVisible()) {
                xPos -= dock->width(); // Ajusta se o dock estiver à direita
                break;
            }
         }
    }


    m_zoomInButton->move(xPos, currentY);
    currentY += buttonHeight + 5;

    m_zoomOutButton->move(xPos, currentY);
    currentY += buttonHeight + 5;

    m_fitToScreenButton->move(xPos, currentY);
    currentY += buttonHeight + 5;

    m_reorganizeLayoutButton->move(xPos, currentY);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionOverlayButtons();
}


void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Arquivo"));
    const QIcon openIcon = QIcon::fromTheme("document-open", QIcon(":/icons/document-open.png"));
    QAction *openAction = new QAction(openIcon, tr("&Abrir..."), this);
    openAction->setShortcuts(QKeySequence::Open);
    openAction->setStatusTip(tr("Abrir um arquivo de mapa existente"));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(openAction);
    fileMenu->addSeparator();
    const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":/icons/document-save.png"));
    m_saveAction = new QAction(saveIcon, tr("&Salvar"), this);
    m_saveAction->setShortcuts(QKeySequence::Save);
    m_saveAction->setStatusTip(tr("Salvar o mapa atual"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    fileMenu->addAction(m_saveAction);
    const QIcon saveAsIcon = QIcon::fromTheme("document-save-as", QIcon(":/icons/document-save-as.png"));
    m_saveAsAction = new QAction(saveAsIcon, tr("Salvar &Como..."), this);
    m_saveAsAction->setShortcuts(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip(tr("Salvar o mapa atual em um novo arquivo"));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    const QIcon exitIcon = QIcon::fromTheme("application-exit", QIcon(":/icons/application-exit.png"));
    QAction *exitAction = new QAction(exitIcon, tr("Sai&r"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    exitAction->setStatusTip(tr("Sair da aplicação"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    fileMenu->addAction(exitAction);

    QMenu *explorationMenu = menuBar()->addMenu(tr("&Exploração"));
    m_newExplorationAction = new QAction(tr("&Nova Exploração..."), this);
    m_newExplorationAction->setStatusTip(tr("Iniciar uma nova exploração de rede para alvos especificados"));
    connect(m_newExplorationAction, &QAction::triggered, this, &MainWindow::startNewExploration);
    explorationMenu->addAction(m_newExplorationAction);

    m_reExploreAction = new QAction(tr("&Re-explorar Alvos Atuais"), this);
    m_reExploreAction->setStatusTip(tr("Executar traceroute novamente para todos os alvos no mapa atual"));
    connect(m_reExploreAction, &QAction::triggered, this, &MainWindow::reExploreTargets);
    explorationMenu->addAction(m_reExploreAction);

    // Menu Visualizar para controlar visibilidade de Docks
    QMenu *viewMenu = menuBar()->addMenu(tr("&Visualizar"));
    if (m_detailsPanel) {
        viewMenu->addAction(m_detailsPanel->toggleViewAction());
    }
}

void MainWindow::setupCoreComponents()
{
    m_graphManager = new GraphManager(this);
    connect(m_graphManager, &GraphManager::graphChanged, this, &MainWindow::onGraphChanged);
    connect(m_graphManager, &GraphManager::progressUpdate, this, &MainWindow::onGraphManagerProgress);
    connect(m_graphManager, &GraphManager::errorOccurred, this, &MainWindow::onGraphManagerError);

    m_geoIPClient = new GeoIPClient(this);
    connect(m_geoIPClient, &GeoIPClient::publicIpDetected, this, &MainWindow::onPublicIpDetected);
    connect(m_geoIPClient, &GeoIPClient::errorOccurred, this, &MainWindow::onGeoIPError);

    m_geoIPClient->fetchPublicIp();
}


void MainWindow::showWelcomeDialog()
{
    WelcomeDialog welcomeDialog(this);
    connect(&welcomeDialog, &WelcomeDialog::openExistingMapRequested, this, &MainWindow::handleOpenExistingMapRequested);
    connect(&welcomeDialog, &WelcomeDialog::newMapRequested, this, &MainWindow::handleNewMapRequested);

    if (welcomeDialog.exec() == QDialog::Rejected) {
        qDebug() << "WelcomeDialog rejeitado, fechando aplicação.";
        QTimer::singleShot(0, this, &MainWindow::close);
        return;
    }
}

void MainWindow::handleOpenExistingMapRequested()
{
    openFile();
}

void MainWindow::handleNewMapRequested()
{
    m_graphManager->clearGraph();
    if(m_mapView) m_mapView->clearMap();
    if(m_detailsPanel) m_detailsPanel->clearPanel();
    m_currentFilePath.clear();
    setWindowTitle(tr("Novo Mapa - Cartógrafo de Rede"));
    onGraphChanged();
    statusBar()->showMessage(tr("Novo mapa iniciado."), 5000);
}


void MainWindow::openFile()
{
    if (m_explorationThreads.size() > 0) {
        QMessageBox::warning(this, tr("Exploração em Andamento"), tr("Por favor, aguarde a conclusão das explorações atuais antes de abrir um novo arquivo."));
        return;
    }
    QSettings settings;
    QString lastDir = settings.value("dialogs/lastDirectory", QDir::homePath()).toString();
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    tr("Abrir Mapa"), lastDir,
                                                    tr("Arquivos de Grafo (*.graph);;Arquivos JSON (*.json);;Todos os Arquivos (*)"));
    if (!filePath.isEmpty()) {
        settings.setValue("dialogs/lastDirectory", QFileInfo(filePath).absolutePath());
        QProgressDialog progressDialog(tr("Carregando arquivo..."), tr("Cancelar"), 0, 0, this);
        progressDialog.setWindowModality(Qt::WindowModal);
        progressDialog.setMinimumDuration(0);

        disconnect(m_graphManager, &GraphManager::progressUpdate, this, &MainWindow::onGraphManagerProgress);
        connect(m_graphManager, &GraphManager::progressUpdate, &progressDialog,
            [&progressDialog](const QString& message, int value) {
                if (!message.isEmpty()) progressDialog.setLabelText(message);
        });

        bool success = m_graphManager->loadGraph(filePath);

        progressDialog.close();
        connect(m_graphManager, &GraphManager::progressUpdate, this, &MainWindow::onGraphManagerProgress);

        if (success) {
            m_currentFilePath = filePath;
            setWindowTitle(QFileInfo(m_currentFilePath).fileName() + tr(" - Cartógrafo de Rede"));
            statusBar()->showMessage(tr("Arquivo '%1' carregado.").arg(QFileInfo(filePath).fileName()), 5000);
            onGraphChanged();
            checkExplorerIdentity();
        }
    }
}

void MainWindow::saveFile()
{
    if (m_currentFilePath.isEmpty()) {
        saveFileAs();
    } else {
        QProgressDialog progressDialog(tr("Salvando arquivo..."), QString(), 0, 0, this);
        progressDialog.setWindowModality(Qt::WindowModal);
        progressDialog.setMinimumDuration(0);
        disconnect(m_graphManager, &GraphManager::progressUpdate, this, &MainWindow::onGraphManagerProgress);
        connect(m_graphManager, &GraphManager::progressUpdate, &progressDialog,
            [&progressDialog](const QString& message, int value){
                 if (!message.isEmpty()) progressDialog.setLabelText(message);
            });

        bool success = m_graphManager->saveGraph(m_currentFilePath);
        progressDialog.close();
        connect(m_graphManager, &GraphManager::progressUpdate, this, &MainWindow::onGraphManagerProgress);

        if (success) {
            setWindowTitle(QFileInfo(m_currentFilePath).fileName() + tr(" - Cartógrafo de Rede"));
            statusBar()->showMessage(tr("Arquivo salvo em '%1'.").arg(m_currentFilePath), 5000);
            m_saveAction->setEnabled(false);
        }
    }
}

void MainWindow::saveFileAs()
{
    QSettings settings;
    QString lastDir = settings.value("dialogs/lastDirectory", QDir::homePath()).toString();
    QString defaultFileName = m_currentFilePath.isEmpty() ? "novo_mapa.graph" : QFileInfo(m_currentFilePath).fileName();
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    tr("Salvar Mapa Como"), lastDir + "/" + defaultFileName,
                                                    tr("Arquivos de Grafo (*.graph);;Arquivos JSON (*.json);;Todos os Arquivos (*)"));
    if (!filePath.isEmpty()) {
        settings.setValue("dialogs/lastDirectory", QFileInfo(filePath).absolutePath());
        m_currentFilePath = filePath;
        saveFile();
    }
}

void MainWindow::exitApplication()
{
    if (m_explorationThreads.size() > 0) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Exploração em Andamento"),
                                      tr("Há explorações de rede em andamento. Sair agora irá cancelá-las. Deseja continuar?"),
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
        for(TracerouteTask* task : qAsConst(m_explorationTasks)) {
            task->cancel();
        }
    }
    QApplication::quit();
}

void MainWindow::onGraphChanged()
{
    GraphData currentGraph = m_graphManager->getCurrentGraphData();
    bool graphHasData = !currentGraph.nodes.isEmpty() || !currentGraph.edges.isEmpty();

    m_saveAsAction->setEnabled(graphHasData);
    if(m_reExploreAction) m_reExploreAction->setEnabled(graphHasData);

    if (m_mapView) {
        m_mapView->updateGraph(currentGraph);
    }
    if (m_detailsPanel) {
        m_detailsPanel->clearPanel();
    }

    bool explorationActive = !m_explorationThreads.isEmpty();
    setUiForExplorationState(explorationActive);

    bool enableMapControls = graphHasData && !explorationActive;
    if(m_zoomInButton) m_zoomInButton->setEnabled(enableMapControls);
    if(m_zoomOutButton) m_zoomOutButton->setEnabled(enableMapControls);
    if(m_fitToScreenButton) m_fitToScreenButton->setEnabled(enableMapControls);
    if(m_reorganizeLayoutButton) m_reorganizeLayoutButton->setEnabled(enableMapControls);


    qDebug() << "GraphManager emitiu graphChanged. MapView e UI atualizados.";
}

void MainWindow::onGraphManagerProgress(const QString& message, int value)
{
    if (value < 100 && value >= 0) {
        statusBar()->showMessage(message + QString(" (%1%)").arg(value));
    } else {
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::onGraphManagerError(const QString& errorMessage)
{
    QMessageBox::critical(this, tr("Erro do Gerenciador de Grafo"), errorMessage);
    statusBar()->showMessage(tr("Erro: %1").arg(errorMessage), 5000);
}

void MainWindow::onPublicIpDetected(const QString& ip)
{
    m_publicIpAddress = ip;
    statusBar()->showMessage(tr("IP Público: %1").arg(ip), 5000);
    qDebug() << "MainWindow: IP Público detectado:" << ip;

    if (!m_graphManager->getCurrentGraphData().nodes.isEmpty()) {
        // checkExplorerIdentity();
    }
}

void MainWindow::onGeoIPError(const QString& requestType, const QString& errorString)
{
    if (requestType == "publicIp") {
        QMessageBox::warning(this, tr("Erro de Rede"), tr("Não foi possível obter o IP público: %1").arg(errorString));
        statusBar()->showMessage(tr("Falha ao obter IP público."), 5000);
    } else {
         qDebug() << "Erro GeoIP para" << requestType << ":" << errorString;
    }
}


void MainWindow::checkExplorerIdentity() {
    if (m_publicIpAddress.isEmpty()) {
        qWarning() << "IP público não conhecido. Não é possível verificar a identidade do explorador.";
        return;
    }

    GraphData currentGraph = m_graphManager->getCurrentGraphData();
    bool publicIpMatchesExplorer = false;
    if (currentGraph.explorers.contains(m_publicIpAddress)) {
        publicIpMatchesExplorer = true;
    }

    if (!currentGraph.explorers.isEmpty() && !publicIpMatchesExplorer) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle(tr("Conflito de Identidade do Explorador"));
        msgBox.setText(tr("Seu IP público atual (%1) é diferente dos IPs exploradores encontrados no arquivo.\n"
                          "Deseja adicionar seu IP atual como um novo explorador neste mapa?")
                       .arg(m_publicIpAddress));
        QPushButton *continueButton = msgBox.addButton(tr("Continuar e Adicionar IP"), QMessageBox::AcceptRole);
        msgBox.addButton(tr("Cancelar Carregamento"), QMessageBox::RejectRole);
        msgBox.setDefaultButton(continueButton);

        msgBox.exec();

        if (msgBox.clickedButton() == continueButton) {
            proceedWithNewExplorerIp(m_publicIpAddress);
        } else {
            m_graphManager->clearGraph();
            if(m_mapView) m_mapView->clearMap();
            if(m_detailsPanel) m_detailsPanel->clearPanel();
            m_currentFilePath.clear();
            setWindowTitle(tr("Cartógrafo de Rede"));
            statusBar()->showMessage(tr("Carregamento do arquivo cancelado devido a conflito de IP."), 5000);
        }
    } else if (currentGraph.explorers.isEmpty() && !m_publicIpAddress.isEmpty()){
        qDebug() << "Nenhum explorador no arquivo. Adicionando IP público atual" << m_publicIpAddress << "como explorador.";
        proceedWithNewExplorerIp(m_publicIpAddress);
    }
}

void MainWindow::proceedWithNewExplorerIp(const QString& newIp) {
    ExplorerInfo newExplorer;
    newExplorer.id = newIp;
    newExplorer.label = newIp;

    m_graphManager->addOrUpdateExplorer(newExplorer);
    m_saveAction->setEnabled(true);

    statusBar()->showMessage(tr("IP %1 adicionado como explorador. Salve o arquivo para persistir.").arg(newIp), 5000);
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings(); // RNF02: Salvar configurações da UI ao fechar
    if (m_explorationThreads.size() > 0) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Exploração em Andamento"),
                                      tr("Há explorações de rede em andamento. Fechar agora irá cancelá-las. Deseja continuar?"),
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
        for(TracerouteTask* task : qAsConst(m_explorationTasks)) {
            task->cancel();
        }
    }
    QMainWindow::closeEvent(event);
}

// --- Slots para Ações de Exploração ---
void MainWindow::startNewExploration() {
    if (!m_publicIpAddress.isEmpty()) {
        NewExplorationDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            ExplorationSettings settings = dialog.getSettings();
            if (settings.targets.isEmpty()) {
                QMessageBox::warning(this, tr("Nenhum Alvo"), tr("Nenhum alvo foi especificado para a exploração."));
                return;
            }
            executeExplorationTasks(settings);
        }
    } else {
        QMessageBox::warning(this, tr("IP Desconhecido"), tr("Não foi possível determinar seu IP público. A exploração não pode começar. Verifique sua conexão com a internet."));
    }
}

void MainWindow::reExploreTargets() {
    GraphData currentGraph = m_graphManager->getCurrentGraphData();
    if (currentGraph.nodes.isEmpty()) {
        QMessageBox::information(this, tr("Mapa Vazio"), tr("Não há alvos para re-explorar no mapa atual."));
        return;
    }
    if (m_publicIpAddress.isEmpty()) {
        QMessageBox::warning(this, tr("IP Desconhecido"), tr("Não foi possível determinar seu IP público. A re-exploração não pode começar."));
        return;
    }

    ExplorationSettings settings;
    settings.params.maxHops = 30;
    settings.params.queriesPerHop = 3;
    settings.params.useIcmp = false;

    for (const Node& node : currentGraph.nodes.values()) {
        if (node.type == NodeType::Target) {
            settings.targets.append(node.id);
        }
    }

    if (settings.targets.isEmpty()) {
        QMessageBox::information(this, tr("Nenhum Alvo"), tr("Não foram encontrados nós do tipo 'alvo' no mapa atual para re-explorar."));
        return;
    }

    QString targetList = settings.targets.join(", ");
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Re-explorar Alvos"),
                                 tr("Você está prestes a re-explorar os seguintes alvos: %1.\nDeseja continuar?").arg(targetList),
                                 QMessageBox::Yes|QMessageBox::No);
    if(reply == QMessageBox::Yes) {
        executeExplorationTasks(settings);
    }
}

void MainWindow::executeExplorationTasks(const ExplorationSettings& settings) {
    if (m_explorationThreads.size() > 0) {
        QMessageBox::information(this, tr("Ocupado"), tr("Outra exploração já está em andamento. Por favor, aguarde."));
        return;
    }

    m_totalTasksToRun = settings.targets.size();
    m_tasksCompleted = 0;

    if (!m_globalExplorationProgressDialog) {
        m_globalExplorationProgressDialog = new QProgressDialog(tr("Iniciando exploração..."), tr("Cancelar Todas"), 0, m_totalTasksToRun, this);
        m_globalExplorationProgressDialog->setWindowTitle(tr("Progresso da Exploração"));
        m_globalExplorationProgressDialog->setWindowModality(Qt::WindowModal);
        m_globalExplorationProgressDialog->setAutoClose(true);
        m_globalExplorationProgressDialog->setAutoReset(true);
        connect(m_globalExplorationProgressDialog, &QProgressDialog::canceled, this, [this](){
            statusBar()->showMessage(tr("Cancelando todas as explorações..."), 3000);
            for(TracerouteTask* task : qAsConst(m_explorationTasks)) {
                task->cancel();
            }
        });
    }
    m_globalExplorationProgressDialog->setLabelText(tr("Iniciando exploração para %1 alvo(s)...").arg(m_totalTasksToRun));
    m_globalExplorationProgressDialog->setMaximum(m_totalTasksToRun);
    m_globalExplorationProgressDialog->setValue(0);
    m_globalExplorationProgressDialog->show();

    setUiForExplorationState(true);

    for (const QString& target : settings.targets) {
        TracerouteParams currentParams = settings.params;
        currentParams.target = target;

        TracerouteTask *task = new TracerouteTask(currentParams, m_publicIpAddress);
        QThread *thread = new QThread;
        task->moveToThread(thread);

        connect(task, &TracerouteTask::taskStarted, this, &MainWindow::onTracerouteTaskStarted);
        connect(task, &TracerouteTask::progressUpdate, this, &MainWindow::onTracerouteProgressUpdate);
        connect(task, &TracerouteTask::taskFinished, this, &MainWindow::onTracerouteTaskFinished);
        connect(task, &TracerouteTask::taskError, this, &MainWindow::onTracerouteTaskError);
        connect(task, &TracerouteTask::taskCancelled, this, &MainWindow::onTracerouteTaskCancelled);

        connect(thread, &QThread::started, task, &TracerouteTask::start);
        connect(task, &TracerouteTask::taskFinished, thread, &QThread::quit);
        connect(task, &TracerouteTask::taskError, thread, &QThread::quit);
        connect(task, &TracerouteTask::taskCancelled, thread, &QThread::quit);

        connect(thread, &QThread::finished, task, &QObject::deleteLater);
        connect(thread, &QThread::finished, this, &MainWindow::onTracerouteThreadFinished);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);


        m_explorationThreads.append(thread);
        m_explorationTasks.append(task);
        thread->start();
    }
}

void MainWindow::setUiForExplorationState(bool isExploring) {
    if (m_newExplorationAction) m_newExplorationAction->setEnabled(!isExploring);
    if (m_reExploreAction) m_reExploreAction->setEnabled(!isExploring && (!m_graphManager->getCurrentGraphData().nodes.isEmpty()));

    for (QAction *action : menuBar()->findChild<QMenu*>(tr("&Arquivo"))->actions()) {
        if (action != m_saveAction && action != m_saveAsAction && !action->menu() && !action->isSeparator()) {
             if(action->text() != tr("Sai&r")) action->setEnabled(!isExploring);
        }
    }
    bool enableMapControls = !isExploring && (!m_graphManager->getCurrentGraphData().nodes.isEmpty());
    if(m_zoomInButton) m_zoomInButton->setEnabled(enableMapControls);
    if(m_zoomOutButton) m_zoomOutButton->setEnabled(enableMapControls);
    if(m_fitToScreenButton) m_fitToScreenButton->setEnabled(enableMapControls);
    if(m_reorganizeLayoutButton) m_reorganizeLayoutButton->setEnabled(enableMapControls);

    if (isExploring) {
        setCursor(Qt::BusyCursor);
    } else {
        unsetCursor();
        if (m_globalExplorationProgressDialog && m_globalExplorationProgressDialog->isVisible()) {
            m_globalExplorationProgressDialog->setValue(m_totalTasksToRun);
        }
        statusBar()->showMessage(tr("Pronto."), 3000);
    }
}


// --- Slots para TracerouteTask ---
void MainWindow::onTracerouteTaskStarted(const QString& target) {
    qDebug() << "Traceroute iniciado para:" << target;
    statusBar()->showMessage(tr("Iniciando traceroute para %1...").arg(target));
    if (m_globalExplorationProgressDialog) {
         m_globalExplorationProgressDialog->setLabelText(tr("Explorando %1 (%2 de %3)...")
                                                        .arg(target)
                                                        .arg(m_tasksCompleted + 1)
                                                        .arg(m_totalTasksToRun));
    }
}

void MainWindow::onTracerouteProgressUpdate(const QString& target, const QString& message, int hopCount, int currentHop) {
    statusBar()->showMessage(tr("Traceroute para %1: %2 (Salto %3/%4)")
                             .arg(target, message, QString::number(currentHop), QString::number(hopCount)));
}

void MainWindow::onTracerouteHopDiscovered(const QString& target, int ttl, const QString& ip, const QString& hostname, const QList<double>& latencies) {
    // Silenciado para não poluir demais o log/statusbar
}

void MainWindow::onTracerouteGhostNodeInferred(const QString& target, int ttlStart, int ttlEnd) {
    // Silenciado
}

void MainWindow::onTracerouteTaskFinished(const QString& target, const GraphData& resultData) {
    qDebug() << "Traceroute para" << target << "finalizado com sucesso.";
    statusBar()->showMessage(tr("Traceroute para %1 concluído.").arg(target), 3000);

    m_graphManager->concatenateExplorationData(resultData);
    m_saveAction->setEnabled(true);

    m_tasksCompleted++;
    if (m_globalExplorationProgressDialog) {
        m_globalExplorationProgressDialog->setValue(m_tasksCompleted);
    }

    for(int i=0; i < m_explorationTasks.size(); ++i) {
        if(m_explorationTasks.at(i)->getTarget() == target) {
            m_explorationTasks.removeAt(i);
            break;
        }
    }
}

void MainWindow::onTracerouteTaskError(const QString& target, const QString& errorMessage) {
    qWarning() << "Erro no traceroute para" << target << ":" << errorMessage;
    QMessageBox::warning(this, tr("Erro de Exploração"), tr("Falha no traceroute para %1: %2").arg(target, errorMessage));
    statusBar()->showMessage(tr("Erro no traceroute para %1.").arg(target), 5000);

    m_tasksCompleted++;
    if (m_globalExplorationProgressDialog) {
        m_globalExplorationProgressDialog->setValue(m_tasksCompleted);
    }
    for(int i=0; i < m_explorationTasks.size(); ++i) {
        if(m_explorationTasks.at(i)->getTarget() == target) {
            m_explorationTasks.removeAt(i);
            break;
        }
    }
}

void MainWindow::onTracerouteTaskCancelled(const QString& target) {
    qDebug() << "Traceroute para" << target << "foi cancelado.";
    statusBar()->showMessage(tr("Traceroute para %1 cancelado.").arg(target), 3000);

    m_tasksCompleted++;
    if (m_globalExplorationProgressDialog) {
        m_globalExplorationProgressDialog->setValue(m_tasksCompleted);
    }
     for(int i=0; i < m_explorationTasks.size(); ++i) {
        if(m_explorationTasks.at(i)->getTarget() == target) {
            m_explorationTasks.removeAt(i);
            break;
        }
    }
}

void MainWindow::onTracerouteThreadFinished() {
    QThread* finishedThread = qobject_cast<QThread*>(sender());
    if (finishedThread) {
        m_explorationThreads.removeAll(finishedThread);
        qDebug() << "Thread de exploração finalizada e removida da lista. Threads restantes:" << m_explorationThreads.size();
    }
    if (m_explorationThreads.isEmpty()) {
        qDebug() << "Todas as threads de exploração foram concluídas.";
        setUiForExplorationState(false);
        m_totalTasksToRun = 0;
        m_tasksCompleted = 0;
    }
}


// Slots para os botões de controle do MapView
void MainWindow::onZoomInClicked()
{
    if(m_mapView) m_mapView->zoomIn();
}

void MainWindow::onZoomOutClicked()
{
    if(m_mapView) m_mapView->zoomOut();
}

void MainWindow::onFitToScreenClicked()
{
    if(m_mapView) m_mapView->fitToScreen();
}

void MainWindow::onReorganizeLayoutClicked()
{
    if(m_mapView) m_mapView->начальная компоновка();
}

void MainWindow::onMapViewItemSelected(const QVariant& itemData)
{
    if (m_detailsPanel) {
        m_detailsPanel->displayItemData(itemData);
    }

    if (itemData.isValid() && itemData.typeId() == QMetaType::QJsonObject) {
        QJsonObject data = itemData.toJsonObject();
        QString itemType = data.value("item_type").toString();
        QString id = data.value("id").toString();
        statusBar()->showMessage(tr("Item selecionado: %1 %2").arg(itemType, id), 3000);
    } else {
        statusBar()->showMessage(tr("Seleção limpa."), 3000);
    }
}
