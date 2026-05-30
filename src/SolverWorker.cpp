#include "SolverWorker.hpp"
#include "Simple.hpp"
#include "Mesh.hpp"
#include "io.hpp"
#include <QDir>

SolverWorker::SolverWorker(QObject* parent) : QObject(parent) {}
SolverWorker::~SolverWorker() = default;

// Sestaví plný 3D snapshot z aktuálního stavu řešiče (double → float).
static FieldSnapshot makeSnapshot(const ChannelSolver& sim, int iter) {
    const Mesh& m = sim.m;
    FieldSnapshot s;
    s.nx = m.nx; s.ny = m.ny; s.nz = m.nz;
    s.dx = m.dx; s.dy = m.dy; s.dz = m.dz;
    s.iter = iter;

    const int N = m.N();
    s.ux.resize(N); s.uy.resize(N); s.uz.resize(N);
    s.p.resize(N);  s.res.resize(N); s.solid.resize(N);
    for (int c = 0; c < N; ++c) {
        s.ux[c]    = static_cast<float>(sim.U[c].x);
        s.uy[c]    = static_cast<float>(sim.U[c].y);
        s.uz[c]    = static_cast<float>(sim.U[c].z);
        s.p[c]     = static_cast<float>(sim.p[c]);
        s.res[c]   = static_cast<float>(sim.resField[c]);
        s.solid[c] = m.solid[c] ? 1 : 0;
    }
    return s;
}

void SolverWorker::setView(SlicePlane plane, int index, FieldKind kind) {
    viewPlane_ = static_cast<int>(plane);
    viewIndex_ = index;
    viewKind_  = static_cast<int>(kind);
}

void SolverWorker::run(SimParams p) {
    stopRequested_ = false;
    sendFull_      = p.sendFullField;

#ifdef _OPENMP
    // Počet vláken výpočtu. 0 = nech OpenMP rozhodnout (všechna jádra).
    if (p.threads > 0) omp_set_num_threads(p.threads);
    else               omp_set_num_threads(omp_get_num_procs());
#endif

    const double h = p.cubeD * 0.5;
    sim_ = std::make_unique<ChannelSolver>(
        p.nx, p.ny, p.nz, p.Lx, p.Ly, p.Lz,
        p.cubeCx - h, p.cubeCx + h,
        p.cubeCy - h, p.cubeCy + h,
        p.cubeCz - h, p.cubeCz + h,
        p.Re, p.U_in, p.alphaU, p.alphaP, p.maxIter, p.tol);

    const int every = std::max(1, p.snapshotEvery);

    // Callback volaný řešičem každou iteraci. Vrací false → Stop.
    sim_->onIteration = [&](int it, double res) -> bool {
        emit iteration(it, res);
        if (it % every == 0) {
            // Snapshot stavíme vždy; volba režimu řídí, co (a jak velké) pošleme.
            // Pro výchozí síť je kopie zanedbatelná; u velkých sítí volí uživatel
            // „rychlý 2D náhled", aby se přes vlákno neposílalo celé pole.
            FieldSnapshot snap = makeSnapshot(*sim_, it);
            if (sendFull_.load()) {
                emit snapshotReady(std::move(snap));
            } else {
                SliceData sl = makeSlice(snap,
                                         static_cast<SlicePlane>(viewPlane_.load()),
                                         viewIndex_.load(),
                                         static_cast<FieldKind>(viewKind_.load()));
                emit sliceReady(std::move(sl));
            }
        }
        return !stopRequested_.load();
    };

    sim_->solve();

    // Finální plný snapshot pošleme vždy — GUI tak může plynule procházet
    // ustálené pole a uživatel může uložit výsledky.
    emit snapshotReady(makeSnapshot(*sim_, sim_->maxIter));
    emit finished(!stopRequested_.load(), sim_->maxIter);
}

void SolverWorker::saveResults(QString dir) {
    if (!sim_) { emit saved(QStringLiteral("Není co uložit — nejdřív spusť výpočet.")); return; }

    QDir d(dir);
    if (!d.exists() && !d.mkpath(".")) {
        emit saved(QStringLiteral("Nelze vytvořit adresář: %1").arg(dir));
        return;
    }
    const std::string vtk    = d.filePath("channel_cube.vtk").toStdString();
    const std::string fields = d.filePath("fields.dat").toStdString();
    const std::string mesh   = d.filePath("mesh.dat").toStdString();

    io::writeVTK   (vtk,    sim_->m, sim_->U, sim_->p, sim_->resField);
    io::writeFields(fields, sim_->m, sim_->U, sim_->p, sim_->resField);
    io::writeMesh  (mesh,   sim_->m);

    emit saved(QStringLiteral("Uloženo do %1: channel_cube.vtk, fields.dat, mesh.dat").arg(dir));
}
