#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QThread> // Para executar TracerouteTask
#include <QProgressDialog> // Para feedback geral de exploração

// Forward declarations
namespace Ui {
class MainWindow;
}
class GraphManager;
class MapView;
class DetailsPanel;
class GeoIPClient;
class QPushButton;
class TracerouteTask; // Da parte Core
struct ExplorationSettings; // Do NewExplorationDialog

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;


private slots:
    // Arquivo
    void openFile();
    void saveFile();
    void saveFileAs();
    void exitApplication();

    // Exploração Menu
    void startNewExploration();
    void reExploreTargets();

    // WelcomeDialog
    void handleOpenExistingMapRequested();
    void handleNewMapRequested();

    // GraphManager
    void onGraphChanged();
    void onGraphManagerProgress(const QString& message, int value);
    void onGraphManagerError(const QString& errorMessage);

    // GeoIPClient
    void onPublicIpDetected(const QString& ip);
    void onGeoIPError(const QString& requestType, const QString& errorString);

    // MapView & Controles
    void onMapViewItemSelected(const QVariant& itemData);
    void onZoomInClicked();
    void onZoomOutClicked();
    void onFitToScreenClicked();
    void onReorganizeLayoutClicked();

    // Slots para TracerouteTask
    void onTracerouteTaskStarted(const QString& target);
    void onTracerouteProgressUpdate(const QString& target, const QString& message, int hopCount, int currentHop);
    void onTracerouteHopDiscovered(const QString& target, int ttl, const QString& ip, const QString& hostname, const QList<double>& latencies);
    void onTracerouteGhostNodeInferred(const QString& target, int ttlStart, int ttlEnd);
    void onTracerouteTaskFinished(const QString& target, const GraphData& resultData);
    void onTracerouteTaskError(const QString& target, const QString& errorMessage);
    void onTracerouteTaskCancelled(const QString& target);
    void onTracerouteThreadFinished(); // Para limpar QThread


private:
    void setupMenuBar();
    void setupCoreComponents();
    void setupMainLayout();
    void setupOverlayButtons();
    void positionOverlayButtons();
    void showWelcomeDialog();

    void checkExplorerIdentity();
    void proceedWithNewExplorerIp(const QString& newIp);

    void executeExplorationTasks(const ExplorationSettings& settings);
    void setUiForExplorationState(bool isExploring); // Centraliza o estado da UI

    Ui::MainWindow *ui;
    GraphManager *m_graphManager;
    GeoIPClient *m_geoIPClient;
    QString m_currentFilePath;
    QString m_publicIpAddress;

    MapView *m_mapView;
    DetailsPanel *m_detailsPanel;

    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_newExplorationAction; // Ação para nova exploração
    QAction *m_reExploreAction;     // Ação para re-explorar

    QPushButton *m_zoomInButton;
    QPushButton *m_zoomOutButton;
    QPushButton *m_fitToScreenButton;
    QPushButton *m_reorganizeLayoutButton;

    // Gerenciamento de tarefas de exploração
    QList<QThread*> m_explorationThreads; // Lista de threads ativas
    QList<TracerouteTask*> m_explorationTasks; // Lista de tarefas ativas
    int m_totalTasksToRun;
    int m_tasksCompleted;
    QProgressDialog *m_globalExplorationProgressDialog;
};

#endif // MAINWINDOW_H
