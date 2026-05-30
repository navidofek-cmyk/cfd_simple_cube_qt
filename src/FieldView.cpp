#include "FieldView.hpp"
#include <QPainter>
#include <QImage>
#include <algorithm>
#include <cmath>

FieldView::FieldView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 200);
}

void FieldView::setSlice(SliceData slice) {
    slice_ = std::move(slice);
    update();   // naplánuje paintEvent
}

// Zjednodušená "turbo" colormap (modrá → zelená → žlutá → červená).
QColor FieldView::turbo(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float r = std::clamp(1.5f - std::fabs(4*t - 3), 0.0f, 1.0f);
    float g = std::clamp(1.5f - std::fabs(4*t - 2), 0.0f, 1.0f);
    float b = std::clamp(1.5f - std::fabs(4*t - 1), 0.0f, 1.0f);
    return QColor(int(r*255), int(g*255), int(b*255));
}

void FieldView::paintEvent(QPaintEvent*) {
    QPainter pnt(this);
    pnt.fillRect(rect(), QColor(20, 24, 28));

    if (slice_.w == 0 || slice_.h == 0) {
        pnt.setPen(Qt::gray);
        pnt.drawText(rect(), Qt::AlignCenter, "Spusť výpočet…");
        return;
    }

    const bool   mesh = slice_.isMesh;
    const int    cbW  = mesh ? 0 : 18;          // šířka colorbaru
    const int    margin = 8;
    const int    topH = 22;                     // pruh na titulek

    // ── obrázek řezu (1 pixel = 1 buňka) ────────────────────────────
    QImage img(slice_.w, slice_.h, QImage::Format_RGB32);
    const float span = slice_.vmax - slice_.vmin;
    for (int j = 0; j < slice_.h; ++j)
        for (int i = 0; i < slice_.w; ++i) {
            int idx = i + j * slice_.w;
            QColor c;
            if (slice_.solid[idx]) {
                c = mesh ? QColor(90, 90, 100) : QColor(70, 70, 75);   // krychle
            } else if (mesh) {
                c = QColor(40, 46, 54);                                // tekutá buňka
            } else {
                float t = span > 0 ? (slice_.value[idx] - slice_.vmin) / span : 0.0f;
                c = turbo(t);
            }
            img.setPixel(i, slice_.h - 1 - j, c.rgb());   // y nahoru
        }

    // ── plocha pro kreslení obrázku (bez colorbaru a titulku) ───────
    QRect area(margin, topH, width() - 2*margin - cbW - (cbW ? margin : 0),
               height() - topH - margin);
    QImage scaled = img.scaled(area.size(),
                               mesh ? Qt::KeepAspectRatio : Qt::KeepAspectRatio,
                               Qt::FastTransformation);
    int x = area.x() + (area.width()  - scaled.width())  / 2;
    int y = area.y() + (area.height() - scaled.height()) / 2;
    pnt.drawImage(x, y, scaled);

    // mřížka pro řez sítě (jen když buňky nejsou příliš malé)
    if (mesh && scaled.width() > 0 && scaled.height() > 0) {
        double cw = double(scaled.width())  / slice_.w;
        double ch = double(scaled.height()) / slice_.h;
        if (cw >= 4 && ch >= 4) {
            pnt.setPen(QPen(QColor(70, 80, 92), 0));
            for (int i = 0; i <= slice_.w; ++i)
                pnt.drawLine(QPointF(x + i*cw, y), QPointF(x + i*cw, y + scaled.height()));
            for (int j = 0; j <= slice_.h; ++j)
                pnt.drawLine(QPointF(x, y + j*ch), QPointF(x + scaled.width(), y + j*ch));
        }
    }

    // ── titulek ─────────────────────────────────────────────────────
    pnt.setPen(QColor(220, 224, 230));
    pnt.drawText(QRect(margin, 2, width() - 2*margin, topH - 2),
                 Qt::AlignLeft | Qt::AlignVCenter, title_);

    // ── colorbar ────────────────────────────────────────────────────
    if (!mesh) {
        int bx = width() - margin - cbW;
        QRect bar(bx, area.y(), cbW, area.height());
        for (int yy = 0; yy < bar.height(); ++yy) {
            float t = 1.0f - float(yy) / std::max(1, bar.height() - 1);
            pnt.setPen(turbo(t));
            pnt.drawLine(bar.left(), bar.top() + yy, bar.right(), bar.top() + yy);
        }
        pnt.setPen(QColor(200, 205, 210));
        pnt.drawRect(bar);
        QString hi = QString::number(slice_.vmax, 'g', 3);
        QString lo = QString::number(slice_.vmin, 'g', 3);
        pnt.drawText(QRect(bx - 64, bar.top() - 2, 60, 14),
                     Qt::AlignRight | Qt::AlignVCenter, hi);
        pnt.drawText(QRect(bx - 64, bar.bottom() - 12, 60, 14),
                     Qt::AlignRight | Qt::AlignVCenter, lo);
    }
}
