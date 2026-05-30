#pragma once
#include <QWidget>
#include <QPointF>

// Lehký 3D náhled výpočetní domény (tunel + krychle) bez OpenGL — jen QPainter.
// Kreslí drátový kvádr tunelu a krychli jako překážku, v ortografické projekci.
// Otáčení tažením myší, kolečko = zoom. Slouží k vizuální kontrole zadání
// (poloha/velikost krychle v tunelu) ještě před spuštěním výpočtu.
class DomainView3D : public QWidget {
    Q_OBJECT
public:
    explicit DomainView3D(QWidget* parent = nullptr);

    // Rozměry tunelu a krychle (střed + strana) + rozlišení sítě. Překreslí.
    void setDomain(double Lx, double Ly, double Lz,
                   double cx, double cy, double cz, double D,
                   int nx, int ny, int nz);

    void setShowGrid(bool on) { showGrid_ = on; update(); }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    QSize sizeHint() const override { return {640, 360}; }

private:
    double Lx_ = 4, Ly_ = 1, Lz_ = 1;
    double cx_ = 1, cy_ = 0.5, cz_ = 0.5, D_ = 0.2;
    int    nx_ = 80, ny_ = 20, nz_ = 20;
    bool   showGrid_ = true;

    double yaw_ = -0.6, pitch_ = 0.5;   // úhly pohledu (rad)
    double zoom_ = 1.0;
    QPointF lastPos_;
    bool   dragging_ = false;

    // Promítne bod (x,y,z) v souřadnicích domény do 2D pixelů widgetu.
    QPointF project(double x, double y, double z, double scale, QPointF origin) const;
};
