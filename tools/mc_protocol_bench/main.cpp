/**
 * @file main.cpp
 * @brief Entry point for the MC protocol bench.
 *
 * A commissioning tool, not part of the product: it drives the shipped MC frame codecs and
 * transports against a real PLC and reports what the round trip actually costs. The codecs are
 * compiled from `src/`, so the numbers describe the code the application runs.
 *
 * It is also the only thing that can verify the 1C and 3C frames at all. Their unit tests
 * (`tests/mc_frame_test`) assert that the codecs build the frames the reference implementations
 * describe — if that reading of a reference is wrong, the codec and its test are wrong together
 * and both stay green. Only a PLC answering can settle it.
 */

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Names the QSettings store the window uses to remember the last-used parameters.
    QCoreApplication::setOrganizationName(QStringLiteral("NCR"));
    QCoreApplication::setApplicationName(QStringLiteral("McProtocolBench"));

    MainWindow window;
    window.show();
    return app.exec();
}
