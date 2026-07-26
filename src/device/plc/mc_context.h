#ifndef MC_CONTEXT_H
#define MC_CONTEXT_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QMetaType>
#include <qtmetamacros.h>
#include "device/plc/mc_define.h"
#include "device/plc/mc_device_map.h"
#include "device/plc/mc_msg_interface.h"
#include "device/plc/mc_msg_tcp_client.h"

using namespace vc::device::mc;

/// PLC (Mitsubishi MC-protocol family) device classes: frame contexts, message
/// interfaces, device maps, and the MC protocol/frame constants they share.
namespace vc::device {

/// Abstract base context for an MC-protocol PLC connection: holds the
/// frame-independent addressing/refresh settings (M/D device ranges, poll
/// interval) exposed to the UI via Q_PROPERTY, plus the message-interface
/// config and device map shared by every concrete frame type (see
/// Context_Mc3E for the 3E-frame specialization).
class McContext {
    Q_GADGET

    // --- Metadata for UI UI (Min/Max) ---
    Q_PROPERTY(vc::device::mc::McFrameType frameType READ frameType CONSTANT)
    Q_PROPERTY(vc::device::mc::McMsgItfType msgItfType READ msgIntefaceType CONSTANT)
    Q_PROPERTY(vc::device::mc::McDataCode dataCode READ dataCode CONSTANT)

    Q_PROPERTY(int refreshInterval READ refreshInterval WRITE setRefreshInterval)
    Q_PROPERTY(int activeMDevice READ activeMDevice WRITE setActiveMDevice)
    Q_PROPERTY(int startMAddress READ startMAddress WRITE setStartMAddress)
    Q_PROPERTY(int amountMAddress READ amountMAddress WRITE setAmountMAddress)
    Q_PROPERTY(int startDAddress READ startDAddress WRITE setStartDAddress)
    Q_PROPERTY(int amountDAddress READ amountDAddress WRITE setAmountDAddress)

    // --- Metadata for UI ---
    Q_CLASSINFO("frameType_name", "Frame")
    Q_CLASSINFO("msgItfType_name", "Interface")
    Q_CLASSINFO("dataCode_name", "Data Code")
    Q_CLASSINFO("refreshInterval_name", "Refresh Interval")
    Q_CLASSINFO("activeMDevice_name", "Active M Device")
    Q_CLASSINFO("startMAddress_name", "Start M-Address")
    Q_CLASSINFO("amountMAddress_name", "Amount M-Devices")
    Q_CLASSINFO("startDAddress_name", "Start D-Address")
    Q_CLASSINFO("amountDAddress_name", "Amount D-Devices")

    Q_CLASSINFO("refreshInterval_min", "10")
    Q_CLASSINFO("refreshInterval_max", "10000")

    Q_CLASSINFO("activeMDevice_min", "0")
    Q_CLASSINFO("activeMDevice_max", "65535")

    Q_CLASSINFO("startMAddress_min", "0")
    Q_CLASSINFO("startMAddress_max", "65535")

    Q_CLASSINFO("amountMAddress_min", "1")
    Q_CLASSINFO("amountMAddress_max", "1024")

    Q_CLASSINFO("startDAddress_min", "0")
    Q_CLASSINFO("startDAddress_max", "65535")

    Q_CLASSINFO("amountDAddress_min", "1")
    Q_CLASSINFO("amountDAddress_max", "1024")

public:
    // McContext() {};
    /// Default destructor.
    virtual ~McContext() = default;

    // --- abstract getter ---
    /// Returns the QMetaObject of the concrete (most-derived) context, for
    /// Q_GADGET-based property introspection.
    virtual const QMetaObject &getMetaObject() const = 0;
    /// Returns the MC frame type this context is configured for (e.g. Frame_3E).
    virtual McFrameType frameType() const = 0;
    /// Returns the message-interface type this context communicates over
    /// (e.g. EthernetTCPIP).
    virtual McMsgItfType msgIntefaceType() const = 0;

    // --- Getters ---
    /// Returns the configured data code (Binary/Ascii) used to encode/decode frames.
    McDataCode dataCode() const { return m_data_code; }
    /// Returns the active M-device address (used for the "in-progress" handshake bit).
    int activeMDevice() const { return m_activeMDevice; }
    /// Returns the polling/refresh interval, in milliseconds.
    int refreshInterval() const { return m_refreshInterval; }
    /// Returns the first M-device address in the subscribed range.
    int startMAddress() const { return m_startMAddress; }
    /// Returns the number of M-devices in the subscribed range.
    int amountMAddress() const { return m_amountMAddress; }
    /// Returns the first D-device address in the subscribed range.
    int startDAddress() const { return m_startDAddress; }
    /// Returns the number of D-devices in the subscribed range.
    int amountDAddress() const { return m_amountDAddress; }

