#include "mainwindow.h"

#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Identity used by QSettings (same approach as AmigaED):
    //   Windows: HKEY_CURRENT_USER\Software\mbergmann-sh\GuitarPROtoTeX-Convert
    //   Linux:   ~/.config/mbergmann-sh/GuitarPROtoTeX-Convert.conf
    //   macOS:   ~/Library/Preferences/com.mbergmann-sh.GuitarPROtoTeX-Convert.plist
    // Must be set BEFORE the first QSettings object is created (i.e. before MainWindow).
    QCoreApplication::setOrganizationName("mbergmann-sh");
    QCoreApplication::setOrganizationDomain("github.com/mbergmann-sh");
    QCoreApplication::setApplicationName("GuitarPROtoTeX-Convert");
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));   // VERSION in the .pro file

    // Translators are installed by MainWindow (stored GUI language, runtime switching)
    MainWindow w;
    w.show();
    return QApplication::exec();
}
