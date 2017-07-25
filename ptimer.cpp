#include "ptimer.h"

#include <QtWidgets>

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

class EditTimeDialog : public QDialog
{
public:
    EditTimeDialog(dur_s basetime, QWidget* parent = Q_NULLPTR)
        : QDialog(parent)
    {
        setMinimumWidth(300);
        setWindowTitle("Edit Time");

        auto split = break_down_durations<std::chrono::hours, std::chrono::minutes, std::chrono::seconds, std::chrono::milliseconds>(basetime);

        auto hours   = std::get<0>(split).count();
        auto minutes = std::get<1>(split).count();
        auto seconds = std::get<2>(split).count();
        auto milli   = std::get<3>(split).count();

        QSpinBox* hourSpin = new QSpinBox;
        hourSpin->setMinimum(0);
        hourSpin->setMaximum(999);
        hourSpin->setSingleStep(1);
        hourSpin->setValue(hours);

        QSpinBox* minSpin = new QSpinBox;
        minSpin->setMinimum(0);
        minSpin->setMaximum(59);
        minSpin->setSingleStep(1);
        minSpin->setValue(minutes);

        QSpinBox* secSpin = new QSpinBox;
        secSpin->setMinimum(0);
        secSpin->setMaximum(59);
        secSpin->setSingleStep(1);
        secSpin->setValue(seconds);

        QSpinBox* milSpin = new QSpinBox;
        milSpin->setMinimum(0);
        milSpin->setMaximum(999);
        milSpin->setSingleStep(1);
        milSpin->setValue(milli);

        QHBoxLayout* tlayout = new QHBoxLayout;
        tlayout->addWidget(new QLabel("Time (h:m:s.z):"));
        tlayout->addWidget(hourSpin);
        tlayout->addWidget(new QLabel(":"));
        tlayout->addWidget(minSpin);
        tlayout->addWidget(new QLabel(":"));
        tlayout->addWidget(secSpin);
        tlayout->addWidget(new QLabel("."));
        tlayout->addWidget(milSpin);

        QPushButton* okBtn     = new QPushButton(tr("OK"));
        QPushButton* cancelBtn = new QPushButton(tr("Cancel"));

        okBtn->setDefault(true);

        QHBoxLayout* btnlayout = new QHBoxLayout;
        btnlayout->addWidget(okBtn);
        btnlayout->addWidget(cancelBtn);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addLayout(tlayout);
        layout->addLayout(btnlayout);

        // cancel button
        connect(cancelBtn, &QPushButton::clicked, [=](bool checked) {
            close();
        });

        // ok button
        connect(okBtn, &QPushButton::clicked, [=](bool checked) {
            m_hour  = hourSpin->value();
            m_min   = minSpin->value();
            m_sec   = secSpin->value();
            m_milli = milSpin->value();
            accept();
        });

        setLayout(layout);
    }

    std::tuple<int, int, int, int> getTime()
    {
        return { m_hour, m_min, m_sec, m_milli };
    }

private:
    int m_hour;
    int m_min;
    int m_sec;
    int m_milli;
};

PTimer::PTimer(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    createContextMenuActions();
    createContextMenu();

    // Load settings
    QSettings settings("ptimer.ini", QSettings::IniFormat);

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
    restoreGeometry(settings.value("ptimer/geometry").toByteArray());

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

    m_edittime = new QAction(tr("Edit Time"), this);
    connect(m_edittime, &QAction::triggered, [=] {
        EditTimeDialog editdialog(m_basetime, this);

        if (editdialog.exec() == QDialog::Accepted)
        {
            auto time = editdialog.getTime();

            auto hours   = std::get<0>(time);
            auto minutes = std::get<1>(time);
            auto seconds = std::get<2>(time);
            auto milli   = std::get<3>(time);

            dur_s newtime = std::chrono::hours(hours) + std::chrono::minutes(minutes) + std::chrono::seconds(seconds) + std::chrono::milliseconds(milli);
            m_basetime    = newtime;

            if (!m_started)
            {
                drawTimer(m_basetime);
            }
        }
    });

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
    m_Menu->addAction(m_edittime);
    m_Menu->addAction(m_showMsAction);
    m_Menu->addAction(m_showFulltimeAction);
}

void PTimer::drawTimer(std::chrono::duration<double, std::nano> dur)
{
    static QString timelabel("");

    auto split = break_down_durations<std::chrono::hours, std::chrono::minutes, std::chrono::seconds, std::chrono::milliseconds>(dur);

    auto hours   = std::get<0>(split).count();
    auto minutes = std::get<1>(split).count();
    auto seconds = std::get<2>(split).count();
    auto milli   = std::get<3>(split).count();

    if (m_showfulltime || hours > 0)
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
        m_edittime->setEnabled(!m_started);

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

        m_started = true;
        m_edittime->setEnabled(!m_started);
        m_starttp = std::chrono::high_resolution_clock::now();
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
        m_edittime->setEnabled(!m_started);
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
