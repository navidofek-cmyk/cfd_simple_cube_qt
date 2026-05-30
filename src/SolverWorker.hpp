#pragma once
#include <QObject>
#include <QString>
#include <memory>
#include <atomic>
#include "FieldData.hpp"

struct ChannelSolver;   // dopředná deklarace (řešič z ../cfd_simple_cube)

// Parametry jedné simulace (zadání nastaví GUI). Plně editovatelné.
struct SimParams {
    // síť
    int    nx = 80, ny = 20, nz = 20;
    // doména (tunel)
    double Lx = 4.0, Ly = 1.0, Lz = 1.0;
    // krychle: střed + strana
    double cubeCx = 1.0, cubeCy = 0.5, cubeCz = 0.5;
    double cubeD  = 0.2;
    // fyzika
    double Re = 20.0, U_in = 1.0;
    // relaxace a konvergence
    double alphaU = 0.7, alphaP = 0.3;
    int    maxIter = 2000;
    double tol = 2e-5;
    // přenos dat do GUI
    bool   sendFullField = true;  // true = plný 3D snapshot, false = jen 2D řez
    int    snapshotEvery = 10;    // jak často (v iteracích) posílat pole
    // paralelizace (OpenMP)
    int    threads = 0;           // počet vláken výpočtu; 0 = všechna dostupná jádra
};
Q_DECLARE_METATYPE(SimParams)

// Obal nad ChannelSolver běžící ve vlastním vlákně.
// Komunikace s GUI přes signály (thread-safe). Volba pohledu/režimu jde
// přes atomické proměnné, aby fungovala i během běžící (blokující) smyčky.
class SolverWorker : public QObject {
    Q_OBJECT
public:
    explicit SolverWorker(QObject* parent = nullptr);   // def. v .cpp (unique_ptr neúplného typu)
    ~SolverWorker() override;

    // Voláno přímo z GUI vlákna (zapisuje jen atomiky) — bezpečné i za běhu.
    void stop()    { stopRequested_ = true; }
    void setView(SlicePlane plane, int index, FieldKind kind);
    void setMode(bool sendFullField) { sendFull_ = sendFullField; }

public slots:
    void run(SimParams p);          // spustí výpočet (běží ve worker vlákně)
    void saveResults(QString dir);  // uloží VTK + fields.dat + mesh.dat do adresáře

signals:
    void iteration(int it, double residual);   // každou iteraci
    void snapshotReady(FieldSnapshot snap);    // plný 3D snapshot (režim „plné pole")
    void sliceReady(SliceData slice);          // hotový 2D řez (režim „rychlý náhled")
    void finished(bool converged, int iters);
    void saved(QString message);               // hlášení o uložení (úspěch/chyba)

private:
    std::unique_ptr<ChannelSolver> sim_;       // drží stav i po doběhnutí (kvůli uložení)
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> sendFull_{true};
    std::atomic<int>  viewPlane_{0};           // SlicePlane jako int
    std::atomic<int>  viewIndex_{0};
    std::atomic<int>  viewKind_{0};            // FieldKind jako int
};
