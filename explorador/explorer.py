# explorador_de_rede_v0_6.py
# Versão 0.6: Lógica de migração e fluxo do programa corrigidos.
#
# Requerimentos:
# - Python 3
# - Ferramentas 'traceroute' e 'curl' (sudo apt-get install traceroute curl)
# - Bibliotecas Python: tqdm, requests (pip install tqdm requests)

import subprocess
import json
import re
import os
import sys
import time
import uuid
import shutil
import threading
import concurrent.futures
from datetime import datetime
import requests
from tqdm import tqdm

# --- Bloco para entrada de uma tecla só (multi-plataforma) ---
try:
    import msvcrt
    def getch():
        return msvcrt.getch().decode()
except ImportError:
    import tty, termios
    def getch():
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch
# -----------------------------------------------------------

ARQUIVO_DE_SAIDA = "grafo_da_rede.graph"
SCRIPT_VERSION = "0.6.9"
API_DELAY = 1.5

# --- Definição de Funções ---

def get_yes_no_input(prompt):
    """Pede uma resposta s/n e retorna True/False."""
    while True:
        sys.stdout.write(prompt)
        sys.stdout.flush()
        char = getch().lower()
        if char in ['s', 'y']:
            print("Sim")
            return True
        elif char == 'n':
            print("Não")
            return False
        else:
            print("\nOpção inválida. Por favor, pressione 's' ou 'n'.")

def catalogar_geo_info(data):
    """Busca informações de Geo-IP para nós não catalogados."""
    nodes_to_catalog = [node_id for node_id, node_data in data['nodes'].items() if re.match(r"^(?!10\.|172\.(1[6-9]|2[0-9]|3[0-1])\.|192\.168\.)\d{1,3}(\.\d{1,3}){3}$", node_id) and 'geo_info' not in node_data]

    if not nodes_to_catalog:
        print("\nTodos os IPs públicos já estão catalogados.")
        return True

    print(f"\nEncontrados {len(nodes_to_catalog)} IPs públicos não catalogados.")
    if get_yes_no_input("Deseja buscar as informações de geolocalização para eles agora? (s/n): "):
        with tqdm(total=len(nodes_to_catalog), desc="Catalogando IPs", unit="ip", bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}]') as pbar:
            for node_id in nodes_to_catalog:
                pbar.set_description(f"Buscando {node_id}")
                try:
                    response = requests.get(f"http://ip-api.com/json/{node_id}?lang=pt-BR&fields=17557273", timeout=10)
                    response.raise_for_status()
                    data['nodes'][node_id]['geo_info'] = response.json()
                except requests.RequestException as e:
                    tqdm.write(f"Erro ao buscar info para {node_id}: {e}")
                    data['nodes'][node_id]['geo_info'] = {"status": "fail", "message": str(e)}
                pbar.update(1)
                time.sleep(API_DELAY)
        return True
    else:
        print("\nOk. O mapa será salvo sem as novas informações de geolocalização.")
        print("AVISO: Para que o Cartógrafo funcione corretamente, será necessário catalogar estes IPs depois.")
        return False

def migrar_e_catalogar_dados_antigos(old_data):
    print("\nIniciando processo de migração de dados antigos...")
    new_data = {"nodes": {}, "edges": {}, "explorers": {}, "file_version": SCRIPT_VERSION}

    nodes_antigos_dict = old_data.get('nodes', {})
    if not isinstance(nodes_antigos_dict, dict):
        print("Formato do arquivo antigo não reconhecido. Não é possível migrar.")
        return None

    for node_id, node_data in nodes_antigos_dict.items():
        if isinstance(node_data, dict) and 'id' in node_data:
            new_data['nodes'][node_id] = node_data

    edges_antigos_dict = old_data.get('edges', {})
    if isinstance(edges_antigos_dict, dict):
        for edge_key, edge_data in edges_antigos_dict.items():
             if isinstance(edge_key, str) and '-' in edge_key:
                 from_node, to_node = edge_key.split('-', 1)
                 new_key = (from_node, to_node)
                 new_data["edges"][new_key] = edge_data
             else:
                 new_data["edges"][edge_key] = edge_data

    print(f"Dados do arquivo antigo lidos com sucesso. O mapa será reconstruído e enriquecido.")

    if not catalogar_geo_info(new_data):
        print("Migração cancelada pelo usuário. O arquivo original não será modificado.")
        return None

    return new_data

