#include "status_lamp.h"

#include <QStyle>

StatusLamp::StatusLamp(QWidget *parent)
    : QFrame(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(3);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setProperty("lampPart", QStringLiteral("name"));
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setProperty("lampPart", QStringLiteral("state"));
    m_stateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    layout->addWidget(m_nameLabel);
    layout->addWidget(m_stateLabel);

    // Set initial property so the first QSS polish reads the correct value
    setProperty("status", statusProperty(m_status));
}

void StatusLamp::setLampName(const QString &name)
{
    m_nameLabel->setText(name.toUpper());
}

void StatusLamp::setStatus(Status status, const QString &stateText)
{
    m_status = status;

    if (!stateText.isEmpty())
        m_stateLabel->setText(stateText);

    setProperty("status", statusProperty(status));
    style()->unpolish(this);
    style()->polish(this);
    update();
}

const char *StatusLamp::statusProperty(Status s)
{
    switch (s) {
    case Status::Ok:      return "ok";
    case Status::Warning: return "warning";
    case Status::Error:   return "error";
    default:              return "off";
    }
}
