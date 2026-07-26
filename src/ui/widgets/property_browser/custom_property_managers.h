#pragma once

#include "qtpropertybrowser/qtpropertybrowser.h"
#include "qtpropertybrowser/qtpropertymanager.h"

#include <QVector>
#include <QStringList>
#include <QPoint>
#include <QPointF>

#include <climits>

/// Enables the OpenCV cv::Point/cv::Point2f/cv::Point3f/... interoperability (valueAsCvPoint*,
/// setValue(cv::Point...) overloads) when opencv2/core/types.hpp is available; those APIs are
/// compiled out otherwise.
#if __has_include(<opencv2/core/types.hpp>)
#  include <opencv2/core/types.hpp>
#  define NCR_PROP_HAS_OPENCV 1
#endif

/// Compound property manager for robot / machine positions, exposing X, Y, Z (and Roll,
/// Pitch, Yaw) as individual QtDoubleProperty sub-properties depending on the active Mode.
/// Three display modes:
///   - XY     — X, Y                   (2 components, e.g. image-plane point)
///   - XYZ    — X, Y, Z                (3 components, e.g. 3-D Cartesian)
///   - XYZRPY — X, Y, Z, Roll, Pitch, Yaw (6-DOF robot pose)
///
/// Each component is a QtDoubleProperty created internally. The sub-properties must have
/// their editor factory registered in the browser:
/// @code
///     browser->setFactoryForManager(posMan->subDoubleManager(), &dblFactory);
/// @endcode
///
/// Example:
/// @code
///     auto *pos = posMan->addProperty("TCP Position");
///     posMan->setMode(pos, PositionPropertyManager::XYZRPY);
///     posMan->setRange(pos, -1000.0, 1000.0);
///     posMan->setDecimals(pos, 2);
///     posMan->setValue(pos, {100.0, 200.0, 300.0, 0.0, 0.0, 45.0});
///     browser->addProperty(pos);
/// @endcode

class PositionPropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    /// Display mode selecting which components make up the compound value: XY (2 components),
    /// XYZ (3), or XYZRPY (6, full robot pose).
    enum Mode { XY = 0, XYZ = 1, XYZRPY = 2 };
    Q_ENUM(Mode)

    /// Returns the number of sub-property components for m (2 for XY, 3 for XYZ, 6 for XYZRPY).
    static int         modeComponentCount(Mode m);
    /// Returns the sub-property labels for m, e.g. {"X","Y"} or {"X","Y","Z","Roll","Pitch","Yaw"}.
    static QStringList modeLabels(Mode m);

    /// Constructs the manager and its internal sub QtDoublePropertyManager used for the
    /// X/Y/Z/Roll/Pitch/Yaw components.
    explicit PositionPropertyManager(QObject *parent = nullptr);
    ~PositionPropertyManager() override;

    /// Returns the internal double manager that owns every X/Y/Z/RPY sub-property; register
    /// this with the browser's double editor factory.
    QtDoublePropertyManager *subDoubleManager() const;

    // Value accessors
    /// Returns the current component values of property, in mode order.
    QVector<double> value     (const QtProperty *property) const;
    /// Returns the current display mode of property.
    Mode            mode      (const QtProperty *property) const;
    /// Returns the decimal places used by property's double sub-editors.
    int             decimals  (const QtProperty *property) const;
    /// Returns the minimum bound shared by every component of property.
    double          minimum   (const QtProperty *property) const;
    /// Returns the maximum bound shared by every component of property.
    double          maximum   (const QtProperty *property) const;
    /// Returns the single-step increment shared by every component of property.
    double          singleStep(const QtProperty *property) const;

public slots:
    /// Sets the component values of property (size must match modeComponentCount(mode));
    /// updates the sub-properties and emits valueChanged.
    void setValue    (QtProperty *property, const QVector<double> &val);
    /// Switches the display mode of property, destroying and recreating its sub-properties to
    /// match the new component count.
    void setMode     (QtProperty *property, Mode mode);
    /// Sets the decimal places shown by every sub double-property of property.
    void setDecimals (QtProperty *property, int prec);
    /// Sets the minimum/maximum bounds applied to every sub double-property of property.
    void setRange    (QtProperty *property, double minVal, double maxVal);
    /// Sets the single-step increment applied to every sub double-property of property.
    void setSingleStep(QtProperty *property, double step);