def carregar_grafo():
    if os.path.exists(ARQUIVO_DE_SAIDA):
        with open(ARQUIVO_DE_SAIDA, 'r') as f:
            print(f"Arquivo '{ARQUIVO_DE_SAIDA}' encontrado. Carregando dados...")
            try:
                data = json.load(f)
                if data.get("file_version") != SCRIPT_VERSION:
                    print(f"AVISO: A versão do arquivo ({data.get('file_version', 'desconhecida')}) é incompatível com a do script ({SCRIPT_VERSION}).")
                    if get_yes_no_input("Deseja TENTAR MIGRAR e catalogar os dados do arquivo antigo para o novo formato? (s/n): "):
                        migrated_data = migrar_e_catalogar_dados_antigos(data)
                        if migrated_data:
                            return migrated_data
                        else:
                            print("Encerrando script.")
                            sys.exit(0)
                    else:
                        print("Encerrando script. Para usar o mapa antigo, utilize uma versão anterior do Cartógrafo.")
                        sys.exit(0)

                data['nodes'] = {node['id']: node for node in data.get('nodes', [])}
                data['edges'] = {(e.get('from'), e.get('to')): e for e in data.get('edges', []) if e.get('from') and e.get('to')}
                return data
            except (json.JSONDecodeError, TypeError):
                print("Arquivo de grafo corrompido. Começando um novo.")
    return {"nodes": {}, "edges": {}, "explorers": {}, "file_version": SCRIPT_VERSION}

def salvar_grafo(data):
    graph_data = {
        "file_version": SCRIPT_VERSION,
        "nodes": list(data['nodes'].values()),
        "edges": list(data['edges'].values()),
        "explorers": data.get('explorers', {}),
        "timestamp": datetime.now().isoformat()
    }
    with open(ARQUIVO_DE_SAIDA, 'w') as f:
        json.dump(graph_data, f, indent=4)
    print(f"\n--- Mapa atualizado e salvo em: {ARQUIVO_DE_SAIDA} ---")

def get_explorer_id():
    # Implementação da função (sem mudanças)
    try:
        ip = requests.get('https://ifconfig.me/ip', timeout=10).text.strip()
        if not re.match(r"^\d{1,3}(\.\d{1,3}){3}$", ip): raise Exception()
        return ip
    except: return str(uuid.uuid4())

