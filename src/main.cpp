#include <QApplication>
#include <QMetaType>
#include "MainWindow.hpp"
#include "SolverWorker.hpp"
#include "FieldData.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // Registrace typů posílaných přes signály/sloty mezi vlákny.
    qRegisterMetaType<SimParams>("SimParams");
    qRegisterMetaType<SliceData>("SliceData");
    qRegisterMetaType<FieldSnapshot>("FieldSnapshot");

    MainWindow w;
    w.show();
    return app.exec();
}