signals:
    /// Emitted after setValue (or an edit on a sub-property) changes property's values.
    void valueChanged   (QtProperty *property, const QVector<double> &val);
    /// Emitted after setMode changes property's display mode.
    void modeChanged    (QtProperty *property, Mode mode);
    /// Emitted after setDecimals changes property's decimal places.
    void decimalsChanged(QtProperty *property, int prec);

protected:
    /// Returns the text shown in the browser for property (its component values formatted for
    /// display).
    QString valueText(const QtProperty *property) const override;
    /// QtAbstractPropertyManager hook: creates the default Data entry and its sub-properties
    /// when property is added to the manager.
    void    initializeProperty  (QtProperty *property) override;
    /// QtAbstractPropertyManager hook: destroys property's sub-properties and Data entry when
    /// property is removed from the manager.
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    /// Reacts to a value change on sub (one of the X/Y/Z/RPY sub-properties): updates the
    /// owning Data::values entry and emits valueChanged for the parent compound property.
    void slotDoubleChanged    (QtProperty *sub, double value);
    /// Cleans up the m_subToParent mapping when sub is destroyed externally.
    void slotPropertyDestroyed(QtProperty *sub);

private:
    /// Per-property state backing one compound position property.
    struct Data {
        QVector<double>    values;           ///< Current component values in mode order.
        Mode               mode{XY};         ///< Active display mode.
        int                decimals{3};      ///< Decimal places shown by the sub double editors.
        double             minimum{-1e6};    ///< Shared minimum bound for all components.
        double             maximum{+1e6};    ///< Shared maximum bound for all components.
        double             singleStep{0.001}; ///< Shared step increment for all components.
        QList<QtProperty*> subProps;         ///< Child sub-properties owned by m_doubleManager.
    };

    /// Creates and appends the X/Y/Z/RPY sub double-properties matching data.mode under parent.
    void createSubProperties(QtProperty *parent, Data &data);
    /// Removes and deletes the sub-properties tracked in data.subProps.
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;       ///< Per top-level property state, keyed by the compound property.
    QMap<QtProperty *, QtProperty *> m_subToParent;  ///< Maps each sub double-property back to its owning compound property.
    QtDoublePropertyManager         *m_doubleManager; ///< Internal manager owning all created X/Y/Z/RPY sub-properties.

    Q_DISABLE_COPY(PositionPropertyManager)
};

/// Compound property manager for physical or pixel sizes, exposing Width, Height (and Depth)
/// as individual QtDoubleProperty sub-properties depending on the active Mode.
/// Two display modes:
///   - WH  — Width, Height          (2-D, e.g. image dimensions, pick-box)
///   - WHD — Width, Height, Depth   (3-D, e.g. bounding box)
///
/// Same factory-registration requirement as PositionPropertyManager: register
/// subDoubleManager() with the browser's double editor factory.

class SizePropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    /// Display mode selecting which components make up the compound value: WH (2 components)
    /// or WHD (3, adds Depth).
    enum Mode { WH = 0, WHD = 1 };
    Q_ENUM(Mode)

    /// Returns the number of sub-property components for m (2 for WH, 3 for WHD).
    static int         modeComponentCount(Mode m);
    /// Returns the sub-property labels for m, e.g. {"Width","Height"} or {"Width","Height","Depth"}.
    static QStringList modeLabels(Mode m);

    /// Constructs the manager and its internal sub QtDoublePropertyManager used for the
    /// Width/Height/Depth components.
    explicit SizePropertyManager(QObject *parent = nullptr);
    ~SizePropertyManager() override;

    /// Returns the internal double manager that owns every Width/Height/Depth sub-property;
    /// register this with the browser's double editor factory.
    QtDoublePropertyManager *subDoubleManager() const;

    /// Returns the current component values of property, in mode order.
    QVector<double> value     (const QtProperty *property) const;
    /// Returns the current display mode of property.
    Mode            mode      (const QtProperty *property) const;
    /// Returns the decimal places used by property's double sub-editors.
    int             decimals  (const QtProperty *property) const;
    /// Returns the minimum bound shared by every component of property.
    double          minimum   (const QtProperty *property) const;
    /// Returns the maximum bound shared by every component of property.
    double          maximum   (const QtProperty *property) const;
    /// Returns the single-step increment shared by every component of property.
    double          singleStep(const QtProperty *property) const;

