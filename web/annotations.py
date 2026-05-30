"""
Obsah průvodce kódem (anotace).

Filozofie: web NIKDY needituje ani neduplikuje zdrojový kód — čte reálné soubory
z disku. Tady jsou jen *komentáře* k jednotlivým úsekům. Každá sekce má „kotvu"
(unikátní úryvek kódu); renderer kotvu najde a zobrazí kód od ní po další kotvu.
Díky tomu se anotace nerozejdou s kódem ani po přečíslování řádků.

Schéma:
    FILES[slug] = {
        title, path, lang, group, summary, role,
        sections: [ {heading, anchor, note}, ... ]
    }
"""

from pathlib import Path

# Kořeny obou projektů (web/ leží v cfd_simple_cube_qt/web)
WEB_DIR    = Path(__file__).resolve().parent
GUI_DIR    = WEB_DIR.parent                       # cfd_simple_cube_qt
SOLVER_DIR = GUI_DIR.parent / "cfd_simple_cube"   # sourozenecký řešič

# Skupiny (pořadí = pořadí v navigaci)
GROUPS = [
    "Řešič — základní typy",
    "Řešič — lineární algebra",
    "Řešič — SIMPLE",
    "Řešič — I/O a vstup",
    "GUI — data a vlákno",
    "GUI — vykreslování",
    "GUI — okno a sestavení",
]

FILES: dict[str, dict] = {}

def _add(slug, *, title, path, lang, group, summary, role, sections):
    FILES[slug] = dict(title=title, path=path, lang=lang, group=group,
                       summary=summary, role=role, sections=sections)


# ──────────────────────────────────────────────────────────────────────────
# ŘEŠIČ — ZÁKLADNÍ TYPY
# ──────────────────────────────────────────────────────────────────────────

_add("vec3",
    title="Vec3.hpp",
    path=SOLVER_DIR / "include/Vec3.hpp",
    lang="cpp", group="Řešič — základní typy",
    summary="Minimalistický 3D vektor (double x,y,z) s operátory.",
    role="Reprezentuje vektorové veličiny — hlavně rychlost U=(u,v,w) v každé buňce.",
    sections=[
        {"heading": "Datová struktura", "anchor": "struct Vec3",
         "note": "Prostá agregace tří doublů s výchozí inicializací na nulu. "
                 "Žádné virtuální metody, žádná dynamická alokace — je to POD-like "
                 "typ, který se vejde do registru a dá se levně kopírovat. To je "
                 "důležité, protože vektorů jsou miliony (jeden na buňku)."},
        {"heading": "Aritmetické operátory", "anchor": "inline Vec3  operator+",
         "note": "Sčítání/odčítání po složkách a násobení skalárem zleva (s*a). "
                 "Volné funkce (ne členské) + `inline` → zero-overhead, kompilátor "
                 "je vloží přímo. Umožňují psát fyziku čitelně: `U[c] = HbyA[c] - rAU[c]*gp[c]`."},
        {"heading": "Pomocné normy", "anchor": "inline double mag2",
         "note": "`mag2` = kvadrát velikosti (bez odmocniny — levné, stačí pro "
                 "porovnání a sumy reziduí). Přetížené pro double i Vec3, takže "
                 "stejný kód funguje pro skalární i vektorová pole."},
    ])

_add("field",
    title="Field.hpp",
    path=SOLVER_DIR / "include/Field.hpp",
    lang="cpp", group="Řešič — základní typy",
    summary="Generické pole hodnot nad buňkami (šablona).",
    role="Jednotné úložiště pro skalární (tlak) i vektorová (rychlost) pole.",
    sections=[
        {"heading": "Šablona Field<T>", "anchor": "struct Field",
         "note": "Tenká obálka nad `std::vector<T>` s indexací `operator[]`. "
                 "Smysl: typová bezpečnost a čitelnost — `volScalar` vs `volVector` "
                 "místo holého vektoru. Drží se RAII (vektor se sám uvolní)."},
        {"heading": "Aliasy typů", "anchor": "using volScalar",
         "note": "`volScalar = Field<double>` (tlak, reziduum, toky), "
                 "`volVector = Field<Vec3>` (rychlost). Pojmenování „vol\" = "
                 "volume field (hodnota na objem buňky), konvence z OpenFOAM."},
    ])

