#include "traceroute_task.h"
#include <QDebug>
#include <QRegularExpression>
#include <QHostInfo> // Para resolver hostname se necessário

// Expressão regular para parsear a saída do traceroute (formato comum do Linux)
// Exemplo: " 1  192.168.1.1 (192.168.1.1)  0.528 ms  0.439 ms  0.398 ms"
// Exemplo: " 2  some.host.com (10.0.0.1)  10.123 ms  *  12.321 ms"
// Exemplo: " 3  * * *" (timeout)
// Captura: TTL, Hostname (opcional), IP, Latências (até 3)
// A RE pode precisar de ajustes dependendo da versão exata do traceroute e suas opções.
// Esta RE é um ponto de partida.
const QRegularExpression TRACEROUTE_LINE_RE(
    "^\\s*(\\d+)\\s+"                          // 1: TTL
    "(?:([\\w\\.-]+)\\s+\\(([\\d\\.]+)\\)|"   // 2: Hostname (opcional), 3: IP Address
    "([\\d\\.]+))"                           // 4: IP Address (se hostname não estiver presente)
    "\\s*(?:(\\d+\\.\\d+)\\s+ms)?"             // 5: Latency 1 (opcional)
    "\\s*(?:(\\d+\\.\\d+)\\s+ms)?"             // 6: Latency 2 (opcional)
    "\\s*(?:(\\d+\\.\\d+)\\s+ms)?"             // 7: Latency 3 (opcional)
    "|(\\s*\\d+\\s+\\*\\s*\\*\\s*\\*)"         // 8: Timeout line like " 1  * * *"
);

// Para linhas onde apenas o IP é retornado sem hostname inicialmente, e depois o hostname aparece.
// Ex: " 1  192.168.1.1  0.528 ms  0.439 ms  0.398 ms"
const QRegularExpression TRACEROUTE_LINE_IP_ONLY_RE(
    "^\\s*(\\d+)\\s+"                          // 1: TTL
    "([\\d\\.]+)"                            // 2: IP Address
    "\\s*(?:(\\d+\\.\\d+)\\s+ms)?"             // 3: Latency 1 (opcional)
    "\\s*(?:(\\d+\\.\\d+)\\s+ms)?"             // 4: Latency 2 (opcional)
    "\\s*(?:(\\d+\\.\\d+)\\s+ms)?"             // 5: Latency 3 (opcional)
);


TracerouteTask::TracerouteTask(const TracerouteParams& params, const QString& localExplorerIp, QObject *parent)
    : QObject(parent),
      m_params(params),
      m_localExplorerIp(localExplorerIp),
      m_process(nullptr),
      m_currentTtl(0),
      m_consecutiveTimeouts(0),
      m_lastSuccessfulTtl(0),
      m_isCancelled(false),
      m_hasStarted(false)
{
    // Inicializa o nó explorador nos resultados
    Node explorerNode;
    explorerNode.id = m_localExplorerIp;
    explorerNode.label = m_localExplorerIp; // Pode ser melhorado com um lookup reverso de DNS ou GeoIP
    explorerNode.type = NodeType::Explorer;
    explorerNode.group = "explorer";
    m_resultGraphData.nodes.insert(explorerNode.id, explorerNode);
    m_resultGraphData.fileVersion = "0.6.0"; // Ou a versão que será usada
    m_resultGraphData.timestamp = QDateTime::currentDateTime();

    ExplorerInfo exInfo;
    exInfo.id = m_localExplorerIp;
    exInfo.label = m_localExplorerIp;
    m_resultGraphData.explorers.insert(exInfo.id, exInfo);
}

TracerouteTask::~TracerouteTask()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill(); // Garante que o processo seja terminado
            m_process->waitForFinished(1000); // Espera um pouco
        }
        delete m_process;
        m_process = nullptr;
    }
}

