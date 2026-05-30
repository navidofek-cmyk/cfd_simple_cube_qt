#pragma once
#include <QWidget>
#include <QString>
#include "FieldData.hpp"

// Vykresluje 2D řez polem jako barevnou mapu (turbo colormap) přes QPainter.
// Pevné buňky (krychle) jsou tmavě šedé. Vpravo se kreslí colorbar s rozsahem,
// nahoře titulek (veličina / rovina / index / iterace). Pro řez sítě (isMesh)
// se renderuje pevné/tekuté + mřížka.
class FieldView : public QWidget {
    Q_OBJECT
public:
    explicit FieldView(QWidget* parent = nullptr);

    void setTitle(const QString& t) { title_ = t; update(); }

public slots:
    void setSlice(SliceData slice);   // přijme nová data a překreslí

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {640, 320}; }

private:
    SliceData slice_;
    QString   title_;
    static QColor turbo(float t);     // t ∈ <0,1> → barva
};
