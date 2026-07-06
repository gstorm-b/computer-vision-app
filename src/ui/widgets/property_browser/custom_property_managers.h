#pragma once

#include "qtpropertybrowser/qtpropertybrowser.h"
#include "qtpropertybrowser/qtpropertymanager.h"

#include <QVector>
#include <QStringList>
#include <QPoint>
#include <QPointF>

#include <climits>

#if __has_include(<opencv2/core/types.hpp>)
#  include <opencv2/core/types.hpp>
#  define NCR_PROP_HAS_OPENCV 1
#endif

// ============================================================================
//  PositionPropertyManager
//
//  A compound property for robot / machine positions.
//  Three display modes:
//    XY     — X, Y                   (2 components, e.g. image-plane point)
//    XYZ    — X, Y, Z                (3 components, e.g. 3-D Cartesian)
//    XYZRPY — X, Y, Z, Roll, Pitch, Yaw (6-DOF robot pose)
//
//  Each component is a QtDoubleProperty created internally.
//  The sub-properties must have their editor factory registered in the
//  browser:
//      browser->setFactoryForManager(posMan->subDoubleManager(), &dblFactory);
//
//  Example:
//      auto *pos = posMan->addProperty("TCP Position");
//      posMan->setMode(pos, PositionPropertyManager::XYZRPY);
//      posMan->setRange(pos, -1000.0, 1000.0);
//      posMan->setDecimals(pos, 2);
//      posMan->setValue(pos, {100.0, 200.0, 300.0, 0.0, 0.0, 45.0});
//      browser->addProperty(pos);
// ============================================================================

class PositionPropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    enum Mode { XY = 0, XYZ = 1, XYZRPY = 2 };
    Q_ENUM(Mode)

    static int         modeComponentCount(Mode m);
    static QStringList modeLabels(Mode m);

    explicit PositionPropertyManager(QObject *parent = nullptr);
    ~PositionPropertyManager() override;

    QtDoublePropertyManager *subDoubleManager() const;

    // Value accessors
    QVector<double> value     (const QtProperty *property) const;
    Mode            mode      (const QtProperty *property) const;
    int             decimals  (const QtProperty *property) const;
    double          minimum   (const QtProperty *property) const;
    double          maximum   (const QtProperty *property) const;
    double          singleStep(const QtProperty *property) const;

public slots:
    void setValue    (QtProperty *property, const QVector<double> &val);
    void setMode     (QtProperty *property, Mode mode);
    void setDecimals (QtProperty *property, int prec);
    void setRange    (QtProperty *property, double minVal, double maxVal);
    void setSingleStep(QtProperty *property, double step);

signals:
    void valueChanged   (QtProperty *property, const QVector<double> &val);
    void modeChanged    (QtProperty *property, Mode mode);
    void decimalsChanged(QtProperty *property, int prec);

protected:
    QString valueText(const QtProperty *property) const override;
    void    initializeProperty  (QtProperty *property) override;
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    void slotDoubleChanged    (QtProperty *sub, double value);
    void slotPropertyDestroyed(QtProperty *sub);

private:
    struct Data {
        QVector<double>    values;
        Mode               mode{XY};
        int                decimals{3};
        double             minimum{-1e6};
        double             maximum{+1e6};
        double             singleStep{0.001};
        QList<QtProperty*> subProps;
    };

    void createSubProperties(QtProperty *parent, Data &data);
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;
    QMap<QtProperty *, QtProperty *> m_subToParent;
    QtDoublePropertyManager         *m_doubleManager;

    Q_DISABLE_COPY(PositionPropertyManager)
};

// ============================================================================
//  SizePropertyManager
//
//  A compound property for physical or pixel sizes.
//  Two display modes:
//    WH  — Width, Height          (2-D, e.g. image dimensions, pick-box)
//    WHD — Width, Height, Depth   (3-D, e.g. bounding box)
//
//  Same factory-registration requirement as PositionPropertyManager.
// ============================================================================

class SizePropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    enum Mode { WH = 0, WHD = 1 };
    Q_ENUM(Mode)

    static int         modeComponentCount(Mode m);
    static QStringList modeLabels(Mode m);

    explicit SizePropertyManager(QObject *parent = nullptr);
    ~SizePropertyManager() override;

    QtDoublePropertyManager *subDoubleManager() const;

    QVector<double> value     (const QtProperty *property) const;
    Mode            mode      (const QtProperty *property) const;
    int             decimals  (const QtProperty *property) const;
    double          minimum   (const QtProperty *property) const;
    double          maximum   (const QtProperty *property) const;
    double          singleStep(const QtProperty *property) const;

public slots:
    void setValue    (QtProperty *property, const QVector<double> &val);
    void setMode     (QtProperty *property, Mode mode);
    void setDecimals (QtProperty *property, int prec);
    void setRange    (QtProperty *property, double minVal, double maxVal);
    void setSingleStep(QtProperty *property, double step);

signals:
    void valueChanged   (QtProperty *property, const QVector<double> &val);
    void modeChanged    (QtProperty *property, Mode mode);
    void decimalsChanged(QtProperty *property, int prec);

protected:
    QString valueText(const QtProperty *property) const override;
    void    initializeProperty  (QtProperty *property) override;
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    void slotDoubleChanged    (QtProperty *sub, double value);
    void slotPropertyDestroyed(QtProperty *sub);

private:
    struct Data {
        QVector<double>    values;
        Mode               mode{WH};
        int                decimals{3};
        double             minimum{0.0};
        double             maximum{+1e6};
        double             singleStep{0.001};
        QList<QtProperty*> subProps;
    };

    void createSubProperties(QtProperty *parent, Data &data);
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;
    QMap<QtProperty *, QtProperty *> m_subToParent;
    QtDoublePropertyManager         *m_doubleManager;

    Q_DISABLE_COPY(SizePropertyManager)
};

// ============================================================================
//  PointPropertyManager
//
//  Integer-coordinate compound property. Two display modes:
//    XY  — X, Y       (maps to QPoint, cv::Point/cv::Point2i)
//    XYZ — X, Y, Z    (maps to cv::Point3i)
//
//  The sub-properties must have their editor factory registered in the
//  browser:
//      browser->setFactoryForManager(ptMan->subIntManager(), &spinFactory);
//
//  Example:
//      auto *p = ptMan->addProperty("Pick pixel");
//      ptMan->setMode(p, PointPropertyManager::XY);
//      ptMan->setRange(p, 0, 4096);
//      ptMan->setValue(p, QPoint{640, 480});
//      browser->addProperty(p);
// ============================================================================

class PointPropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    enum Mode { XY = 0, XYZ = 1 };
    Q_ENUM(Mode)

    static int         modeComponentCount(Mode m);
    static QStringList modeLabels(Mode m);

    explicit PointPropertyManager(QObject *parent = nullptr);
    ~PointPropertyManager() override;

    QtIntPropertyManager *subIntManager() const;

    // Value accessors
    QVector<int> value     (const QtProperty *property) const;
    QPoint       valueAsQPoint(const QtProperty *property) const;          // XY
    Mode         mode      (const QtProperty *property) const;
    int          minimum   (const QtProperty *property) const;
    int          maximum   (const QtProperty *property) const;
    int          singleStep(const QtProperty *property) const;

#ifdef NCR_PROP_HAS_OPENCV
    cv::Point   valueAsCvPoint  (const QtProperty *property) const;        // XY
    cv::Point3i valueAsCvPoint3i(const QtProperty *property) const;        // XYZ
#endif

public slots:
    void setValue    (QtProperty *property, const QVector<int> &val);
    void setValue    (QtProperty *property, const QPoint &val);
    void setMode     (QtProperty *property, Mode mode);
    void setRange    (QtProperty *property, int minVal, int maxVal);
    void setSingleStep(QtProperty *property, int step);

#ifdef NCR_PROP_HAS_OPENCV
    void setValue    (QtProperty *property, const cv::Point &val);
    void setValue    (QtProperty *property, const cv::Point3i &val);
#endif

signals:
    void valueChanged(QtProperty *property, const QVector<int> &val);
    void modeChanged (QtProperty *property, Mode mode);

protected:
    QString valueText(const QtProperty *property) const override;
    void    initializeProperty  (QtProperty *property) override;
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    void slotIntChanged       (QtProperty *sub, int value);
    void slotPropertyDestroyed(QtProperty *sub);

