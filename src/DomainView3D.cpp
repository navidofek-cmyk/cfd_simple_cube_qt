#include "DomainView3D.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <array>
#include <algorithm>
#include <cmath>

DomainView3D::DomainView3D(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 240);
    setMouseTracking(false);
}

void DomainView3D::setDomain(double Lx, double Ly, double Lz,
                             double cx, double cy, double cz, double D,
                             int nx, int ny, int nz) {
    Lx_ = Lx; Ly_ = Ly; Lz_ = Lz;
    cx_ = cx; cy_ = cy; cz_ = cz; D_ = D;
    nx_ = nx; ny_ = ny; nz_ = nz;
    update();
}

// Rotace (yaw kolem svislé osy y, pitch kolem osy x) + ortografická projekce.
QPointF DomainView3D::project(double x, double y, double z,
                              double scale, QPointF origin) const {
    // posun do středu domény
    x -= Lx_ * 0.5; y -= Ly_ * 0.5; z -= Lz_ * 0.5;

    const double cy = std::cos(yaw_),   sy = std::sin(yaw_);
    const double cp = std::cos(pitch_), sp = std::sin(pitch_);

    // yaw kolem y
    double x1 =  x * cy + z * sy;
    double z1 = -x * sy + z * cy;
    double y1 =  y;
    // pitch kolem x
    double y2 = y1 * cp - z1 * sp;
    // (z2 nepotřebujeme pro ortho projekci)

    return QPointF(origin.x() + x1 * scale,
                   origin.y() - y2 * scale);   // y nahoru
}

void DomainView3D::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), QColor(18, 20, 24));

    // měřítko tak, aby se domena vešla (podle nejdelšího rozměru)
    double maxL = std::max({Lx_, Ly_, Lz_});
    double scale = zoom_ * 0.42 * std::min(width(), height()) / std::max(1e-9, maxL);
    QPointF origin(width() * 0.5, height() * 0.5);

    auto P = [&](double x, double y, double z){ return project(x, y, z, scale, origin); };

    // ── 8 rohů tunelu ───────────────────────────────────────────────
    std::array<QPointF, 8> c = {
        P(0,0,0),     P(Lx_,0,0),     P(Lx_,Ly_,0),     P(0,Ly_,0),
        P(0,0,Lz_),   P(Lx_,0,Lz_),   P(Lx_,Ly_,Lz_),   P(0,Ly_,Lz_)
    };
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
    };
    // ── mřížka sítě na stěnách tunelu ───────────────────────────────
    // Čáry kreslíme na dvou „zadních" stěnách (podlaha y=0 a stěna z=0),
    // aby drátový model zůstal čitelný. Hustotu omezíme, ať to není kaše.
    if (showGrid_) {
        g.setPen(QPen(QColor(55, 65, 78), 0.6));
        // Skutečný počet buněk — čára pro každou hranici buňky (i+0..n).
        // podlaha y=0: čáry podél x a podél z
        for (int i = 0; i <= nx_; ++i) { double x = Lx_*i/nx_;
            g.drawLine(P(x,0,0), P(x,0,Lz_)); }
        for (int k = 0; k <= nz_; ++k) { double z = Lz_*k/nz_;
            g.drawLine(P(0,0,z), P(Lx_,0,z)); }
        // stěna z=0: čáry podél x a podél y
        for (int i = 0; i <= nx_; ++i) { double x = Lx_*i/nx_;
            g.drawLine(P(x,0,0), P(x,Ly_,0)); }
        for (int j = 0; j <= ny_; ++j) { double y = Ly_*j/ny_;
            g.drawLine(P(0,y,0), P(Lx_,y,0)); }
    }

    g.setPen(QPen(QColor(120, 140, 160), 1.2));
    for (auto& e : edges) g.drawLine(c[e[0]], c[e[1]]);

    // ── krychle (překážka) ──────────────────────────────────────────
    double h = D_ * 0.5;
    double x0 = cx_-h, x1 = cx_+h, y0 = cy_-h, y1 = cy_+h, z0 = cz_-h, z1 = cz_+h;
    std::array<QPointF, 8> q = {
        P(x0,y0,z0), P(x1,y0,z0), P(x1,y1,z0), P(x0,y1,z0),
        P(x0,y0,z1), P(x1,y0,z1), P(x1,y1,z1), P(x0,y1,z1)
    };
    // stěny krychle (čtveřice rohů) — vyplníme poloprůhledně + obrys
    static const int faces[6][4] = {
        {0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,3,7,4}
    };
    g.setPen(QPen(QColor(255, 180, 90), 1.4));
    QColor fill(230, 140, 50, 70);
    for (auto& f : faces) {
        QPolygonF poly;
        poly << q[f[0]] << q[f[1]] << q[f[2]] << q[f[3]];
        g.setBrush(fill);
        g.drawPolygon(poly);
    }
    g.setBrush(Qt::NoBrush);

    // ── popisky / osy ───────────────────────────────────────────────
    g.setPen(QColor(150, 200, 120));
    g.drawLine(P(0,0,0), P(Lx_*0.18,0,0));  g.drawText(P(Lx_*0.20,0,0), "x");
    g.setPen(QColor(120, 170, 230));
    g.drawLine(P(0,0,0), P(0,Ly_*0.30,0));  g.drawText(P(0,Ly_*0.33,0), "y");
    g.setPen(QColor(220, 130, 200));
    g.drawLine(P(0,0,0), P(0,0,Lz_*0.30));  g.drawText(P(0,0,Lz_*0.33), "z");

    g.setPen(QColor(200, 205, 210));
    g.drawText(QRect(8, 6, width()-16, 18), Qt::AlignLeft,
               QString("Doména %1×%2×%3 m   krychle D=%4 @ (%5, %6, %7)")
                   .arg(Lx_,0,'g',3).arg(Ly_,0,'g',3).arg(Lz_,0,'g',3)
                   .arg(D_,0,'g',3).arg(cx_,0,'g',3).arg(cy_,0,'g',3).arg(cz_,0,'g',3));
    g.setPen(QColor(120, 125, 130));
    g.drawText(QRect(8, height()-22, width()-16, 18), Qt::AlignLeft,
               QString("síť %1×%2×%3 · tažením otáčej · kolečkem přibliž")
                   .arg(nx_).arg(ny_).arg(nz_));
}

void DomainView3D::mousePressEvent(QMouseEvent* e) {
    dragging_ = true;
    lastPos_  = QPointF(e->pos());
}

void DomainView3D::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_) return;
    QPointF d = QPointF(e->pos()) - lastPos_;
    lastPos_  = QPointF(e->pos());
    yaw_   += d.x() * 0.01;
    pitch_ += d.y() * 0.01;
    pitch_  = std::clamp(pitch_, -1.5, 1.5);
    update();
}

void DomainView3D::wheelEvent(QWheelEvent* e) {
    double f = std::pow(1.0015, e->angleDelta().y());
    zoom_ = std::clamp(zoom_ * f, 0.2, 8.0);
    update();
}