public slots:
    /// Sets the component values of property (size must match modeComponentCount(mode));
    /// updates the sub-properties and emits valueChanged.
    void setValue    (QtProperty *property, const QVector<double> &val);
    /// Switches the display mode of property, destroying and recreating its sub-properties to
    /// match the new component count.
    void setMode     (QtProperty *property, Mode mode);
    /// Sets the decimal places shown by every sub double-property of property.
    void setDecimals (QtProperty *property, int prec);
    /// Sets the minimum/maximum bounds applied to every sub double-property of property.
    void setRange    (QtProperty *property, double minVal, double maxVal);
    /// Sets the single-step increment applied to every sub double-property of property.
    void setSingleStep(QtProperty *property, double step);

signals:
    /// Emitted after setValue (or an edit on a sub-property) changes property's values.
    void valueChanged   (QtProperty *property, const QVector<double> &val);
    /// Emitted after setMode changes property's display mode.
    void modeChanged    (QtProperty *property, Mode mode);
    /// Emitted after setDecimals changes property's decimal places.
    void decimalsChanged(QtProperty *property, int prec);

protected:
    /// Returns the text shown in the browser for property (its component values formatted for
    /// display).
    QString valueText(const QtProperty *property) const override;
    /// QtAbstractPropertyManager hook: creates the default Data entry and its sub-properties
    /// when property is added to the manager.
    void    initializeProperty  (QtProperty *property) override;
    /// QtAbstractPropertyManager hook: destroys property's sub-properties and Data entry when
    /// property is removed from the manager.
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    /// Reacts to a value change on sub (one of the Width/Height/Depth sub-properties): updates
    /// the owning Data::values entry and emits valueChanged for the parent compound property.
    void slotDoubleChanged    (QtProperty *sub, double value);
    /// Cleans up the m_subToParent mapping when sub is destroyed externally.
    void slotPropertyDestroyed(QtProperty *sub);

private:
    /// Per-property state backing one compound size property.
    struct Data {
        QVector<double>    values;           ///< Current component values in mode order.
        Mode               mode{WH};         ///< Active display mode.
        int                decimals{3};      ///< Decimal places shown by the sub double editors.
        double             minimum{0.0};     ///< Shared minimum bound for all components.
        double             maximum{+1e6};    ///< Shared maximum bound for all components.
        double             singleStep{0.001}; ///< Shared step increment for all components.
        QList<QtProperty*> subProps;         ///< Child sub-properties owned by m_doubleManager.
    };

    /// Creates and appends the Width/Height/Depth sub double-properties matching data.mode
    /// under parent.
    void createSubProperties(QtProperty *parent, Data &data);
    /// Removes and deletes the sub-properties tracked in data.subProps.
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;       ///< Per top-level property state, keyed by the compound property.
    QMap<QtProperty *, QtProperty *> m_subToParent;  ///< Maps each sub double-property back to its owning compound property.
    QtDoublePropertyManager         *m_doubleManager; ///< Internal manager owning all created Width/Height/Depth sub-properties.

    Q_DISABLE_COPY(SizePropertyManager)
};

/// Compound property manager for integer-coordinate points, exposing X, Y (and Z) as
/// individual QtIntProperty sub-properties depending on the active Mode.
/// Two display modes:
///   - XY  — X, Y       (maps to QPoint, cv::Point/cv::Point2i)
///   - XYZ — X, Y, Z    (maps to cv::Point3i)
///
/// The sub-properties must have their editor factory registered in the browser:
/// @code
///     browser->setFactoryForManager(ptMan->subIntManager(), &spinFactory);
/// @endcode
///
/// Example:
/// @code
///     auto *p = ptMan->addProperty("Pick pixel");
///     ptMan->setMode(p, PointPropertyManager::XY);
///     ptMan->setRange(p, 0, 4096);
///     ptMan->setValue(p, QPoint{640, 480});
///     browser->addProperty(p);
/// @endcode

class PointPropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    /// Display mode selecting which components make up the compound value: XY (2 components)
    /// or XYZ (3, adds Z).
    enum Mode { XY = 0, XYZ = 1 };
    Q_ENUM(Mode)

    /// Returns the number of sub-property components for m (2 for XY, 3 for XYZ).
    static int         modeComponentCount(Mode m);
    /// Returns the sub-property labels for m, e.g. {"X","Y"} or {"X","Y","Z"}.
    static QStringList modeLabels(Mode m);

    /// Constructs the manager and its internal sub QtIntPropertyManager used for the X/Y/Z
    /// components.
    explicit PointPropertyManager(QObject *parent = nullptr);
    ~PointPropertyManager() override;

    /// Returns the internal int manager that owns every X/Y/Z sub-property; register this
    /// with the browser's spin-box editor factory.
    QtIntPropertyManager *subIntManager() const;

    // Value accessors
    /// Returns the current component values of property, in mode order.
    QVector<int> value     (const QtProperty *property) const;
    /// Returns the value of property as a QPoint (XY mode only).
    QPoint       valueAsQPoint(const QtProperty *property) const;          // XY
    /// Returns the current display mode of property.
    Mode         mode      (const QtProperty *property) const;
    /// Returns the minimum bound shared by every component of property.
    int          minimum   (const QtProperty *property) const;
    /// Returns the maximum bound shared by every component of property.
    int          maximum   (const QtProperty *property) const;
    /// Returns the single-step increment shared by every component of property.
    int          singleStep(const QtProperty *property) const;

#ifdef NCR_PROP_HAS_OPENCV
    /// Returns the value of property as a cv::Point (XY mode only).
    cv::Point   valueAsCvPoint  (const QtProperty *property) const;        // XY
    /// Returns the value of property as a cv::Point3i (XYZ mode only).
    cv::Point3i valueAsCvPoint3i(const QtProperty *property) const;        // XYZ
#endif

public slots:
    /// Sets the component values of property (size must match modeComponentCount(mode));
    /// updates the sub-properties and emits valueChanged.
    void setValue    (QtProperty *property, const QVector<int> &val);
    /// Convenience overload: sets property to val.x()/val.y() (switches property to XY mode).
    void setValue    (QtProperty *property, const QPoint &val);
    /// Switches the display mode of property, destroying and recreating its sub-properties to
    /// match the new component count.
    void setMode     (QtProperty *property, Mode mode);
    /// Sets the minimum/maximum bounds applied to every sub int-property of property.
    void setRange    (QtProperty *property, int minVal, int maxVal);
    /// Sets the single-step increment applied to every sub int-property of property.
    void setSingleStep(QtProperty *property, int step);

#ifdef NCR_PROP_HAS_OPENCV
    /// Convenience overload: sets property from a cv::Point (switches property to XY mode).
    void setValue    (QtProperty *property, const cv::Point &val);
    /// Convenience overload: sets property from a cv::Point3i (switches property to XYZ mode).
    void setValue    (QtProperty *property, const cv::Point3i &val);
#endif

signals:
    /// Emitted after setValue (or an edit on a sub-property) changes property's values.
    void valueChanged(QtProperty *property, const QVector<int> &val);
    /// Emitted after setMode changes property's display mode.
    void modeChanged (QtProperty *property, Mode mode);

protected:
    /// Returns the text shown in the browser for property (its component values formatted for
    /// display).
    QString valueText(const QtProperty *property) const override;
    /// QtAbstractPropertyManager hook: creates the default Data entry and its sub-properties
    /// when property is added to the manager.
    void    initializeProperty  (QtProperty *property) override;
    /// QtAbstractPropertyManager hook: destroys property's sub-properties and Data entry when
    /// property is removed from the manager.
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    /// Reacts to a value change on sub (one of the X/Y/Z sub-properties): updates the owning
    /// Data::values entry and emits valueChanged for the parent compound property.
    void slotIntChanged       (QtProperty *sub, int value);
    /// Cleans up the m_subToParent mapping when sub is destroyed externally.
    void slotPropertyDestroyed(QtProperty *sub);