    // --- Setters ---
    /// Sets the active M-device address.
    void setActiveMDevice(int val) { m_activeMDevice = val; }
    /// Sets the polling/refresh interval, in milliseconds.
    void setRefreshInterval(int val) { m_refreshInterval = val; }
    /// Sets the first M-device address in the subscribed range.
    void setStartMAddress(int val) { m_startMAddress = val; }
    /// Sets the number of M-devices in the subscribed range.
    void setAmountMAddress(int val) { m_amountMAddress = val; }
    /// Sets the first D-device address in the subscribed range.
    void setStartDAddress(int val) { m_startDAddress = val; }
    /// Sets the number of D-devices in the subscribed range.
    void setAmountDAddress(int val) { m_amountDAddress = val; }

    /// Creates a heap-allocated deep copy of this context. Caller takes ownership
    /// of the returned pointer.
    virtual McContext* clone() const = 0;

    /// Serializes the frame-independent settings (data code, refresh interval,
    /// M/D address ranges) to JSON. Concrete contexts override to add their own
    /// fields on top of this base object.
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["dataCode"] = qenumToString(m_data_code);
        obj["refreshInterval"] = m_refreshInterval;
        obj["activeMDevice"] = m_activeMDevice;
        obj["startMAddress"] = m_startMAddress;
        obj["amountMAddress"] = m_amountMAddress;
        obj["startDAddress"] = m_startDAddress;
        obj["amountDAddress"] = m_amountDAddress;
        return obj;
    }

    /// Restores the frame-independent settings from `obj` (falling back to the
    /// current field value when a key is missing). If "dataCode" is absent/unknown,
    /// resets m_data_code to Ascii and returns false; concrete contexts override
    /// to also restore their own fields and chain to this base implementation.
    /// @return false when the data code could not be parsed (default applied).
    virtual bool fromJson(const QJsonObject &obj) {
        m_data_code = stringToQEnum(obj["dataCode"].toString(), McDataCode::DataCode_User);
        m_refreshInterval = obj["refreshInterval"].toInt(m_refreshInterval);
        m_activeMDevice = obj["activeMDevice"].toInt(m_activeMDevice);
        m_startMAddress  = obj["startMAddress"].toInt(m_startMAddress);
        m_amountMAddress = obj["amountMAddress"].toInt(m_amountMAddress);
        m_startDAddress  = obj["startDAddress"].toInt(m_startDAddress);
        m_amountDAddress = obj["amountDAddress"].toInt(m_amountDAddress);
        if (m_data_code == McDataCode::DataCode_User) {
            m_data_code = McDataCode::Ascii;
            return false;
        }
        return true;
    }

    /// Returns the message-interface config (e.g. TCP host/port) backing this
    /// context. Non-owning raw pointer into m_msg_cfg; null only if a subclass
    /// failed to initialize it.
    virtual McMsgItfConfig* msgConfig() const {
        return m_msg_cfg.get();
    }

    /// Sets the device map used to track subscribed M/D device ranges. Ignores
    /// a null pointer (leaves any previously set map untouched) and does not
    /// take ownership of `dv_map`.
    void setDeviceMap(McDeviceMap* dv_map) {
        if (dv_map == nullptr) {
            return;
        }

        this->m_device_map = dv_map;
    }

    /// Returns the device map set via setDeviceMap(), or nullptr if none was set.
    McDeviceMap* getDeviceMap() {
        return this->m_device_map;
    }

public:
    int m_refreshInterval{100};   ///< Polling/refresh interval, in milliseconds.
    int m_activeMDevice{2000};    ///< Active M-device address (in-progress handshake bit).
    int m_startMAddress{2000};    ///< First M-device address in the subscribed range.
    int m_amountMAddress{64};     ///< Number of M-devices in the subscribed range.
    int m_startDAddress{2000};    ///< First D-device address in the subscribed range.
    int m_amountDAddress{64};     ///< Number of D-devices in the subscribed range.

protected:
    McDataCode m_data_code;                          ///< Data code (Binary/Ascii) used to encode/decode frames.
    std::shared_ptr<McMsgItfConfig> m_msg_cfg;        ///< Owning pointer to the message-interface config.
    McDeviceMap* m_device_map{nullptr};               ///< Non-owning pointer to the subscribed device-range map, if set.
};

}

Q_DECLARE_METATYPE(vc::device::McContext)

#endif // MC_CONTEXT_H
