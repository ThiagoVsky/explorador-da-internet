# Explorador da Internet e Cartógrafo de Rede

Este projeto consiste em duas ferramentas principais: um **Explorador de Rede** que coleta dados de topologia da internet usando `traceroute`, e um **Cartógrafo da Internet** que visualiza esses dados de forma interativa.

## Funcionalidades

*   **Explorador de Rede (`explorador/explorer.py`)**:
    *   Executa `traceroute` para alvos especificados (IPs ou FQDNs) usando protocolos UDP, ICMP e TCP.
    *   Identifica diferentes tipos de nós: explorador, alvo, hop (salto intermediário) e nós fantasmas (hops não responsivos).
    *   Coleta informações de latência para as conexões.
    *   Utiliza a API `ip-api.com` para obter dados de geolocalização dos IPs.
    *   Salva os dados coletados no arquivo `grafo_da_rede.graph` na raiz do projeto.
    *   Pode migrar dados de arquivos `.graph` de versões anteriores.

*   **Cartógrafo da Internet (`cartografo/`)**:
    *   Interface web para carregar e visualizar os arquivos `.graph` gerados pelo Explorador.
    *   Renderiza a topologia da rede de forma interativa usando a biblioteca `vis.js`.
    *   Permite zoom, pan, seleção de nós/arestas para exibir detalhes.
    *   Exibe informações geográficas e de latência.
    *   Versão atual: v0.6.10. Compatível com arquivos `.graph` versão 0.6+.

## Requisitos e Configuração

### Para o Explorador de Rede (`explorador/explorer.py`):

1.  **Python 3**: Certifique-se de ter o Python 3 instalado.
2.  **Ferramentas de Sistema**:
    *   `traceroute`: Geralmente disponível em sistemas Linux/macOS.
        *   Para instalar no Debian/Ubuntu: `sudo apt-get install traceroute`
        *   No macOS, já vem instalado.
    *   `curl`: Usado para buscar o IP público do explorador.
        *   Para instalar no Debian/Ubuntu: `sudo apt-get install curl`
3.  **Bibliotecas Python**:
    *   `requests`: Para consultas à API Geo-IP e outras requisições HTTP.
    *   `tqdm`: Para exibir barras de progresso.
    *   Instale com pip: `pip install requests tqdm`

    Você pode também criar um arquivo `explorador/requirements.txt` com o conteúdo:
    ```
    requests
    tqdm
    ```
    E instalar com: `pip install -r explorador/requirements.txt`

### Para o Cartógrafo da Internet (`cartografo/`):

1.  **Navegador Web Moderno**:
    *   Google Chrome, Mozilla Firefox, Microsoft Edge (últimas versões são recomendadas).
2.  **Conexão com a Internet**:
    *   Necessário para carregar bibliotecas JavaScript externas (TailwindCSS, vis.js, Chart.js) via CDN.

## Como Usar

### 1. Executar o Explorador de Rede

1.  Navegue até o diretório raiz do projeto no seu terminal.
2.  Execute o script `explorer.py`:
    ```bash
    python explorador/explorer.py
    ```
3.  O script solicitará:
    *   **Alvos**: IPs ou nomes de domínio para explorar (separados por vírgula).
    *   **Número de tentativas por salto**: Quantas vezes tentar cada salto no traceroute.
    *   **Opções de protocolo**: Se deseja usar ICMP ou TCP além do UDP padrão.
4.  Após a execução, o script salvará os dados no arquivo `grafo_da_rede.graph` na raiz do projeto. Ele também imprimirá o caminho absoluto para este arquivo.

    **Exemplo de Saída do Explorador:**
    ```
    --- Mapa atualizado e salvo em: /caminho/completo/para/o/projeto/grafo_da_rede.graph ---
    ```

### 2. Visualizar os Dados com o Cartógrafo

1.  Abra o arquivo `cartografo/index.html` em seu navegador web.
    *   Você pode fazer isso navegando até o diretório `cartografo` no seu explorador de arquivos e clicando duas vezes em `index.html`, ou usando um servidor web local simples.
2.  Na página do Cartógrafo, você verá uma área para upload de arquivo.
3.  Clique no botão "Selecionar Arquivo" ou arraste e solte o arquivo `grafo_da_rede.graph` (gerado pelo Explorador na etapa anterior) na área designada.
4.  O Cartógrafo carregará o arquivo, processará os dados e exibirá a visualização da rede.

## Estrutura do Arquivo `.graph`

O arquivo `grafo_da_rede.graph` é um JSON contendo:
*   `file_version`: Versão do formato do arquivo.
*   `timestamp`: Data e hora da geração do arquivo.
*   `nodes`: Uma lista de objetos, cada um representando um nó na rede (IP, explorador, alvo, etc.). Inclui ID, label, grupo (tipo), informações de geolocalização (`geo_info`), e FQDNs.
*   `edges`: Uma lista de objetos, cada um representando uma conexão (aresta) entre dois nós. Inclui IDs dos nós de origem (`from`) e destino (`to`), e uma lista de latências (`latencies`).
*   `explorers`: Informações sobre os nós exploradores.

## Solução de Problemas

*   **Erro no Cartógrafo**: Se o Cartógrafo exibir um alerta de erro ao carregar o arquivo:
    *   Verifique se o arquivo `grafo_da_rede.graph` não está corrompido ou vazio.
    *   Certifique-se de que o arquivo foi gerado pela versão compatível do `explorador.py`.
    *   O alerta de erro no Cartógrafo agora inclui detalhes técnicos que podem ajudar a identificar o problema.
*   **`explorer.py` não encontra `traceroute`**: Verifique se o `traceroute` está instalado e no PATH do sistema.
*   **Problemas de permissão**: Em alguns sistemas, `traceroute` pode requerer privilégios de administrador para certos tipos de pacotes.

## Contribuindo

Contribuições são bem-vindas! Sinta-se à vontade para abrir issues ou pull requests.
