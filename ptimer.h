#pragma once

#include "ui_ptimer.h"

#include <QTimer>
#include <QtWidgets/QMainWindow>
#include <chrono>
#include <memory>

QT_FORWARD_DECLARE_CLASS(QLabel)

using namespace std::chrono_literals;

using dur_s  = std::chrono::duration<double>;
using tpoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

class PTimer : public QMainWindow
{
    Q_OBJECT;

public:
    PTimer(QWidget* parent = Q_NULLPTR);
    ~PTimer();

private:
    void createContextMenuActions();
    void createContextMenu();

    void drawTimer(std::chrono::duration<double, std::nano> dur);

    Q_SLOT void startPauseTimer();
    Q_SLOT void resetTimer();
    Q_SLOT void processAndDisplay();

private:
    Ui::PTimerClass ui;

    bool m_started      = false;
    bool m_showms       = false;
    bool m_showfulltime = true;

    dur_s  m_basetime = 0s;
    tpoint m_starttp;

    std::unique_ptr<QTimer> m_timer;
    QLabel*                 m_timelabel;

    // context menu
    QMenu*   m_Menu;
    QAction* m_startPause;
    QAction* m_reset;
    QAction* m_edittime;
    QAction* m_showMsAction;
    QAction* m_showFulltimeAction;
};
