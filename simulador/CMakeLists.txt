# =============================================================================
# CMakeLists.txt — Build do simulador de SO multitarefa (UTFPR, Sistemas Operacionais)
#
# COMO COMPILAR (é o que fazemos ao vivo na defesa; executar dentro desta pasta):
#
#   Windows (Qt instalado pelo Qt Online Installer em C:\Qt, MinGW 64 bits):
#     1) Abrir o atalho "Qt 6.12.0 (MinGW 13.1.0 64-bit)" do menu Iniciar, OU
#        colocar as ferramentas no PATH do terminal (cmd):
#          set PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;%PATH%
#     2) Configurar (só na primeira vez; o -D diz ao CMake onde está o Qt):
#          cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
#     3) Compilar:
#          cmake --build build
#     4) Rodar os testes do núcleo (não usam Qt):
#          build\sim_tests.exe
#     5) Gerar a pasta standalone (sim_gui.exe + DLLs + plugins do Qt), que roda
#        sem instalar nada (RG5, resposta do professor Q9):
#          cmake --install build --prefix dist
#        e executar dist\sim_gui.exe. Sem janela (plano B da defesa):
#          dist\sim_gui.exe --exportar exemplo_rm.txt <pasta>
#
#   Linux (g++ e Qt 6 do sistema):
#     cmake -S . -B build && cmake --build build && ./build/sim_tests
#
#   Só o núcleo, sem Qt (ex.: máquina sem Qt):
#     cmake -S . -B build -DBUILD_GUI=OFF && cmake --build build
#
# ALVOS
#   simcore   : biblioteca estática com o núcleo da simulação (C++17 puro, SEM Qt).
#   sim_tests : testes do núcleo (tests.cpp), ligados só ao simcore (docs/testes.md).
#   sim_gui   : interface Qt 6 (Widgets + Svg); opcional (BUILD_GUI). Se o Qt não
#               for encontrado, o build avisa e continua só com o núcleo, para que
#               ninguém da dupla fique bloqueado sem Qt.
#
# Requisitos: RG5 (executar sem instalar nada), RG6 (comentários), A4.2 (núcleo
# separado da interface). Decisões: D22 (CMake com simcore/sim_tests/sim_gui),
# D23 (pasta plana), D26 (Qt dinâmico + deploy em pasta).
# =============================================================================

# 3.21 é o mínimo do design (§2). O deploy por qt_generate_deploy_app_script
# exige Qt >= 6.5 (verificado abaixo).
cmake_minimum_required(VERSION 3.21)

project(SimuladorSO
    VERSION 0.1
    DESCRIPTION "Simulador de SO multitarefa com escalonadores RM e EDF"
    LANGUAGES CXX)

# C++17 sem extensões do compilador (-std=c++17 e não -std=gnu++17): garante que
# o código compila igual no MinGW, no MSVC e no g++ do Linux.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sem tipo de build informado, usamos Release: o executável entregue fica
# otimizado e sem informações de depuração.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

option(BUILD_GUI "Compila a interface gráfica (exige Qt 6 com Widgets e Svg)" ON)