void TracerouteTask::start()
{
    if (m_hasStarted) {
        qWarning() << "TracerouteTask para" << m_params.target << "já foi iniciado.";
        return;
    }
    m_hasStarted = true;
    m_isCancelled = false;
    m_consecutiveTimeouts = 0;
    m_lastSuccessfulTtl = 0;
    m_previousHopIp = m_localExplorerIp; // O primeiro "salto" é do explorador

    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &TracerouteTask::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &TracerouteTask::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &TracerouteTask::onProcessErrorOccurred);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TracerouteTask::onProcessReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &TracerouteTask::onProcessReadyReadStandardError);

    QStringList arguments;
    // Opções comuns do traceroute (Linux). Podem precisar de ajuste para outros SOs.
    // -n: Não resolve IPs para hostnames (fazemos isso seletivamente ou depois)
    // -q <queries>: Número de pacotes por salto
    // -m <max_hops>: Máximo de saltos
    // -w <timeout_seconds_float>: Tempo de espera por resposta (ex: 2.0)
    // -I: Usar pacotes ICMP ECHO (geralmente requer root)
    // -T: Usar pacotes TCP SYN (pode passar por firewalls, às vezes requer root)
    // O padrão é UDP.

    arguments << "-n"; // Começamos sem resolução DNS para obter IPs rapidamente.
                      // A resolução pode ser feita depois se necessário.
    arguments << "-q" << QString::number(m_params.queriesPerHop);
    arguments << "-m" << QString::number(m_params.maxHops);
    arguments << "-w" << "3"; // Timeout de 3 segundos por pacote (ajustar conforme necessário)

    if (m_params.useIcmp) {
        arguments << "-I"; // ICMP
    }
    // Adicionar -T para TCP se for uma opção desejada.

    arguments << m_params.target;

    qDebug() << "Iniciando traceroute para" << m_params.target << "com argumentos:" << arguments.join(" ");

    // Adiciona o nó alvo inicial ao grafo de resultados
    // Isso é importante para que o alvo exista mesmo que não seja alcançável
    Node targetNode;
    targetNode.id = m_params.target; // Se for um hostname, será resolvido pelo traceroute ou por nós
    targetNode.label = m_params.target;
    targetNode.type = NodeType::Target;
    targetNode.group = "target";
    m_resultGraphData.nodes.insert(targetNode.id, targetNode);

    m_process->start("traceroute", arguments);
}

void TracerouteTask::cancel()
{
    if (!m_hasStarted || m_isCancelled) {
        return;
    }
    m_isCancelled = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "Cancelando traceroute para" << m_params.target;
        m_process->kill(); // Envia SIGTERM, depois SIGKILL se não terminar
    }
    // finalizeExtraction() será chamado pelo onProcessFinished ou onProcessErrorOccurred
    // emit taskCancelled(m_params.target); // Emitir aqui ou em finalizeExtraction
}

GraphData TracerouteTask::getExplorationResults() const
{
    return m_resultGraphData;
}

void TracerouteTask::onProcessStarted()
{
    qDebug() << "Processo traceroute iniciado para" << m_params.target;
    emit taskStarted(m_params.target);
    emit progressUpdate(m_params.target, tr("Traceroute iniciado..."), m_params.maxHops, 0);
}

void TracerouteTask::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Processo traceroute para" << m_params.target << "finalizado. Código de saída:" << exitCode << "Status:" << exitStatus;

    // Processar qualquer saída restante
    QString remainingOutput = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    for (const QString& line : remainingOutput.split('\n', Qt::SkipEmptyParts)) {
        parseTracerouteOutput(line.trimmed());
    }
    QString errorOutput = QString::fromLocal8Bit(m_process->readAllStandardError());
    if(!errorOutput.isEmpty()){
        qDebug() << "Traceroute stderr for" << m_params.target << ":" << errorOutput;
        // Não necessariamente um erro fatal, pode ser um aviso.
    }

    finalizeExtraction(); // Processa o último hop/timeouts e emite sinais finais

    if (m_isCancelled) {
        // O sinal taskCancelled já deve ter sido emitido por finalizeExtraction
    } else if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        // Sucesso (embora traceroute possa retornar != 0 e ainda ter dados úteis)
        // emit taskFinished(m_params.target, m_resultGraphData); // Movido para finalizeExtraction
    } else if (exitStatus == QProcess::CrashExit) {
        emit taskError(m_params.target, tr("O processo traceroute falhou (crash)."));
    } else {
         // Mesmo com exitCode != 0, podemos ter dados parciais.
         // O taskFinished é emitido em finalizeExtraction se não cancelado.
         // Se não houve dados e houve erro, taskError também pode ser emitido lá.
        // qWarning() << "Traceroute para" << m_params.target << "concluído com código de erro:" << exitCode;
        // emit taskError(m_params.target, tr("Traceroute concluído com código de erro: %1").arg(exitCode));
    }
}

