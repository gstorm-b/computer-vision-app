#ifndef VISION_OUTPUT_REQUEST_H
#define VISION_OUTPUT_REQUEST_H

#include "device/irequest.h"

#include <QString>
#include <QVector>
#include <memory>

namespace vc::device {

/**
 * @brief Một vị trí matching gồm 4 trục x, y, z, r.
 */
struct VisionOutputPosition {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double r{0.0};

    QString toString() const {
        // Wire contract: each axis is fixed-width, zero-padded to 8 chars with
        // 2 decimals ("%08.2f"), e.g. 1.0 -> "00001.00". The downstream robot
        // parser relies on this exact field format.
        return QString("%1,%2,%3,%4")
            .arg(QString::asprintf("%08.2f", x),
                 QString::asprintf("%08.2f", y),
                 QString::asprintf("%08.2f", z),
                 QString::asprintf("%08.2f", r));
    }
};

/**
 * @brief Request gửi tới VisionOutputDevice. Có 2 loại:
 *      - Result: Server đẩy kết quả matching xuống client port 1.
 *      - Raw: gửi raw bytes thẳng xuống port 1 (dùng cho test / mở rộng).
 *
 * Format kết quả: "{detected number},{Position 1},{Position 2},...;"
 * với mỗi position là "x,y,z,r".
 */
class VisionOutputRequest : public IRequest {
public:
    enum Kind {
        Result,
        Raw
    };

    VisionOutputRequest() = default;

    explicit VisionOutputRequest(QVector<VisionOutputPosition> positions)
        : m_kind(Result), m_positions(std::move(positions)) {}

    explicit VisionOutputRequest(QByteArray rawPayload)
        : m_kind(Raw), m_rawPayload(std::move(rawPayload)) {}

    RequestType type() const override {
        return RequestType::Request_VisionOutput;
    }

    std::shared_ptr<IRequest> clone() const override {
        return std::make_shared<VisionOutputRequest>(*this);
    }

    Kind kind() const { return m_kind; }

    const QVector<VisionOutputPosition>& positions() const {
        return m_positions;
    }

    const QByteArray& rawPayload() const {
        return m_rawPayload;
    }

    /**
     * @brief Tạo payload đúng định dạng theo spec. Mỗi trục dùng "%08.2f".
     *        Ví dụ: "2,00010.00,00020.00,00000.00,00090.00,00015.00,00025.00,00000.00,00000.00;"
     */
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
    Kind m_kind{Result};
    QVector<VisionOutputPosition> m_positions;
    QByteArray m_rawPayload;
};

} // namespace vc::device

#endif // VISION_OUTPUT_REQUEST_H
