#pragma once
#include <QMainWindow>
#include <QThread>
#include "SolverWorker.hpp"
#include "FieldData.hpp"

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QSlider;
class QLabel;
class QStackedWidget;
class FieldView;
class DomainView3D;
QT_BEGIN_NAMESPACE
namespace QtCharts {
class QChartView; class QLineSeries; class QLogValueAxis; class QValueAxis;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;   // zastaví výpočet při zavření okna

signals:
    void startSolver(SimParams p);   // → SolverWorker::run (přes vlákno)
    void requestSave(QString dir);   // → SolverWorker::saveResults

private slots:
    void onStart();
    void onStop();
    void onSave();
    void onIteration(int it, double residual);
    void onSnapshot(FieldSnapshot snap);     // plný 3D snapshot
    void onSlice(SliceData slice);           // hotový 2D řez (rychlý režim)
    void onFinished(bool converged, int iters);
    void onSaved(const QString& message);
    void onViewChanged();                    // změna veličiny/roviny/indexu/režimu

private:
    // ── zadání ──
    QSpinBox*       reBox_   = nullptr;
    QDoubleSpinBox* uinBox_  = nullptr;
    QSpinBox*       nxBox_   = nullptr;
    QSpinBox*       nyBox_   = nullptr;
    QSpinBox*       nzBox_   = nullptr;
    QDoubleSpinBox* lxBox_   = nullptr;
    QDoubleSpinBox* lyBox_   = nullptr;
    QDoubleSpinBox* lzBox_   = nullptr;
    QDoubleSpinBox* cubeXBox_= nullptr;
    QDoubleSpinBox* cubeYBox_= nullptr;
    QDoubleSpinBox* cubeZBox_= nullptr;
    QDoubleSpinBox* cubeDBox_= nullptr;
    QDoubleSpinBox* aUBox_   = nullptr;
    QDoubleSpinBox* aPBox_   = nullptr;
    QSpinBox*       maxItBox_= nullptr;
    QDoubleSpinBox* tolBox_  = nullptr;
    QSpinBox*       threadsBox_ = nullptr;   // počet vláken výpočtu (OpenMP)
    QSpinBox*       everyBox_   = nullptr;   // jak často (v iteracích) obnovit obraz pole

    // ── ovládání ──
    QPushButton* startBtn_ = nullptr;
    QPushButton* stopBtn_  = nullptr;
    QPushButton* saveBtn_  = nullptr;
    QPushButton* exitBtn_  = nullptr;
    QLabel*      statusLbl_ = nullptr;

    // ── vizualizace ──
    QComboBox* kindBox_  = nullptr;   // veličina
    QComboBox* planeBox_ = nullptr;   // rovina řezu
    QSlider*   sliceSld_ = nullptr;   // index řezu (procházení 3D)
    QLabel*    sliceLbl_ = nullptr;
    QComboBox* modeBox_  = nullptr;   // režim přenosu dat
    QComboBox* viewModeBox_ = nullptr;   // 2D řez / 3D doména
    FieldView*    fieldView_ = nullptr;
    DomainView3D* domain3D_  = nullptr;
    QStackedWidget* viewStack_ = nullptr;

    // ── graf konvergence ──
    QtCharts::QChartView*    chartView_ = nullptr;
    QtCharts::QLineSeries*   series_    = nullptr;
    QtCharts::QValueAxis*    axX_       = nullptr;
    QtCharts::QLogValueAxis* axY_       = nullptr;

    // ── vlákno řešiče ──
    QThread       thread_;
    SolverWorker* worker_ = nullptr;

    // ── stav GUI ──
    FieldSnapshot snap_;        // poslední přijatý snapshot (pro lokální řezy)
    bool running_ = false;
    bool snapIsPreview_ = false;   // true = snap_ je jen náhled geometrie (ne výsledek)

    void buildUi();
    SimParams collectParams() const;
    SlicePlane currentPlane() const;
    FieldKind  currentKind() const;
    void       updateSliceRange();   // nastaví rozsah posuvníku podle roviny/snapshotu
    void       refreshView();        // vyřízne řez ze snap_ a zobrazí
    QString    viewTitle() const;
    void       setRunning(bool r);
    void       updateDomainPreview();  // živý náhled domény (3D widget + 2D síť) ze zadání
};
