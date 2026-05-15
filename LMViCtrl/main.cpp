#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/logo.svg"));

    QFont appFont(QStringLiteral("Microsoft YaHei"), 10);
    appFont.setStyleHint(QFont::SansSerif);
    a.setFont(appFont);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "LMViCtrl_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