void TracerouteTask::onProcessErrorOccurred(QProcess::ProcessError error)
{
    if (m_isCancelled && error == QProcess::Killed) {
        qDebug() << "Processo traceroute para" << m_params.target << "foi morto (cancelado).";
        finalizeExtraction(); // Garante que o estado seja limpo e o sinal de cancelamento seja emitido
        return;
    }

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart: errorMsg = tr("Falha ao iniciar o processo traceroute. Verifique se está instalado e no PATH."); break;
        case QProcess::Crashed: errorMsg = tr("O processo traceroute travou."); break;
        case QProcess::Timedout: errorMsg = tr("Timeout ao executar o traceroute."); break; // Timeout do QProcess, não do traceroute em si
        case QProcess::ReadError: errorMsg = tr("Erro de leitura na saída do traceroute."); break;
        case QProcess::WriteError: errorMsg = tr("Erro de escrita para o traceroute."); break;
        case QProcess::UnknownError: default: errorMsg = tr("Erro desconhecido com o processo traceroute."); break;
    }
    qWarning() << "Erro no processo traceroute para" << m_params.target << ":" << errorMsg << "(QProcess::ProcessError code" << error << ")";
    finalizeExtraction(); // Tenta finalizar com o que tem
    emit taskError(m_params.target, errorMsg);
}

void TracerouteTask::onProcessReadyReadStandardOutput()
{
    while (m_process->canReadLine()) {
        QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (!line.isEmpty()) {
            parseTracerouteOutput(line);
        }
    }
}

void TracerouteTask::onProcessReadyReadStandardError()
{
    QString errorOutput = QString::fromLocal8Bit(m_process->readAllStandardError());
    if (!errorOutput.isEmpty()) {
        qWarning() << "Saída de erro do traceroute para" << m_params.target << ":" << errorOutput.trimmed();
        // Pode ser um aviso ou um erro. Não necessariamente fatal.
        // emit taskError(m_params.target, tr("Erro do traceroute: %1").arg(errorOutput.trimmed()));
    }
}

