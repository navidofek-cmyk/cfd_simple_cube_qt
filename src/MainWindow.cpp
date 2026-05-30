#include "MainWindow.hpp"
#include "FieldView.hpp"
#include "DomainView3D.hpp"

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QStackedWidget>
#include <QFileDialog>
#include <QCloseEvent>
#include <QMessageBox>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include <algorithm>

using namespace QtCharts;

// ── pomocné factory pro spinboxy ────────────────────────────────────────
static QSpinBox* mkInt(int lo, int hi, int val, int step = 1) {
    auto* s = new QSpinBox; s->setRange(lo, hi); s->setSingleStep(step); s->setValue(val);
    return s;
}
static QDoubleSpinBox* mkDbl(double lo, double hi, double val, double step, int dec) {
    auto* s = new QDoubleSpinBox; s->setRange(lo, hi); s->setSingleStep(step);
    s->setDecimals(dec); s->setValue(val); return s;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();

    // ── worker do vlastního vlákna ──────────────────────────────────
    worker_ = new SolverWorker;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &MainWindow::startSolver, worker_, &SolverWorker::run);
    connect(this, &MainWindow::requestSave, worker_, &SolverWorker::saveResults);
    connect(worker_, &SolverWorker::iteration,     this, &MainWindow::onIteration);
    connect(worker_, &SolverWorker::snapshotReady, this, &MainWindow::onSnapshot);
    connect(worker_, &SolverWorker::sliceReady,    this, &MainWindow::onSlice);
    connect(worker_, &SolverWorker::finished,      this, &MainWindow::onFinished);
    connect(worker_, &SolverWorker::saved,         this, &MainWindow::onSaved);
    thread_.start();

    updateSliceRange();
    refreshView();
}

