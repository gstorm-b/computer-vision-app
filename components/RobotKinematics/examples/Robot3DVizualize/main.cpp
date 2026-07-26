#include "mainwindow.h"

#include <QApplication>
#include <QFile>

#include <vtkAutoInit.h>

VTK_MODULE_INIT(vtkInteractionStyle)
VTK_MODULE_INIT(vtkRenderingOpenGL2)

/// Entry point for the Robot3DVizualize example: initializes VTK's interaction/OpenGL2
/// modules, applies the app's QSS stylesheet if present, then shows the main window.
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFile styleSheet(QStringLiteral(":/styles/robot3dvisualize.qss"));
    if (styleSheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
        a.setStyleSheet(QString::fromUtf8(styleSheet.readAll()));
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
