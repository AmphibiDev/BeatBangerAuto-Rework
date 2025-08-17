#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "MemoryScanner.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    MemoryScanner scanner;
    engine.rootContext()->setContextProperty("scanner", &scanner);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("BeatBangerAuto", "Main");

    return app.exec();
}
