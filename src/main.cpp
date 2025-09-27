#include "core/appcontroller.h"
#include "core/memoryscanner.h"
#include "utils/updatemanager.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QQmlApplicationEngine engine;

    MemoryScanner scanner;
    UpdateManager updateManager;

    AppController controller(&scanner, &updateManager);

    engine.rootContext()->setContextProperty("scanner", &scanner);
    engine.rootContext()->setContextProperty("updateManager", &updateManager);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("BeatBangerAuto", "Main");

    return app.exec();
}
