cmake_minimum_required(VERSION 3.16)
project(CartografoCppTests)

# Encontrar Qt Test
find_package(Qt6 REQUIRED COMPONENTS Test Core Json) # Json para testar parsing

# Adicionar o executável de teste
add_executable(cartografo_tests
    # Adicione seus arquivos .cpp de teste aqui
    test_example.cpp # Um teste de exemplo inicial
    # test_graph_manager.cpp # Exemplo para GraphManager
)

# Vincular bibliotecas Qt necessárias para os testes
target_link_libraries(cartografo_tests PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Json
    # Se os testes precisarem de classes do projeto principal,
    # precisamos vincular o target do projeto principal ou seus objetos.
    # Para simplificar, podemos compilar as classes do Core diretamente nos testes
    # ou criar uma biblioteca estática/compartilhada para o Core.
    # Por agora, vamos adicionar os .cpp do Core diretamente aqui se necessário,
    # ou melhor, incluir os diretórios e deixar o linker encontrá-los se
    # estivermos construindo como parte do mesmo projeto CMake maior.
    # Se o projeto principal é ${PROJECT_NAME_MAIN_APP} (ex: CartografoCpp)
    # target_link_libraries(cartografo_tests PRIVATE ${PROJECT_NAME_MAIN_APP}) # Se CartografoCpp fosse uma lib
)

# Incluir diretórios do projeto principal para que os headers sejam encontrados
target_include_directories(cartografo_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src" # Acessa src/ a partir de tests/
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core"
    # Adicione outros diretórios de include se necessário
)

# Adicionar os fontes do Core diretamente ao target de teste para que possam ser testados
# Isso é mais simples do que criar uma biblioteca separada para o Core apenas para testes iniciais.
target_sources(cartografo_tests PRIVATE
    ../src/Core/data_types.h # Headers precisam ser visíveis
    ../src/Core/graph_manager.h
    ../src/Core/graph_manager.cpp
    # Adicione outros arquivos .cpp/.h do Core que são testados
)


# Adicionar o teste ao CTest
include(CTest)
add_test(NAME RunCartografoTests COMMAND cartografo_tests)

# Mensagem de status
message(STATUS "Configurando testes Qt para o projeto.")