_add("mesh",
    title="Mesh.hpp",
    path=SOLVER_DIR / "include/Mesh.hpp",
    lang="cpp", group="Řešič — základní typy",
    summary="Uniformní 3D kartézská mřížka s maskou pevné překážky.",
    role="Geometrie výpočtu: rozměry buněk, plochy stěn, objem, mapování (i,j,k)→index, krychle.",
    sections=[
        {"heading": "Geometrické veličiny", "anchor": "struct Mesh",
         "note": "Mřížka je *kolokovaná* a *uniformní*: dx,dy,dz jsou konstantní. "
                 "Předpočítané `Ax,Ay,Az` (plochy stěn) a `V` (objem buňky) se "
                 "používají všude ve FVM diskretizaci — počítají se jednou v konstruktoru."},
        {"heading": "Konstruktor", "anchor": "Mesh(int nx_",
         "note": "Z rozměrů domény (Lx,Ly,Lz) a počtu buněk odvodí krok mřížky. "
                 "`solid` je bitová maska velikosti N, na začátku vše false (tekuté). "
                 "`std::vector<bool>` je zde úsporný (1 bit/buňka)."},
        {"heading": "Indexace", "anchor": "int id(int i",
         "note": "Klíčová funkce: 3D index (i,j,k) → 1D index do polí. Pořadí "
                 "`i + j*nx + k*nx*ny` znamená, že nejrychleji se mění i (osa x). "
                 "Tohle pořadí určuje cache-efektivitu všech smyček v solveru."},
        {"heading": "Označení překážky", "anchor": "void markSolid",
         "note": "Z fyzikálních souřadnic krychle spočítá indexy buněk a nastaví "
                 "`solid=true`. Zaokrouhlení `(int)(v+0.5)` = na nejbližší buňku — "
                 "tuto logiku replikuje i GUI náhled domény, aby seděl s realitou."},
    ])


# ──────────────────────────────────────────────────────────────────────────
# ŘEŠIČ — LINEÁRNÍ ALGEBRA
# ──────────────────────────────────────────────────────────────────────────

_add("fvmatrix",
    title="fvMatrix.hpp",
    path=SOLVER_DIR / "include/fvMatrix.hpp",
    lang="cpp", group="Řešič — lineární algebra",
    summary="Řídká 7-diagonální matice pro 3D strukturovanou mřížku.",
    role="Reprezentuje soustavu A·ψ = b z diskretizace rovnic. Jádro celého řešiče.",
    sections=[
        {"heading": "Uložení matice", "anchor": "struct fvMatrix",
         "note": "Matice se NEukládá hustě (to by byla N×N, tj. miliardy prvků). "
                 "Místo toho 7 polí koeficientů: `aP` (diagonála) + 6 sousedů "
                 "(E/W/N/S/F/B). Tomu se říká *stencil* uložení — pro 3D Laplacián "
                 "ideální, paměť je jen 7·N. `psi` je reference na řešené pole."},
        {"heading": "Konstruktor — alokace stencilu", "anchor": "fvMatrix(const Mesh",
         "note": "Alokuje 7 polí koeficientů + zdroj, vše vynulované na velikost N. "
                 "Drží reference na mřížku a řešené pole (`psi`) — matice „ví\", "
                 "nad jakou geometrií a polem pracuje. Samotné maticové násobení "
                 "(SpMV) je v CG řešiči (CgSolver, lambda negAv) — tam je výkonově kritické."},
        {"heading": "Diagonála A()", "anchor": "volScalar A() const",
         "note": "Vrátí diagonálu aP (dělenou objemem). V SIMPLE je `A` koeficient u "
                 "centrální buňky hybnostní rovnice — potřebný pro rAU = 1/A. "
                 "Paralelní smyčka přes všechny buňky."},
        {"heading": "Operátor H()", "anchor": "Field<T> H() const",
         "note": "H = zdroj − mimodiagonální příspěvky (vše kromě aP·ψ). V SIMPLE "
                 "se z toho dělá HbyA = H/A, což je rychlost bez vlivu tlaku. "
                 "Klíčový krok Rhie-Chow interpolace (potlačení šachovnicového tlaku)."},
        {"heading": "Podrelaxace relax()", "anchor": "void relax",
         "note": "Patankarova podrelaxace: aP/α a zbytek do pravé strany. "
                 "Stabilizuje nelineární SIMPLE iterace (α<1 brzdí změny). "
                 "Bez ní by řešení divergovalo. Pozor: mění matici → exportuje se "
                 "raw verze bez relaxace."},
        {"heading": "Řešení — paralelní Jacobi", "anchor": "void solveWithExtra",
         "note": "Iterační řešič hybnostní rovnice (ne CG — ta je jen pro symetrickou "
                 "tlakovou matici). Komentář v kódu zdůrazňuje proč Jacobi a ne "
                 "Gauss-Seidel: každé vlákno čte STARÉ hodnoty psi a zapisuje do `tmp` "
                 "→ žádný race condition, plně paralelizovatelné (`swap` na konci sweepu). "
                 "`extra/scale` přidá gradient tlaku jako zdroj. Konverguje na rel. reziduum 1e-3."},
    ])

_add("cgsolver",
    title="CgSolver.hpp",
    path=SOLVER_DIR / "include/CgSolver.hpp",
    lang="cpp", group="Řešič — lineární algebra",
    summary="Hlavička CG řešiče tlakové rovnice + workspace.",
    role="Řeší symetrickou pozitivně definitní soustavu (tlaková Poissonova rovnice).",
    sections=[
        {"heading": "Workspace", "anchor": "struct CgWorkspace",
         "note": "Drží pomocné vektory CG (r=reziduum, p=směr, Ap, z=předpodmíněné). "
                 "`ensure(n)` alokuje jen jednou (znovupoužití mezi iteracemi SIMPLE) "
                 "— v hot-loopu nechceme opakované alokace."},
        {"heading": "Deklarace cgSolve", "anchor": "void cgSolve",
         "note": "Jacobiho předpodmíněný Conjugate Gradient. Tlaková matice je SPD, "
                 "takže CG je optimální volba (rychlejší než Gauss-Seidel). "
                 "Implementace je v Simple.cpp."},
    ])


