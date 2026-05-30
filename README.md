# cfd_cube_qt — Qt GUI pro CFD řešič

Nativní **Qt 5 C++** grafické rozhraní k řešiči
[`cfd_simple_cube`](../cfd_simple_cube) (3D obtékání krychle, FVM + SIMPLE).
Spustíš/zastavíš simulaci, sleduješ **živě** konvergenci, procházíš 3D doménu
a uložíš výsledky.

## Co umí

- **Spuštění / zastavení** řešiče (výpočet běží ve vlastním vlákně, GUI zůstává responzivní)
- **Editovatelné zadání** — Re, U_in, síť (NX/NY/NZ), rozměry domény, poloha
  a velikost krychle, relaxace αU/αP, max. iterací, tolerance
- **Vizualizace polí** — rychlost (`|U|`, `u_x`, `u_y`, `u_z`), **tlak `p`**,
  kontinuitní **reziduum**, a **síť** (mřížka + pevné buňky)
- **Procházení 3D doménou** — volba roviny řezu (XY / XZ / YZ) a posuvník indexu
- **Živý graf konvergence** (QtCharts, logaritmická osa)
- **Uložení výsledků** — `channel_cube.vtk`, `fields.dat`, `mesh.dat`
- **Přepínání režimu přenosu dat**:
  - *Plné 3D pole* — worker pošle celý snapshot, GUI pak řeže lokálně (plynulé
    procházení rovin a veličin bez přepočtu),
  - *Rychlý 2D náhled* — worker posílá jen aktuální 2D řez (lehčí přenos pro
    velké sítě).

## Architektura

```
MainWindow  ──signál startSolver──▶  SolverWorker (QThread)
    ▲                                     │ drží ChannelSolver
    │  iteration / snapshotReady          │ callback onIteration každou iteraci
    │  sliceReady / finished / saved      │ (snapshot/řez podle režimu)
    └─────────────────────────────────────┘
        volba pohledu/režimu/stop ── atomicky (funguje i za běhu smyčky)
```

| Soubor | Role |
|--------|------|
| `src/main.cpp` | vstupní bod (QApplication, registrace metatypů) |
| `src/MainWindow.*` | okno: zadání + ovládání + FieldView + graf |
| `src/SolverWorker.*` | obal nad ChannelSolver ve vlákně, snapshot/řez/uložení |
| `src/FieldView.*` | barevná mapa řezu (QPainter, turbo colormap + colorbar) |
| `src/FieldData.*` | sdílené typy (`FieldSnapshot`, `SliceData`) + `makeSlice` |
| `CMakeLists.txt` | build, link na řešič z `../cfd_simple_cube` |

## Sestavení

### 1. Závislosti (Qt 5)

```bash
sudo apt install qtbase5-dev libqt5charts5-dev cmake g++
```

> Pozn.: na některých distribucích se balíček jmenuje `qtcharts5-dev`.

### 2. Build

```bash
cmake -B build
cmake --build build -j
./build/cfd_cube_qt
```

Callback hook v řešiči (`ChannelSolver::onIteration`) je **už přidaný** — GUI
dostává živé updaty a Stop funguje předčasným ukončením smyčky. Hook je
neinvazivní: bez nastavení se řešič chová stejně jako dřív (CLI `channelCube`).

## Poznámky

- Závislost na řešiči je přes `../cfd_simple_cube` (CMake `SOLVER_DIR`).
  Lze přepsat: `cmake -B build -DSOLVER_DIR=/cesta/k/cfd_simple_cube`.
- Výchozí síť je malá (80×20×20) pro rychlý běh; pro jemnější síť zvětši NX/NY/NZ
  a případně přepni na *Rychlý 2D náhled* (menší přenos dat z výpočtu).
- Viz [`DISCUSSION.md`](DISCUSSION.md) pro plán a kontext (projekt vznikl jako
  ukázka na pohovor).
```