private:
    struct Data {
        QVector<int>       values;
        Mode               mode{XY};
        int                minimum{INT_MIN};
        int                maximum{INT_MAX};
        int                singleStep{1};
        QList<QtProperty*> subProps;
    };

    void createSubProperties(QtProperty *parent, Data &data);
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;
    QMap<QtProperty *, QtProperty *> m_subToParent;
    QtIntPropertyManager            *m_intManager;

    Q_DISABLE_COPY(PointPropertyManager)
};

// ============================================================================
//  PointFPropertyManager
//
//  Floating-point compound property. Two display modes:
//    XY  — X, Y       (maps to QPointF, cv::Point2f, cv::Point2d)
//    XYZ — X, Y, Z    (maps to cv::Point3f, cv::Point3d)
//
//  The sub-properties must have their editor factory registered in the
//  browser:
//      browser->setFactoryForManager(ptFMan->subDoubleManager(), &dblFactory);
// ============================================================================

class PointFPropertyManager : public QtAbstractPropertyManager {
    Q_OBJECT
public:
    enum Mode { XY = 0, XYZ = 1 };
    Q_ENUM(Mode)

    static int         modeComponentCount(Mode m);
    static QStringList modeLabels(Mode m);

    explicit PointFPropertyManager(QObject *parent = nullptr);
    ~PointFPropertyManager() override;

    QtDoublePropertyManager *subDoubleManager() const;

    QVector<double> value         (const QtProperty *property) const;
    QPointF         valueAsQPointF(const QtProperty *property) const;      // XY
    Mode            mode          (const QtProperty *property) const;
    int             decimals      (const QtProperty *property) const;
    double          minimum       (const QtProperty *property) const;
    double          maximum       (const QtProperty *property) const;
    double          singleStep    (const QtProperty *property) const;

#ifdef NCR_PROP_HAS_OPENCV
    cv::Point2f valueAsCvPoint2f(const QtProperty *property) const;        // XY
    cv::Point2d valueAsCvPoint2d(const QtProperty *property) const;        // XY
    cv::Point3f valueAsCvPoint3f(const QtProperty *property) const;        // XYZ
    cv::Point3d valueAsCvPoint3d(const QtProperty *property) const;        // XYZ
#endif

public slots:
    void setValue    (QtProperty *property, const QVector<double> &val);
    void setValue    (QtProperty *property, const QPointF &val);
    void setMode     (QtProperty *property, Mode mode);
    void setDecimals (QtProperty *property, int prec);
    void setRange    (QtProperty *property, double minVal, double maxVal);
    void setSingleStep(QtProperty *property, double step);

#ifdef NCR_PROP_HAS_OPENCV
    void setValue    (QtProperty *property, const cv::Point2f &val);
    void setValue    (QtProperty *property, const cv::Point2d &val);
    void setValue    (QtProperty *property, const cv::Point3f &val);
    void setValue    (QtProperty *property, const cv::Point3d &val);
#endif

signals:
    void valueChanged   (QtProperty *property, const QVector<double> &val);
    void modeChanged    (QtProperty *property, Mode mode);
    void decimalsChanged(QtProperty *property, int prec);

protected:
    QString valueText(const QtProperty *property) const override;
    void    initializeProperty  (QtProperty *property) override;
    void    uninitializeProperty(QtProperty *property) override;

private slots:
    void slotDoubleChanged    (QtProperty *sub, double value);
    void slotPropertyDestroyed(QtProperty *sub);

private:
    struct Data {
        QVector<double>    values;
        Mode               mode{XY};
        int                decimals{3};
        double             minimum{-1e6};
        double             maximum{+1e6};
        double             singleStep{0.001};
        QList<QtProperty*> subProps;
    };

    void createSubProperties(QtProperty *parent, Data &data);
    void destroySubProperties(QtProperty *parent, Data &data);

    QMap<const QtProperty *, Data>   m_values;
    QMap<QtProperty *, QtProperty *> m_subToParent;
    QtDoublePropertyManager         *m_doubleManager;

    Q_DISABLE_COPY(PointFPropertyManager)
};