void TracerouteTask::parseTracerouteOutput(const QString& outputLine)
{
    if (outputLine.isEmpty() || m_isCancelled) return;
    qDebug() << "Traceroute output for" << m_params.target << ":" << outputLine;

    QRegularExpressionMatch match = TRACEROUTE_LINE_RE.match(outputLine);
    if (!match.hasMatch()) {
        match = TRACEROUTE_LINE_IP_ONLY_RE.match(outputLine); // Tenta a RE mais simples
    }

    if (match.hasMatch()) {
        int ttl = match.captured(1).toInt();
        m_currentTtl = ttl; // Atualiza o TTL atual

        // Se houve timeouts antes deste hop bem-sucedido
        if (m_consecutiveTimeouts > 0) {
            if (m_consecutiveTimeouts >= 1) { // Considerar 1 ou mais timeouts como fantasma
                 // O nó fantasma vai do m_previousHopIp até o IP atual (match.captured(3) ou match.captured(4))
                 // O TTL inicial do fantasma é m_lastSuccessfulTtl + 1
                 // O TTL final do fantasma é ttl -1
                QString currentHopIp = match.captured(3).isEmpty() ? match.captured(4) : match.captured(3);
                if (m_lastSuccessfulTtl + 1 <= ttl -1 ) {
                    addGhostNodeToResults(m_lastSuccessfulTtl + 1, ttl - 1, m_previousHopIp, currentHopIp);
                    emit ghostNodeInferred(m_params.target, m_lastSuccessfulTtl + 1, ttl -1);
                }
            }
            m_consecutiveTimeouts = 0; // Reseta contador de timeouts
        }

        QString hostname = match.captured(2);
        QString ip = match.captured(3);
        if (ip.isEmpty()) ip = match.captured(4); // Caso da segunda alternativa da RE principal ou da RE_IP_ONLY

        if (ip.isEmpty() && match.captured(8).isEmpty()) { // Não é timeout, mas não capturou IP
             qDebug() << "Linha não reconhecida (sem IP e não é timeout total):" << outputLine;
             return;
        }

        // Verifica se é um timeout total para este TTL (ex: " 1  * * *")
        if (!match.captured(8).isEmpty() || (ip.isEmpty() && hostname.isEmpty() && match.captured(5).isEmpty())) { // Linha de timeout "X * * *" ou linha sem IP/hostname/latencia
            m_consecutiveTimeouts++;
            emit progressUpdate(m_params.target, tr("Timeout no salto %1").arg(ttl), m_params.maxHops, ttl);
            // m_previousHopIp não muda aqui, continua sendo o último IP bem-sucedido
            return; // Pula para a próxima linha
        }


        QList<double> latencies;
        if (match.hasCaptured(5) && !match.captured(5).isEmpty()) latencies.append(match.captured(5).toDouble());
        if (match.hasCaptured(6) && !match.captured(6).isEmpty()) latencies.append(match.captured(6).toDouble());
        if (match.hasCaptured(7) && !match.captured(7).isEmpty()) latencies.append(match.captured(7).toDouble());
        // Para TRACEROUTE_LINE_IP_ONLY_RE, os índices de captura são diferentes para latências
        if (latencies.isEmpty()) {
            if (match.hasCaptured(3) && TRACEROUTE_LINE_IP_ONLY_RE.match(outputLine).hasMatch()) { // Verifica se foi a IP_ONLY_RE
                 if (!match.captured(3).isEmpty() && match.captured(3).contains('.')) latencies.append(match.captured(3).toDouble()); // Lat1 da IP_ONLY
                 if (match.hasCaptured(4) && !match.captured(4).isEmpty() && match.captured(4).contains('.')) latencies.append(match.captured(4).toDouble()); // Lat2 da IP_ONLY
                 if (match.hasCaptured(5) && !match.captured(5).isEmpty() && match.captured(5).contains('.')) latencies.append(match.captured(5).toDouble()); // Lat3 da IP_ONLY
            }
        }


        if (hostname.isEmpty() && !ip.isEmpty()) {
            // Tentar resolver o hostname se não veio do traceroute (opcional, pode ser lento)
            // QHostInfo::lookupHost(ip, this, [this, ttl, ip, latencies](const QHostInfo &host){ ... });
            // Por ora, usamos o IP como label se o hostname for vazio.
            hostname = ip;
        }

        // Se o IP for igual ao do explorador, não adiciona como hop novo, mas pode ser o alvo.
        bool isTargetReached = (ip == m_params.target || hostname == m_params.target);
        if (ip == m_localExplorerIp && ttl > 1) { // Evita self-loop no primeiro hop se o primeiro hop for o próprio router
            // Não faz nada especial, a menos que seja o alvo
        }

        addHopToResults(ttl, ip, hostname, latencies, isTargetReached);
        emit hopDiscovered(m_params.target, ttl, ip, hostname, latencies);
        emit progressUpdate(m_params.target, tr("Salto %1: %2 (%3)").arg(ttl).arg(ip).arg(hostname), m_params.maxHops, ttl);

        m_lastSuccessfulTtl = ttl;
        m_previousHopIp = ip; // Atualiza o último IP bem-sucedido

        if (isTargetReached) {
            qDebug() << "Alvo" << m_params.target << "alcançado.";
            // O processo traceroute pode continuar por mais alguns saltos dependendo da implementação.
            // Podemos optar por cancelar o processo aqui se desejado.
            // cancel(); // Descomente se quiser parar ao atingir o alvo.
        }

    } else {
        // Linha não reconhecida, pode ser um cabeçalho ou uma mensagem de erro do traceroute.
        // Ex: "traceroute to google.com (172.217.10.142), 30 hops max, 60 byte packets"
        if (outputLine.contains("hops max", Qt::CaseInsensitive) || outputLine.contains("byte packets", Qt::CaseInsensitive)) {
            // É uma linha de cabeçalho, ignorar.
        } else {
            qDebug() << "Linha de saída do traceroute não reconhecida para" << m_params.target << ":" << outputLine;
        }
    }
}

