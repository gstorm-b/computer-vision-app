#include "mainwindow.h"
#include "modbusdefs.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("ModbusDemo"));
    QCoreApplication::setApplicationName(QStringLiteral("Modbus TCP Server Demo"));

    // These structs travel between the Modbus worker thread and the GUI thread
    // through queued connections, so they need a metatype.
    qRegisterMetaType<ModbusBlock>("ModbusBlock");
    qRegisterMetaType<ModbusStats>("ModbusStats");

    MainWindow window;
    window.show();

    return app.exec();
}
