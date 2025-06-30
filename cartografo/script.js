// --- Referências Globais ---
const uploadArea = document.getElementById('upload-area');
const loadingOverlay = document.getElementById('loading-overlay');
const loadingTitle = document.getElementById('loading-title');
const progressBar = document.getElementById('progress-bar');
const loadingStatus = document.getElementById('loading-status');
const mapWrapper = document.getElementById('map-wrapper');
const fileUpload = document.getElementById('file-upload');
const infoPanel = document.getElementById('info-panel');
const infoTitle = document.getElementById('info-title');
const infoContent = document.getElementById('info-content');
const fileInfoSpan = document.getElementById('file-info');
const legend = document.getElementById('legend');

let network = null;
let allNodes = {};
let allEdges = {};
let nodesDataSet = new vis.DataSet();
let edgesDataSet = new vis.DataSet();
let graphMetadata = {};
let latencyChart = null;
let physicsEnabled = true;

// --- Lógica de Upload e Processamento ---
fileUpload.addEventListener('change', (e) => handleFiles(e.target.files));
['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eName => {
    uploadArea.addEventListener(eName, preventDefaults, false);
    document.body.addEventListener(eName, preventDefaults, false);
});
['dragenter', 'dragover'].forEach(eName => uploadArea.addEventListener(eName, () => uploadArea.classList.add('dragover'), false));
['dragleave', 'drop'].forEach(eName => uploadArea.addEventListener(eName, () => uploadArea.classList.remove('dragover'), false));
uploadArea.addEventListener('drop', (e) => handleFiles(e.dataTransfer.files), false);

function preventDefaults(e) { e.preventDefault(); e.stopPropagation(); }

function handleFiles(files) {
    if (files.length > 0 && files[0].name.endsWith('.graph')) {
        processFile(files[0]);
    } else {
        alert('Por favor, carregue um arquivo com a extensão .graph');
    }
}

function processFile(file) {
    uploadArea.style.display = 'none';
    mapWrapper.style.display = 'flex';
    loadingOverlay.style.display = 'flex';
    loadingTitle.textContent = "Analisando Topologia...";
    legend.style.display = 'none';
    hideInfoPanel();
    progressBar.style.width = '10%';
    physicsEnabled = true;

    const reader = new FileReader();
    reader.onload = (e) => {
        try {
            let graphData = JSON.parse(e.target.result);
            if (!graphData.file_version || parseFloat(graphData.file_version) < 0.6) graphData = migrateData(graphData);

            const loadedData = loadGraphData(graphData);
            allNodes = loadedData.nodes;
            allEdges = loadedData.edges;
            graphMetadata = loadedData.meta;

            fileInfoSpan.textContent = `v${graphMetadata.version} (${new Date(graphMetadata.timestamp).toLocaleString()})`;
            drawGraph();
        } catch (error) {
            console.error("Erro fatal:", error);
            alert('Erro ao analisar o arquivo .graph.');
            location.reload();
        }
    };
    reader.readAsText(file);
}

// --- Lógica de Dados ---
function migrateData(oldData) {
    return {
        file_version: "0.6.8",
        nodes: (typeof oldData.nodes === 'object' && !Array.isArray(oldData.nodes)) ? Object.values(oldData.nodes) : (oldData.nodes || []),
        edges: (typeof oldData.edges === 'object' && !Array.isArray(oldData.edges)) ? Object.values(oldData.edges).map(e => { if(!e.from || !e.to) {[e.from, e.to] = e.id.split('-',2);}; return e;}) : (oldData.edges || []),
        explorers: oldData.explorers || {},
        timestamp: oldData.timestamp || new Date().toISOString()
    };
}

function loadGraphData(graphData) {
    const nodes = {};
    const edges = {};
    graphData.nodes.forEach(node => {
        if (!node.group) {
            if (node.id.startsWith("phantom")) node.group = "phantom";
            else if (node.label.includes("📍") || node.label.includes("Explorador")) node.group = "explorer";
            else if (node.label.includes("🎯")) node.group = "target";
            else node.group = "hop";
        }
        nodes[node.id] = node;
    });
    graphData.edges.forEach(edge => {
        const key = `${edge.from}-${edge.to}-${edge.label || 'conn'}`;
        if (!edge.latencies) edge.latencies = [];
        edges[key] = edge;
    });
    return { nodes, edges, meta: { version: graphData.file_version, timestamp: graphData.timestamp }};
}

