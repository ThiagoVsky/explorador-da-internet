// Cartógrafo da Internet - v0.6.11
// Autor: Thiago

document.addEventListener('DOMContentLoaded', () => {

    // --- Referências Globais do DOM ---
    const uploadArea = document.getElementById('upload-area');
    const mapContainer = document.getElementById('map-container');
    const loadingOverlay = document.getElementById('loading-overlay');
    const loadingTitle = document.getElementById('loading-title');
    const progressBar = document.getElementById('progress-bar');
    const loadingStatus = document.getElementById('loading-status');
    const fileUpload = document.getElementById('file-upload');
    const infoTitle = document.getElementById('info-title');
    const infoContent = document.getElementById('info-content');
    const fileInfoSpan = document.getElementById('file-info');
    const networkContainer = document.getElementById('mynetwork');

    // --- Variáveis de Estado ---
    let network = null;
    let allNodes = {};
    let allEdges = {};
    let nodesDataSet = new vis.DataSet();
    let edgesDataSet = new vis.DataSet();
    let graphMetadata = {};
    let latencyChart = null;

    // --- Configurações --- 
    const ZOOM_CONFIG = {
        FADE_THRESHOLD_START: 0.4, 
        FADE_THRESHOLD_END: 0.2,   
        MIN_NODE_SIZE: 8,          
        MAX_NODE_SIZE: 25,         
        SCALING_STOP_ZOOM: 1.8,    
    };

    const CARTOGRAPHER_VERSION = "0.6.11";
    const CARTOGRAPHER_COMPATIBILITY = "0.6";

    const NODE_COLORS = {
        explorer: { background: '#FFD700', border: '#DAA520' },
        target: { background: '#FF0000', border: '#B22222' },
        hop: { background: '#0000FF', border: '#00008B' },
        phantom: { background: '#6b7280', border: '#4b5563' }
    };

    // --- Lógica de Upload e Processamento ---
    function setupUploadArea() {
        fileUpload.addEventListener('change', (e) => handleFiles(e.target.files));
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eName => {
            uploadArea.addEventListener(eName, preventDefaults, false);
            document.body.addEventListener(eName, preventDefaults, false);
        });
        ['dragenter', 'dragover'].forEach(eName => uploadArea.addEventListener(eName, () => uploadArea.classList.add('dragover'), false));
        ['dragleave', 'drop'].forEach(eName => uploadArea.addEventListener(eName, () => uploadArea.classList.remove('dragover'), false));
        uploadArea.addEventListener('drop', (e) => handleFiles(e.dataTransfer.files), false);
    }

    function preventDefaults(e) { e.preventDefault(); e.stopPropagation(); }

    function handleFiles(files) {
        if (files.length > 0 && files[0].name.endsWith('.graph')) {
            processFile(files[0]);
        } else {
            alert('Por favor, carregue um arquivo com a extensão .graph');
        }
    }

    function processFile(file) {
        // Esconde a área de upload e exibe a área do mapa e o overlay de carregamento.
        uploadArea.classList.add('hidden');
        mapContainer.classList.remove('hidden');
        mapContainer.style.display = 'flex'; // Garante que o container do mapa seja flexível.
        loadingOverlay.style.display = 'flex'; // Exibe o overlay de carregamento.
        loadingTitle.textContent = "Analisando Topologia..."; // Define o título inicial do carregamento.
        progressBar.style.width = '10%'; // Define um progresso inicial para a barra.

        const reader = new FileReader(); // Cria um leitor de arquivos.
        reader.onload = (e) => { // Callback executado quando o arquivo é carregado.
            try {
                let graphData = JSON.parse(e.target.result); // Tenta parsear o conteúdo do arquivo como JSON.
                const fileVersion = graphData.file_version || '0.0'; // Obtém a versão do arquivo, ou '0.0' se não definida.

                // Lógica de verificação de versão e migração.
                // CARTOGRAPHER_COMPATIBILITY define a versão base mínima que o cartógrafo suporta diretamente.
                if (parseFloat(fileVersion) < parseFloat(CARTOGRAPHER_COMPATIBILITY)) {
                    // Se a versão do arquivo for mais antiga que a compatibilidade base.
                    if (confirm(`O arquivo (v${fileVersion}) é de uma versão antiga. Deseja tentar migrá-lo para um formato compatível?`)) {
                        graphData = migrateData(graphData); // Tenta migrar os dados para a versão atual.
                    } else {
                        location.reload(); // Recarrega a página se o usuário cancelar a migração.
                        return;
                    }
                } else if (parseInt(fileVersion.split('.')[0], 10) > parseInt(CARTOGRAPHER_COMPATIBILITY.split('.')[0], 10)) {
                    // Se a versão MAJOR do arquivo for mais nova que a versão MAJOR de compatibilidade do cartógrafo.
                    // Ex: Arquivo v1.0, Cartógrafo compatível com v0.x.
                    alert(`Incompatibilidade de Versão!\n\nO Cartógrafo (v${CARTOGRAPHER_VERSION}) não pode abrir este arquivo (v${fileVersion}) de uma versão futura.`);
                    location.reload(); // Recarrega a página.
                    return;
                }
                
                // Carrega os dados do grafo (nós e arestas) a partir do objeto JSON (original ou migrado).
                const loadedData = loadGraphData(graphData);
                allNodes = loadedData.nodes; // Armazena os nós processados globalmente.
                allEdges = loadedData.edges; // Armazena as arestas processadas globalmente.
                graphMetadata = loadedData.meta;

                fileInfoSpan.textContent = `v${graphMetadata.version} (${new Date(graphMetadata.timestamp).toLocaleString()})`;
                drawGraph();
            } catch (error) {
                console.error("Erro fatal ao processar o arquivo .graph:", error);
                console.error("Detalhes do erro:", error.message);
                if (error.stack) {
                    console.error("Stack Trace:", error.stack);
                }
                alert('Houve um erro! Verifique o console'); // Changed alert message
                location.reload();
            }
        };
        reader.readAsText(file);
    }

    // --- Lógica de Dados ---
    /**
     * Tenta migrar dados de um formato de arquivo antigo para um formato compatível com a versão atual do cartógrafo.
     * Esta função é chamada se a versão do arquivo carregado for anterior à `CARTOGRAPHER_COMPATIBILITY`.
     * @param {object} oldData - Os dados do arquivo .graph no formato antigo.
     * @returns {object} Os dados transformados para o formato novo/compatível.
     */
    function migrateData(oldData) {
        // IMPORTANTE: Esta função de migração é um exemplo e pode precisar de ajustes
        // detalhados dependendo das diferenças entre as versões do formato do arquivo.
        // O objetivo é transformar 'oldData' na estrutura esperada por 'loadGraphData'.
        console.log("Tentando migrar dados da versão:", oldData.file_version);
        return {
            // Ao migrar, atualiza a versão do arquivo para a versão de compatibilidade do cartógrafo atual,
            // ou para uma versão específica se a migração for para um formato intermediário.
            // No 'requisitos.md', ER05 menciona "Conversão automática v0.5+ para v0.6+".
            // Se CARTOGRAPHER_COMPATIBILITY é "0.6", então migrar para "0.6" é o correto.
            // A versão "0.6.9" aqui parecia ser um valor hardcoded que poderia ser desalinhado.
            // Usar CARTOGRAPHER_COMPATIBILITY garante que a migração visa a versão que `loadGraphData` espera.
            file_version: CARTOGRAPHER_COMPATIBILITY,
            nodes: (typeof oldData.nodes === 'object' && !Array.isArray(oldData.nodes))
                   ? Object.values(oldData.nodes) // Converte de objeto para array, se necessário.
                   : (oldData.nodes || []), // Garante que seja um array ou um array vazio.
            edges: (typeof oldData.edges === 'object' && !Array.isArray(oldData.edges))
                   ? Object.values(oldData.edges).map(e => {
                       // Lógica de exemplo para corrigir arestas que podem ter 'from'/'to' em um campo 'id'.
                       if(!e.from || !e.to && e.id && typeof e.id === 'string' && e.id.includes('-')) {
                           const parts = e.id.split('-',2);
                           e.from = parts[0];
                           e.to = parts[1];
                       }
                       return e;
                     })
                   : (oldData.edges || []), // Garante que seja um array ou um array vazio.
            explorers: oldData.explorers || {}, // Mantém os exploradores, se existirem.
            timestamp: oldData.timestamp || new Date().toISOString() // Mantém ou define um novo timestamp.
        };
    }

    /**
     * Processa os dados brutos do arquivo .graph (após JSON.parse e possível migração)
     * e os transforma na estrutura interna que a visualização usará (dicionários de nós e arestas).
     * @param {object} graphData - Os dados do grafo como lidos do arquivo (ou migrados),
     *                             espera-se que `graphData.nodes` e `graphData.edges` sejam arrays.
     * @returns {object} Um objeto contendo 'nodes' (dicionário), 'edges' (dicionário) e 'meta' informações.
     */
    function loadGraphData(graphData) {
        const nodes = {}; // Dicionário para armazenar nós, usando ID como chave para acesso rápido.
        const edges = {}; // Dicionário para armazenar arestas, usando uma chave composta.

        (graphData.nodes || []).forEach(node => { // Itera sobre o array de nós.
            if (!node.id) { // Pula nós sem ID, pois são essenciais para a estrutura do grafo.
                console.warn("Nó encontrado sem ID no arquivo, será ignorado:", node);
                return; // Continua para o próximo nó.
            }
            // Lógica para determinar o grupo (tipo) do nó se não estiver explicitamente definido.
            // Isso é crucial para a correta estilização e comportamento do nó na visualização.
            if (!node.group) {
                if (node.id.startsWith("phantom")) node.group = "phantom";
                else if (node.label && typeof node.label === 'string' && node.label.includes("Explorador")) node.group = "explorer";
                else if (node.label && typeof node.label === 'string' && node.label.includes("Alvo")) node.group = "target";
                else node.group = "hop"; // Define como 'hop' por padrão se nenhuma outra condição for atendida.
            }
            nodes[node.id] = node; // Adiciona o nó ao dicionário de nós.
        });

        (graphData.edges || []).forEach(edge => { // Itera sobre o array de arestas.
            if (!edge.from || !edge.to) { // Pula arestas sem 'from' ou 'to' definidos, pois são inválidas.
                console.warn("Aresta encontrada sem 'from' ou 'to' no arquivo, será ignorada:", edge);
                return; // Continua para a próxima aresta.
            }
            // Cria uma chave única para a aresta para evitar duplicatas e facilitar a busca/atualização.
            // O 'label' da aresta (protocolo/porta) é incluído na chave se existir.
            const key = `${edge.from}-${edge.to}-${edge.label || 'conn'}`;
            if (!edge.latencies) edge.latencies = []; // Garante que 'latencies' seja sempre um array.
            // Calcula a latência média para a aresta. Se não houver amostras de latência, a média é 0.
            edge.avg_latency = edge.latencies.length > 0 ? edge.latencies.reduce((a, b) => a + b, 0) / edge.latencies.length : 0;
            edges[key] = edge; // Adiciona a aresta ao dicionário de arestas.
        });

        // Retorna os dicionários de nós e arestas processados, e metadados do grafo.
        return {
            nodes,
            edges,
            meta: {
                version: graphData.file_version || CARTOGRAPHER_COMPATIBILITY, // Usa a versão do arquivo ou a de compatibilidade.
                timestamp: graphData.timestamp || new Date().toISOString() // Usa o timestamp do arquivo ou o atual.
            }
        };
    }

    // --- Lógica de Visualização (vis.js) ---
    function drawGraph() {
        const processedNodes = Object.values(allNodes).map(node => ({
            ...node,
            label: node.label.replace(/[\u{1F300}-\u{1F5FF}\u{1F600}-\u{1F64F}\u{1F680}-\u{1F6FF}\u{2600}-\u{26FF}\u{2700}-\u{27BF}]/gu, '').trim(),
            hidden: false, // Garantir que todos os nós comecem visíveis
            opacity: 1
        }));

        const processedEdges = Object.values(allEdges).map(edge => {
            const isPhantom = allNodes[edge.from]?.group === 'phantom' || allNodes[edge.to]?.group === 'phantom';
            return {
                ...edge,
                id: `${edge.from}-${edge.to}-${edge.label || 'conn'}`,
                dashes: isPhantom,
                length: 100 + (edge.avg_latency * 2) // Distância baseada na latência
            };
        });

        nodesDataSet.clear();
        edgesDataSet.clear();
        nodesDataSet.add(processedNodes);
        edgesDataSet.add(processedEdges);

        const options = {
            nodes: {
                borderWidth: 2,
                font: { size: 14, face: "'Inter', sans-serif", color: '#ffffff' },
                shapeProperties: { interpolation: false },
            },
            edges: {
                width: 1.5,
                color: { color: '#4b5563', highlight: '#6366f1' },
                arrows: { to: { enabled: true, scaleFactor: 0.5 } },
                smooth: { type: "continuous", roundness: 0.2 }
            },
            physics: {
                enabled: true,
                barnesHut: {
                    gravitationalConstant: -40000,
                    springConstant: 0.05,
                    springLength: 200,
                    avoidOverlap: 1
                },
                stabilization: { iterations: 2500 },
            },
            interaction: { tooltipDelay: 100, hideEdgesOnDrag: true, navigationButtons: false },
            groups: {
                explorer: { shape: 'star', color: { background: '#FFD700', border: '#DAA520' }, size: ZOOM_CONFIG.MAX_NODE_SIZE },
                target: { shape: 'triangle', color: { background: '#FF0000', border: '#B22222' }, size: ZOOM_CONFIG.MAX_NODE_SIZE },
                hop: { shape: 'dot', color: { background: '#0000FF', border: '#00008B' }, size: 15 },
                phantom: { shape: 'dot', color: { background: '#6b7280', border: '#4b5563' }, size: 10 }
            }
        };

        if (network) network.destroy();
        network = new vis.Network(networkContainer, { nodes: nodesDataSet, edges: edgesDataSet }, options);
        setupEventListeners(network);
    }

    // --- Lógica de Interação e UI ---
    function setupEventListeners(networkInstance) {
        networkInstance.on("stabilizationProgress", p => {
            const perc = Math.floor((p.iterations / p.total) * 100);
            progressBar.style.width = perc + '%';
            loadingStatus.textContent = `Estabilizando a rede... ${p.iterations} / ${p.total}`;
        });

        networkInstance.once("stabilizationIterationsDone", () => {
            loadingOverlay.style.opacity = '0';
            setTimeout(() => { 
                loadingOverlay.style.display = 'none'; 
                loadingOverlay.style.opacity = '1';
            }, 500);
            updateLOD(); // Chamar LOD inicial
        });

        networkInstance.on("click", params => {
            if (params.nodes.length > 0) {
                displayNodeInfo(allNodes[params.nodes[0]]);
            } else if (params.edges.length > 0) {
                displayEdgeInfo(allEdges[params.edges[0]]);
            } else {
                infoTitle.textContent = "Informações";
                infoContent.innerHTML = "Selecione um nó ou aresta para ver os detalhes.";
            }
        });

        const processZoom = debounce(() => updateLOD(), 100);
        networkInstance.on('zoom', processZoom);
        networkInstance.on('dragEnd', processZoom); // Atualizar LOD após arrastar
        networkInstance.on('afterDrawing', (ctx) => drawISPTerritories(ctx, networkInstance));
    
        // Lógica da barra lateral redimensionável
        const resizeHandle = document.getElementById('resize-handle');
        const sidebar = document.getElementById('sidebar');
        let isResizing = false;

        resizeHandle.addEventListener('mousedown', (e) => {
            isResizing = true;
            document.addEventListener('mousemove', handleMouseMove);
            document.addEventListener('mouseup', () => {
                isResizing = false;
                document.removeEventListener('mousemove', handleMouseMove);
                network.redraw(); // Redesenhar a rede após o redimensionamento
            });
        });

        function handleMouseMove(e) {
            if (!isResizing) return;
            const newWidth = window.innerWidth - e.clientX;
            sidebar.style.width = `${newWidth}px`;
        }
    }

    function updateLOD() {
        const scale = network.getScale();
        const nodeUpdates = [];

        // Ajusta o springLength e gravitationalConstant para afastar os nós em zooms altos
        if (scale > ZOOM_CONFIG.SCALING_STOP_ZOOM) {
            network.setOptions({
                physics: {
                    barnesHut: {
                        springLength: 200 * (scale / ZOOM_CONFIG.SCALING_STOP_ZOOM), // Aumenta o comprimento da mola
                        gravitationalConstant: -50000, // Reduz a força de atração
                        centralGravity: 0.0 // Permite que os nós se espalhem mais livremente
                    }
                }
            });
        } else {
            network.setOptions({
                physics: {
                    barnesHut: {
                        springLength: 200, // Valor padrão
                        gravitationalConstant: -40000, // Valor padrão
                        centralGravity: 0.05 // Valor padrão
                    }
                }
            });
        }

        const view = network.getViewPosition();
        const scaleFactor = network.getScale();

        nodesDataSet.forEach(node => {
            let newOpacity = 1;
            let newSize = node.size;

            // Lógica de Fade para Hops
            if (node.group === 'hop' || node.group === 'phantom') {
                if (scale < ZOOM_CONFIG.FADE_THRESHOLD_START) {
                    newOpacity = Math.max(0, (scale - ZOOM_CONFIG.FADE_THRESHOLD_END) / (ZOOM_CONFIG.FADE_THRESHOLD_START - ZOOM_CONFIG.FADE_THRESHOLD_END));
                }
            }

            // Lógica de Tamanho Fixo
            if (scale > ZOOM_CONFIG.SCALING_STOP_ZOOM) {
                newSize = ZOOM_CONFIG.MAX_NODE_SIZE;
            } else {
                 const baseSize = (node.group === 'explorer' || node.group === 'target') ? ZOOM_CONFIG.MAX_NODE_SIZE : 15;
                 newSize = Math.max(ZOOM_CONFIG.MIN_NODE_SIZE, baseSize * Math.pow(scale, 0.5));
            }

            nodeUpdates.push({
                id: node.id,
                font: { color: `rgba(255, 255, 255, ${newOpacity})` },
                color: {
                    background: `rgba(${parseInt(NODE_COLORS[node.group].background.substring(1, 3), 16)}, ${parseInt(NODE_COLORS[node.group].background.substring(3, 5), 16)}, ${parseInt(NODE_COLORS[node.group].background.substring(5, 7), 16)}, ${newOpacity})`,
                    border: `rgba(${parseInt(NODE_COLORS[node.group].border.substring(1, 3), 16)}, ${parseInt(NODE_COLORS[node.group].border.substring(3, 5), 16)}, ${parseInt(NODE_COLORS[node.group].border.substring(5, 7), 16)}, ${newOpacity})`
                },
                size: newSize
            });
        });

        if (nodeUpdates.length > 0) {
            nodesDataSet.update(nodeUpdates);
        }
    }

    function applyAutoLayout() {
        network.setOptions({ physics: true });
        network.once("stabilizationIterationsDone", () => {
            
        });
    }

    function displayNodeInfo(node) {
        infoTitle.textContent = `Detalhes do Nó`;
        let geoHtml = '<tr><td colspan="2" class="text-gray-500">Não há informações de geolocalização para este nó.</td></tr>';
        if (node.geo_info && node.geo_info.status === "success") {
            geoHtml = `
                <tr><th>País</th><td>${node.geo_info.country || 'N/A'}</td></tr>
                <tr><th>Região</th><td>${node.geo_info.regionName || 'N/A'}</td></tr>
                <tr><th>Cidade</th><td>${node.geo_info.city || 'N/A'}</td></tr>
                <tr><th>Provedor</th><td>${node.geo_info.isp || 'N/A'}</td></tr>
                <tr><th>Coordenadas</th><td>${node.geo_info.lat || 'N/A'}, ${node.geo_info.lon || 'N/A'}</td></tr>
            `;
        }
        infoContent.innerHTML = `
            <div class="mb-4 p-3 rounded bg-gray-700/50">
                <h4 class="font-bold text-lg text-white">${node.label}</h4>
                <p class="text-gray-400 text-sm break-all">${node.id}</p>
            </div>
            <table class="w-full text-sm">
                <tbody>
                    <tr><th>Tipo</th><td class="capitalize">${node.group}</td></tr>
                    ${node.fqdns?.length ? `<tr><th>FQDNs</th><td>${node.fqdns.join(', ')}</td></tr>` : ''}
                    ${geoHtml}
                </tbody>
            </table>`;
    }

    function displayEdgeInfo(edge) {
        infoTitle.textContent = `Detalhes da Conexão`;
        const samples = edge.latencies.length;
        const minLatency = samples > 0 ? Math.min(...edge.latencies) : 0;
        const maxLatency = samples > 0 ? Math.max(...edge.latencies) : 0;

        infoContent.innerHTML = `
            <div class="mb-4 p-3 rounded bg-gray-700/50">
                <h4 class="font-semibold text-white break-all">${edge.from}</h4>
                <p class="text-center text-gray-400 text-xs py-1">PARA</p>
                <h4 class="font-semibold text-white break-all">${edge.to}</h4>
            </div>
            <table class="w-full text-sm">
                <tbody>
                    <tr><th>Protocolo/Porta</th><td>${edge.label || 'N/A'}</td></tr>
                    <tr><th>Amostras</th><td>${samples}</td></tr>
                    <tr><th>Latência Média</th><td>${edge.avg_latency.toFixed(2)} ms</td></tr>
                    <tr><th>Latência Mín/Máx</th><td>${minLatency.toFixed(2)} / ${maxLatency.toFixed(2)} ms</td></tr>
                </tbody>
            </table>
            <div class="mt-4 h-48"><canvas id="latency-chart"></canvas></div>`;
        
        if (latencyChart) latencyChart.destroy();
        if (samples > 1) {
            const ctx = document.getElementById('latency-chart').getContext('2d');
            latencyChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: edge.latencies.map((_, i) => `ping ${i + 1}`),
                    datasets: [{
                        label: 'Latência (ms)',
                        data: edge.latencies,
                        borderColor: '#6366f1',
                        backgroundColor: 'rgba(99, 102, 241, 0.2)',
                        fill: true,
                        tension: 0.3
                    }]
                },
                options: {
                    maintainAspectRatio: false,
                    scales: { y: { beginAtZero: false, ticks: {color: '#9ca3af'}}, x: {ticks: {color: '#9ca3af'}}},
                    plugins: { legend: { display: false } }
                }
            });
        }
    }

    // Função para calcular o Convex Hull (Andrew's Monotone Chain algorithm)
    function cross(o, a, b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    }

    function convexHull(points) {
        if (points.length <= 3) return points.sort((a, b) => a.x - b.x || a.y - b.y);

        points.sort((a, b) => a.x - b.x || a.y - b.y);

        const lower = [];
        for (const p of points) {
            while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], p) <= 0) {
                lower.pop();
            }
            lower.push(p);
        }

        const upper = [];
        for (let i = points.length - 1; i >= 0; i--) {
            const p = points[i];
            while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], p) <= 0) {
                upper.pop();
            }
            upper.push(p);
        }

        upper.pop();
        lower.pop();
        return lower.concat(upper);
    }

    function drawISPTerritories(ctx, networkInstance) {
        const ispGroups = {};
        nodesDataSet.forEach(node => {
            const isp = allNodes[node.id]?.geo_info?.isp;
            if (isp) {
                if (!ispGroups[isp]) {
                    ispGroups[isp] = [];
                }
                const canvasPos = networkInstance.canvas.canvasViewAndModule.canvas.DOMtoCanvas({ x: node.x, y: node.y });
                ispGroups[isp].push(canvasPos);
            }
        });

        const scale = networkInstance.getScale();
        const minZoomForTerritories = 0.8; // CI03: Visíveis em zoom <0.8x

        if (scale < minZoomForTerritories) {
            for (const isp in ispGroups) {
                const points = ispGroups[isp];
                if (points.length > 2) { // Precisa de pelo menos 3 pontos para um polígono
                    const hull = convexHull(points);

                    ctx.beginPath();
                    ctx.moveTo(hull[0].x, hull[0].y);
                    for (let i = 1; i < hull.length; i++) {
                        ctx.lineTo(hull[i].x, hull[i].y);
                    }
                    ctx.closePath();

                    // Estilo do território
                    ctx.fillStyle = `rgba(75, 192, 192, ${0.15 * (1 - (scale / minZoomForTerritories))})`; // Cor ciano com opacidade baseada no zoom
                    ctx.strokeStyle = `rgba(75, 192, 192, ${0.5 * (1 - (scale / minZoomForTerritories))})`; // Borda ciano com opacidade
                    ctx.lineWidth = 2;
                    ctx.fill();
                    ctx.stroke();

                    // Desenhar o nome do ISP no centro do território
                    const centroid = hull.reduce((acc, p) => ({ x: acc.x + p.x, y: acc.y + p.y }), { x: 0, y: 0 });
                    centroid.x /= hull.length;
                    centroid.y /= hull.length;

                    ctx.font = `${16 / scale}px Arial`; // Tamanho da fonte ajustado pelo zoom
                    ctx.fillStyle = `rgba(255, 255, 255, ${0.8 * (1 - (scale / minZoomForTerritories))})`;
                    ctx.textAlign = 'center';
                    ctx.textBaseline = 'middle';
                    ctx.fillText(isp, centroid.x, centroid.y);
                }
            }
        }
    }

    function debounce(func, timeout = 150){
        let timer;
        return (...args) => {
            clearTimeout(timer);
            timer = setTimeout(() => { func.apply(this, args); }, timeout);
        };
    }

    // --- Inicialização ---
    setupUploadArea();
});