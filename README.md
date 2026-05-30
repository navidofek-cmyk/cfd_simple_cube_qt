# cfd_cube_qt — Qt GUI pro CFD řešič

Nativní **Qt 5 / C++20** grafické rozhraní k řešiči
[`cfd_simple_cube`](https://github.com/navidofek-cmyk/cfd_simple_cube)
(3D obtékání krychle v kanálu, metoda konečných objemů + SIMPLE, OpenMP).
Spustíš a zastavíš simulaci, sleduješ **živě** konvergenci, **procházíš 3D
doménu** (řezy i drátový 3D náhled), měníš zadání a ukládáš výsledky.

Součástí je i **webový průvodce kódem** (FastAPI + `uv`), který rozepisuje a
komentuje úplně všechny části kódu — viz [Webová dokumentace](#-webová-dokumentace-průvodce-kódem).

> Projekt vznikl jako **ukázka na pracovní pohovor** (C++, Qt, vlákna,
> integrace s existujícím kódem, build systém, dokumentace). Viz
> [`DISCUSSION.md`](DISCUSSION.md) pro kontext a plán.

---

## Obsah

- [Co aplikace umí](#-co-aplikace-umí)
- [Náhled rozhraní](#-náhled-rozhraní)
- [Architektura](#-architektura)
- [Struktura zdrojového kódu](#-struktura-zdrojového-kódu)
- [Sestavení](#-sestavení)
- [Spuštění (vč. SSH / X11 / VNC)](#-spuštění)
- [Ovládání krok za krokem](#-ovládání-krok-za-krokem)
- [Paralelizace (OpenMP)](#-paralelizace-openmp)
- [Napojení na řešič (callback hook)](#-napojení-na-řešič-callback-hook)
- [Webová dokumentace (průvodce kódem)](#-webová-dokumentace-průvodce-kódem)
- [Výstupní soubory](#-výstupní-soubory)
- [Řešení potíží](#-řešení-potíží)
- [Co by se dalo dotáhnout](#-co-by-se-dalo-dotáhnout)
- [Licence a poznámky](#-licence-a-poznámky)

---

## ✨ Co aplikace umí

| Oblast | Detail |
|--------|--------|
| **Spuštění / zastavení** | ▶ Spustit, ■ Stop (okamžité, přes callback), ✕ Konec (s potvrzením, pokud výpočet běží) |
| **Editovatelné zadání** | Re, vstupní rychlost U_in, síť NX/NY/NZ, rozměry domény Lx/Ly/Lz, poloha a velikost krychle, relaxace αU/αP, max. iterací, tolerance |
| **Vizualizace polí** | rychlost `\|U\|`, složky `u_x` / `u_y` / `u_z`, **tlak `p`**, kontinuitní **reziduum**, a **síť** (mřížka + pevné buňky) |
| **Procházení 3D domény** | volba roviny řezu **XY / XZ / YZ** + posuvník indexu řezu |
| **3D náhled domény** | drátový model tunelu + krychle + **mřížka sítě**, otáčení myší, zoom kolečkem; **živý náhled geometrie ještě před výpočtem** |
| **Konvergence živě** | graf kontinuitního rezidua (QtCharts, logaritmická osa), aktualizovaný každou iteraci |
| **Paralelizace** | počet vláken výpočtu (OpenMP) volitelný z GUI |
| **Frekvence obnovy** | jak často (po kolika iteracích) se obnoví obraz pole |
| **Uložení výsledků** | `channel_cube.vtk` (ParaView), `fields.dat` (text/Python), `mesh.dat` |
| **Režim přenosu dat** | *Plné 3D pole* (plynulé lokální procházení) ↔ *Rychlý 2D náhled* (lehčí přenos pro velké sítě) |

---

## 🖼 Náhled rozhraní

```
┌───────────────────────────────────────────────────────────────────────┐
│ CFD Cube — Qt                                                           │
├────────────────┬──────────────────────────────────────────────────────┤
│ Zadání         │  Zobrazení: [2D řez ▾]  Veličina: [|U| ▾]  Rovina:[XY▾]│
│ Re:     [ 20 ] │  Řez: ──────●─────────  12/20      Data: [Plné 3D ▾]   │
│ U_in:   [1.0 ] │ ┌──────────────────────────────────────────────────┐  │
│ NX:     [ 80 ] │ │                                                  ▓ │  │
│ NY:     [ 20 ] │ │       barevná mapa řezu  /  3D drátová doména    ▓ │  │
│ NZ:     [ 20 ] │ │       (QPainter, turbo colormap + colorbar)      ▓ │  │
│ Lx/Ly/Lz …     │ └──────────────────────────────────────────────────┘  │
│ krychle …      │ ┌──────────────────────────────────────────────────┐  │
│ αU/αP …        │ │   graf konvergence (QtCharts, log osa)   📉       │  │
│ vlákna: [ 12 ] │ └──────────────────────────────────────────────────┘  │
│ [▶ Spustit]    │                                                        │
│ [■ Stop]       │  iter 142   res = 8.7e-05                              │
│ [💾 Uložit]    │                                                        │
│ [✕ Konec]      │                                                        │
└────────────────┴──────────────────────────────────────────────────────┘
```

---

## 🏗 Architektura

Výpočet běží ve **vlastním vlákně** (`QThread` + worker objekt), takže GUI
zůstává responzivní. Komunikace probíhá **výhradně přes signály/sloty** (mezi
vlákny bezpečné, *queued connection*). Volbu pohledu a Stop lze měnit **i za
běhu** výpočtu — přes `std::atomic` proměnné (lock-free).

```
            ┌──────────────────────────────────────────────────────────┐
            │                  GUI proces (cfd_cube_qt)                  │
            │   GUI vlákno                         Worker vlákno         │
            │  ┌───────────────────┐    signály   ┌────────────────────┐│
            │  │ MainWindow        │  startSolver  │ SolverWorker       ││
            │  │  · formulář zadání│ ────────────► │  · drží            ││
            │  │  · FieldView (2D) │               │    ChannelSolver   ││
            │  │  · DomainView3D   │  iteration    │  · run()           ││
            │  │  · QChart rezidua │ ◄──────────── │  · callback /iter  ││
            │  │                   │ snapshot/slice│                    ││
            │  └─────────┬─────────┘  finished     └─────────┬──────────┘│
            │            │ setView/setMode/stop (std::atomic)│           │
            │            └───────────────────────────────────┘           │
            └──────────────────────────────────────────────│────────────┘
                                                            │ #include + link (cfdcore)
                                                            ▼
            ┌──────────────────────────────────────────────────────────┐
            │        CFD řešič (cfd_simple_cube → statická knihovna)     │
            │   ChannelSolver::solve()  — SIMPLE smyčka                  │
            │   fvMatrix (7-diag) · CgSolver (CG) · Mesh · io           │
            └──────────────────────────────────────────────────────────┘
```

**Proč právě takto:**
- *Worker object* (ne dědění z `QThread`) — doporučený a čistší Qt vzor.
- *Signály/sloty* — žádné ruční zámky pro předávání dat mezi vlákny; vlastní
  typy registrovány přes `qRegisterMetaType`.
- *Atomiky* — ovládání pohledu/Stop funguje i během blokující smyčky `solve()`.
- *Neinvazivní napojení* — do řešiče přidán jediný volitelný callback (2 řádky),
  bez něj se chová přesně jako dřív.

Podrobné vysvětlení každého souboru najdeš ve [webovém průvodci kódem](#-webová-dokumentace-průvodce-kódem)
(stránka **Architektura** tam má i krok-za-krokem tok dat).

---

## 📁 Struktura zdrojového kódu

```
cfd_simple_cube_qt/
├── CMakeLists.txt          build: Qt5 (Widgets+Charts), OpenMP, link na řešič
├── README.md               tento soubor
├── DISCUSSION.md           kontext a plán projektu
├── src/
│   ├── main.cpp            vstupní bod (QApplication, registrace metatypů)
│   ├── MainWindow.*        hlavní okno: zadání + ovládání + plátna + graf
│   ├── SolverWorker.*      obal řešiče ve vlákně; SimParams; snapshot/řez; uložení
│   ├── FieldData.*         sdílené typy (FieldSnapshot, SliceData) + makeSlice()
│   ├── FieldView.*         2D barevná mapa řezu (QPainter, turbo + colorbar)
│   └── DomainView3D.*      3D drátový náhled domény (QPainter, bez OpenGL)
└── web/                    webový průvodce kódem (FastAPI + uv) — viz níže
```

| Soubor | Řádky | Role |
|--------|------:|------|
| `src/MainWindow.cpp` | ~360 | sestavení UI, propojení vlákna, interakční logika |
| `src/SolverWorker.cpp` | ~110 | běh výpočtu, snapshoty, OpenMP, ukládání |
| `src/FieldView.cpp` | ~100 | heatmapa řezu, turbo colormap, colorbar |
| `src/DomainView3D.cpp` | ~140 | vlastní 3D projekce + mřížka + interakce myší |
| `src/FieldData.cpp` | ~65 | vyřezávání 2D řezu z 3D snapshotu |
| ostatní (`*.hpp`, `main.cpp`) | ~140 | deklarace a vstupní bod |
| **celkem `src/`** | **~912** | |

---

## 🔧 Sestavení

### 1. Závislosti (Qt 5)

```bash
sudo apt install qtbase5-dev libqt5charts5-dev cmake g++
```

> Na některých distribucích se balíček QtCharts jmenuje `qtcharts5-dev`.
> Projekt cílí **Qt 5** (kód používá `namespace QtCharts`; `QMouseEvent::pos()`).

### 2. Konfigurace + build (CMake)

```bash
cmake -B build
cmake --build build -j
```

CMake zkompiluje zdroje řešiče z `../cfd_simple_cube` jako statickou knihovnu
`cfdcore` (jeden zdroj pravdy — žádná kopie kódu). Cestu lze přepsat:

```bash
cmake -B build -DSOLVER_DIR=/cesta/k/cfd_simple_cube
```

Výstupní binárka: `build/cfd_cube_qt`.

---

## ▶ Spuštění

```bash
./build/cfd_cube_qt
# volitelně omezit počet vláken výpočtu (jde i z GUI):
OMP_NUM_THREADS=8 ./build/cfd_cube_qt
```

### Přes SSH

Aplikace je grafická → potřebuje X server. Možnosti:

| Způsob | Jak | Poznámka |
|--------|-----|----------|
| **X11 forwarding** | `ssh -X uživatel@stroj`, pak `./build/cfd_cube_qt` | **MobaXterm** má X server vestavěný — funguje rovnou. Ověř `echo $DISPLAY`. |
| **VNC** | `./build/cfd_cube_qt -platform vnc:size=1120x680` | připoj se VNC klientem na `:5900`, bez X11 |
| **offscreen** | `QT_QPA_PLATFORM=offscreen ./build/cfd_cube_qt` | bez okna (jen test, že naběhne) |

---

## 🎮 Ovládání krok za krokem

1. **Nastav zadání** v levém panelu (Re, U_in, síť, doménu, krychli…).
   Náhled domény se aktualizuje **živě** — hned vidíš, kde krychle bude.
2. Vlevo nahoře přepni **Zobrazení**:
   - **2D řez** — barevná mapa; vyber *Veličinu* a *Rovinu*, posuvníkem *Řez*
     projížděj doménou.
   - **3D doména** — drátový tunel + krychle + mřížka; **táhni myší** pro otočení,
     **kolečkem** přibliž.
3. **▶ Spustit** — výpočet běží ve vlákně, graf reziduí a obraz pole se aktualizují.
   Pohled (veličina/rovina/řez) můžeš měnit **i za běhu**.
4. **■ Stop** — ukončí výpočet po nejbližší iteraci (korektně, ne uprostřed).
5. **💾 Uložit výsledky** — vyber adresář; uloží VTK + fields.dat + mesh.dat.
6. **✕ Konec** — zavře aplikaci (pokud výpočet běží, zeptá se na potvrzení).

---

## ⚡ Paralelizace (OpenMP)

Řešič je paralelizovaný OpenMP (`#pragma omp parallel for` ve všech těžkých
smyčkách). Počet vláken nastavíš **přímo v GUI** (pole „Vlákna (jádra)") —
rozsah 1 … počet jader stroje, výchozí = všechna jádra.

- Počet vláken **nemění výsledek**, jen **rychlost** výpočtu (až na zaokrouhlení
  v posledních cifrách u redukcí).
- Zrychlení není lineární (Amdahlův zákon, paměťová propustnost) — typicky ~8–10×
  na 12 jádrech u dostatečně velké sítě.
- U **velké sítě** se paralelizace vyplatí naplno; u malé je relativní režie větší.

Implementačně: `SolverWorker::run()` volá `omp_set_num_threads(...)` před výpočtem
(viz `#ifdef _OPENMP`, takže jde přeložit i bez OpenMP).

---

## 🔌 Napojení na řešič (callback hook)

Aby GUI dostávalo živé updaty, dostal `ChannelSolver` **jediné rozšíření** —
volitelný callback (přidáno v repu řešiče, 2 řádky + include):

```cpp
// Simple.hpp (do struktury ChannelSolver)
#include <functional>
std::function<bool(int, double)> onIteration;   // (iterace, rel. reziduum) → false = stop
```
```cpp
// Simple.cpp (na konci smyčky solve())
if (onIteration && !onIteration(it, cont/contInit)) break;
```

Je to **neinvazivní**: bez nastaveného callbacku se řešič chová přesně jako dřív
(CLI `channelCube` beze změny). GUI ho využívá pro:
- emitování signálů `iteration` (reziduum) a `snapshotReady` / `sliceReady` (pole),
- **Stop** — vrácením `false` se smyčka ukončí.

---

## 🌐 Webová dokumentace (průvodce kódem)

V `web/` je **FastAPI** web (spravovaný přes `uv`), který **rozepisuje a
komentuje všechny části kódu** projektu (řešič i GUI). Kód se **čte živě z disku**
(žádná duplikace), rozseká se na sekce podle „kotev" a okomentuje česky; zvýraznění
přes Pygments. Obsahuje i stránku **Architektura** s diagramem a tokem dat.

### Dynamicky (FastAPI)

```bash
cd web
uv run python server.py          # nebo: uv run uvicorn server:app --reload
```
→ běží na **portu 8085**, host `0.0.0.0` (přes SSH: `http://<IP-stroje>:8085`).

### Staticky (GitHub Pages)

```bash
cd web
uv run python build_static.py    # vygeneruje web/site/
python3 -m http.server -d site 8090   # lokální náhled
```

`build_static.py` vyrenderuje celý web do `web/site/` s **relativními odkazy**
a navíc do `web/site/theory/` zkopíruje **teoretický web řešiče**
(`cfd_simple_cube/docs/site` — teorie: Navier–Stokes, FVM, SIMPLE, Rhie–Chow, CG…),
takže je **vše na jednom místě** jako jeden hostovatelný celek.

> Dvě úrovně dokumentace se doplňují: **tento web** vysvětluje *zdrojový kód*
> (co dělá který soubor a proč), **teoretický web řešiče** vysvětluje *fyziku a
> algoritmus*.

---

## 📤 Výstupní soubory

Tlačítko **💾 Uložit výsledky** zapíše do zvoleného adresáře:

| Soubor | Formát | K čemu |
|--------|--------|--------|
| `channel_cube.vtk` | VTK STRUCTURED_POINTS (ASCII) | ParaView / VisIt — rychlost, tlak, reziduum, maska |
| `fields.dat` | text (řádek na buňku) | Python/NumPy — `i j k x y z ux uy uz p res solid` |
| `mesh.dat` | text | geometrie sítě + maska pevných buněk |

---

## 🧯 Řešení potíží

| Problém | Řešení |
|---------|--------|
| `find_package(Qt5 …)` selže | doinstaluj `qtbase5-dev libqt5charts5-dev` |
| `Řešič nenalezen v …` | nastav `cmake -B build -DSOLVER_DIR=/cesta/k/cfd_simple_cube` |
| Okno se neotevře přes SSH | `echo $DISPLAY` prázdné → zapni X11 forwarding (MobaXterm: ikona „X" + X11-Forwarding), nebo použij `-platform vnc` |
| GUI „zamrzne" při velké síti | přepni **Data → Rychlý 2D náhled**, zvyš „Obnova pole á (iter)" |
| Linker hlásí chybějící `onIteration` | v repu řešiče chybí callback hook — viz [Napojení na řešič](#-napojení-na-řešič-callback-hook) |
| Web hlásí 500 | použij novější Starlette API `TemplateResponse(request, name, ctx)` (už opraveno) |

---

## 🚧 Co by se dalo dotáhnout

- Throttling snapshotů podle toho, jestli GUI stíhá vykreslovat.
- Jednotkové testy na `makeSlice()` (čistá funkce, snadno testovatelná).
- Export obrázku aktuálního pohledu do PNG.
- Volba „obnova pole" i za běhu (nyní se čte při startu).

---

## 📜 Licence a poznámky

- Závislost na řešiči je přes `../cfd_simple_cube` (CMake `SOLVER_DIR`).
- Výchozí síť je malá (80×20×20) pro rychlý běh; pro jemnější zvětši NX/NY/NZ
  a případně přepni na *Rychlý 2D náhled*.
- Repozitáře:
  [Qt GUI](https://github.com/navidofek-cmyk/cfd_simple_cube_qt) ·
  [řešič](https://github.com/navidofek-cmyk/cfd_simple_cube)
- Kontext a plán: [`DISCUSSION.md`](DISCUSSION.md).
```