# ──────────────────────────────────────────────────────────────────────────
# ŘEŠIČ — SIMPLE
# ──────────────────────────────────────────────────────────────────────────

_add("simple_h",
    title="Simple.hpp",
    path=SOLVER_DIR / "include/Simple.hpp",
    lang="cpp", group="Řešič — SIMPLE",
    summary="Deklarace ChannelSolver — stav a rozhraní řešiče.",
    role="Drží všechna pole, parametry a okrajové podmínky úlohy obtékání krychle.",
    sections=[
        {"heading": "Popis okrajových podmínek", "anchor": "// SIMPLE solver",
         "note": "Komentář shrnuje fyziku: vtok (Dirichlet rychlost), výtok "
                 "(Neumann rychlost, p=0), stěny a krychle (no-slip). Re = U·D/ν "
                 "definuje viskozitu. Tohle je „zadání\" celé úlohy."},
        {"heading": "Pole a parametry", "anchor": "struct ChannelSolver",
         "note": "U (rychlost), p (tlak), phiE/N/F (objemové toky stěnami), "
                 "resField (reziduum). Plus Re, viskozita, relaxační faktory, "
                 "limity iterací. Cesty k exportům umožní volitelný zápis matic/konvergence."},
        {"heading": "Callback hook (pro GUI)", "anchor": "std::function<bool(int, double)> onIteration",
         "note": "★ Toto jsem přidal kvůli GUI (2 řádky). Volitelný callback volaný "
                 "každou iteraci s (číslo, reziduum). Vrátí-li false → smyčka skončí "
                 "(tlačítko Stop). Neinvazivní: bez nastavení se solver chová jako dřív. "
                 "Ukázka čistého rozšíření cizího kódu bez zásahu do logiky."},
        {"heading": "Konstruktor", "anchor": "ChannelSolver(int nx",
         "note": "Přebírá geometrii domény, hranice krychle, Re a numerické parametry. "
                 "Výchozí hodnoty (=1.0, =0.7…) umožňují volat ho jednoduše z CLI."},
    ])

_add("simple_cpp",
    title="Simple.cpp",
    path=SOLVER_DIR / "src/Simple.cpp",
    lang="cpp", group="Řešič — SIMPLE",
    summary="Srdce řešiče: diskretizace rovnic + hlavní SIMPLE smyčka.",
    role="Implementuje celý algoritmus SIMPLE pro nestlačitelné proudění (~420 řádků).",
    sections=[
        {"heading": "Konstruktor — inicializace polí a toků", "anchor": "ChannelSolver::ChannelSolver",
         "note": "Spočítá viskozitu z Re, označí krychli, vynuluje rychlost v pevných "
                 "buňkách a inicializuje objemové toky φ z počátečního pole. "
                 "Toky na stěnách (ne hodnoty v buňkách) jsou klíč k FVM bez šachovnice."},
        {"heading": "Hybnostní rovnice buildUEqn()", "anchor": "static fvMatrix<Vec3> buildUEqn",
         "note": "Sestaví matici hybnostní rovnice (konvekce + difúze) pro každou buňku. "
                 "Klíčový komentář v kódu: buňkový přístup — každá buňka počítá VŠECHNY "
                 "své příspěvky sama, jen ČTE sousedy → žádný zápis do sousedů → lze "
                 "paralelizovat jediným pragma bez race condition. Upwind schéma pro "
                 "konvekci (stabilita), centrální pro difúzi. Inlet/outlet jako zdroje."},
        {"heading": "Tlaková rovnice buildPEqn()", "anchor": "static fvMatrix<double> buildPEqn",
         "note": "Sestaví Poissonovu rovnici pro tlakovou korekci: ∇·(rAU·∇p)=∇·HbyA. "
                 "Matice je symetrická → řeší se CG. Outlet má Dirichlet p=0 (kotví "
                 "absolutní hladinu tlaku), zbytek Neumann."},
        {"heading": "Interpolace toků interpFlux()", "anchor": "static void interpFlux",
         "note": "Z buňkových rychlostí HbyA spočítá toky na stěnách (lineární "
                 "interpolace 0.5·(H[c]+H[soused])·plocha). Toto je část Rhie-Chow "
                 "interpolace, která zabraňuje oscilaci tlaku na kolokované mřížce."},
        {"heading": "Divergence divFlux()", "anchor": "static volScalar divFlux",
         "note": "Bilance toků přes 6 stěn buňky = zdroj tlakové rovnice (a zároveň "
                 "kontinuitní reziduum). Inlet přispívá konstantním U·Ax. Když je "
                 "divergence nula → splněna kontinuita (nestlačitelnost)."},
        {"heading": "Gradient tlaku gradP()", "anchor": "static volVector gradP",
         "note": "Spočítá ∇p v každé buňce (centrální diference přes stěny). "
                 "Použije se na korekci rychlosti. Ošetřuje pevné buňky i hranice."},
        {"heading": "Hlavní smyčka solve()", "anchor": "void ChannelSolver::solve",
         "note": "Vlastní SIMPLE iterace. Kroky: (1) hybnost→U*, (2) A a HbyA, "
                 "(3) toky, (4) tlaková rovnice (CG), (5) Rhie-Chow korekce toků, "
                 "(6) korekce rychlosti, (7) podrelaxace tlaku, (8) reziduum. "
                 "Zde je i živý zápis konvergence a — nově — volání callbacku pro GUI."},
        {"heading": "★ Callback hook v akci", "anchor": "if(onIteration",
         "note": "Přidaný háček: po výpočtu rezidua zavolá GUI callback. Vrátí-li "
                 "false (uživatel dal Stop), smyčka se ukončí `break`em. Bez "
                 "nastaveného callbacku se tato podmínka přeskočí (krátké vyhodnocení)."},
    ])


