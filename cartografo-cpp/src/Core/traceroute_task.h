#ifndef TRACEROUTETASK_H
#define TRACEROUTETASK_H

#include <QObject>
#include <QString>
#include <QProcess>
#include "data_types.h" // Para GraphData, Node, Edge

// Parâmetros para a tarefa de traceroute
struct TracerouteParams {
    QString target; // IP ou hostname
    int maxHops = 30;
    int queriesPerHop = 3; // Número de pacotes por salto
    bool useIcmp = false;  // Usar ICMP (requer root em alguns sistemas)
    // Outros parâmetros como timeout, tamanho do pacote, etc., podem ser adicionados
};

class TracerouteTask : public QObject
{
    Q_OBJECT

public:
    explicit TracerouteTask(const TracerouteParams& params, const QString& localExplorerIp, QObject *parent = nullptr);
    ~TracerouteTask();

    // Inicia a execução da tarefa de traceroute.
    // Esta função deve ser chamada para iniciar o processo.
    Q_INVOKABLE void start();

    // Cancela a tarefa de traceroute se estiver em execução.
    Q_INVOKABLE void cancel();

    // Retorna os dados do grafo resultante desta exploração específica.
    GraphData getExplorationResults() const;

    QString getTarget() const { return m_params.target; }

signals:
    // Emitido quando a tarefa é iniciada.
    void taskStarted(const QString& target);

    // Emitido para atualizar o progresso (ex: hop atual / total hops).
    // Pode ser uma string descritiva ou um percentual.
    void progressUpdate(const QString& target, const QString& message, int hopCount, int currentHop);

    // Emitido quando um novo hop é descoberto.
    void hopDiscovered(const QString& target, int ttl, const QString& ip, const QString& hostname, const QList<double>& latencies);

    // Emitido quando um nó fantasma é inferido.
    void ghostNodeInferred(const QString& target, int ttlStart, int ttlEnd);

    // Emitido quando a tarefa é concluída com sucesso.
    // Retorna o GraphData resultante da exploração para este alvo.
    void taskFinished(const QString& target, const GraphData& resultData);

    // Emitido se ocorrer um erro durante a tarefa.
    void taskError(const QString& target, const QString& errorMessage);

    // Emitido se a tarefa for cancelada.
    void taskCancelled(const QString& target);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();

private:
    void parseTracerouteOutput(const QString& outputLine);
    void finalizeExtraction(); // Chamado quando o processo termina ou é cancelado

    TracerouteParams m_params;
    QString m_localExplorerIp; // IP do nó explorador local
    QProcess *m_process;
    GraphData m_resultGraphData; // Contém apenas os nós e arestas desta exploração

    QString m_currentProcessingIp; // Para lidar com múltiplas linhas de latência para o mesmo IP
    QString m_currentHostname;
    QList<double> m_currentLatencies;
    int m_currentTtl;

    // Para detecção de nós fantasmas
    int m_consecutiveTimeouts;
    int m_lastSuccessfulTtl;
    QString m_previousHopIp; // IP do último hop bem-sucedido

    bool m_isCancelled;
    bool m_hasStarted;

    // Função utilitária para adicionar nós e arestas ao m_resultGraphData
    void addHopToResults(int ttl, const QString& ip, const QString& hostname, const QList<double>& latencies, bool isTarget = false);
    void addGhostNodeToResults(int startTtl, int endTtl, const QString& fromIp, const QString& toIp);
};

#endif // TRACEROUTETASK_H
