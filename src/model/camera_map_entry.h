#ifndef CAMERA_MAP_ENTRY_H
#define CAMERA_MAP_ENTRY_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

namespace vc::model {

/// Model types for camera/signal mapping: CameraMapEntry associates a PLC signal value with a
/// camera device id, with Qt property (Q_GADGET) and JSON (de)serialization support.
class CameraMapEntry {
    Q_GADGET

    Q_PROPERTY(int     signalValue
                   READ signalValue WRITE setSignalValue)
    Q_PROPERTY(QString cameraDeviceId
                   READ cameraDeviceId WRITE setCameraDeviceId)

public:
    /// Constructs a zero-value entry with an empty camera device id.
    CameraMapEntry() = default;
    /// Constructs an entry pairing signal value `sv` with camera device id `camId`.
    CameraMapEntry(int sv, const QString& camId)
        : m_signalValue(sv), m_cameraDeviceId(camId) {}

    /// Returns the PLC signal value this entry maps from.
    int     signalValue()    const { return m_signalValue;    }
    /// Returns the camera device id this entry maps to.
    QString cameraDeviceId() const { return m_cameraDeviceId; }

    /// Sets the PLC signal value.
    void setSignalValue(int v)           { m_signalValue    = v; }
    /// Sets the camera device id.
    void setCameraDeviceId(const QString& v) { m_cameraDeviceId = v; }

    /// Serializes this entry to a JSON object with "signalValue" and "cameraDeviceId" keys.
    /// @return the JSON representation of this entry
    QJsonObject toJson() const {
        return QJsonObject {
                           { "signalValue",    m_signalValue    },
                           { "cameraDeviceId", m_cameraDeviceId },
                           };
    }

    /// Builds a CameraMapEntry from a JSON object produced by toJson().
    /// @param o JSON object with "signalValue" and "cameraDeviceId" fields
    /// @return the reconstructed entry; missing fields default to 0 / an empty string
    static CameraMapEntry fromJson(const QJsonObject& o) {
        return CameraMapEntry {
            o["signalValue"].toInt(),
            o["cameraDeviceId"].toString()
        };
    }

private:
    int     m_signalValue    = 0;   ///< PLC signal value this entry maps from.
    QString m_cameraDeviceId;       ///< Camera device id this entry maps to.
};

} // namespace vc::model

Q_DECLARE_METATYPE(vc::model::CameraMapEntry)

#endif // CAMERA_MAP_ENTRY_H
