#ifndef VISION_OUTPUT_REQUEST_H
#define VISION_OUTPUT_REQUEST_H

#include "device/irequest.h"

#include <QString>
#include <QVector>
#include <memory>

/// Request/payload types for the vision-output device family: matching-result positions
/// and the wire-format request sent down to the external system.
namespace vc::device {

/// A single matching position/pose expressed as 4 axes: x, y, z, r.
struct VisionOutputPosition {
    double x{0.0};  ///< X coordinate (mm).
    double y{0.0};  ///< Y coordinate (mm).
    double z{0.0};  ///< Z coordinate (mm).
    double r{0.0};  ///< Rotation about Z (deg).

    /// Formats this position as the wire-format "x,y,z,r" field group.
    /// @return "%1,%2,%3,%4" with each axis fixed-width, zero-padded to 8 chars with 2
    /// decimals (e.g. 1.0 -> "00001.00"); the downstream robot parser relies on this
    /// exact field format.
    QString toString() const {
        return QString("%1,%2,%3,%4")
            .arg(QString::asprintf("%08.2f", x),
                 QString::asprintf("%08.2f", y),
                 QString::asprintf("%08.2f", z),
                 QString::asprintf("%08.2f", r));
    }
};

/// Request sent to VisionOutputDevice. Comes in 2 kinds:
///      - Result: the server pushes matching results down to the client on port 1.
///      - Raw: raw bytes are sent straight down port 1 (used for test / extension).
///
/// Result format: "{detected number},{Position 1},{Position 2},...;" where each
/// position is "x,y,z,r".
class VisionOutputRequest : public IRequest {
public:
    /// Discriminates which payload this request carries.
    enum Kind {
        Result,  ///< Structured matching-result positions (see positions()).
        Raw      ///< Opaque raw bytes to send as-is (see rawPayload()).
    };

    VisionOutputRequest() = default;

    /// Builds a Result-kind request carrying the given matching positions.
    explicit VisionOutputRequest(QVector<VisionOutputPosition> positions)
        : m_kind(Result), m_positions(std::move(positions)) {}

    /// Builds a Raw-kind request carrying the given opaque payload.
    explicit VisionOutputRequest(QByteArray rawPayload)
        : m_kind(Raw), m_rawPayload(std::move(rawPayload)) {}

    /// Returns RequestType::Request_VisionOutput.
    RequestType type() const override {
        return RequestType::Request_VisionOutput;
    }

    /// Returns a deep copy of this request via the copy constructor.
    std::shared_ptr<IRequest> clone() const override {
        return std::make_shared<VisionOutputRequest>(*this);
    }

    /// Returns whether this request carries structured positions (Result) or an
    /// opaque byte payload (Raw).
    Kind kind() const { return m_kind; }

    /// Returns the matching positions to send; only meaningful when kind() == Result.
    const QVector<VisionOutputPosition>& positions() const {
        return m_positions;
    }

    /// Returns the raw byte payload to send; only meaningful when kind() == Raw.
    const QByteArray& rawPayload() const {
        return m_rawPayload;
    }

    /// Builds the wire payload per spec. For Kind::Raw, returns rawPayload() unchanged;
    /// for Kind::Result, returns "{count},{x,y,z,r},{x,y,z,r},...;" with each axis using
    /// "%08.2f" (e.g.
    /// "2,00010.00,00020.00,00000.00,00090.00,00015.00,00025.00,00000.00,00000.00;").
    QByteArray buildPayload() const {
        if (m_kind == Raw) {
            return m_rawPayload;
        }

        QString payload = QString::number(m_positions.size());
        for (const VisionOutputPosition &pos : m_positions) {
            payload += QLatin1Char(',');
            payload += pos.toString();
        }
        payload += QLatin1Char(';');
        return payload.toUtf8();
    }

private:
    Kind m_kind{Result};                          ///< Which payload kind this request carries.
    QVector<VisionOutputPosition> m_positions;     ///< Matching positions (Kind::Result).
    QByteArray m_rawPayload;                       ///< Opaque bytes to send (Kind::Raw).
};

} // namespace vc::device

#endif // VISION_OUTPUT_REQUEST_H