# ──────────────────────────────────────────────────────────────────────────
# ŘEŠIČ — I/O A VSTUP
# ──────────────────────────────────────────────────────────────────────────

_add("io_h",
    title="io.hpp",
    path=SOLVER_DIR / "include/io.hpp",
    lang="cpp", group="Řešič — I/O a vstup",
    summary="Deklarace zápisu výsledků (VTK, textová pole, mřížka, matice).",
    role="Rozhraní pro export dat k vizualizaci a ladění.",
    sections=[
        {"heading": "Funkce zápisu", "anchor": "namespace io",
         "note": "writeVTK (pro ParaView/VisIt), writeFields (text pro Python), "
                 "writeMesh, a export koeficientů matic (diagnostika). GUI volá "
                 "writeVTK/writeFields/writeMesh přes SolverWorker::saveResults."},
    ])

_add("io_cpp",
    title="io.cpp",
    path=SOLVER_DIR / "src/io.cpp",
    lang="cpp", group="Řešič — I/O a vstup",
    summary="Implementace zápisu souborů.",
    role="Serializace polí do formátů VTK a textových .dat.",
    sections=[
        {"heading": "VTK export", "anchor": "void writeVTK",
         "note": "Píše STRUCTURED_POINTS VTK: rychlost (VECTORS), tlak, masku solid "
                 "a reziduum (SCALARS). Standardní formát → otevře ParaView. ASCII "
                 "verze (čitelná, větší); pro 320³ síť to jsou stovky MB."},
        {"heading": "Textový export polí", "anchor": "void writeFields",
         "note": "Řádek na buňku: i j k x y z ux uy uz p res solid. Snadno načte "
                 "Python/NumPy pro vlastní grafy. Hlavička `# DIMS` nese rozměry."},
        {"heading": "Export mřížky", "anchor": "void writeMesh",
         "note": "Geometrie + maska solid bez fyziky — pro vizualizaci sítě a krychle."},
        {"heading": "Export matic", "anchor": "void writePMatrix",
         "note": "Vypíše koeficienty (aP,aE,…,src) tlakové i hybnostní matice. "
                 "Slouží k ověření diskretizace (lze zkontrolovat řádkový součet, "
                 "symetrii, sparsitu). Jen pro malé sítě."},
    ])

_add("main_solver",
    title="main.cpp (řešič)",
    path=SOLVER_DIR / "src/main.cpp",
    lang="cpp", group="Řešič — I/O a vstup",
    summary="CLI vstupní bod samostatného řešiče.",
    role="Spustí jeden běh s pevně danými parametry a uloží výstupy.",
    sections=[
        {"heading": "Definice úlohy", "anchor": "int main",
         "note": "Pevně nastaví síť 320×80×80, tunel 4×1×1 m, krychli 0.2³ uprostřed, "
                 "Re=20. Vypíše hlavičku s parametry. Toto je referenční „produkční\" "
                 "běh; GUI naopak parametry bere z formuláře."},
        {"heading": "Spuštění a uložení", "anchor": "sim.solve()",
         "note": "Nastaví snapshoty/konvergenci, zavolá solve() a uloží VTK+fields+mesh. "
                 "Export matic jen pro malé sítě. Řádkové bufferování stdout → progress "
                 "je vidět i při přesměrování do souboru."},
    ])


# ──────────────────────────────────────────────────────────────────────────
# GUI — DATA A VLÁKNO
# ──────────────────────────────────────────────────────────────────────────

_add("gui_main",
    title="main.cpp (GUI)",
    path=GUI_DIR / "src/main.cpp",
    lang="cpp", group="GUI — data a vlákno",
    summary="Vstupní bod Qt aplikace.",
    role="Vytvoří QApplication, zaregistruje metatypy a zobrazí hlavní okno.",
    sections=[
        {"heading": "Registrace metatypů", "anchor": "qRegisterMetaType",
         "note": "Aby šly vlastní typy (SimParams, SliceData, FieldSnapshot) posílat "
                 "přes signály/sloty MEZI VLÁKNY (queued connection), musí je Qt znát. "
                 "Bez registrace by mezivláknový signál tiše selhal za běhu."},
        {"heading": "Spuštění", "anchor": "MainWindow w",
         "note": "Standardní Qt boilerplate: okno na zásobníku, show(), app.exec() "
                 "spustí event loop (běží do zavření okna)."},
    ])