# -----------------------------------------------------------------------------
# Avisos do compilador: a regra do projeto é compilar SEM warnings com
# -Wall -Wextra -Wpedantic (CLAUDE.md). A função é aplicada a cada alvo nosso
# (e não globalmente) para não afetar código gerado por terceiros.
# -----------------------------------------------------------------------------
function(sim_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# simcore — núcleo da simulação (Model, Parser, Scheduler, Simulator).
# Biblioteca estática: os testes e a interface usam exatamente o mesmo código.
# Regra: NENHUM arquivo daqui inclui Qt (design.md §1, princípio 1).
# A lista é explícita (sem file(GLOB)) para que adicionar/remover arquivo seja
# uma decisão visível. Model.h é o contrato de dados (design.md §5).
# -----------------------------------------------------------------------------
add_library(simcore STATIC
    Parser.cpp
    Scheduler.cpp
    Simulator.cpp
    # Cabeçalhos listados só para aparecerem nas IDEs.
    Model.h
    Parser.h
    Scheduler.h
    Simulator.h
)
# Includes planos ("Task.h"): a própria pasta é o diretório de include (D23).
target_include_directories(simcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
sim_enable_warnings(simcore)

# -----------------------------------------------------------------------------
# sim_tests — testes do núcleo, sem Qt (docs/testes.md §1 e §8).
# -----------------------------------------------------------------------------
add_executable(sim_tests tests.cpp)
target_link_libraries(sim_tests PRIVATE simcore)
sim_enable_warnings(sim_tests)

# MinGW: o executável de testes é ligado de forma totalmente estática (inclui
# libgcc, libstdc++ e libwinpthread), como fazia o alvo "windows" do Makefile
# antigo. Assim ele roda em qualquer Windows sem as DLLs do MinGW (RG5).
if(MINGW)
    target_link_options(sim_tests PRIVATE -static)
endif()

# -----------------------------------------------------------------------------
# sim_gui — interface Qt 6 (opcional).
# -----------------------------------------------------------------------------
if(BUILD_GUI)
    # Pasta do executável dentro de dist/: "." = raiz de dist/, para o usuário
    # só precisar dar dois cliques em dist\sim_gui.exe. PRECISA vir antes do
    # find_package(Qt6): o Qt grava esse valor no seu script de deploy nesse
    # momento (se vier depois, as DLLs iriam para dist/bin e o .exe não as acharia).
    set(CMAKE_INSTALL_BINDIR ".")
    include(GNUInstallDirs)

    # QUIET: se o Qt não existir, não abortamos; avisamos e seguimos sem GUI.
    find_package(Qt6 6.5 QUIET COMPONENTS Widgets Svg)
endif()

if(BUILD_GUI AND Qt6_FOUND)
    message(STATUS "Qt ${Qt6_VERSION} encontrado: a interface sim_gui será compilada.")

    # Fontes da interface: janela principal (Tela 1), diálogos (Telas 2 a 6)
    # e o desenho único do Gantt para tela, PNG e SVG (A2.4).
    add_executable(sim_gui
        main.cpp
        MainWindow.cpp
        MainWindow.h
        Dialogs.h
        GanttRenderer.cpp
        GanttRenderer.h
    )
    target_link_libraries(sim_gui PRIVATE simcore Qt6::Widgets Qt6::Svg)
    sim_enable_warnings(sim_gui)

    set_target_properties(sim_gui PROPERTIES
        AUTOMOC ON              # o moc do Qt roda sozinho nas classes com Q_OBJECT
        WIN32_EXECUTABLE ON     # aplicativo de janela: sem console preto atrás
        MACOSX_BUNDLE ON
    )

    # MinGW: embute libgcc e libstdc++ no .exe (design.md §2, "Runtime do
    # compilador"). Observação: as DLLs do Qt continuam precisando das DLLs do
    # MinGW (libgcc_s_seh-1, libstdc++-6, libwinpthread-1); o deploy abaixo as
    # copia para dist/ junto com o Qt.
    if(MINGW)
        target_link_options(sim_gui PRIVATE -static-libgcc -static-libstdc++)
    endif()

    # --- Deploy standalone (RG5, D26) --------------------------------------
    # "cmake --install build --prefix dist" copia o sim_gui.exe para dist/ e
    # roda o script gerado pelo Qt, que copia as DLLs do Qt, os plugins
    # (platforms/qwindows.dll, imageformats, ...) e as DLLs do compilador.
    # O executável fica na raiz de dist/ (CMAKE_INSTALL_BINDIR = ".", definido
    # antes do find_package). --no-translations: o programa não carrega as
    # traduções do Qt, então copiá-las só aumentaria a pasta.
    install(TARGETS sim_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE  DESTINATION .
    )
    qt_generate_deploy_app_script(
        TARGET sim_gui
        OUTPUT_SCRIPT sim_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS --no-translations
    )
    install(SCRIPT ${sim_deploy_script})

    # Alternativa manual: "cmake --build build --target windeploy" roda o
    # windeployqt direto sobre build/sim_gui.exe (deixa build/ executável).
    # Útil se o deploy pelo install falhar em alguma máquina.
    if(WIN32)
        find_program(SIM_WINDEPLOYQT NAMES windeployqt windeployqt6
            HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
        if(SIM_WINDEPLOYQT)
            add_custom_target(windeploy
                COMMAND "${SIM_WINDEPLOYQT}" --no-translations "$<TARGET_FILE:sim_gui>"
                DEPENDS sim_gui
                COMMENT "windeployqt: copiando DLLs e plugins do Qt para junto do sim_gui.exe"
                VERBATIM)
        endif()
    endif()
elseif(BUILD_GUI)
    # Não é erro: o núcleo e os testes continuam compilando (planejamento.md 0.5).
    message(WARNING
        "Qt 6 (>= 6.5, com Widgets e Svg) NÃO foi encontrado: compilando só simcore "
        "e sim_tests, sem a interface. Para a interface, informe o Qt com "
        "-DCMAKE_PREFIX_PATH=<pasta do Qt>/mingw_64 (ex.: C:/Qt/6.12.0/mingw_64).")
else()
    message(STATUS "BUILD_GUI=OFF: compilando só simcore e sim_tests.")
endif()
