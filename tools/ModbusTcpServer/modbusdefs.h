#ifndef MODBUSDEFS_H
#define MODBUSDEFS_H

//
// Shared, Modbus-agnostic definitions used by both the GUI thread (MainWindow)
// and the worker thread (ModbusRunner).
//
// Nothing here includes QtSerialBus on purpose: MainWindow must not depend on
// the Modbus back-end, it only speaks in plain "area index / address / value".
//

#include <QtGlobal>
#include <QList>
#include <QMetaType>
#include <QString>

namespace ModbusDemo {

//! Number of addresses exposed per Modbus area. Change it here only.
constexpr int kAddressCount = 64;

//! First Modbus address of every area.
constexpr int kStartAddress = 0;

constexpr int kDefaultPort   = 502;
constexpr int kDefaultUnitId = 1;

//! Index of a Modbus area. Matches the tab order of the HMI.
enum Area {
    Coils            = 0,   // 0x - read/write bits   (FC 01 / 05 / 15)
    DiscreteInputs   = 1,   // 1x - read only bits    (FC 02)
    InputRegisters   = 2,   // 3x - read only words   (FC 04)
    HoldingRegisters = 3,   // 4x - read/write words  (FC 03 / 06 / 16)
    AreaCount        = 4
};

//! Severity of a log line. Kept as a plain int inside signals.
enum LogLevel {
    LogDebug   = 0,
    LogInfo    = 1,
    LogWarning = 2,
    LogError   = 3
};

//! true for the two bit oriented areas (coils / discrete inputs).
inline bool isBitArea(int area)
{
    return area == Coils || area == DiscreteInputs;
}

//! true for the areas a Modbus client is allowed to write into.
inline bool isClientWritableArea(int area)
{
    return area == Coils || area == HoldingRegisters;
}

//! Short name of an area, used in log lines.
inline QString areaName(int area)
{
    switch (area) {
    case Coils:            return QStringLiteral("Coils");
    case DiscreteInputs:   return QStringLiteral("DiscreteInputs");
    case InputRegisters:   return QStringLiteral("InputRegisters");
    case HoldingRegisters: return QStringLiteral("HoldingRegisters");
    default:               return QStringLiteral("Unknown");
    }
}

//! Conventional first PLC reference of an area. The HMI lets the user change
//! it per area, so this is only the value the spin boxes start from.
inline int defaultPlcBase(int area)
{
    switch (area) {
    case Coils:            return 1;
    case DiscreteInputs:   return 10001;
    case InputRegisters:   return 30001;
    case HoldingRegisters: return 40001;
    default:               return 0;
    }
}

} // namespace ModbusDemo

//!
//! One contiguous chunk of an area. This is the only payload travelling
//! between the worker thread and the GUI thread when values change.
//!
struct ModbusBlock
{
    int area = -1;                 //!< ModbusDemo::Area
    int startAddress = 0;
    QList<quint16> values;
    bool fromClient = false;       //!< true when a Modbus client caused the change
};
Q_DECLARE_METATYPE(ModbusBlock)

//! Counters shown in the HMI status panel.
struct ModbusStats
{
    quint64 readRequests  = 0;
    quint64 writeRequests = 0;
    quint64 otherRequests = 0;
    quint64 exceptions    = 0;
    int connectedClients  = 0;
};
Q_DECLARE_METATYPE(ModbusStats)

#endif // MODBUSDEFS_H