_add("fielddata_h",
    title="FieldData.hpp",
    path=GUI_DIR / "src/FieldData.hpp",
    lang="cpp", group="GUI — data a vlákno",
    summary="Sdílené datové typy pro přenos polí z výpočtu do GUI.",
    role="Definuje FieldSnapshot (plné 3D pole), SliceData (2D řez) a enumy.",
    sections=[
        {"heading": "Enum veličiny a roviny", "anchor": "enum class FieldKind",
         "note": "FieldKind = co zobrazit (|U|, složky, tlak, reziduum, síť). "
                 "SlicePlane = kterou rovinou řezat 3D doménu (XY/XZ/YZ). Pořadí "
                 "FieldKind ODPOVÍDÁ pořadí položek v comboboxu (přímý cast indexu)."},
        {"heading": "FieldSnapshot — 3D pole", "anchor": "struct FieldSnapshot",
         "note": "Plná kopie polí ve float (z double v solveru). Float stačí na "
                 "vykreslení a je poloviční na přenos. Drží i rozměry a masku solid. "
                 "GUI pak řeže libovolnou rovinu lokálně bez dotazu na worker."},
        {"heading": "Pomocné metody", "anchor": "int  idx(int i",
         "note": "Stejná indexace jako Mesh v řešiči. `sliceCount` vrací rozsah "
                 "posuvníku pro danou rovinu — kolik řezů doménou existuje."},
        {"heading": "SliceData — 2D řez", "anchor": "struct SliceData",
         "note": "Hotový řez k vykreslení: hodnoty, maska solid, min/max pro colorbar. "
                 "Lehčí přenos než celé pole — používá se v režimu „rychlý 2D náhled\"."},
        {"heading": "makeSlice + metatypy", "anchor": "SliceData makeSlice",
         "note": "Čistá funkce (snadno testovatelná): vyřízne 2D řez z 3D snapshotu. "
                 "`Q_DECLARE_METATYPE` zpřístupní typy systému signálů/slotů."},
    ])

_add("fielddata_cpp",
    title="FieldData.cpp",
    path=GUI_DIR / "src/FieldData.cpp",
    lang="cpp", group="GUI — data a vlákno",
    summary="Implementace vyřezávání 2D řezu z 3D pole.",
    role="Mapuje rovinu+index+veličinu na 2D pole hodnot a najde rozsah.",
    sections=[
        {"heading": "Vzorkování veličiny", "anchor": "static float sample",
         "note": "Z buňky vytáhne zvolenou skalární veličinu — |U| počítá z složek, "
                 "ostatní přímo. Oddělení od geometrie řezu = čistý kód."},
        {"heading": "makeSlice", "anchor": "SliceData makeSlice(const FieldSnapshot",
         "note": "Podle roviny zvolí rozměry řezu a mapování (a,b)→(i,j,k), ořízne "
                 "index do platného rozsahu (clamp), projde řez a spočítá min/max "
                 "PŘES TEKUTÉ buňky (pevné by zkreslily rozsah colorbaru)."},
    ])

_add("worker_h",
    title="SolverWorker.hpp",
    path=GUI_DIR / "src/SolverWorker.hpp",
    lang="cpp", group="GUI — data a vlákno",
    summary="Deklarace workeru — obal řešiče běžící ve vlákně.",
    role="Mezivrstva mezi GUI a ChannelSolverem; definuje SimParams a signály.",
    sections=[
        {"heading": "SimParams — zadání", "anchor": "struct SimParams",
         "note": "Všechny editovatelné parametry z formuláře: síť, doména, krychle, "
                 "fyzika, relaxace, limity, počet vláken (OpenMP) a režim přenosu dat. "
                 "Jedna struktura = jeden běh; posílá se workeru přes signál."},
        {"heading": "Třída SolverWorker", "anchor": "class SolverWorker",
         "note": "QObject žijící ve worker vlákně. Veřejné settery (stop/setView/"
                 "setMode) volá GUI vlákno — proto zapisují jen do `std::atomic`, "
                 "aby byly bezpečné i během běžící blokující smyčky solve()."},
        {"heading": "Signály", "anchor": "signals:",
         "note": "iteration (reziduum), snapshotReady (plné pole), sliceReady (řez), "
                 "finished, saved. Signály = JEDINÝ směr komunikace worker→GUI, "
                 "Qt je doručí bezpečně do GUI vlákna (queued connection)."},
        {"heading": "Atomické proměnné", "anchor": "std::atomic<bool> stopRequested_",
         "note": "Stav pohledu/stopu jako atomiky. Worker je čte v callbacku každou "
                 "iteraci, GUI je zapisuje při kliknutí — žádný zámek není potřeba, "
                 "protože jde o nezávislé prosté hodnoty (klasický lock-free pattern)."},
    ])