private:
    /// Per-property state backing one compound integer point property.
    struct Data {
        QVector<int>       values;      ///< Current component values in mode order.
        Mode               mode{XY};    ///< Active display mode.
        int                minimum{INT_MIN}; ///< Shared minimum bound for all components.
        int                maximum{INT_MAX}; ///< Shared maximum bound for all components.
        int                singleStep{1};    ///< Shared step increment for all components.
        QList<QtProperty*> subProps;    ///< Child sub-properties owned by m_intManager.
    };

    /// Creates and appends the X/Y/Z sub int-properties matching data.mode under parent.
    void createSubProperties(QtProperty *parent, Data &data);
    /// Removes and deletes the sub-properties tracked in data.subProps.
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;       ///< Per top-level property state, keyed by the compound property.
    QMap<QtProperty *, QtProperty *> m_subToParent;  ///< Maps each sub int-property back to its owning compound property.
    QtIntPropertyManager            *m_intManager;   ///< Internal manager owning all created X/Y/Z sub-properties.

    Q_DISABLE_COPY(PointPropertyManager)
};

/// Compound property manager for floating-point points, exposing X, Y (and Z) as individual
/// QtDoubleProperty sub-properties depending on the active Mode.
/// Two display modes:
///   - XY  — X, Y       (maps to QPointF, cv::Point2f, cv::Point2d)
///   - XYZ — X, Y, Z    (maps to cv::Point3f, cv::Point3d)
///
/// The sub-properties must have their editor factory registered in the browser:
/// @code
///     browser->setFactoryForManager(ptFMan->subDoubleManager(), &dblFactory);
/// @endcode

class PointFPropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    /// Display mode selecting which components make up the compound value: XY (2 components)
    /// or XYZ (3, adds Z).
    enum Mode { XY = 0, XYZ = 1 };
    Q_ENUM(Mode)

    /// Returns the number of sub-property components for m (2 for XY, 3 for XYZ).
    static int         modeComponentCount(Mode m);
    /// Returns the sub-property labels for m, e.g. {"X","Y"} or {"X","Y","Z"}.
    static QStringList modeLabels(Mode m);

    /// Constructs the manager and its internal sub QtDoublePropertyManager used for the X/Y/Z
    /// components.
    explicit PointFPropertyManager(QObject *parent = nullptr);
    ~PointFPropertyManager() override;

    /// Returns the internal double manager that owns every X/Y/Z sub-property; register this
    /// with the browser's double editor factory.
    QtDoublePropertyManager *subDoubleManager() const;

    /// Returns the current component values of property, in mode order.
    QVector<double> value         (const QtProperty *property) const;
    /// Returns the value of property as a QPointF (XY mode only).
    QPointF         valueAsQPointF(const QtProperty *property) const;      // XY
    /// Returns the current display mode of property.
    Mode            mode          (const QtProperty *property) const;
    /// Returns the decimal places used by property's double sub-editors.
    int             decimals      (const QtProperty *property) const;
    /// Returns the minimum bound shared by every component of property.
    double          minimum       (const QtProperty *property) const;
    /// Returns the maximum bound shared by every component of property.
    double          maximum       (const QtProperty *property) const;
    /// Returns the single-step increment shared by every component of property.
    double          singleStep    (const QtProperty *property) const;

#ifdef NCR_PROP_HAS_OPENCV
    /// Returns the value of property as a cv::Point2f (XY mode only).
    cv::Point2f valueAsCvPoint2f(const QtProperty *property) const;        // XY
    /// Returns the value of property as a cv::Point2d (XY mode only).
    cv::Point2d valueAsCvPoint2d(const QtProperty *property) const;        // XY
    /// Returns the value of property as a cv::Point3f (XYZ mode only).
    cv::Point3f valueAsCvPoint3f(const QtProperty *property) const;        // XYZ
    /// Returns the value of property as a cv::Point3d (XYZ mode only).
    cv::Point3d valueAsCvPoint3d(const QtProperty *property) const;        // XYZ
