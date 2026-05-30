# Diskuze a plán — Qt GUI pro CFD řešič

Tento dokument shrnuje, jak projekt vznikl a jaký je plán pro Qt GUI.
Navazuje na řešič `cfd_simple_cube` (sourozenecká složka).

---

## Kontext

`cfd_simple_cube` je hotový 3D CFD řešič (metoda konečných objemů, SIMPLE,
OpenMP) obtékání krychle v tunelu při Re = 20. Má C++ jádro, Python vizualizaci
a dokumentaci (FastAPI + statický web na GitHub Pages).

**Tento projekt (`cfd_simple_cube_qt`)** je samostatná nativní **Qt C++ GUI**
nadstavba — slouží jako ukázka na **pracovní pohovor** (C++, Qt, vlákna,
integrace s existujícím kódem).

## Co GUI má umět

```
┌─────────────────────────────────────────────────────────┐
│  CFD Cube — Qt                                            │
├───────────────┬─────────────────────────────────────────┤
│ Parametry     │   ┌─────────────────────────────────┐    │
│ Re:    [20  ] │   │   FieldView                       │   │
│ Síť:   [80  ] │   │   barevná mapa řezu (QPainter)    │   │
│ U_in:  [1.0 ] │   │   |U| / p / u_x ...               │   │
│ řez:   [XY▾]  │   └─────────────────────────────────┘    │
│               │   ┌─────────────────────────────────┐    │
│ [Spustit]     │   │  graf konvergence (QtCharts) 📉   │   │
│ [Stop]        │   └─────────────────────────────────┘    │
│ iter: 142     │                                           │
│ res: 8.7e-05  │                                           │
└───────────────┴─────────────────────────────────────────┘
```

- **Panel parametrů** — Re, velikost sítě, vstupní rychlost, volba řezu/veličiny.
- **FieldView** — barevná mapa 2D řezu polem (vlastní `QWidget` + `QPainter`),
  bez externích závislostí.
- **Graf konvergence** — `QtCharts` s logaritmickou osou, aktualizovaný živě.
- **Běh ve vlákně** — řešič běží v `QThread`/worker objektu, GUI zůstává
  responzivní; reziduum a pole se posílají přes signály/sloty.

## Architektura

```
        Qt GUI vlákno                     Worker vlákno
   ┌──────────────────────┐         ┌────────────────────────┐
   │ MainWindow            │ start   │ SolverWorker            │
   │  - panel parametrů    │────────▶│  drží ChannelSolver     │
   │  - FieldView          │◀────────│  spustí solve()         │
   │  - QChart konvergence │ signály │  callback každou iteraci│
   └──────────────────────┘         └────────────────────────┘
        (signals/slots = bezpečné předání dat mezi vlákny)
```

## Klíčová rozhodnutí (k upřesnění)

1. **Závislost na řešiči** — varianty:
   - *git submodule* `cfd_simple_cube` do `external/` (jeden zdroj pravdy),
   - *sdílená knihovna* `libcfdcube.a`,
   - *kopie* zdrojů.
   Zatím scaffold odkazuje na sourozeneckou složku `../cfd_simple_cube` přes
   CMake (viz `CMakeLists.txt`).

2. **Živé napojení** — doporučeno: **callback hook** v `ChannelSolver::solve()`,
   volaný každou iteraci. Vyžaduje drobný přídavek do řešiče (viz níže).

3. **Git** — samostatný repozitář (rozhodne se později).

## Potřebný drobný přídavek do řešiče (callback hook)

Aby GUI dostávalo živé updaty, `ChannelSolver` potřebuje volitelný callback.
Návrh (do `Simple.hpp`):

```cpp
#include <functional>
// volitelný callback: (iterace, rel. reziduum) — volán každou iteraci
std::function<void(int, double)> onIteration;
```

A v `Simple.cpp` na konci smyčky:

```cpp
if (onIteration) onIteration(it, cont/contInit);
```

Je to neinvazivní (když není nastaven, nic se neděje) a využitelné i jinde.
Scaffold `SolverWorker` s tímto hookem počítá.

## Stav

> **Toto je startovní scaffold** — struktura projektu, kód GUI a CMake.
> Pro překlad je potřeba doinstalovat Qt dev balíčky:
>
> ```bash
> sudo apt install qtbase5-dev qtcharts5-dev cmake
> ```
>
> a přidat callback hook do řešiče (viz výše).
