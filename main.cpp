#include "ptimer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PTimer w;
    w.show();
    return a.exec();
}
