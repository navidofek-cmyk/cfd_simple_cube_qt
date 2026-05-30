cmake_minimum_required(VERSION 3.16)
project(cfd_cube_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # automatické zpracování Qt meta-objektů (signály/sloty)

# ── Qt5 (Widgets + Charts) ─────────────────────────────────────────────
find_package(Qt5 REQUIRED COMPONENTS Widgets Charts)

# ── OpenMP (řešič ho používá) ──────────────────────────────────────────
find_package(OpenMP)

# ── Zdroje řešiče ze sourozenecké složky ───────────────────────────────
# Jeden zdroj pravdy: kód řešiče se kompiluje z ../cfd_simple_cube.
set(SOLVER_DIR "${CMAKE_SOURCE_DIR}/../cfd_simple_cube" CACHE PATH
    "Cesta k projektu cfd_simple_cube (řešič)")

if(NOT EXISTS "${SOLVER_DIR}/include/Simple.hpp")
    message(FATAL_ERROR
        "Řešič nenalezen v ${SOLVER_DIR}. "
        "Nastav -DSOLVER_DIR=/cesta/k/cfd_simple_cube")
endif()

add_library(cfdcore STATIC
    ${SOLVER_DIR}/src/Simple.cpp
    ${SOLVER_DIR}/src/io.cpp
)
target_include_directories(cfdcore PUBLIC ${SOLVER_DIR}/include)
if(OpenMP_CXX_FOUND)
    target_link_libraries(cfdcore PUBLIC OpenMP::OpenMP_CXX)
endif()

# ── GUI aplikace ───────────────────────────────────────────────────────
add_executable(cfd_cube_qt
    src/main.cpp
    src/MainWindow.cpp
    src/SolverWorker.cpp
    src/FieldView.cpp
    src/FieldData.cpp
    src/DomainView3D.cpp
)
target_link_libraries(cfd_cube_qt PRIVATE
    cfdcore
    Qt5::Widgets
    Qt5::Charts
)