_add("worker_cpp",
    title="SolverWorker.cpp",
    path=GUI_DIR / "src/SolverWorker.cpp",
    lang="cpp", group="GUI — data a vlákno",
    summary="Implementace běhu výpočtu, snapshotů, OpenMP a ukládání.",
    role="Spojuje řešič s GUI přes callback; staví snapshoty a ukládá výsledky.",
    sections=[
        {"heading": "Konstruktor/destruktor v .cpp", "anchor": "SolverWorker::SolverWorker",
         "note": "Záměrně NE inline: `unique_ptr<ChannelSolver>` na neúplný typ "
                 "(dopředná deklarace v hlavičce) vyžaduje, aby se destruktor "
                 "instancioval tam, kde je typ úplný — tj. v .cpp. Klasická C++ past."},
        {"heading": "Stavba snapshotu", "anchor": "static FieldSnapshot makeSnapshot",
         "note": "Zkopíruje stav řešiče (double→float) do FieldSnapshotu. Volá se "
                 "periodicky během výpočtu i na konci. Kopie je cena za bezpečné "
                 "předání dat mezi vlákny (worker pak může pole dál měnit)."},
        {"heading": "Nastavení pohledu (atomicky)", "anchor": "void SolverWorker::setView",
         "note": "Uloží rovinu/index/veličinu do atomiků. Worker je přečte při "
                 "stavbě řezu — díky tomu lze přepínat pohled i ZA BĚHU výpočtu."},
        {"heading": "Hlavní běh run()", "anchor": "void SolverWorker::run",
         "note": "Nastaví OpenMP vlákna, vytvoří ChannelSolver a — klíčové — "
                 "nastaví mu lambda callback. Lambda emituje signály a podle režimu "
                 "pošle buď celý snapshot, nebo jen 2D řez. Vrací `!stopRequested` "
                 "(false = Stop). Po doběhnutí pošle finální snapshot + finished."},
        {"heading": "Nastavení OpenMP", "anchor": "#ifdef _OPENMP",
         "note": "★ Volba počtu jader z GUI: `omp_set_num_threads`. 0 = všechna "
                 "jádra. `#ifdef` zajistí, že kód jde přeložit i bez OpenMP. "
                 "Počet vláken NEMĚNÍ výsledek, jen rychlost (Amdahlův zákon)."},
        {"heading": "Ukládání výsledků", "anchor": "void SolverWorker::saveResults",
         "note": "Zavolá io::writeVTK/writeFields/writeMesh do zvoleného adresáře. "
                 "Běží ve worker vlákně (I/O nesmí blokovat GUI). Výsledek hlásí "
                 "signálem `saved`. Ošetřuje i chybu (neexistující adresář, prázdný stav)."},
    ])


# ──────────────────────────────────────────────────────────────────────────
# GUI — VYKRESLOVÁNÍ
# ──────────────────────────────────────────────────────────────────────────

_add("fieldview_h",
    title="FieldView.hpp",
    path=GUI_DIR / "src/FieldView.hpp",
    lang="cpp", group="GUI — vykreslování",
    summary="Deklarace widgetu pro 2D barevnou mapu řezu.",
    role="Vlastní QWidget zobrazující SliceData jako heatmapu.",
    sections=[
        {"heading": "Rozhraní widgetu", "anchor": "class FieldView",
         "note": "Dědí QWidget. Slot `setSlice` přijme data, `setTitle` titulek. "
                 "Privátní `turbo()` = colormapa. Žádná externí knihovna — vše "
                 "přes QPainter. To je vědomé rozhodnutí (jednoduchá závislost, "
                 "plná kontrola, snadná obhajoba na pohovoru)."},
    ])

_add("fieldview_cpp",
    title="FieldView.cpp",
    path=GUI_DIR / "src/FieldView.cpp",
    lang="cpp", group="GUI — vykreslování",
    summary="Vykreslení heatmapy: QImage po buňkách, turbo colormapa, colorbar.",
    role="Převede 2D řez na barevný obraz s legendou.",
    sections=[
        {"heading": "Příjem dat", "anchor": "void FieldView::setSlice",
         "note": "Uloží řez (move = bez kopie) a zavolá `update()`, což jen "
                 "naplánuje překreslení (nepřekresluje hned). Qt sloučí vícenásobné "
                 "update() do jednoho paintEventu — efektivní při rychlých datech."},
        {"heading": "Turbo colormapa", "anchor": "QColor FieldView::turbo",
         "note": "Aproximace Google „turbo\" mapy (modrá→zelená→žlutá→červená) "
                 "trojicí lineárních ramp. Vnímatelně uniformní, lepší než klasická "
                 "jet. Vstup t∈<0,1>, výstup QColor."},
        {"heading": "Vykreslení paintEvent", "anchor": "void FieldView::paintEvent",
         "note": "Postaví QImage (1 px = 1 buňka), namapuje hodnoty přes colormapu, "
                 "pevné buňky našedo. Obrázek pak škáluje na widget se zachováním "
                 "poměru stran. Osu y obrací (řádek 0 dole = fyzikální konvence)."},
        {"heading": "Colorbar a mřížka", "anchor": "if (!mesh)",
         "note": "Vpravo vykreslí svislý colorbar s popisky min/max. Pro režim "
                 "„síť\" místo heatmapy nakreslí mřížku buněk (pokud nejsou moc malé). "
                 "Titulek nahoře nese veličinu/rovinu/index/iteraci."},
    ])

