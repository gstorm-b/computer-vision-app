#include "gadget_table.h"

#include "core/qgadget_macro.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QSpinBox>

namespace {

/// Reads an integer Q_CLASSINFO entry such as "<prop>_min".
/// @return the value, or `fallback` when the entry is absent or not a number
int classInfoInt(const QMetaObject &meta, const char *prop, const char *suffix, int fallback) {
    const QByteArray key = QByteArray(prop) + suffix;
    const int index = meta.indexOfClassInfo(key.constData());
    if (index < 0) {
        return fallback;
    }
    bool ok = false;
    const int value = QByteArray(meta.classInfo(index).value()).toInt(&ok);
    return ok ? value : fallback;
}

} // namespace

GadgetTable::GadgetTable(QWidget *parent)
    : QTableWidget(parent) {

    setColumnCount(2);
    setHorizontalHeaderLabels({tr("Parameter"), tr("Value")});
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
}

void GadgetTable::setGadget(const QMetaObject &meta, void *gadget) {
    m_meta = &meta;
    m_gadget = gadget;

    clearContents();
    setRowCount(0);

    if (gadget == nullptr) {
        return;
    }

    for (int i = 0; i < meta.propertyCount(); ++i) {
        const QMetaProperty prop = meta.property(i);
        const QByteArray name = prop.name();
        if (name == "objectName") {
            continue;
        }

        const int row = rowCount();
        insertRow(row);

        // The display name comes from the same helper the product's property browsers use, so
        // the bench shows an operator exactly the labels the application does.
        auto *label = new QTableWidgetItem(vc::gadget_meta::displayName(meta, name.constData()));
        label->setToolTip(QString::fromLatin1(name));
        setItem(row, 0, label);

        const QVariant value = prop.readOnGadget(gadget);
        QWidget *editor = nullptr;

        if (prop.isEnumType()) {
            auto *combo = new QComboBox(this);
            const QMetaEnum metaEnum = prop.enumerator();
            const QStringList labels = vc::gadget_meta::enumKeyNames(prop);
            for (int k = 0; k < metaEnum.keyCount() && k < labels.size(); ++k) {
                combo->addItem(labels.at(k), metaEnum.value(k));
            }
            const int index = combo->findData(value.toInt());
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
            editor = combo;
        } else if (value.typeId() == QMetaType::Bool) {
            auto *check = new QCheckBox(this);
            check->setChecked(value.toBool());
            editor = check;
        } else if (value.typeId() == QMetaType::Int) {
            auto *spin = new QSpinBox(this);
            // Ranges come from the property's own Q_CLASSINFO where it declares them; the wide
            // fallback is deliberate, because refusing a value the PLC would have accepted is
            // worse on a bench than letting it fail visibly at build time.
            spin->setRange(classInfoInt(meta, name.constData(), "_min", -2147483647),
                           classInfoInt(meta, name.constData(), "_max", 2147483647));
            spin->setValue(value.toInt());
            editor = spin;
        } else {
            auto *line = new QLineEdit(value.toString(), this);
            editor = line;
        }

        editor->setEnabled(prop.isWritable());
        setCellWidget(row, 1, editor);
    }
}

void GadgetTable::commit() {
    if ((m_meta == nullptr) || (m_gadget == nullptr)) {
        return;
    }

    int row = 0;
    for (int i = 0; i < m_meta->propertyCount(); ++i) {
        const QMetaProperty prop = m_meta->property(i);
        if (QByteArray(prop.name()) == "objectName") {
            continue;
        }

        QWidget *editor = cellWidget(row, 1);
        ++row;

        if ((editor == nullptr) || !prop.isWritable()) {
            continue;
        }

        if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            prop.writeOnGadget(m_gadget, combo->currentData());
        } else if (auto *check = qobject_cast<QCheckBox *>(editor)) {
            prop.writeOnGadget(m_gadget, check->isChecked());
        } else if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
            prop.writeOnGadget(m_gadget, spin->value());
        } else if (auto *line = qobject_cast<QLineEdit *>(editor)) {
            prop.writeOnGadget(m_gadget, line->text());
        }
    }

    emit valueCommitted();
}
