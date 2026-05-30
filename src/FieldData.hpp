#pragma once
#include <vector>
#include <QMetaType>

// ──────────────────────────────────────────────────────────────────────
// Sdílené datové typy pro přenos polí z výpočtu (worker vlákno) do GUI.
//
//  FieldSnapshot ... plný 3D snapshot polí (kopie ze solveru do floatů).
//                    Umožní GUI lokálně vyřezávat libovolnou rovinu/veličinu
//                    bez dalšího dotazu na worker → plynulé procházení domény.
//  SliceData     ... hotový 2D řez připravený k vykreslení (lehčí přenos).
//  makeSlice()   ... vyřízne 2D řez ze snapshotu (plane + index + veličina).
// ──────────────────────────────────────────────────────────────────────

// Zobrazovaná veličina.
enum class FieldKind {
    Umag,       // |U|
    Ux, Uy, Uz, // složky rychlosti
    Pressure,   // p
    Residual,   // kontinuitní reziduum (buňkové)
    Mesh        // geometrie sítě (jen pevné/tekuté buňky)
};

// Rovina řezu 3D doménou.
//   XY: rovina z = index  (procházíme přes z)
//   XZ: rovina y = index  (procházíme přes y)
//   YZ: rovina x = index  (procházíme přes x — podél tunelu)
enum class SlicePlane { XY, XZ, YZ };

// Plný 3D snapshot (kolokovaná mřížka nx×ny×nz).
struct FieldSnapshot {
    int    nx = 0, ny = 0, nz = 0;
    double dx = 1, dy = 1, dz = 1;
    std::vector<float> ux, uy, uz;   // složky rychlosti, velikost nx*ny*nz
    std::vector<float> p;            // tlak
    std::vector<float> res;          // kontinuitní reziduum
    std::vector<char>  solid;        // 1 = pevná buňka (krychle/překážka)
    int    iter = 0;                 // iterace, ke které snapshot patří

    int  idx(int i, int j, int k) const { return i + j*nx + k*nx*ny; }
    bool valid() const { return nx > 0 && ny > 0 && nz > 0; }
    bool empty() const { return !valid(); }

    // Počet platných indexů řezu pro danou rovinu (rozsah posuvníku).
    int sliceCount(SlicePlane pl) const {
        switch (pl) {
            case SlicePlane::XY: return nz;   // z-index
            case SlicePlane::XZ: return ny;   // y-index
            case SlicePlane::YZ: return nx;   // x-index
        }
        return 0;
    }
};

// 2D řez připravený pro FieldView.
struct SliceData {
    int  w = 0, h = 0;               // rozměry řezu (počet buněk)
    std::vector<float> value;        // skalární hodnota v buňce
    std::vector<char>  solid;        // 1 = pevná buňka
    float vmin = 0, vmax = 1;        // rozsah hodnot (přes tekuté buňky)
    bool  isMesh = false;            // řez je geometrie sítě (renderuje se jinak)
    int   iter = 0;                  // iterace, ke které řez patří
};

// Vyřízne 2D řez ze snapshotu. `index` se ořízne do platného rozsahu roviny.
SliceData makeSlice(const FieldSnapshot& s, SlicePlane plane, int index, FieldKind kind);

Q_DECLARE_METATYPE(SliceData)
Q_DECLARE_METATYPE(FieldSnapshot)
