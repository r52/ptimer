#include "ptimer.h"

#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QTextStream>
#include <QVBoxLayout>

// From https://stackoverflow.com/questions/42138599/how-to-format-stdchrono-durations
template <class... Durations, class DurationIn>
std::tuple<Durations...> break_down_durations(DurationIn d)
{
    std::tuple<Durations...> retval;
    using discard = int[];
    (void) discard{
        0, (void(((std::get<Durations>(retval) = std::chrono::duration_cast<Durations>(d)), (d -= std::chrono::duration_cast<DurationIn>(std::get<Durations>(retval))))), 0)...
    };
    return retval;
}

PTimer::PTimer(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    createContextMenuActions();
    createContextMenu();

    // Load settings
    QSettings settings("ptimer.ini", QSettings::IniFormat);
    restoreGeometry(settings.value("ptimer/geometry").toByteArray());

    m_showms = settings.value("ptimer/showms", m_showms).toBool();
    m_showMsAction->setChecked(m_showms);

    m_showfulltime = settings.value("ptimer/showfulltime", m_showfulltime).toBool();
    m_showFulltimeAction->setChecked(m_showfulltime);

    m_basetime = dur_s(settings.value("ptimer/savetime", 0.0).toDouble());

    // Load styles
    QString labelstyle("");

    QFile file("ptimer.css");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        labelstyle = in.readAll();
    }

    m_timelabel = new QLabel(this);
    m_timelabel->setAlignment(Qt::AlignCenter);

    if (!labelstyle.isEmpty())
    {
        m_timelabel->setStyleSheet(labelstyle);
    }

    QVBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_timelabel);

    ui.centralWidget->setLayout(layout);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, [=](const QPoint& pos) {
        m_Menu->exec(mapToGlobal(pos));
    });

    resize(480, 100);

    drawTimer(m_basetime);
}

PTimer::~PTimer()
{
    QSettings settings("ptimer.ini", QSettings::IniFormat);
    settings.setValue("ptimer/geometry", saveGeometry());
    settings.setValue("ptimer/showms", m_showms);
    settings.setValue("ptimer/showfulltime", m_showfulltime);

    if (!m_started || m_basetime > 0s)
    {
        settings.setValue("ptimer/savetime", m_basetime.count());
    }
}

void PTimer::createContextMenuActions()
{
    m_startPause = new QAction(tr("Start Timer"), this);
    m_startPause->setShortcut(Qt::Key_0 | Qt::KeypadModifier);
    connect(m_startPause, &QAction::triggered, this, &PTimer::startPauseTimer);
    addAction(m_startPause);

    m_reset = new QAction(tr("Reset Timer"), this);
    m_reset->setShortcut(Qt::Key_1 | Qt::KeypadModifier);
    connect(m_reset, &QAction::triggered, this, &PTimer::resetTimer);
    addAction(m_reset);

    m_showMsAction = new QAction(tr("&Show milliseconds"), this);
    m_showMsAction->setCheckable(true);
    connect(m_showMsAction, &QAction::triggered, [=](bool enabled) {
        m_showms = enabled;

        if (!m_started)
        {
            drawTimer(m_basetime);
        }
    });

    m_showFulltimeAction = new QAction(tr("&Show Full HH:MM:SS"), this);
    m_showFulltimeAction->setCheckable(true);
    connect(m_showFulltimeAction, &QAction::triggered, [=](bool enabled) {
        m_showfulltime = enabled;

        if (!m_started)
        {
            drawTimer(m_basetime);
        }
    });
}

void PTimer::createContextMenu()
{
    m_Menu = new QMenu("PTimer", this);
    m_Menu->addAction(m_startPause);
    m_Menu->addAction(m_reset);
    m_Menu->addAction(m_showMsAction);
    m_Menu->addAction(m_showFulltimeAction);
}

void PTimer::drawTimer(std::chrono::duration<double, std::nano> dur)
{
    static QString timelabel("");

    auto clean_duration = break_down_durations<std::chrono::hours, std::chrono::minutes, std::chrono::seconds, std::chrono::milliseconds>(dur);

    auto hours   = std::get<0>(clean_duration).count();
    auto minutes = std::get<1>(clean_duration).count();
    auto seconds = std::get<2>(clean_duration).count();
    auto milli   = std::get<3>(clean_duration).count();

    if (hours > 0 || m_showfulltime)
    {
        timelabel = QString::number(hours).rightJustified(2, '0') + ":" +
                    QString::number(minutes).rightJustified(2, '0') + ":" +
                    QString::number(seconds).rightJustified(2, '0');
    }
    else if (minutes > 0)
    {
        timelabel = QString::number(minutes) + ":" +
                    QString::number(seconds).rightJustified(2, '0');
    }
    else
    {
        timelabel = QString::number(seconds);
    }

    if (m_showms)
    {
        timelabel += "." + QString::number(milli).rightJustified(3, '0');
    }

    m_timelabel->setText(timelabel);
}

void PTimer::startPauseTimer()
{
    if (m_started)
    {
        // pause
        m_timer->stop();
        m_started = false;

        auto now = std::chrono::high_resolution_clock::now();
        auto dur = (now - m_starttp) + m_basetime;

        m_basetime = dur;

        drawTimer(dur);
    }
    else
    {
        // start
        if (!m_timer)
        {
            m_timer = std::make_unique<QTimer>(this);
            connect(m_timer.get(), &QTimer::timeout, this, &PTimer::processAndDisplay);
        }

        m_starttp = std::chrono::high_resolution_clock::now();
        m_started = true;
        m_timer->start(2);
    }
}

void PTimer::resetTimer()
{
    if (m_started)
    {
        // stop
        m_timer->stop();
        m_started = false;
    }

    // reset
    m_basetime = 0s;

    drawTimer(0s);
}

void PTimer::processAndDisplay()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto dur = (now - m_starttp) + m_basetime;

    drawTimer(dur);
}