_add("domain3d_h",
    title="DomainView3D.hpp",
    path=GUI_DIR / "src/DomainView3D.hpp",
    lang="cpp", group="GUI — vykreslování",
    summary="Deklarace 3D náhledu domény (bez OpenGL).",
    role="Widget kreslící drátový tunel + krychli + mřížku, otáčení myší.",
    sections=[
        {"heading": "Rozhraní", "anchor": "class DomainView3D",
         "note": "`setDomain` dostane rozměry, krychli a rozlišení sítě. Stav: úhly "
                 "yaw/pitch, zoom, příznak mřížky. Vše vykresleno čistě QPainterem — "
                 "vlastní mini 3D projekce, žádný OpenGL (jednoduchost > výkon, "
                 "doména je jen pár hran)."},
    ])

_add("domain3d_cpp",
    title="DomainView3D.cpp",
    path=GUI_DIR / "src/DomainView3D.cpp",
    lang="cpp", group="GUI — vykreslování",
    summary="Vlastní 3D drátová projekce přes QPainter + interakce myší.",
    role="Ortografická projekce, rotace, mřížka na stěnách, stěny krychle.",
    sections=[
        {"heading": "Nastavení domény", "anchor": "void DomainView3D::setDomain",
         "note": "Uloží geometrii i rozlišení (nx,ny,nz) a překreslí. Mřížka pak "
                 "ukazuje SKUTEČNÝ počet buněk — důležité pro kontrolu jemnosti sítě."},
        {"heading": "3D projekce", "anchor": "QPointF DomainView3D::project",
         "note": "Jádro náhledu: bod (x,y,z) → 2D pixel. Posun do středu, rotace "
                 "yaw (kolem y) a pitch (kolem x) maticemi rotace, pak ortografické "
                 "promítnutí (zahodí hloubku). Ručně, bez knihovny — pár řádků lineární algebry."},
        {"heading": "Vykreslení scény", "anchor": "void DomainView3D::paintEvent",
         "note": "Spočítá měřítko (aby se doména vešla), promítne 8 rohů tunelu a "
                 "nakreslí 12 hran. Pak mřížka na stěnách, poloprůhledná krychle, "
                 "barevné osy x/y/z a popisky."},
        {"heading": "Mřížka sítě", "anchor": "if (showGrid_)",
         "note": "Na podlaze a zadní stěně nakreslí čáru pro každou hranici buňky "
                 "(0..N) → reálné rozlišení sítě. U jemných sítí hustá, ale věrná."},
        {"heading": "Interakce myší", "anchor": "void DomainView3D::mousePressEvent",
         "note": "Tažení mění yaw/pitch (s clampem, aby se scéna nepřevrátila), "
                 "kolečko zoom. Pozor: Qt5 používá `pos()` (Qt6 `position()`) — "
                 "drobnost, na které build původně spadl."},
    ])


# ──────────────────────────────────────────────────────────────────────────
# GUI — OKNO A SESTAVENÍ
# ──────────────────────────────────────────────────────────────────────────

_add("mainwindow_h",
    title="MainWindow.hpp",
    path=GUI_DIR / "src/MainWindow.hpp",
    lang="cpp", group="GUI — okno a sestavení",
    summary="Deklarace hlavního okna — všechny prvky a sloty.",
    role="Drží formulář zadání, ovládání, plátna a vlákno řešiče.",
    sections=[
        {"heading": "Signály a closeEvent", "anchor": "void closeEvent",
         "note": "`startSolver`/`requestSave` jdou do workeru. `closeEvent` (override) "
                 "zajistí bezpečné ukončení výpočtu při zavření okna (i křížkem)."},
        {"heading": "Sloty", "anchor": "private slots:",
         "note": "Reakce na události workeru (onIteration/onSnapshot/onSlice/onFinished/"
                 "onSaved) a na ovládací prvky (onStart/onStop/onSave/onViewChanged). "
                 "Sloty = vstupní body do GUI logiky."},
        {"heading": "Prvky zadání", "anchor": "QSpinBox*       reBox_",
         "note": "Ukazatele na všechny vstupní widgety (Re, U_in, síť, doména, krychle, "
                 "relaxace, iterace, tolerance, vlákna, frekvence obnovy). Drží se jako "
                 "členy, aby z nich šlo číst při startu."},
        {"heading": "Vizualizace a stav", "anchor": "QStackedWidget* viewStack_",
         "note": "`viewStack_` přepíná 2D řez ↔ 3D doménu. `snap_` je poslední "
                 "snapshot (i náhled před výpočtem), `snapIsPreview_` rozliší náhled "
                 "od skutečného výsledku (kvůli povolení Uložit a volbě veličiny)."},
        {"heading": "Vlákno a pomocné metody", "anchor": "QThread       thread_",
         "note": "`thread_` + `worker_` = výpočet mimo GUI vlákno. Privátní metody "
                 "(collectParams, refreshView, updateDomainPreview…) drží logiku "
                 "okna přehledně rozdělenou."},
    ])

