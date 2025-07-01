cmake_minimum_required(VERSION 3.16)
project(CartografoCpp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Charts Test) # Adicionado Test

# Opção para construir testes
option(BUILD_TESTS "Build CTest tests" ON) # Ligar por padrão, pode ser desligado com -DBUILD_TESTS=OFF

set(PROJECT_SOURCES
    src/main.cpp
    src/Core/graph_manager.cpp
    src/Core/traceroute_task.cpp
    src/Core/geoip_client.cpp
    src/Core/logging.cpp
    src/GUI/main_window.cpp
    src/GUI/welcome_dialog.cpp
    src/GUI/map_view.cpp
    src/GUI/node_item.cpp
    src/GUI/edge_item.cpp
    src/GUI/details_panel.cpp
    src/GUI/new_exploration_dialog.cpp
)

set(PROJECT_HEADERS
    src/Core/data_types.h
    src/Core/graph_manager.h
    src/Core/traceroute_task.h
    src/Core/geoip_client.h
    src/Core/logging.h
    src/GUI/main_window.h
    src/GUI/welcome_dialog.h
    src/GUI/map_view.h
    src/GUI/node_item.h
    src/GUI/edge_item.h
    src/GUI/details_panel.h
    src/GUI/new_exploration_dialog.h
)

set(PROJECT_RESOURCES
    resources/app.qrc
)

set(PROJECT_UI_FILES
    src/GUI/main_window.ui
    src/GUI/welcome_dialog.ui
    src/GUI/details_panel.ui
    src/GUI/new_exploration_dialog.ui
)

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${PROJECT_RESOURCES}
    ${PROJECT_UI_FILES}
)

# Adicionar headers ao target principal para que IDEs os reconheçam melhor
# e para garantir que o MOC possa encontrá-los se houver alguma configuração incomum.
# No entanto, com CMAKE_AUTOMOC ON, isso geralmente não é estritamente necessário
# se os headers forem incluídos pelos .cpp listados em PROJECT_SOURCES.
# target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_HEADERS}) # Comentado por enquanto, confiar no AUTOMOC
# Se os headers em target_sources no tests/CMakeLists.txt forem suficientes para os testes,
# e os includes nos .cpp do app principal forem suficientes para o MOC, está ok.

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Configuração de Testes
if(BUILD_TESTS)
    message(STATUS "Configurando para construir testes...")
    enable_testing() # Habilita CTest
    add_subdirectory(tests)
else()
    message(STATUS "Pulando configuração de testes (BUILD_TESTS=OFF).")
endif()

message(STATUS "Configurado para Qt version: ${Qt6_VERSION}")
message(STATUS "Usando C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Fontes do projeto (cpp): ${PROJECT_SOURCES}")
message(STATUS "Arquivos UI: ${PROJECT_UI_FILES}")