void TracerouteTask::addHopToResults(int ttl, const QString& ip, const QString& hostname, const QList<double>& latencies, bool isTarget)
{
    if (ip.isEmpty()) return;

    // Adiciona/Atualiza o nó do hop
    Node hopNode;
    if (m_resultGraphData.nodes.contains(ip)) {
        hopNode = m_resultGraphData.nodes.value(ip);
    } else {
        hopNode.id = ip;
    }
    hopNode.label = hostname.isEmpty() ? ip : hostname;
    if (!hostname.isEmpty() && !hopNode.fqdns.contains(hostname)) hopNode.fqdns.append(hostname);


    // Se este IP é o alvo da exploração, marca como Target.
    // Se o target original era um hostname, o IP resolvido se torna o ID do nó alvo.
    if (isTarget || ip == m_params.target || hostname == m_params.target) {
        hopNode.type = NodeType::Target;
        hopNode.group = "target";
        // Se o m_params.target era um hostname e 'ip' é sua resolução,
        // precisamos garantir que o nó alvo original seja atualizado ou substituído.
        if (m_params.target != ip && m_resultGraphData.nodes.contains(m_params.target)) {
            Node originalTargetNode = m_resultGraphData.nodes.take(m_params.target); // Remove o placeholder
            hopNode.label = originalTargetNode.label; // Mantém o label original se era um hostname
        }
        m_resultGraphData.nodes[ip] = hopNode; // Garante que o nó alvo tenha o IP como ID.
    } else if (ip == m_localExplorerIp) {
        hopNode.type = NodeType::Explorer; // Caso o primeiro hop seja o próprio explorador
        hopNode.group = "explorer";
    } else {
        // Só define como Hop se não for Target ou Explorer
        if (hopNode.type == NodeType::Unknown) { // Não sobrescreve se já é Target ou Explorer
            hopNode.type = NodeType::Hop;
            hopNode.group = "hop";
        }
    }
    m_resultGraphData.nodes[ip] = hopNode;


    // Adiciona a aresta do hop anterior para este hop
    if (ttl > 0 && !m_previousHopIp.isEmpty() && m_previousHopIp != ip) {
        Edge edge;
        edge.id = m_previousHopIp + "_" + ip;
        edge.from = m_previousHopIp;
        edge.to = ip;

        if (m_resultGraphData.edges.contains(edge.id)) { // Aresta já existe
            Edge& existingEdge = m_resultGraphData.edges[edge.id];
            existingEdge.latencies.append(latencies);
            // Recalcular média
            double sum = 0;
            for(double l : existingEdge.latencies) sum += l;
            if (!existingEdge.latencies.isEmpty()) existingEdge.averageLatency = sum / existingEdge.latencies.size();

        } else { // Nova aresta
            edge.latencies = latencies;
            if (!latencies.isEmpty()) {
                double sum = 0;
                for(double l : latencies) sum += l;
                edge.averageLatency = sum / latencies.size();
            }
            m_resultGraphData.edges.insert(edge.id, edge);
        }
    }
}

void TracerouteTask::addGhostNodeToResults(int startTtl, int endTtl, const QString& fromIp, const QString& toIp)
{
    if (fromIp.isEmpty() || toIp.isEmpty() || startTtl > endTtl) return;

    QString ghostNodeId = QString("ghost_%1_ttl%2-%3").arg(fromIp).arg(startTtl).arg(endTtl);
    Node ghostNode;
    ghostNode.id = ghostNodeId;
    ghostNode.label = tr("Nó Fantasma (TTL %1-%2)").arg(startTtl).arg(endTtl);
    ghostNode.type = NodeType::Ghost;
    ghostNode.group = "ghost";
    m_resultGraphData.nodes.insert(ghostNodeId, ghostNode);

    // Aresta do último hop conhecido para o nó fantasma
    Edge edgeToGhost;
    edgeToGhost.id = fromIp + "_" + ghostNodeId;
    edgeToGhost.from = fromIp;
    edgeToGhost.to = ghostNodeId;
    // Latência para nós fantasmas é tipicamente indefinida ou muito alta, representamos como 0 ou um valor especial.
    edgeToGhost.latencies.append(-1); // Usar -1 para indicar timeout/fantasma
    edgeToGhost.averageLatency = -1;
    m_resultGraphData.edges.insert(edgeToGhost.id, edgeToGhost);

    // Aresta do nó fantasma para o próximo hop conhecido
    Edge edgeFromGhost;
    edgeFromGhost.id = ghostNodeId + "_" + toIp;
    edgeFromGhost.from = ghostNodeId;
    edgeFromGhost.to = toIp;
    edgeFromGhost.latencies.append(-1);
    edgeFromGhost.averageLatency = -1;
    m_resultGraphData.edges.insert(edgeFromGhost.id, edgeFromGhost);

    qDebug() << "Nó fantasma adicionado:" << ghostNodeId << "entre" << fromIp << "e" << toIp;
}