MainWindow::~MainWindow() {
    if (worker_) worker_->stop();
    thread_.quit();
    thread_.wait();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // Zavření okna (křížek i tlačítko Konec) = zastav výpočet a ukonči vlákno.
    // Pokud výpočet běží, nejdřív se zeptáme (snadno přehlédnutelné kliknutí).
    if (running_) {
        auto ans = QMessageBox::question(
            this, "Ukončit aplikaci",
            "Výpočet stále běží. Opravdu ukončit?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes) { e->ignore(); return; }
    }
    // stop() nastaví atomik → callback vrátí false → solve() skončí po nejbližší
    // iteraci; thread_.wait() pak počká na bezpečné doběhnutí worker vlákna.
    if (worker_) worker_->stop();
    thread_.quit();
    thread_.wait();
    e->accept();   // okno se zavře; protože je hlavní, aplikace skončí
}

void MainWindow::buildUi() {
    setWindowTitle("CFD Cube — Qt");

    // ── panel: zadání (editovatelné) ────────────────────────────────
    reBox_   = mkInt(1, 2000, 20);
    uinBox_  = mkDbl(0.01, 50.0, 1.0, 0.1, 3);
    nxBox_   = mkInt(8, 400, 80, 4);
    nyBox_   = mkInt(4, 200, 20, 4);
    nzBox_   = mkInt(4, 200, 20, 4);
    lxBox_   = mkDbl(0.1, 50.0, 4.0, 0.1, 2);
    lyBox_   = mkDbl(0.1, 50.0, 1.0, 0.1, 2);
    lzBox_   = mkDbl(0.1, 50.0, 1.0, 0.1, 2);
    cubeXBox_= mkDbl(0.0, 50.0, 1.0, 0.05, 2);
    cubeYBox_= mkDbl(0.0, 50.0, 0.5, 0.05, 2);
    cubeZBox_= mkDbl(0.0, 50.0, 0.5, 0.05, 2);
    cubeDBox_= mkDbl(0.02, 10.0, 0.2, 0.02, 2);
    aUBox_   = mkDbl(0.05, 1.0, 0.7, 0.05, 2);
    aPBox_   = mkDbl(0.05, 1.0, 0.3, 0.05, 2);
    maxItBox_= mkInt(10, 100000, 2000, 100);
    tolBox_  = mkDbl(1e-9, 1e-1, 2e-5, 1e-5, 9);
    // počet vláken: max = počet jader stroje, default = všechna
    const int maxCores = std::max(1, QThread::idealThreadCount());
    threadsBox_ = mkInt(1, maxCores, maxCores);
    threadsBox_->setToolTip(QString("Počet vláken výpočtu (OpenMP). Stroj má %1 jader.")
                                .arg(maxCores));
    everyBox_ = mkInt(1, 1000, 10);
    everyBox_->setToolTip("Jak často (po kolika iteracích) se obnoví obraz pole.\n"
                          "Graf reziduí se kreslí každou iteraci. Menší číslo = plynulejší,\n"
                          "ale více přenosů/vykreslení.");

    auto* form = new QFormLayout;
    form->addRow("Reynolds (Re):", reBox_);
    form->addRow("U_in [m/s]:",    uinBox_);
    form->addRow("Síť NX:", nxBox_);
    form->addRow("Síť NY:", nyBox_);
    form->addRow("Síť NZ:", nzBox_);
    form->addRow("Doména Lx:", lxBox_);
    form->addRow("Doména Ly:", lyBox_);
    form->addRow("Doména Lz:", lzBox_);
    form->addRow("Krychle střed x:", cubeXBox_);
    form->addRow("Krychle střed y:", cubeYBox_);
    form->addRow("Krychle střed z:", cubeZBox_);
    form->addRow("Krychle strana D:", cubeDBox_);
    form->addRow("Relaxace αU:", aUBox_);
    form->addRow("Relaxace αP:", aPBox_);
    form->addRow("Max. iterací:", maxItBox_);
    form->addRow("Tolerance:", tolBox_);
    form->addRow("Vlákna (jádra):", threadsBox_);
    form->addRow("Obnova pole á (iter):", everyBox_);

    auto* paramGrp = new QGroupBox("Zadání");
    paramGrp->setLayout(form);

    startBtn_ = new QPushButton("▶ Spustit");
    stopBtn_  = new QPushButton("■ Stop");
    saveBtn_  = new QPushButton("💾 Uložit výsledky");
    exitBtn_  = new QPushButton("✕ Konec");
    stopBtn_->setEnabled(false);
    saveBtn_->setEnabled(false);
    statusLbl_ = new QLabel("Připraveno.");
    statusLbl_->setWordWrap(true);

    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(stopBtn_,  &QPushButton::clicked, this, &MainWindow::onStop);
    connect(saveBtn_,  &QPushButton::clicked, this, &MainWindow::onSave);
    connect(exitBtn_,  &QPushButton::clicked, this, &QWidget::close);  // → closeEvent

    auto* panel = new QVBoxLayout;
    panel->addWidget(paramGrp);
    panel->addWidget(startBtn_);
    panel->addWidget(stopBtn_);
    panel->addWidget(saveBtn_);
    panel->addWidget(exitBtn_);
    panel->addWidget(statusLbl_);
    panel->addStretch();

    auto* panelHost = new QWidget;
    panelHost->setLayout(panel);
    auto* panelScroll = new QScrollArea;
    panelScroll->setWidget(panelHost);
    panelScroll->setWidgetResizable(true);
    panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    panelScroll->setMinimumWidth(260);
    panelScroll->setMaximumWidth(320);

    // ── ovládání vizualizace ────────────────────────────────────────
    kindBox_ = new QComboBox;
    kindBox_->addItems({"|U| (rychlost)", "u_x", "u_y", "u_z",
                        "p (tlak)", "reziduum", "síť (mřížka)"});
    planeBox_ = new QComboBox;
    planeBox_->addItems({"XY (z = konst)", "XZ (y = konst)", "YZ (x = konst)"});
    modeBox_ = new QComboBox;
    modeBox_->addItems({"Plné 3D pole (procházení)", "Rychlý 2D náhled"});
    viewModeBox_ = new QComboBox;
    viewModeBox_->addItems({"2D řez", "3D doména"});

    sliceSld_ = new QSlider(Qt::Horizontal);
    sliceSld_->setRange(0, 0);
    sliceLbl_ = new QLabel("—");
    sliceLbl_->setMinimumWidth(90);

    connect(kindBox_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewChanged);
    connect(planeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewChanged);
    connect(modeBox_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewChanged);
    connect(sliceSld_, &QSlider::valueChanged, this, &MainWindow::onViewChanged);
    connect(viewModeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewChanged);

    auto* vizCtl = new QHBoxLayout;
    vizCtl->addWidget(new QLabel("Zobrazení:")); vizCtl->addWidget(viewModeBox_);
    vizCtl->addWidget(new QLabel("Veličina:"));   vizCtl->addWidget(kindBox_);
    vizCtl->addWidget(new QLabel("Rovina:"));      vizCtl->addWidget(planeBox_);
    vizCtl->addWidget(new QLabel("Řez:"));         vizCtl->addWidget(sliceSld_, 1);
    vizCtl->addWidget(sliceLbl_);
    vizCtl->addWidget(new QLabel("Data:"));        vizCtl->addWidget(modeBox_);

    fieldView_ = new FieldView;
    domain3D_  = new DomainView3D;
    viewStack_ = new QStackedWidget;
    viewStack_->addWidget(fieldView_);   // index 0 = 2D řez
    viewStack_->addWidget(domain3D_);    // index 1 = 3D doména

    // živý náhled domény při změně rozměrů / krychle (i před startem)
    for (QDoubleSpinBox* b : {lxBox_, lyBox_, lzBox_, cubeXBox_, cubeYBox_, cubeZBox_, cubeDBox_})
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){ updateDomainPreview(); });
    for (QSpinBox* b : {nxBox_, nyBox_, nzBox_})
        connect(b, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int){ updateDomainPreview(); });

    // ── graf konvergence ────────────────────────────────────────────
    series_ = new QLineSeries;
    auto* chart = new QChart;
    chart->addSeries(series_);
    chart->setTitle("Konvergence — kontinuitní reziduum");
    chart->legend()->hide();
    axX_ = new QValueAxis;     axX_->setTitleText("iterace");
    axY_ = new QLogValueAxis;  axY_->setTitleText("rel. reziduum");
    axY_->setBase(10.0); axY_->setRange(1e-6, 1.0);
    chart->addAxis(axX_, Qt::AlignBottom);
    chart->addAxis(axY_, Qt::AlignLeft);
    series_->attachAxis(axX_); series_->attachAxis(axY_);
    chartView_ = new QChartView(chart);
    chartView_->setRenderHint(QPainter::Antialiasing);

    auto* right = new QVBoxLayout;
    right->addLayout(vizCtl);
    right->addWidget(viewStack_, 3);
    right->addWidget(chartView_, 2);

    // ── celé okno ───────────────────────────────────────────────────
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->addWidget(panelScroll, 0);
    root->addLayout(right, 1);
    setCentralWidget(central);
    resize(1120, 680);

    // výchozí veličina = |U|, rovina XY
    kindBox_->setCurrentIndex(0);
    planeBox_->setCurrentIndex(0);

    updateDomainPreview();   // náhled domény hned po startu (ještě bez výpočtu)
}