_add("mainwindow_cpp",
    title="MainWindow.cpp",
    path=GUI_DIR / "src/MainWindow.cpp",
    lang="cpp", group="GUI — okno a sestavení",
    summary="Sestavení UI, propojení vlákna, veškerá interakční logika.",
    role="Nejdelší soubor GUI — lepí dohromady všechny komponenty.",
    sections=[
        {"heading": "Konstruktor — start vlákna", "anchor": "MainWindow::MainWindow",
         "note": "Postaví UI, vytvoří workera a přesune ho do `thread_` "
                 "(`moveToThread`). Propojí signály obousměrně a spustí vlákno. "
                 "Toto je kanonický Qt vzor „worker object\" — čistší než dědit QThread."},
        {"heading": "Bezpečné zavření", "anchor": "void MainWindow::closeEvent",
         "note": "★ Při zavření (tlačítko Konec i křížek) se zeptá, pokud výpočet "
                 "běží, pak stopne worker, ukončí vlákno (`quit`+`wait`) a teprve "
                 "přijme událost. Žádné násilné zabití uprostřed iterace."},
        {"heading": "Stavba UI buildUi()", "anchor": "void MainWindow::buildUi",
         "note": "Vytvoří formulář zadání (továrničky mkInt/mkDbl), tlačítka, "
                 "ovládání vizualizace, plátna (FieldView + DomainView3D ve "
                 "QStackedWidget) a graf konvergence (QtCharts, log osa). "
                 "Layouty místo absolutních pozic → okno se škáluje."},
        {"heading": "Sběr parametrů", "anchor": "SimParams MainWindow::collectParams",
         "note": "Načte hodnoty ze všech widgetů do SimParams. Jediné místo, kde se "
                 "formulář převádí na zadání — snadno se udržuje."},
        {"heading": "Náhled domény před výpočtem", "anchor": "static FieldSnapshot buildPreviewSnapshot",
         "note": "★ Sestaví snapshot jen z geometrie (bez fyziky) replikací "
                 "Mesh::markSolid → náhled sítě a krychle JEŠTĚ PŘED spuštěním. "
                 "`updateDomainPreview` ho ukáže ve 2D i 3D a živě reaguje na změny zadání."},
        {"heading": "Povolení prvků za běhu", "anchor": "void MainWindow::setRunning",
         "note": "Přepíná dostupnost tlačítek a zamyká formulář během výpočtu "
                 "(zadání nelze měnit za běhu). `initializer_list<QWidget*>` "
                 "explicitně typovaný — jinak by odvození typu selhalo."},
        {"heading": "Start výpočtu", "anchor": "void MainWindow::onStart",
         "note": "Vyčistí graf, sebere parametry, nastaví workeru režim a pohled, "
                 "přepne UI do běhu a emituje `startSolver` (spustí run() ve vlákně)."},
        {"heading": "Příjem dat z workeru", "anchor": "void MainWindow::onSnapshot",
         "note": "onSnapshot uloží plné pole (a označí, že to není náhled), "
                 "onSlice zobrazí hotový řez (rychlý režim), onIteration přidá bod "
                 "do grafu reziduí. Vše běží už v GUI vlákně (signál to zařídil)."},
        {"heading": "Změna pohledu", "anchor": "void MainWindow::onViewChanged",
         "note": "Přepne 2D/3D, oznámí workeru nový pohled (atomicky, funguje i za "
                 "běhu) a překreslí. Pokud máme plný snapshot, řez se vyřízne lokálně "
                 "OKAMŽITĚ (bez čekání na worker) — proto je plynulé procházení domény."},
        {"heading": "Obnova zobrazení", "anchor": "void MainWindow::refreshView",
         "note": "Vyřízne řez z aktuálního snapshotu a pošle do FieldView. Před "
                 "výpočtem (náhled) vykreslí rovnou „síť\", aby nebyla jen prázdná plocha."},
    ])

_add("cmake",
    title="CMakeLists.txt",
    path=GUI_DIR / "CMakeLists.txt",
    lang="cmake", group="GUI — okno a sestavení",
    summary="Build systém: Qt5, OpenMP a link na řešič.",
    role="Definuje, jak se projekt přeloží a slinkuje s jádrem řešiče.",
    sections=[
        {"heading": "Qt5 a AUTOMOC", "anchor": "find_package(Qt5",
         "note": "`AUTOMOC` automaticky spustí Qt meta-object compiler (zpracuje "
                 "Q_OBJECT, signály/sloty). Bez něj by mezivláknové signály nešly "
                 "slinkovat. Vyžaduje Widgets + Charts."},
        {"heading": "Řešič jako knihovna", "anchor": "add_library(cfdcore",
         "note": "Zdroje řešiče z `../cfd_simple_cube` se kompilují do statické "
                 "knihovny `cfdcore` (jeden zdroj pravdy — žádná kopie kódu). "
                 "`SOLVER_DIR` lze přepsat. FATAL_ERROR pohlídá, že řešič existuje."},
        {"heading": "Spustitelný cíl", "anchor": "add_executable(cfd_cube_qt",
         "note": "Slinkuje GUI zdroje s cfdcore + Qt5 Widgets/Charts. Tady se "
                 "přidávají nové .cpp soubory (FieldData, DomainView3D…)."},
    ])