def parse_traceroute_output(output, target_fqdn):
    # Implementação da função (sem mudanças)
    hops, line_regex = [], re.compile(r"^\s*(\d+)\s+([\s\S]+?)(?:\s+([\d\.]+)\s+ms|\s+\*\s+\*\s+\*)")
    first_line = output.splitlines()[0] if output.splitlines() else ""
    ip_match = re.search(r"\((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\)", first_line)
    if ip_match: hops.append({"type": "target_info", "fqdn": target_fqdn, "resolved_ip": ip_match.group(1)})
    for line in output.splitlines()[1:]:
        match = line_regex.search(line)
        if match:
            full_host_info, latency = match.group(2).strip(), float(match.group(3)) if match.group(3) else None
            if '*' in full_host_info:
                hops.append({"type": "hop", "is_phantom": True})
                continue
            host_name = ip_address = full_host_info
            ip_match_hop = re.search(r"\((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\)", full_host_info)
            if ip_match_hop:
                ip_address, host_name = ip_match_hop.group(1), full_host_info.split('(')[0].strip()
            elif re.match(r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$", full_host_info):
                 host_name = ip_address
            hops.append({"type": "hop", "is_phantom": False, "host": host_name, "ip": ip_address, "latency": latency})
    return hops

def run_traceroute_task(target, protocol, params):
    # Implementação da função (sem mudanças)
    command = f"traceroute -w 3 {params} {target}"
    try:
        result = subprocess.run(command.split(), capture_output=True, text=True, timeout=180)
        return protocol, parse_traceroute_output(result.stdout, target)
    except Exception: return protocol, []

def get_user_input():
    # Implementação da função (sem mudanças)
    alvos = input("Digite os alvos (IPs ou domínios), separados por vírgula: ").strip().split(',')
    num_queries_str = input("Digite o número de tentativas por salto (ex: 1 para rápido, 3 para preciso): ")
    num_queries = int(num_queries_str) if num_queries_str.isdigit() and int(num_queries_str) > 0 else 1

    tarefas = [("UDP (Padrão)", f"-q {num_queries}")]
    if get_yes_no_input("Deseja executar um traceroute adicional com ICMP Echo (ping)? (s/n): "):
        tarefas.append(("ICMP (Echo)", f"-I -q {num_queries}"))
    if get_yes_no_input("Deseja executar traceroute com TCP? (s/n): "):
        portas_tcp = input("Digite as portas TCP, separadas por vírgula (ex: 22,80,443): ").strip().split(',')
        for porta in portas_tcp:
            if porta.strip().isdigit():
                tarefas.append((f"TCP:{porta.strip()}", f"-T -p {porta.strip()} -q {num_queries}"))
    return [alvo.strip() for alvo in alvos if alvo], tarefas

# --- Função Principal ---
def main():
    print("--- Bem-vindo ao Explorador da Internet v0.6 ---")
    data = carregar_grafo()
    lock = threading.Lock()
    alvos = []

    print("\nEtapa de Identificação...")
    explorer_id = get_explorer_id()
    if explorer_id not in data.get('explorers', {}):
        data.setdefault('explorers', {})[explorer_id] = {"id": explorer_id, "first_seen": datetime.now().isoformat()}
    if explorer_id not in data['nodes']:
        data['nodes'][explorer_id] = {"id": explorer_id, "label": f"Explorador ({explorer_id})", "fqdns": []}

    print("\nEtapa de Catalogação...")
    if not catalogar_geo_info(data):
        salvar_grafo(data)
        sys.exit(0)

    print("\nEtapa de Exploração...")
    if get_yes_no_input("Deseja explorar novos alvos? (s/n): "):
        alvos, tarefas = get_user_input()
        with tqdm(total=len(alvos), desc="Explorando Alvos", unit="alvo") as pbar:
            for target in alvos:
                pbar.set_description(f"Investigando {target}")
                with concurrent.futures.ThreadPoolExecutor(max_workers=len(tarefas)) as executor:
                    future_to_task = {executor.submit(run_traceroute_task, target, protocol, params): (protocol, params) for protocol, params in tarefas}
                    time.sleep(0.2)
                    for future in concurrent.futures.as_completed(future_to_task):
                        protocol, hops = future.result()
                        if not hops: continue
                        with lock:
                            target_info = next((h for h in hops if h.get('type') == 'target_info'), None)
                            if target_info:
                                resolved_ip = target_info.get('resolved_ip')
                                if resolved_ip and resolved_ip in data['nodes']:
                                    if 'fqdns' not in data['nodes'][resolved_ip]: data['nodes'][resolved_ip]['fqdns'] = []
                                    if target_info['fqdn'] not in data['nodes'][resolved_ip]['fqdns']:
                                        data['nodes'][resolved_ip]['fqdns'].append(target_info['fqdn'])

                            last_real_hop = {"ip": explorer_id, "latency": 0}
                            phantom_chain = []
                            for hop_data in filter(lambda h: h.get('type') == 'hop', hops):
                                if not hop_data.get('is_phantom'):
                                    if phantom_chain:
                                        phantom_id = f"phantom_{last_real_hop['ip']}_{hop_data['ip']}"
                                        if phantom_id not in data['nodes']:
                                            data['nodes'][phantom_id] = {"id": phantom_id, "label": f"Vértice Fantasma ({len(phantom_chain)} saltos)", "group": "phantom", "fqdns": []}

                                        edge_key_1 = (last_real_hop['ip'], phantom_id)
                                        if edge_key_1 not in data['edges']: data['edges'][edge_key_1] = {"from": last_real_hop['ip'], "to": phantom_id, "latencies": []}

                                        edge_key_2 = (phantom_id, hop_data['ip'])
                                        if edge_key_2 not in data['edges']: data['edges'][edge_key_2] = {"from": phantom_id, "to": hop_data['ip'], "latencies": []}

                                        avg_latency = (last_real_hop.get('latency', 0) + hop_data.get('latency', 0)) / 2
                                        if last_real_hop.get('latency') is not None and hop_data.get('latency') is not None:
                                            data['edges'][edge_key_1]['latencies'].append(avg_latency)
                                            data['edges'][edge_key_2]['latencies'].append(avg_latency)

                                        last_real_hop = hop_data
                                        phantom_chain = []
                                    else:
                                        current_ip = hop_data["ip"]
                                        if current_ip not in data['nodes']:
                                            data['nodes'][current_ip] = {"id": current_ip, "label": hop_data['host'], "group": "hop", "fqdns": []}

                                        edge_key = (last_real_hop['ip'], current_ip)
                                        if edge_key not in data['edges']: data['edges'][edge_key] = {"from": last_real_hop['ip'], "to": current_ip, "label": protocol, "latencies": []}
                                        if hop_data.get('latency') is not None: data['edges'][edge_key]['latencies'].append(hop_data['latency'])
                                        last_real_hop = hop_data
                                else:
                                    phantom_chain.append(hop_data)
                pbar.update(1)

    data['nodes'][explorer_id]['group'] = 'explorer'
    data['nodes'][explorer_id]['label'] = f"📍 Explorador ({explorer_id})"

    todos_alvos_fqdn = set(alvo for alvo in alvos)
    for node in data['nodes'].values():
        if node.get('group') == 'target':
            if 'label' in node: todos_alvos_fqdn.add(node.get('label').replace('🎯 ', ''))

    for node in data['nodes'].values():
        is_target = node.get('id') in todos_alvos_fqdn or any(fqdn in todos_alvos_fqdn for fqdn in node.get('fqdns',[]))
        if is_target and node.get('group') != 'explorer':
            node['group'] = 'target'
            original_label = node.get('label','').replace('🎯 ', '')
            node['label'] = f"🎯 {original_label}"

    salvar_grafo(data)
    print("\nPara catalogar novos sites ou IPs, execute o script novamente.")

if __name__ == "__main__":
    main()