void TracerouteTask::finalizeExtraction()
{
    // Lida com timeouts consecutivos no final do traceroute
    if (m_consecutiveTimeouts > 0) {
        // Se o último evento foi uma série de timeouts, e o alvo não foi alcançado,
        // podemos inferir um nó fantasma até o "infinito" ou até o max_hops.
        // Aqui, vamos apenas registrar que houve timeouts.
        // A lógica de criar um nó fantasma "final" pode ser mais complexa.
        // Por ora, o último previousHopIp é o último nó conhecido.
        // Se o alvo (m_params.target) não está entre os nós com IP, ele permanece como um nó isolado.

        // Se m_previousHopIp não for o alvo, e houve timeouts,
        // cria um fantasma do m_previousHopIp até um "fantasma final"
        // if (m_previousHopIp != m_params.target && !m_resultGraphData.nodes.contains(m_params.target)) {
        //     // Cria um nó fantasma para representar os timeouts finais
        //      addGhostNodeToResults(m_lastSuccessfulTtl + 1, m_params.maxHops, m_previousHopIp, "TARGET_UNREACHED_GHOST");
        // }
        qDebug() << m_consecutiveTimeouts << "timeouts consecutivos no final do traceroute para" << m_params.target;
    }

    // Garante que o nó alvo (se era um hostname e foi resolvido) tenha o tipo Target
    // O ID do nó alvo pode ter mudado de hostname para IP durante o parse.
    // Precisamos encontrar o nó que representa o alvo.
    bool targetFoundAsNode = false;
    Node targetNodeData;
    for(const Node& node : m_resultGraphData.nodes.values()){
        if(node.label == m_params.target || node.id == m_params.target || (node.fqdns.contains(m_params.target) && m_params.target.contains('.')) ){ // Checa label, id e FQDNs
            if(node.type == NodeType::Target){
                targetFoundAsNode = true;
                targetNodeData = node;
                break;
            }
        }
    }
    // Se o alvo foi especificado como IP e esse IP está nos nós, marca-o como Target.
    if(m_resultGraphData.nodes.contains(m_params.target)){
        m_resultGraphData.nodes[m_params.target].type = NodeType::Target;
        m_resultGraphData.nodes[m_params.target].group = "target";
        targetFoundAsNode = true;
        targetNodeData = m_resultGraphData.nodes[m_params.target];
    }


    if (m_isCancelled) {
        emit progressUpdate(m_params.target, tr("Cancelado"), m_params.maxHops, m_currentTtl);
        emit taskCancelled(m_params.target);
    } else {
        if (!targetFoundAsNode && m_currentTtl < m_params.maxHops) {
             // Se o alvo não foi encontrado como um nó específico e não atingimos max_hops,
             // pode ser um erro ou o alvo realmente não foi alcançado.
             // Se o último TTL foi menor que max_hops, é provável que o alvo não foi alcançado.
             emit progressUpdate(m_params.target, tr("Alvo não alcançado."), m_params.maxHops, m_currentTtl);
             // Poderia emitir um erro específico se o alvo não foi encontrado.
             // emit taskError(m_params.target, tr("Alvo %1 não encontrado na rota.").arg(m_params.target));
        } else if (targetFoundAsNode){
             emit progressUpdate(m_params.target, tr("Concluído. Alvo: %1").arg(targetNodeData.id), m_params.maxHops, m_currentTtl);
        } else {
             emit progressUpdate(m_params.target, tr("Concluído."), m_params.maxHops, m_currentTtl);
        }
        emit taskFinished(m_params.target, m_resultGraphData);
    }

    // Limpeza do QProcess se ainda não foi deletado
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            // Isso não deveria acontecer se onProcessFinished foi chamado normalmente
            m_process->kill();
            m_process->waitForFinished(500);
        }
        m_process->deleteLater(); // Garante que seja deletado no loop de eventos
        m_process = nullptr;
    }
}
