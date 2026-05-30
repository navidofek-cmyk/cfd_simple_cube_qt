#include "FieldData.hpp"
#include <algorithm>
#include <cmath>

// Vzorek skalární veličiny v buňce c.
static float sample(const FieldSnapshot& s, int c, FieldKind kind) {
    switch (kind) {
        case FieldKind::Umag:
            return std::sqrt(s.ux[c]*s.ux[c] + s.uy[c]*s.uy[c] + s.uz[c]*s.uz[c]);
        case FieldKind::Ux:       return s.ux[c];
        case FieldKind::Uy:       return s.uy[c];
        case FieldKind::Uz:       return s.uz[c];
        case FieldKind::Pressure: return s.p[c];
        case FieldKind::Residual: return s.res[c];
        case FieldKind::Mesh:     return 0.0f;
    }
    return 0.0f;
}

SliceData makeSlice(const FieldSnapshot& s, SlicePlane plane, int index, FieldKind kind) {
    SliceData out;
    if (!s.valid()) return out;

    out.iter   = s.iter;
    out.isMesh = (kind == FieldKind::Mesh);

    // Rozměry řezu + ořez indexu do platného rozsahu.
    int w, h, maxIdx;
    switch (plane) {
        case SlicePlane::XY: w = s.nx; h = s.ny; maxIdx = s.nz; break;  // z = index
        case SlicePlane::XZ: w = s.nx; h = s.nz; maxIdx = s.ny; break;  // y = index
        case SlicePlane::YZ: w = s.ny; h = s.nz; maxIdx = s.nx; break;  // x = index
        default: return out;
    }
    index = std::clamp(index, 0, maxIdx - 1);

    out.w = w; out.h = h;
    out.value.resize(static_cast<size_t>(w) * h);
    out.solid.resize(static_cast<size_t>(w) * h);

    float vmin = 1e30f, vmax = -1e30f;
    for (int b = 0; b < h; ++b)
        for (int a = 0; a < w; ++a) {
            int i, j, k;
            switch (plane) {
                case SlicePlane::XY: i = a;     j = b;     k = index; break;
                case SlicePlane::XZ: i = a;     j = index; k = b;     break;
                case SlicePlane::YZ: i = index; j = a;     k = b;     break;
                default: i = j = k = 0;
            }
            int c = s.idx(i, j, k);
            int o = a + b * w;
            out.solid[o] = s.solid[c];
            float v = sample(s, c, kind);
            out.value[o] = v;
            if (!s.solid[c]) { vmin = std::min(vmin, v); vmax = std::max(vmax, v); }
        }

    if (vmax < vmin) { vmin = 0.0f; vmax = 1.0f; }          // celý řez pevný
    out.vmin = vmin;
    out.vmax = (vmax > vmin) ? vmax : vmin + 1e-6f;
    return out;
}
