#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ptimer.h"

class PTimer : public QMainWindow
{
    Q_OBJECT

public:
    PTimer(QWidget *parent = Q_NULLPTR);

private:
    Ui::PTimerClass ui;
};