#endif

public slots:
    /// Sets the component values of property (size must match modeComponentCount(mode));
    /// updates the sub-properties and emits valueChanged.
    void setValue    (QtProperty *property, const QVector<double> &val);
    /// Convenience overload: sets property to val.x()/val.y() (switches property to XY mode).
    void setValue    (QtProperty *property, const QPointF &val);
    /// Switches the display mode of property, destroying and recreating its sub-properties to
    /// match the new component count.
    void setMode     (QtProperty *property, Mode mode);
    /// Sets the decimal places shown by every sub double-property of property.
    void setDecimals (QtProperty *property, int prec);
    /// Sets the minimum/maximum bounds applied to every sub double-property of property.
    void setRange    (QtProperty *property, double minVal, double maxVal);
    /// Sets the single-step increment applied to every sub double-property of property.
    void setSingleStep(QtProperty *property, double step);

#ifdef NCR_PROP_HAS_OPENCV
    /// Convenience overload: sets property from a cv::Point2f (switches property to XY mode).
    void setValue    (QtProperty *property, const cv::Point2f &val);
    /// Convenience overload: sets property from a cv::Point2d (switches property to XY mode).
    void setValue    (QtProperty *property, const cv::Point2d &val);
    /// Convenience overload: sets property from a cv::Point3f (switches property to XYZ mode).
    void setValue    (QtProperty *property, const cv::Point3f &val);
    /// Convenience overload: sets property from a cv::Point3d (switches property to XYZ mode).
    void setValue    (QtProperty *property, const cv::Point3d &val);
#endif

signals:
    /// Emitted after setValue (or an edit on a sub-property) changes property's values.
    void valueChanged   (QtProperty *property, const QVector<double> &val);
    /// Emitted after setMode changes property's display mode.
    void modeChanged    (QtProperty *property, Mode mode);
    /// Emitted after setDecimals changes property's decimal places.
    void decimalsChanged(QtProperty *property, int prec);

protected:
    /// Returns the text shown in the browser for property (its component values formatted for
    /// display).
    QString valueText(const QtProperty *property) const override;
    /// QtAbstractPropertyManager hook: creates the default Data entry and its sub-properties
    /// when property is added to the manager.
    void    initializeProperty  (QtProperty *property) override;
    /// QtAbstractPropertyManager hook: destroys property's sub-properties and Data entry when
    /// property is removed from the manager.
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    /// Reacts to a value change on sub (one of the X/Y/Z sub-properties): updates the owning
    /// Data::values entry and emits valueChanged for the parent compound property.
    void slotDoubleChanged    (QtProperty *sub, double value);
    /// Cleans up the m_subToParent mapping when sub is destroyed externally.
    void slotPropertyDestroyed(QtProperty *sub);

private:
    /// Per-property state backing one compound floating-point point property.
    struct Data {
        QVector<double>    values;           ///< Current component values in mode order.
        Mode               mode{XY};         ///< Active display mode.
        int                decimals{3};      ///< Decimal places shown by the sub double editors.
        double             minimum{-1e6};    ///< Shared minimum bound for all components.
        double             maximum{+1e6};    ///< Shared maximum bound for all components.
        double             singleStep{0.001}; ///< Shared step increment for all components.
        QList<QtProperty*> subProps;         ///< Child sub-properties owned by m_doubleManager.
    };

    /// Creates and appends the X/Y/Z sub double-properties matching data.mode under parent.
    void createSubProperties(QtProperty *parent, Data &data);
    /// Removes and deletes the sub-properties tracked in data.subProps.
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;       ///< Per top-level property state, keyed by the compound property.
    QMap<QtProperty *, QtProperty *> m_subToParent;  ///< Maps each sub double-property back to its owning compound property.
    QtDoublePropertyManager         *m_doubleManager; ///< Internal manager owning all created X/Y/Z sub-properties.

    Q_DISABLE_COPY(PointFPropertyManager)
};