// --- Lógica de Visualização (vis.js) ---
function configurePhysics() {
    return {
        enabled: true,
        stabilization: {
            enabled: true,
            iterations: 5000,
            updateInterval: 50,
            onlyDynamicEdges: false,
            fit: true
        },
        barnesHut: {
            gravitationalConstant: -35000,
            springConstant: 0.002,
            springLength: 250,
            avoidOverlap: 1.0,
            damping: 0.5
        },
        minVelocity: 0.1,
        maxVelocity: 10,
        adaptiveTimestep: true
    };
}

function drawGraph() {
    const container = document.getElementById('mynetwork');
    const processedNodes = Object.values(allNodes).map(node => ({
        ...node,
        label: node.label.replace(/[\u{1F300}-\u{1F5FF}\u{1F600}-\u{1F64F}\u{1F680}-\u{1F6FF}\u{2600}-\u{26FF}\u{2700}-\u{27BF}]/gu, '').trim()
    }));
    const processedEdges = Object.values(allEdges).map(edge => {
        const fromNode = allNodes[edge.from];
        const toNode = allNodes[edge.to];
        const isPhantom = fromNode?.group === 'phantom' || toNode?.group === 'phantom';
        const isCritical = fromNode?.group === 'explorer' || fromNode?.group === 'target' || toNode?.group === 'explorer' || toNode?.group === 'target';
        const color = isPhantom ? '#6b7280' : (isCritical ? '#f59e0b' : '#4b5563');

        return {
            ...edge,
            id: `${edge.from}-${edge.to}-${edge.label || 'conn'}`,
            smooth: { type: "continuous", roundness: 0.2 },
            dashes: isPhantom,
            width: isPhantom ? 1 : (isCritical ? 3 : 2),
            color: { color: color, highlight: isCritical ? '#facc15' : '#6366f1' },
            arrows: { to: { enabled: true, scaleFactor: 0.5 } }
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
            scaling: { min: 10, max: 30, label: { enabled: true, min: 14, max: 24, drawThreshold: 5 } },
        },
        physics: configurePhysics(),
        interaction: { tooltipDelay: 100, hideEdgesOnDrag: true, navigationButtons: false },
        groups: {
            explorer: { shape: 'star', color: { background: '#f59e0b', border: '#d97706' }, size: 25 },
            target: { shape: 'triangle', color: { background: '#ef4444', border: '#dc2626' }, size: 25 },
            hop: { shape: 'dot', color: { background: '#6366f1', border: '#4f46e5' }, size: 15 },
            phantom: { shape: 'dot', color: { background: '#6b7280', border: '#4b5563' }, size: 10 }
        }
    };

    if (network) network.destroy();
    network = new vis.Network(container, { nodes: nodesDataSet, edges: edgesDataSet }, options);
    setupEventListeners(network);
}

// --- Helpers ---
function debounce(func, timeout = 150){
    let timer;
    return (...args) => {
        clearTimeout(timer);
        timer = setTimeout(() => { func.apply(this, args); }, timeout);
    };
}

// --- Lógica de Interação ---
const ZOOM_CONFIG = {
    minNodeSize: 6,
    maxNodeSize: 25,
    visibilityThresholds: {
        phantom: 0.35,
        hop: 0.2,
        default: 0.0
    }
};

function updateLOD(scale) {
    const nodeUpdates = [];
    const edgeUpdates = [];

    nodesDataSet.forEach(node => {
        const threshold = ZOOM_CONFIG.visibilityThresholds[node.group] || ZOOM_CONFIG.visibilityThresholds.default;
        const isVisible = scale >= threshold;

        const baseSize = (node.group === 'explorer' || node.group === 'target') ? ZOOM_CONFIG.maxNodeSize : 15;
        const newSize = Math.max(ZOOM_CONFIG.minNodeSize, Math.min(baseSize, baseSize * scale * 1.5));

        if (node.hidden === isVisible) { // Update only on change
             nodeUpdates.push({ id: node.id, hidden: !isVisible, size: newSize });
        }
    });

    if (nodeUpdates.length > 0) nodesDataSet.update(nodeUpdates);

     edgesDataSet.forEach(edge => {
        const fromNode = nodesDataSet.get(edge.from);
        const toNode = nodesDataSet.get(edge.to);
        const isEdgeVisible = fromNode && toNode && !fromNode.hidden && !toNode.hidden;
        if (edge.hidden === isEdgeVisible) {
            edgeUpdates.push({ id: edge.id, hidden: !isEdgeVisible });
        }
    });
    if (edgeUpdates.length > 0) edgesDataSet.update(edgeUpdates);
}

function applyAutoLayout(isAnimated = true) {
    if (Object.keys(allNodes).length === 0) return;
    loadingOverlay.style.display = 'flex';
    loadingTitle.textContent = "Reorganizando Rede...";
    loadingStatus.textContent = "Calculando clusters de ISP...";
    network.stopSimulation();

    setTimeout(() => {
        const ispGroups = {};
        nodesDataSet.forEach(node => {
            const originalNode = allNodes[node.id];
            const isp = originalNode.geo_info?.isp || 'Unknown';
            if (!ispGroups[isp]) ispGroups[isp] = [];
            ispGroups[isp].push(node.id);
        });

        const updates = [];
        const groupCount = Object.keys(ispGroups).length;
        const angleStep = (2 * Math.PI) / (groupCount > 0 ? groupCount : 1);
        let angle = 0;

        Object.values(ispGroups).forEach(nodeIds => {
            const radius = 400 + Math.log(nodeIds.length + 1) * 200;
            nodeIds.forEach((nodeId, i) => {
                const nodeAngle = angle + (i / nodeIds.length) * (angleStep * 0.8);
                updates.push({ id: nodeId, x: radius * Math.cos(nodeAngle), y: radius * Math.sin(nodeAngle) });
            });
            angle += angleStep;
        });

        nodesDataSet.update(updates);

        loadingOverlay.style.display = 'none';
        if (isAnimated) {
            network.setOptions({ physics: { enabled: true, stabilization: { iterations: 1000 } } });
            physicsEnabled = true;
            // Auto-disable physics after re-stabilization
            network.once("stabilizationIterationsDone", () => {
                network.setOptions({ physics: false });
                physicsEnabled = false;
                network.stopSimulation();
            });
        }
    }, 100);
}

function setupEventListeners(networkInstance) {
    networkInstance.on("stabilizationProgress", p => {
        if (!physicsEnabled) return;
        const perc = Math.floor((p.iterations / p.total) * 100);
        progressBar.style.width = perc + '%';
        loadingStatus.textContent = `Estabilizando a rede... ${p.iterations} / ${p.total}`;
    });

    networkInstance.once("stabilizationIterationsDone", () => {
        applyAutoLayout(false);
        networkInstance.setOptions({ physics: false });
        physicsEnabled = false;
        networkInstance.stopSimulation();

        setTimeout(() => {
            loadingOverlay.style.opacity = '0';
            setTimeout(() => {
                loadingOverlay.style.display = 'none';
                loadingOverlay.style.opacity = '1';
                legend.style.display = 'block';
                updateLOD(networkInstance.getScale());
            }, 500);
            networkInstance.fit();
        }, 500);
    });

    networkInstance.on('beforeDrawing', (ctx) => {
         if (!physicsEnabled && network.getScale() < 0.8) drawIspTerritories(ctx);
    });

    networkInstance.on("click", params => {
        if (params.nodes.length > 0) {
            if(allNodes[params.nodes[0]]) displayNodeInfo(allNodes[params.nodes[0]]);
        } else if (params.edges.length > 0) {
            const edgeData = allEdges[Object.keys(allEdges).find(key => key.startsWith(`${params.edges[0].split('-')[0]}-${params.edges[0].split('-')[1]}`))];
            if(edgeData) displayEdgeInfo(edgeData);
        } else {
            hideInfoPanel();
        }
    });

    const processZoom = debounce((params) => updateLOD(params.scale));
    networkInstance.on('zoom', processZoom);

    document.getElementById('reorganize').addEventListener('click', () => applyAutoLayout(true));
    document.getElementById('zoom-in').addEventListener('click', () => networkInstance.moveTo({ scale: networkInstance.getScale() * 1.4, animation: {duration: 300, easingFunction: 'easeOutQuad'} }));
    document.getElementById('zoom-out').addEventListener('click', () => networkInstance.moveTo({ scale: networkInstance.getScale() * 0.7, animation: {duration: 300, easingFunction: 'easeOutQuad'} }));
    document.getElementById('fit-to-screen').addEventListener('click', () => networkInstance.fit({ animation: { duration: 800, easingFunction: 'easeInOutQuad' } }));
}

/* Funções de UI (sem alterações) */
function drawIspTerritories(ctx) { if (network.getScale() > 0.8) return; const ispGroups = {}; nodesDataSet.forEach(node => { if (node.hidden) return; const originalNode = allNodes[node.id]; if (originalNode && originalNode.geo_info?.isp) { const isp = originalNode.geo_info.isp; if (!ispGroups[isp]) ispGroups[isp] = []; ispGroups[isp].push(node.id); } }); Object.entries(ispGroups).forEach(([isp, nodeIds]) => { if (nodeIds.length < 3) return; const positions = nodeIds.map(id => network.getPosition(id)).filter(p => p); if (positions.length < 3) return; let hash = 0; for (let i = 0; i < isp.length; i++) hash = isp.charCodeAt(i) + ((hash << 5) - hash); const r = (hash & 0xFF), g = ((hash >> 8) & 0xFF), b = ((hash >> 16) & 0xFF); positions.sort((a, b) => a.x - b.x || a.y - b.y); const cross = (o, a, b) => (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); const lower = []; for (const p of positions) { while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], p) <= 0) lower.pop(); lower.push(p); } const upper = []; for (let i = positions.length - 1; i >= 0; i--) { const p = positions[i]; while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], p) <= 0) upper.pop(); upper.push(p); } const hull = lower.slice(0, -1).concat(upper.slice(0, -1)); if (hull.length < 3) return; ctx.fillStyle = `rgba(${r}, ${g}, ${b}, 0.1)`; ctx.strokeStyle = `rgba(${r}, ${g}, ${b}, 0.4)`; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(hull[0].x, hull[0].y); for (let i = 1; i < hull.length; i++) ctx.lineTo(hull[i].x, hull[i].y); ctx.closePath(); ctx.fill(); ctx.stroke(); }); }
function showInfoPanel() { infoPanel.classList.remove('hidden'); }
function hideInfoPanel() { infoPanel.classList.add('hidden'); infoTitle.textContent = "Informações"; infoContent.innerHTML = "Selecione um nó ou aresta para ver os detalhes."; }
function displayNodeInfo(node) { showInfoPanel(); infoTitle.textContent = `Detalhes do Nó`; let geoHtml = ''; if (node.geo_info && node.geo_info.status === "success") geoHtml = `<tr><th>País</th><td>${node.geo_info.country || 'N/A'}</td></tr><tr><th>Cidade</th><td>${node.geo_info.city || 'N/A'}</td></tr><tr><th>Provedor</th><td>${node.geo_info.isp || 'N/A'}</td></tr><tr><th>Coordenadas</th><td>${node.geo_info.lat || 'N/A'}, ${node.geo_info.lon || 'N/A'}</td></tr>`; infoContent.innerHTML = `<div class="mb-4 p-3 rounded bg-gray-700/50"><h3 class="font-bold text-lg text-white">${node.label.replace(/[\u{1F300}-\u{1F5FF}\u{1F600}-\u{1F64F}\u{1F680}-\u{1F6FF}\u{2600}-\u{26FF}\u{2700}-\u{27BF}]/gu, '').trim()}</h3><p class="text-gray-400 text-sm break-all">${node.id}</p></div><table class="w-full text-sm"><tbody><tr><th>Tipo</th><td class="capitalize">${node.group}</td></tr>${node.fqdns?.length ? `<tr><th>FQDNs</th><td>${node.fqdns.join(', ')}</td></tr>` : ''}${geoHtml}</tbody></table>`; }
function displayEdgeInfo(edge) { showInfoPanel(); infoTitle.textContent = `Detalhes da Conexão`; const samples = edge.latencies.length; const avgLatency = samples > 0 ? (edge.latencies.reduce((a, b) => a + b, 0) / samples) : 0; const minLatency = samples > 0 ? Math.min(...edge.latencies) : 0; const maxLatency = samples > 0 ? Math.max(...edge.latencies) : 0; infoContent.innerHTML = `<div class="mb-4 p-3 rounded bg-gray-700/50"><h3 class="font-semibold text-white break-all">${edge.from}</h3><p class="text-center text-gray-400 text-xs py-1">PARA</p><h3 class="font-semibold text-white break-all">${edge.to}</h3></div><table class="w-full text-sm"><tbody><tr><th>Protocolo/Porta</th><td>${edge.label || 'N/A'}</td></tr><tr><th>Amostras</th><td>${samples}</td></tr><tr><th>Latência Média</th><td>${avgLatency.toFixed(2)} ms</td></tr><tr><th>Latência Mín/Máx</th><td>${minLatency.toFixed(2)} / ${maxLatency.toFixed(2)} ms</td></tr></tbody></table><div class="mt-4 h-48"><canvas id="latency-chart"></canvas></div>`; if (latencyChart) latencyChart.destroy(); if (samples > 1) { const ctx = document.getElementById('latency-chart').getContext('2d'); latencyChart = new Chart(ctx, { type: 'line', data: { labels: edge.latencies.map((_, i) => `ping ${i + 1}`), datasets: [{ label: 'Latência (ms)', data: edge.latencies, borderColor: '#6366f1', backgroundColor: 'rgba(99, 102, 241, 0.2)', fill: true, tension: 0.3 }] }, options: { maintainAspectRatio: false, scales: { y: { beginAtZero: false, ticks: {color: '#9ca3af'}}, x: {ticks: {color: '#9ca3af'}}}, plugins: { legend: { display: false } } } }); } }
