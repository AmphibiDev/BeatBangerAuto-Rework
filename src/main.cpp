#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "core/AppController.h"
#include "core/MemoryScanner.h"
#include "utils/UpdateManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
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