// ── pomocné ─────────────────────────────────────────────────────────────
SimParams MainWindow::collectParams() const {
    SimParams p;
    p.Re   = reBox_->value();
    p.U_in = uinBox_->value();
    p.nx = nxBox_->value(); p.ny = nyBox_->value(); p.nz = nzBox_->value();
    p.Lx = lxBox_->value(); p.Ly = lyBox_->value(); p.Lz = lzBox_->value();
    p.cubeCx = cubeXBox_->value(); p.cubeCy = cubeYBox_->value();
    p.cubeCz = cubeZBox_->value(); p.cubeD = cubeDBox_->value();
    p.alphaU = aUBox_->value(); p.alphaP = aPBox_->value();
    p.maxIter = maxItBox_->value(); p.tol = tolBox_->value();
    p.threads = threadsBox_->value();
    p.sendFullField = (modeBox_->currentIndex() == 0);
    p.snapshotEvery = everyBox_->value();
    return p;
}

SlicePlane MainWindow::currentPlane() const {
    switch (planeBox_->currentIndex()) {
        case 1:  return SlicePlane::XZ;
        case 2:  return SlicePlane::YZ;
        default: return SlicePlane::XY;
    }
}

FieldKind MainWindow::currentKind() const {
    return static_cast<FieldKind>(kindBox_->currentIndex());   // pořadí odpovídá enumu
}

