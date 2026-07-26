#ifndef MC_CONTEXT_3E_H
#define MC_CONTEXT_3E_H

#include "mc_context.h"
#include <QObject>
#include <QMetaType>

/// PLC (Mitsubishi MC-protocol family) device classes: this file adds the
/// concrete 3E-frame context on top of the shared McContext base.
namespace vc::device {

/// Concrete McContext for the MC 3E frame over Ethernet TCP/IP: adds the 3E
/// frame-header addressing fields (network/station/module-IO/multidrop numbers,
/// monitoring time) on top of the base M/D device-range settings, and always
/// carries a McMsgEthernetTcpCfg as its message-interface config.
class Context_Mc3E : public McContext {
    Q_GADGET

public:
    /// Constructs with Binary data code and a default-constructed
    /// McMsgEthernetTcpCfg message-interface config.
    Context_Mc3E() {
        m_data_code = McDataCode::Binary;
        m_msg_cfg = std::make_shared<McMsgEthernetTcpCfg>();
    }

    /// Constructs with the given data code and a default-constructed
    /// McMsgEthernetTcpCfg message-interface config.
    Context_Mc3E(McDataCode data_code) {
        m_data_code = data_code;
        m_msg_cfg = std::make_shared<McMsgEthernetTcpCfg>();
    }

    /// Default destructor (no owned resources beyond base-class members).
    ~Context_Mc3E() {

    }

    /// Returns this class's static QMetaObject, for Q_GADGET-based property
    /// introspection.
    const QMetaObject &getMetaObject() const override {
        return vc::device::Context_Mc3E::staticMetaObject;
    }

    /// Returns McFrameType::Frame_3E.
    McFrameType frameType() const override {
        return McFrameType::Frame_3E;
    }

    /// Returns McMsgItfType::EthernetTCPIP.
    McMsgItfType msgIntefaceType() const override {
        return McMsgItfType::EthernetTCPIP;
    }

    /// Creates a heap-allocated copy of this context via the copy constructor.
    /// Caller takes ownership of the returned pointer.
    McContext* clone() const override {
        return new Context_Mc3E(*this);
    }

    /// Serializes the base McContext settings plus the 3E frame-header fields
    /// (network/PC/module-IO/multidrop numbers, monitoring time) and, if set,
    /// the message-interface config under "MsgConfig".
    QJsonObject toJson() const override {
        QJsonObject obj = McContext::toJson();
        obj["NetworkNo"] = m_network;
        obj["PcNo"] = m_pc;
        obj["RequestDestNo"] = m_destModuleIo;
        obj["RequestStationNo"] = m_multiStation;
        obj["MonitoringTime"] = m_monitoringTime;

        if (m_msg_cfg) {
            obj["MsgConfig"] = m_msg_cfg->toJson();
        }
        return obj;
    }

    /// Restores the 3E frame-header fields from `obj` (falling back to the 3E
    /// default for each when the key is missing), lazily creates the
    /// McMsgEthernetTcpCfg message-interface config if not already present and
    /// restores it from "MsgConfig", then chains to McContext::fromJson() for
    /// the base settings.
    /// @return the base-class result (false if the base data code could not be parsed).
    bool fromJson(const QJsonObject &obj) override {
        m_network           = obj["NetworkNo"].toInt(0x00);
        m_pc                = obj["PcNo"].toInt(0xFF);
        m_destModuleIo    = obj["RequestDestNo"].toInt(0x03FF);
        m_multiStation     = obj["RequestStationNo"].toInt(0x00);
        m_monitoringTime   = obj["MonitoringTime"].toInt(0x04);

        if (!m_msg_cfg) {
            m_msg_cfg = std::make_shared<McMsgEthernetTcpCfg>();
        }
        m_msg_cfg->fromJson(obj["MsgConfig"].toObject());
        return McContext::fromJson(obj);
    }

public:
    /// 3E frame header layout (see the MC protocol reference for the full frame
    /// format):
    ///   SUB_HEADER                          — fixed for the 3E frame: 0x5000
    ///   REQUEST DESTINATION NETWORK NO.     — 0x00 when connected directly to a station
    ///   REQUEST DESTINATION STATION NO.     — 0xFF when connected directly to a station
    ///   REQUEST DESTINATION MODULE IO NO.   — refer to document; 0x03FF when connected directly
    ///   REQUEST DESTINATION MULTIDROP STATION NO. — normal value is 0x00
    ///   REQUEST DATA LENGTH                 — total bytes of [MONITORING TIME + REQUEST DATA]
    ///   MONITORING TIME
    ///   REQUEST DATA — SUB_HEADER + ACCESS_ROUTE + REQUEST_DATA_LEN + MONITORING_TIME + REQUEST_DATA

    const quint16 m_subHeader = 0x5000;  ///< Fixed sub-header for the 3E frame (0x5000).
    quint8 m_network = 0x00;             ///< Network No. of the access target (0-255); 0x00 when connected directly to a station.
    quint8 m_pc = 0xFF;                  ///< Network-module station No. of the access target (0-255); 0xFF when connected directly to a station.
    quint16 m_destModuleIo = 0x03FF;     ///< Destination module IO No.; 0x03FF when connected directly (see MC protocol docs).
    quint8 m_multiStation = 0x00;        ///< Multidrop station No.; normal value is 0x00.
    quint16 m_monitoringTime = 0x04;     ///< Monitoring time field of the request data.
};

}

Q_DECLARE_METATYPE(vc::device::Context_Mc3E)

#endif // MC_CONTEXT_3E_H
