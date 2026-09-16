#ifndef GADGET_TABLE_H
#define GADGET_TABLE_H

/**
 * @file gadget_table.h
 * @brief GadgetTable — edits any Q_GADGET config through its own meta-object.
 *
 * The bench does not re-declare the MC protocol parameters: it edits the product's own
 * `McContext` and `McMsgItfConfig` objects. Those are Q_GADGETs carrying every parameter as a
 * Q_PROPERTY, with display names and min/max in Q_CLASSINFO — so one generic table covers 3E,
 * 1C, 3C, the TCP transport and the serial transport, and will cover whatever frame is added
 * next without a line of new UI code.
 */

#include <QMetaObject>
#include <QTableWidget>
#include <QWidget>

/**
 * @class GadgetTable
 * @brief A two-column table (name, value) that reads and writes a Q_GADGET's properties.
 */
class GadgetTable : public QTableWidget {
    Q_OBJECT

public:
    /// Constructs an empty table; call setGadget() to populate it.
    explicit GadgetTable(QWidget *parent = nullptr);

    /**
     * @brief Rebuilds the rows for `gadget`, described by `meta`.
     * @param[in] meta the gadget's meta-object (from its getMetaObject())
     * @param[in] gadget pointer to the gadget instance; must outlive the table's edits
     *
     * Read-only properties (no WRITE, e.g. the CONSTANT frame type) are shown disabled rather
     * than hidden: on a commissioning bench, knowing which frame is loaded matters as much as
     * the values you can change.
     */
    void setGadget(const QMetaObject &meta, void *gadget);

    /// Writes every editor's current value back into the gadget.
    void commit();

signals:
    /// Emitted after commit() writes a value, so a caller can persist the settings.
    void valueCommitted();

private:
    const QMetaObject *m_meta{nullptr};  ///< Meta-object of the gadget being edited.
    void *m_gadget{nullptr};             ///< The gadget instance; not owned.
};

#endif // GADGET_TABLE_H