void MainWindow::updateSliceRange() {
    // počet řezů podle roviny — ze snapshotu, jinak z aktuálního zadání
    int nx = snap_.valid() ? snap_.nx : nxBox_->value();
    int ny = snap_.valid() ? snap_.ny : nyBox_->value();
    int nz = snap_.valid() ? snap_.nz : nzBox_->value();
    int count = 1;
    switch (currentPlane()) {
        case SlicePlane::XY: count = nz; break;
        case SlicePlane::XZ: count = ny; break;
        case SlicePlane::YZ: count = nx; break;
    }
    int prevMax = sliceSld_->maximum();
    bool wasMid = (sliceSld_->value() == prevMax / 2);
    sliceSld_->setRange(0, std::max(0, count - 1));
    if (prevMax == 0 || wasMid)            // při startu / změně roviny → na střed
        sliceSld_->setValue((count - 1) / 2);
}

QString MainWindow::viewTitle() const {
    static const char* kinds[] = {"|U|", "u_x", "u_y", "u_z", "p", "reziduum", "síť"};
    static const char* axis[]  = {"z", "y", "x"};
    int pi = planeBox_->currentIndex();
    int it = snap_.valid() ? snap_.iter : 0;
    return QString("%1 — řez %2  %3=%4/%5   (iter %6)")
        .arg(kinds[kindBox_->currentIndex()])
        .arg(planeBox_->currentText().section(' ', 0, 0))
        .arg(axis[pi])
        .arg(sliceSld_->value())
        .arg(sliceSld_->maximum())
        .arg(it);
}

void MainWindow::refreshView() {
    sliceLbl_->setText(QString("%1 / %2").arg(sliceSld_->value()).arg(sliceSld_->maximum()));
    if (!snap_.valid()) { fieldView_->setTitle(viewTitle()); return; }
    // Náhled před výpočtem nemá fyzikální data (samé nuly) → vykresli rovnou síť,
    // aby byla vidět mřížka + krychle, ne jen jednobarevná plocha.
    FieldKind k = snapIsPreview_ ? FieldKind::Mesh : currentKind();
    SliceData sl = makeSlice(snap_, currentPlane(), sliceSld_->value(), k);
    fieldView_->setTitle(viewTitle());
    fieldView_->setSlice(std::move(sl));
}

// Sestaví náhledový snapshot (jen geometrie/solid maska) ze zadání — bez výpočtu.
// Replikuje zaokrouhlení z Mesh::markSolid, aby náhled odpovídal skutečné síti.
static FieldSnapshot buildPreviewSnapshot(const SimParams& p) {
    FieldSnapshot s;
    s.nx = p.nx; s.ny = p.ny; s.nz = p.nz;
    s.dx = p.Lx / p.nx; s.dy = p.Ly / p.ny; s.dz = p.Lz / p.nz;
    const int N = p.nx * p.ny * p.nz;
    s.ux.assign(N, 0); s.uy.assign(N, 0); s.uz.assign(N, 0);
    s.p.assign(N, 0);  s.res.assign(N, 0); s.solid.assign(N, 0);

    auto rnd = [](double v){ return (int)(v + 0.5); };
    const double h = p.cubeD * 0.5;
    int i0 = rnd((p.cubeCx-h)/s.dx), i1 = rnd((p.cubeCx+h)/s.dx);
    int j0 = rnd((p.cubeCy-h)/s.dy), j1 = rnd((p.cubeCy+h)/s.dy);
    int k0 = rnd((p.cubeCz-h)/s.dz), k1 = rnd((p.cubeCz+h)/s.dz);
    i0 = std::max(0,i0); j0 = std::max(0,j0); k0 = std::max(0,k0);
    i1 = std::min(p.nx,i1); j1 = std::min(p.ny,j1); k1 = std::min(p.nz,k1);
    for (int k=k0;k<k1;++k) for (int j=j0;j<j1;++j) for (int i=i0;i<i1;++i)
        s.solid[s.idx(i,j,k)] = 1;
    return s;
}

void MainWindow::updateDomainPreview() {
    SimParams p = collectParams();
    // 3D widget — vždy ukáže aktuální geometrii i rozlišení sítě
    domain3D_->setDomain(p.Lx, p.Ly, p.Lz, p.cubeCx, p.cubeCy, p.cubeCz, p.cubeD,
                         p.nx, p.ny, p.nz);
    // 2D náhled sítě — jen dokud nemáme výsledek z výpočtu (pak nechceme přepsat data)
    if (!running_ && (snap_.empty() || snapIsPreview_)) {
        snap_ = buildPreviewSnapshot(p);
        snapIsPreview_ = true;
        updateSliceRange();
        refreshView();
    }
}

void MainWindow::setRunning(bool r) {
    running_ = r;
    startBtn_->setEnabled(!r);
    stopBtn_->setEnabled(r);
    saveBtn_->setEnabled(!r && snap_.valid() && !snapIsPreview_);
    // zadání nelze měnit za běhu (initializer_list explicitně typovaný na QWidget*)
    const std::initializer_list<QWidget*> inputs = {
        reBox_, uinBox_, nxBox_, nyBox_, nzBox_, lxBox_, lyBox_, lzBox_,
        cubeXBox_, cubeYBox_, cubeZBox_, cubeDBox_, aUBox_, aPBox_, maxItBox_, tolBox_,
        threadsBox_, everyBox_};
    for (QWidget* w : inputs)
        w->setEnabled(!r);
}

// ── sloty ───────────────────────────────────────────────────────────────
void MainWindow::onStart() {
    series_->clear();
    SimParams p = collectParams();

    axX_->setRange(0, p.maxIter);
    worker_->setMode(p.sendFullField);
    worker_->setView(currentPlane(), sliceSld_->value(), currentKind());

    setRunning(true);
    statusLbl_->setText(QString("Počítám… síť %1×%2×%3, %4 vláken")
                            .arg(p.nx).arg(p.ny).arg(p.nz).arg(p.threads));
    emit startSolver(p);
}

void MainWindow::onStop() {
    worker_->stop();
    statusLbl_->setText("Zastavuji…");
}

void MainWindow::onSave() {
    QString dir = QFileDialog::getExistingDirectory(this, "Vyber adresář pro uložení výsledků");
    if (dir.isEmpty()) return;
    statusLbl_->setText("Ukládám…");
    emit requestSave(dir);
}

void MainWindow::onIteration(int it, double residual) {
    series_->append(it, residual);
    statusLbl_->setText(QString("iter %1   res = %2").arg(it).arg(residual, 0, 'e', 3));
}

void MainWindow::onSnapshot(FieldSnapshot snap) {
    snap_ = std::move(snap);
    snapIsPreview_ = false;          // skutečný výsledek z výpočtu
    updateSliceRange();
    refreshView();
    if (!running_) saveBtn_->setEnabled(true);
}

void MainWindow::onSlice(SliceData slice) {
    // rychlý 2D režim — řez už spočítal worker pro aktuálně zvolený pohled
    fieldView_->setTitle(viewTitle());
    fieldView_->setSlice(std::move(slice));
}

void MainWindow::onFinished(bool converged, int iters) {
    Q_UNUSED(iters);
    setRunning(false);
    saveBtn_->setEnabled(snap_.valid());
    statusLbl_->setText(converged ? "Hotovo (konvergováno / dokončeno)." : "Zastaveno.");
}

void MainWindow::onSaved(const QString& message) {
    statusLbl_->setText(message);
}

void MainWindow::onViewChanged() {
    viewStack_->setCurrentIndex(viewModeBox_->currentIndex());   // 0=2D řez, 1=3D doména
    if (viewModeBox_->currentIndex() == 1)
        updateDomainPreview();   // 3D — promítni aktuální zadání
    updateSliceRange();
    // živý 2D režim: oznám workeru nový pohled (čte atomicky i za běhu)
    if (worker_) worker_->setView(currentPlane(), sliceSld_->value(), currentKind());
    if (worker_) worker_->setMode(modeBox_->currentIndex() == 0);
    refreshView();   // pokud máme snapshot, vyřízni lokálně (okamžitě)
}
