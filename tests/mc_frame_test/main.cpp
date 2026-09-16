/**
 * @file main.cpp
 * @brief Hardware-free command coverage for the MC frame codecs: 3E (binary, Ethernet),
 *        1C and 3C (ASCII, computer link).
 *
 * What these tests prove, and what they do not
 * ---------------------------------------------------------------------------------------
 * Every expected byte string here is derived from the frame layout in the reference
 * implementations — `reference source/1C_frame/fx3communicator.cpp` for 1C and
 * `reference source/3C_frame/mc_frame_3c.py` for 3C — or, for 3E, from the codec that has
 * been running against the customer's PLC since before this phase.
 *
 * So they prove **"we build the frame the reference builds, and we read back what the
 * reference would have written"**. They do NOT prove the PLC accepts it: no byte here was
 * captured from real hardware. That gap is what `tools/mc_protocol_bench` (task A8) closes,
 * and until it has run against the real C24 port, 1C and 3C are unverified on the wire.
 *
 * The 3E cases are a REGRESSION BASELINE, not an independent correctness proof: they were
 * written from the shipped implementation, so they can only catch a change to it. That is
 * still the most useful thing they can be — 3E is the one frame with field evidence behind
 * it, so it is also the control case proving this test harness itself drives the codecs
 * correctly.
 */

#include <QtTest>
#include <QByteArray>

#include <QBuffer>
#include <QSignalSpy>
#include <QTimer>

#include <memory>

#include "device/plc/mc_context_1c.h"
#include "device/plc/mc_context_3c.h"
#include "device/plc/mc_context_3e.h"
#include "device/plc/mc_device_map.h"
#include "device/plc/mc_device_map_diff.h"
#include "device/plc/mc_fame_3e.h"
#include "device/plc/mc_frame_1c.h"
#include "device/plc/mc_frame_3c.h"
#include "device/plc/mc_protocol_config.h"
#include "device/plc/mc_protocol_device.h"
#include "device/plc/mc_request.h"

using namespace vc::device;
using RtCode = MCFrameAbstract::FrameReturnCode;

namespace {

constexpr char kEnq = 0x05;
constexpr char kStx = 0x02;
constexpr char kEtx = 0x03;
constexpr char kAck = 0x06;
constexpr char kNak = 0x15;
constexpr char kCr  = 0x0D;
constexpr char kLf  = 0x0A;

/// Independent sum-check implementation for the tests: the low byte of the character sum,
/// as two upper-case hexadecimal characters.
///
/// Deliberately NOT the production helper. Asserting a frame against the same code that built
/// it proves only that the code is consistent with itself.
QByteArray testSum(const QByteArray &body) {
    int sum = 0;
    for (char c : body) {
        sum += static_cast<unsigned char>(c);
    }
    return QByteArray::number(sum & 0xFF, 16).toUpper().rightJustified(2, '0');
}

/// Renders a frame as spaced hex for a readable QCOMPARE failure message.
QByteArray hex(const QByteArray &data) {
    return data.toHex(' ');
}

// ── Phase 9 / E1 harness: driving McProtocolDevice without a PLC ─────────────────────────
//
// The device builds its own transport inside initialize_mc_device() and there is no other
// injection point, so E1 added ONE seam — a virtual createMsgInterface() — and this is what
// uses it. Everything else here is the shipped code: the real 3E codec, the real queue, the
// real polling state machine. A stub codec would prove nothing about completions raised on the
// frames the product actually sends.

/// Stub transport. Records what the device sends, hands back whatever response the test staged,
/// and can be told to fail sends or to answer nothing at all (the timeout path).
class FakeMcPort : public McMsgInterface {
public:
    FakeMcPort() {
        m_buffer.open(QIODevice::ReadWrite);
        m_port_state = MsgIfState::NoConnection;
        m_error_state = MsgErrorState::NoError;
    }

    bool SetConfig(McMsgItfConfig *) override { return true; }
    const bool ConnectionCheck() override { return m_port_state == MsgIfState::Connected; }
    MsgIfState ConnectToPort() override {
        m_port_state = connectSucceeds ? MsgIfState::Connected : MsgIfState::ConnectFail;
        return m_port_state;
    }
    MsgIfState DisconnectFromPort() override {
        m_port_state = MsgIfState::NoConnection;
        return m_port_state;
    }

    const MsgErrorState SendMsg(QByteArray &buffer) override {
        sentFrames.append(buffer);
        if (failSends) {
            m_error_state = MsgErrorState::ErrorOcurred;
            return m_error_state;
        }
        m_error_state = MsgErrorState::NoError;

        // Per-request when responseQueue has entries, falling back to the single canned frame.
        // The queue is what lets a test tell an M read's answer from a D read's — with one frame
        // replayed for everything, a case asserting "the snapshot carries the D values" would
        // pass whether or not the D response had been parsed, which is the whole question.
        const QByteArray reply = responseQueue.isEmpty() ? stagedResponse
                                                         : responseQueue.takeFirst();

        // Deferred, because a real socket never answers inside the send call. Answering
        // synchronously would re-enter response_handle() before request_handle() has set
        // m_wait_for_response, and the reply would be dropped.
        if (!reply.isEmpty()) {
            // The context object is load-bearing, not decoration. Without it the lambda outlives
            // this port — deviceDisconnect() destroys the transport, the queued call fires
            // afterwards, and it writes into a destroyed QBuffer. That crashed the suite once
            // E2/E3 changed the timing enough to lose the race; it was latent from E1.
            // m_buffer is a member QObject, so binding to it cancels the call on destruction.
            //
            // `reply` is captured BY VALUE: it belongs to the request that was just sent, not to
            // whatever the queue holds when the timer fires.
            QTimer::singleShot(0, &m_buffer, [this, reply]() {
                m_pending = reply;
                m_buffer.write(QByteArray(1, '\0'));   // makes ioDevice() emit readyRead
            });
        }
        return m_error_state;
    }

    const MsgErrorState ReceiveMsg(QByteArray &buffer, int = 5) override {
        if (m_pending.isEmpty()) {
            m_error_state = MsgErrorState::BufferEmpty;
            return m_error_state;
        }
        buffer.append(m_pending);
        m_pending.clear();
        m_error_state = MsgErrorState::NoError;
        return m_error_state;
    }

    void clearBuffer() override { m_pending.clear(); }
    const McMsgItfType type() const override { return McMsgItfType::EthernetTCPIP; }
    QIODevice *ioDevice() const override { return const_cast<QBuffer *>(&m_buffer); }
    void DestroyMsgPort() override { m_port_state = MsgIfState::NotInit; }

    QList<QByteArray> sentFrames;    ///< Every frame the device handed to SendMsg().
    QList<QByteArray> responseQueue; ///< Replies delivered in send order; falls back to stagedResponse.
    QByteArray stagedResponse;       ///< Reply delivered after the next send; empty = never answer.
    bool failSends{false};           ///< Make SendMsg() report ErrorOcurred.
    bool connectSucceeds{true};      ///< Make ConnectToPort() fail.

private:
    mutable QBuffer m_buffer;
    QByteArray m_pending;
};

/// McProtocolDevice with its transport replaced. Nothing else is overridden.
class TestableMcDevice : public McProtocolDevice {
public:
    TestableMcDevice() : McProtocolDevice(QStringLiteral("mc1"), QStringLiteral("MC")) {}

    FakeMcPort *port{nullptr};   ///< Valid between deviceConnect() and the next teardown.

protected:
    std::unique_ptr<McMsgInterface> createMsgInterface(mc::McMsgItfType) override {
        auto fake = std::make_unique<FakeMcPort>();
        fake->connectSucceeds = connectSucceeds;
        port = fake.get();
        return fake;
    }

public:
    bool connectSucceeds{true};
};

/// Builds a device configured for 3E over TCP.
///
/// The timer is the ONLY thing that dispatches anything — ad-hoc writes and the round-robin reads
/// share it — so it cannot be parked to keep polling quiet without also parking the write under
/// test. Instead the default subscribes **no** M/D ranges: the polling queue is then empty,
/// polling_query() returns immediately, the comm-active toggle (queued only when a full round
/// completes) never fires, and the only traffic is the write the case pushed. Pass non-zero
/// amounts for the one case that needs real polling.
void configureFor3E(TestableMcDevice *device, int refreshMs = 20,
                    int mAmount = 0, int dAmount = 0) {
    Context_Mc3E ctx;
    ctx.setRefreshInterval(refreshMs);
    ctx.setStartMAddress(2000);
    ctx.setAmountMAddress(mAmount);
    ctx.setStartDAddress(2000);
    ctx.setAmountDAddress(dAmount);
    ctx.setActiveMDevice(2000);
    ctx.msgConfig()->m_responseTimeout = 50;

    McProtocolConfig cfg;
    cfg.setContext(&ctx);
    device->setMcProtocolConfig(cfg);
}

/// A 3E success reply: sub-header, route, length 2, end code 0000. What the PLC sends for an
/// accepted write.
QByteArray mc3eWriteAck() {
    return QByteArray::fromHex("d00000ffff03000200") + QByteArray::fromHex("0000");
}

/// A 3E reply carrying a non-zero end code — the PLC refusing the write.
QByteArray mc3eWriteNak() {
    return QByteArray::fromHex("d00000ffff03000200") + QByteArray::fromHex("5540");
}

/// Wraps `payload` in a successful 3E response header.
///
/// Layout, from Frame3E's own parsers: 7 header bytes, then a little-endian length at index 7
/// covering the 2-byte end code plus the payload, then the end code, then the payload at index 11.
QByteArray mc3eReadReply(const QByteArray &payload) {
    const quint16 length = static_cast<quint16>(2 + payload.size());
    QByteArray frame = QByteArray::fromHex("d00000ffff0300");
    frame.append(static_cast<char>(length & 0xFF));
    frame.append(static_cast<char>((length >> 8) & 0xFF));
    frame.append(QByteArray::fromHex("0000"));   // end code: success
    frame.append(payload);
    return frame;
}

/// A 3E word-read reply carrying `words` in little-endian order.
QByteArray mc3eWordReadReply(const QList<qint16> &words) {
    QByteArray payload;
    for (const qint16 word : words) {
        payload.append(static_cast<char>(word & 0xFF));
        payload.append(static_cast<char>((word >> 8) & 0xFF));
    }
    return mc3eReadReply(payload);
}

/// A 3E reply for a >8-bit M read, which Frame3E sends word-aligned and unpacks LSB-first, one
/// byte per 8 bits.
QByteArray mc3eBitReadReply(const QByteArray &packedBits) {
    return mc3eReadReply(packedBits);
}

} // namespace

/**
 * @class McFrameTest
 * @brief Byte-level build and parse coverage for every MC command in each supported frame.
 */
class McFrameTest : public QObject {
    Q_OBJECT

private:
    /// Builds a 1C context with fixed, non-default addressing so a field that silently falls
    /// back to its default shows up as a byte difference rather than passing by luck.
    Context_Mc1C make1c(bool sumCheck = false,
                        mc::McFrameFormat format = mc::McFrameFormat::Format_4) {
        Context_Mc1C ctx;
        ctx.m_stationNumber = 0;
        ctx.m_plcNumber = 0xFF;
        ctx.m_messageWaitTime = 0;
        ctx.m_useSumCheck = sumCheck;
        ctx.m_frameFormat = format;
        return ctx;
    }

    /// Builds a 3C context matching the reference's default access route (00/00/FF/00).
    Context_Mc3C make3c(mc::McPlcSeries series = mc::McPlcSeries::PlcSeries_Q,
                        bool sumCheck = false,
                        mc::McFrameFormat format = mc::McFrameFormat::Format_4) {
        Context_Mc3C ctx;
        ctx.m_plcSeries = series;
        ctx.m_stationNumber = 0;
        ctx.m_networkNumber = 0;
        ctx.m_pcNumber = 0xFF;
        ctx.m_selfStationNumber = 0;
        ctx.m_useSumCheck = sumCheck;
        ctx.m_frameFormat = format;
        return ctx;
    }

private slots:

    // ── 1C: build ─────────────────────────────────────────────────────────────

    /// ENQ station(2) plc(2) "BR" wait(1) device address(4 dec) count(2 hex) CR LF
    void test_1c_build_read_bit()
    {
        Context_Mc1C ctx = make1c();
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 8);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("00").append("FF").append("BR").append('0')
                .append("M0100").append("08").append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// Same envelope, "WR" command.
    void test_1c_build_read_word()
    {
        Context_Mc1C ctx = make1c();
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 16);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("00").append("FF").append("WR").append('0')
                .append("D2000").append("10").append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// Bit values travel as one ASCII '0'/'1' per device, and the count comes from the value
    /// list rather than from whatever m_amount held.
    void test_1c_build_write_bit()
    {
        Context_Mc1C ctx = make1c();
        Frame1C frame;
        MCRequest request(MCRequest::RqType::WriteBit, 'M', 20, 1);
        QList<quint8> values{1, 0, 1};
        request.buildWriteData_Bit_Device(values);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);
        QCOMPARE(request.m_amount, 3);

        QByteArray expected;
        expected.append(kEnq).append("00").append("FF").append("BW").append('0')
                .append("M0020").append("03").append("101").append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// Word values travel as four upper-case hex characters each, including negatives, which
    /// must appear as their two's-complement pattern.
    void test_1c_build_write_word()
    {
        Context_Mc1C ctx = make1c();
        Frame1C frame;
        MCRequest request(MCRequest::RqType::WriteWord, 'D', 30, 1);
        QList<qint16> values{static_cast<qint16>(0x1234), static_cast<qint16>(-1)};
        request.buildWriteData_Word_Device_Word(values);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("00").append("FF").append("WW").append('0')
                .append("D0030").append("02").append("1234").append("FFFF")
                .append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// Format 1 is format 4 without the CR LF terminator — nothing else differs.
    void test_1c_build_format1_has_no_terminator()
    {
        Context_Mc1C ctx = make1c(false, mc::McFrameFormat::Format_1);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 8);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("00").append("FF").append("BR").append('0')
                .append("M0100").append("08");
        QCOMPARE(hex(out), hex(expected));
    }

    /// With the sum check enabled the frame carries the low byte of the character sum of
    /// everything after ENQ, as two hex characters, before the terminator.
    void test_1c_build_sum_check()
    {
        Context_Mc1C ctx = make1c(true);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 8);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        const QByteArray body = QByteArray("00FFBR0M010008");
        QByteArray expected;
        expected.append(kEnq).append(body).append(testSum(body)).append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// A station or PLC number the count field cannot carry is refused before it reaches the
    /// wire, rather than being truncated into a frame addressed somewhere else.
    void test_1c_build_rejects_out_of_range_count()
    {
        Context_Mc1C ctx = make1c();
        Frame1C frame;
        QByteArray out;

        MCRequest tooMany(MCRequest::RqType::ReadBit, 'M', 100, 8);
        tooMany.m_amount = 256;
        QCOMPARE(frame.makeSendFrame(&tooMany, &ctx, out), RtCode::RequestFrameError);

        MCRequest none(MCRequest::RqType::WriteBit, 'M', 100, 8);
        QCOMPARE(frame.makeSendFrame(&none, &ctx, out), RtCode::RequestFrameError);
    }

    // ── 1C: parse ─────────────────────────────────────────────────────────────

    /// STX station plc data ETX CR LF, one ASCII character per bit.
    void test_1c_parse_read_bit()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kStx).append("00").append("FF").append("1011")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_m[100], quint8(1));
        QCOMPARE(map.device_map_m[101], quint8(0));
        QCOMPARE(map.device_map_m[102], quint8(1));
        QCOMPARE(map.device_map_m[103], quint8(1));
    }

    /// Four hex characters per word, and a value above 0x7FFF must land signed.
    void test_1c_parse_read_word()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 3);

        QByteArray response;
        response.append(kStx).append("00").append("FF").append("0064").append("FFFF")
                .append("8000").append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_d[2000], qint16(100));
        QCOMPARE(map.device_map_d[2001], qint16(-1));
        QCOMPARE(map.device_map_d[2002], qint16(-32768));
    }

    /// A write is answered with a bare ACK.
    void test_1c_parse_write_ack()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::WriteWord, 'D', 30, 1);

        QByteArray response;
        response.append(kAck).append("00").append("FF").append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
    }

    /// A NAK is an error, and its code is decoded into the message rather than dropped.
    void test_1c_parse_nak_reports_the_code()
    {
        Context_Mc1C ctx = make1c();
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kNak).append("00").append("FF").append("06").append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseError);
        QVERIFY(frame.lastErrorDescription().contains(QStringLiteral("character area error")));
    }

    /// An incomplete frame must not be parsed, at any of the points it can be cut: the codec
    /// has to keep waiting instead of treating a partial payload as a short read.
    void test_1c_partial_frame_waits()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray full;
        full.append(kStx).append("00").append("FF").append("1011")
            .append(kEtx).append(kCr).append(kLf);

        for (int cut = 1; cut < full.size(); ++cut) {
            QByteArray partial = full.left(cut);
            const RtCode code = frame.parseReceiveFrame(&request, &ctx, partial);
            QVERIFY2(code == RtCode::WaitingReceive,
                     qPrintable(QStringLiteral("cut at %1 returned %2, expected WaitingReceive")
                                    .arg(cut).arg(int(code))));
        }

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, full), RtCode::ResponseOk);
    }

    /// A corrupt sum check must invalidate the response. Accepting it would write the corrupt
    /// payload into the device map, which is the failure the sum check exists to prevent.
    void test_1c_bad_sum_check_is_invalid()
    {
        Context_Mc1C ctx = make1c(true);
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        const QByteArray body = QByteArray("00FF1011") + QByteArray(1, kEtx);

        QByteArray good;
        good.append(kStx).append(body).append(testSum(body)).append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, good), RtCode::ResponseOk);

        QByteArray bad;
        bad.append(kStx).append(body).append("00").append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, bad), RtCode::ResponseInvalid);
        QVERIFY(frame.lastErrorDescription().contains(QStringLiteral("sum check")));
    }

    /// A response carrying another station's numbers is not our answer; taking its payload
    /// would write a different PLC's values into this device's map.
    void test_1c_wrong_station_is_invalid()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kStx).append("07").append("FF").append("1011")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseInvalid);
        QVERIFY(map.device_map_m.empty());
    }

    /// Leading rubbish before the control code — the tail of an abandoned response after a
    /// retry — must be skipped, not parsed as a frame header.
    void test_1c_skips_leading_garbage()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kCr).append(kLf).append("ZZ")
                .append(kStx).append("00").append("FF").append("1011")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_m[100], quint8(1));
    }

    /// A short payload is a malformed response, not a partial one: ETX has already arrived, so
    /// no further bytes can complete it.
    void test_1c_short_payload_is_invalid()
    {
        Context_Mc1C ctx = make1c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 3);

        QByteArray response;
        response.append(kStx).append("00").append("FF").append("0064")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseInvalid);
    }

    /// Null arguments and a foreign context are refused without dereferencing anything.
    void test_1c_rejects_null_and_foreign_context()
    {
        Context_Mc1C ctx = make1c();
        Context_Mc3E ctx3e;
        Frame1C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);
        QByteArray buffer;

        QCOMPARE(frame.makeSendFrame(nullptr, &ctx, buffer), RtCode::ObjectError);
        QCOMPARE(frame.makeSendFrame(&request, nullptr, buffer), RtCode::ObjectError);
        QCOMPARE(frame.parseReceiveFrame(nullptr, &ctx, buffer), RtCode::ObjectError);
        QCOMPARE(frame.parseReceiveFrame(&request, nullptr, buffer), RtCode::ObjectError);
        QCOMPARE(frame.makeSendFrame(&request, &ctx3e, buffer), RtCode::ObjectError);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx3e, buffer), RtCode::ObjectError);
    }

    // ── 3C: build ─────────────────────────────────────────────────────────────

    /// ENQ "F9" route(8) cmd(4) subcmd(4) device address(6 dec) count(4 hex) CR LF
    void test_3c_build_read_bit()
    {
        Context_Mc3C ctx = make3c();
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 8);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("F9").append("0000FF00")
                .append("0401").append("0001").append("M*").append("000100").append("0008")
                .append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// More than 8 bit points travel as a word read of ceil(amount / 16) words, with the word
    /// sub-command — the same redirection Frame3E performs.
    void test_3c_build_read_bit_redirects_to_word()
    {
        Context_Mc3C ctx = make3c();
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 20);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("F9").append("0000FF00")
                .append("0401").append("0000").append("M*").append("000100").append("0002")
                .append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// The iQ-R series widens the device header to four characters and the address to eight
    /// digits, and uses its own sub-command.
    void test_3c_build_read_word_iqr_widths()
    {
        Context_Mc3C ctx = make3c(mc::McPlcSeries::PlcSeries_iQR);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 4);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expected;
        expected.append(kEnq).append("F9").append("0000FF00")
                .append("0401").append("0002").append("D***").append("00002000").append("0004")
                .append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    /// Write commands use 1401 and carry their values after the count.
    void test_3c_build_write_bit_and_word()
    {
        Context_Mc3C ctx = make3c();
        Frame3C frame;

        MCRequest bits(MCRequest::RqType::WriteBit, 'M', 20, 1);
        QList<quint8> bitValues{1, 0, 1};
        bits.buildWriteData_Bit_Device(bitValues);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&bits, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expectedBits;
        expectedBits.append(kEnq).append("F9").append("0000FF00")
                    .append("1401").append("0001").append("M*").append("000020").append("0003")
                    .append("101").append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expectedBits));

        MCRequest words(MCRequest::RqType::WriteWord, 'D', 30, 1);
        QList<qint16> wordValues{static_cast<qint16>(0x1234), static_cast<qint16>(-1)};
        words.buildWriteData_Word_Device_Word(wordValues);

        QCOMPARE(frame.makeSendFrame(&words, &ctx, out), RtCode::RequestFrameOK);

        QByteArray expectedWords;
        expectedWords.append(kEnq).append("F9").append("0000FF00")
                     .append("1401").append("0000").append("D*").append("000030").append("0002")
                     .append("1234").append("FFFF").append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expectedWords));
    }

    /// A device letter the frame cannot address is refused at build time.
    void test_3c_build_rejects_unknown_device()
    {
        Context_Mc3C ctx = make3c();
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'Z', 100, 8);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameError);
    }

    /// Sum check covers everything after the control code, as for 1C.
    void test_3c_build_sum_check()
    {
        Context_Mc3C ctx = make3c(mc::McPlcSeries::PlcSeries_Q, true);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 8);

        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&request, &ctx, out), RtCode::RequestFrameOK);

        const QByteArray body = QByteArray("F90000FF0004010001M*0001000008");
        QByteArray expected;
        expected.append(kEnq).append(body).append(testSum(body)).append(kCr).append(kLf);
        QCOMPARE(hex(out), hex(expected));
    }

    // ── 3C: parse ─────────────────────────────────────────────────────────────

    /// STX "F9" route data ETX CR LF, one ASCII character per bit.
    void test_3c_parse_read_bit()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kStx).append("F9").append("0000FF00").append("1011")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_m[100], quint8(1));
        QCOMPARE(map.device_map_m[101], quint8(0));
        QCOMPARE(map.device_map_m[103], quint8(1));
    }

    /// A bit read that was redirected to a word read comes back word-packed: four hex
    /// characters per 16 points, unpacked least-significant bit first.
    void test_3c_parse_read_bit_from_word()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 20);

        // 0x8001 -> bits 0 and 15 set; 0x000A -> bits 1 and 3 set.
        QByteArray response;
        response.append(kStx).append("F9").append("0000FF00").append("8001").append("000A")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_m[100], quint8(1));   // bit 0
        QCOMPARE(map.device_map_m[101], quint8(0));
        QCOMPARE(map.device_map_m[115], quint8(1));   // bit 15
        QCOMPARE(map.device_map_m[117], quint8(1));   // second word, bit 1
        QCOMPARE(map.device_map_m[119], quint8(1));   // second word, bit 3

        // Exactly `amount` devices are written, not the full 32 the two words carry.
        QCOMPARE(map.device_map_m.count(119), size_t(1));
        QCOMPARE(map.device_map_m.count(120), size_t(0));
    }

    /// Four hex characters per word, signed on the way into the map.
    void test_3c_parse_read_word()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 2);

        QByteArray response;
        response.append(kStx).append("F9").append("0000FF00").append("0064").append("FFFF")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_d[2000], qint16(100));
        QCOMPARE(map.device_map_d[2001], qint16(-1));
    }

    /// Both acknowledgement shapes a 3C write can come back as are success.
    void test_3c_parse_write_response()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::WriteWord, 'D', 30, 1);

        QByteArray ack;
        ack.append(kAck).append("F9").append("0000FF00").append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, ack), RtCode::ResponseOk);

        QByteArray empty;
        empty.append(kStx).append("F9").append("0000FF00").append(kEtx).append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, empty), RtCode::ResponseOk);
    }

    /// NAK carries a four-character end code and is an error, not data.
    void test_3c_parse_nak()
    {
        Context_Mc3C ctx = make3c();
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kNak).append("F9").append("0000FF00").append("C051")
                .append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseError);
        QVERIFY(frame.lastErrorDescription().contains(QStringLiteral("c051")));
    }

    /// A response addressed to another station must be rejected before its payload is used.
    void test_3c_wrong_access_route_is_invalid()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kStx).append("F9").append("0100FF00").append("1011")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseInvalid);
        QVERIFY(map.device_map_m.empty());
    }

    /// A frame ID that is not F9 is not a 3C response.
    void test_3c_wrong_frame_id_is_invalid()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray response;
        response.append(kStx).append("F8").append("0000FF00").append("1011")
                .append(kEtx).append(kCr).append(kLf);

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseInvalid);
    }

    /// Every truncation point must keep waiting rather than parse a partial frame.
    void test_3c_partial_frame_waits()
    {
        Context_Mc3C ctx = make3c();
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        QByteArray full;
        full.append(kStx).append("F9").append("0000FF00").append("1011")
            .append(kEtx).append(kCr).append(kLf);

        for (int cut = 1; cut < full.size(); ++cut) {
            QByteArray partial = full.left(cut);
            const RtCode code = frame.parseReceiveFrame(&request, &ctx, partial);
            QVERIFY2(code == RtCode::WaitingReceive,
                     qPrintable(QStringLiteral("cut at %1 returned %2, expected WaitingReceive")
                                    .arg(cut).arg(int(code))));
        }

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, full), RtCode::ResponseOk);
    }

    /// A corrupt sum check invalidates the response.
    void test_3c_bad_sum_check_is_invalid()
    {
        Context_Mc3C ctx = make3c(mc::McPlcSeries::PlcSeries_Q, true);
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);

        const QByteArray body = QByteArray("F90000FF001011") + QByteArray(1, kEtx);

        QByteArray good;
        good.append(kStx).append(body).append(testSum(body)).append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, good), RtCode::ResponseOk);

        QByteArray bad;
        bad.append(kStx).append(body).append("00").append(kCr).append(kLf);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, bad), RtCode::ResponseInvalid);
    }

    /// Null arguments and a foreign context are refused without dereferencing anything.
    void test_3c_rejects_null_and_foreign_context()
    {
        Context_Mc3C ctx = make3c();
        Context_Mc1C ctx1c;
        Frame3C frame;
        MCRequest request(MCRequest::RqType::ReadBit, 'M', 100, 4);
        QByteArray buffer;

        QCOMPARE(frame.makeSendFrame(nullptr, &ctx, buffer), RtCode::ObjectError);
        QCOMPARE(frame.makeSendFrame(&request, nullptr, buffer), RtCode::ObjectError);
        QCOMPARE(frame.parseReceiveFrame(nullptr, &ctx, buffer), RtCode::ObjectError);
        QCOMPARE(frame.parseReceiveFrame(&request, nullptr, buffer), RtCode::ObjectError);
        QCOMPARE(frame.makeSendFrame(&request, &ctx1c, buffer), RtCode::ObjectError);
        QCOMPARE(frame.parseReceiveFrame(&request, &ctx1c, buffer), RtCode::ObjectError);
    }

    // ── 3E: regression baseline ───────────────────────────────────────────────

    /// The shipped 3E read frames, pinned byte for byte. This is the frame with field
    /// evidence behind it, so it is also the control case proving the harness drives a codec
    /// the way McProtocolDevice does.
    void test_3e_build_read_baseline()
    {
        Context_Mc3E ctx;
        Frame3E frame;

        MCRequest bits(MCRequest::RqType::ReadBit, 'M', 100, 8);
        QByteArray out;
        QCOMPARE(frame.makeSendFrame(&bits, &ctx, out), RtCode::RequestFrameOK);
        QCOMPARE(hex(out),
                 QByteArray("50 00 00 ff ff 03 00 0c 00 04 00 01 04 01 00 64 00 00 90 08 00"));

        MCRequest words(MCRequest::RqType::ReadWord, 'D', 2000, 4);
        QCOMPARE(frame.makeSendFrame(&words, &ctx, out), RtCode::RequestFrameOK);
        QCOMPARE(hex(out),
                 QByteArray("50 00 00 ff ff 03 00 0c 00 04 00 01 04 00 00 d0 07 00 a8 04 00"));
    }

    /// A 3E word read, parsed back into the device map, including a negative value.
    void test_3e_parse_read_word_baseline()
    {
        Context_Mc3E ctx;
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3E frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 2);

        // response header: sub-header D000, route, length (2 + 4), end code 0000, then the
        // two little-endian words 100 and -1.
        QByteArray response = QByteArray::fromHex("d00000ffff030006000000") +
                              QByteArray::fromHex("6400ffff");

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseOk);
        QCOMPARE(map.device_map_d[2000], qint16(100));
        QCOMPARE(map.device_map_d[2001], qint16(-1));
    }

    /// A non-zero 3E end code is an error, and the code reaches the message.
    void test_3e_error_end_code_baseline()
    {
        Context_Mc3E ctx;
        McDeviceMap map;
        ctx.setDeviceMap(&map);
        Frame3E frame;
        MCRequest request(MCRequest::RqType::ReadWord, 'D', 2000, 2);

        QByteArray response = QByteArray::fromHex("d00000ffff03000600") +
                              QByteArray::fromHex("5bc0");

        QCOMPARE(frame.parseReceiveFrame(&request, &ctx, response), RtCode::ResponseError);
        QVERIFY(frame.lastErrorDescription().contains(QStringLiteral("c05b")));
    }

    // ── Phase 9 / A1: the polled-value shadow diff (backlog 59.3b + 59.3) ─────
    //
    // Nothing in this repository has ever executed a McProtocolDevice method: the device builds
    // its own transport with no injection point, and this .pro compiles four src/ files that do
    // not include mc_protocol_device.cpp. The diff was moved into a header-only helper so the two
    // defects below could be pinned at all. The transport state machine remains untested — a
    // recorded coverage gap, not coverage.

    /// **The defect (59.3b).** A station configured with M ranges and no D ranges is an ordinary
    /// configuration. The device returned before emitting `valueChanged` whenever either D map was
    /// empty, so on such a station every `bExecuteTrigger` was accumulated and then discarded —
    /// while `deviceMChanged` still fired, leaving the device widget looking perfectly alive and
    /// the task sitting in ReadyForTrigger forever with nothing logged.
    void test_m_changes_survive_a_station_with_no_d_ranges()
    {
        const std::map<int, quint8> liveM{{10, 1}, {11, 0}};
        std::map<int, quint8> shadowM{{10, 0}, {11, 0}};
        const std::map<int, qint16> liveD;   // exactly what update_d_map() leaves for no D ranges
        std::map<int, qint16> shadowD;

        const McDeviceMapDiff diff =
            diffMcDeviceMaps(liveM, shadowM, liveD, shadowD, /*suppressReporting=*/false);

        QCOMPARE(diff.changedValues.size(), 1);
        QCOMPARE(diff.changedValues.value(QStringLiteral("M0010")).toInt(), 1);
        QCOMPARE(diff.mChanges.size(), 1);
        QCOMPARE(diff.mChanges.at(0).address, 10);
        QCOMPARE(shadowM.at(10), quint8(1));
    }

    /// The D area must keep working when the M area is the empty one — the same independence,
    /// checked from the other side so a fix cannot simply swap which area is privileged.
    void test_d_changes_survive_a_station_with_no_m_ranges()
    {
        const std::map<int, quint8> liveM;
        std::map<int, quint8> shadowM;
        const std::map<int, qint16> liveD{{2000, 7}};
        std::map<int, qint16> shadowD{{2000, 3}};

        const McDeviceMapDiff diff =
            diffMcDeviceMaps(liveM, shadowM, liveD, shadowD, /*suppressReporting=*/false);

        QCOMPARE(diff.changedValues.value(QStringLiteral("D2000")).toInt(), 7);
        QCOMPARE(diff.dChanges.size(), 1);
        QCOMPARE(diff.dChanges.at(0).previousValue, 3);
    }

    /// **The second defect (59.3).** The device walked both maps with a lockstep iterator pair,
    /// assuming identical key sets. The shadow is only ever added to, never erased, so once the
    /// polled range shrinks the two walk out of step and every address is compared against a
    /// neighbour's value. Here the shadow carries an address the live map no longer has.
    void test_a_shadow_larger_than_the_live_map_still_compares_by_address()
    {
        // M10's shadow value is deliberately EQUAL to M20's live value. Under lockstep, live M20
        // is compared against shadow M10, they match, and the change vanishes. Under keyed lookup
        // it is compared against shadow M20 and is reported. Without this asymmetry the test
        // passes either way and pins nothing.
        const std::map<int, quint8> liveM{{20, 1}};
        std::map<int, quint8> shadowM{{10, 1}, {20, 0}};  // M10 lingers from a wider range
        const std::map<int, qint16> liveD{{1, 1}};
        std::map<int, qint16> shadowD{{1, 1}};

        const McDeviceMapDiff diff =
            diffMcDeviceMaps(liveM, shadowM, liveD, shadowD, /*suppressReporting=*/false);

        // Lockstep would have compared live M20 against shadow M10 and reported address 20 with
        // M10's value as its previous — or, with the sizes differing, resynced and reported nothing.
        QCOMPARE(diff.mChanges.size(), 1);
        QCOMPARE(diff.mChanges.at(0).address, 20);
        QCOMPARE(diff.mChanges.at(0).previousValue, 0);
        QCOMPARE(diff.mChanges.at(0).currentValue, 1);
        QCOMPARE(diff.changedValues.value(QStringLiteral("M0020")).toInt(), 1);
    }

    /// An address the shadow has never seen is reported as a value and seeded, but carries no
    /// previous value — so the caller must not emit a per-address changed signal for it. Reporting
    /// the value matters: a newly visible tag the consumer never learns about is the same class of
    /// defect as item 58.
    void test_an_address_new_to_the_shadow_is_reported_without_a_previous_value()
    {
        const std::map<int, quint8> liveM{{30, 1}};
        std::map<int, quint8> shadowM;                  // never polled this address before
        const std::map<int, qint16> liveD{{1, 0}};
        std::map<int, qint16> shadowD{{1, 0}};

        const McDeviceMapDiff diff =
            diffMcDeviceMaps(liveM, shadowM, liveD, shadowD, /*suppressReporting=*/false);

        QCOMPARE(diff.mChanges.size(), 1);
        QCOMPARE(diff.mChanges.at(0).hasPrevious, false);
        QCOMPARE(diff.changedValues.value(QStringLiteral("M0030")).toInt(), 1);
        QCOMPARE(shadowM.at(30), quint8(1));
    }

    /// First-polling-round suppression is preserved exactly: nothing is reported, but the shadows
    /// are still brought up to date, so the round after it reports only genuine changes. This is
    /// deliberately NOT "fixed" here — it is the mechanism the held-trigger item depends on.
    void test_the_first_polling_round_updates_shadows_without_reporting()
    {
        const std::map<int, quint8> liveM{{10, 1}};
        std::map<int, quint8> shadowM{{10, 0}};
        const std::map<int, qint16> liveD{{2000, 5}};
        std::map<int, qint16> shadowD{{2000, 0}};

        const McDeviceMapDiff first =
            diffMcDeviceMaps(liveM, shadowM, liveD, shadowD, /*suppressReporting=*/true);

        QVERIFY(first.changedValues.isEmpty());
        QVERIFY(first.mChanges.isEmpty());
        QVERIFY(first.dChanges.isEmpty());
        QCOMPARE(shadowM.at(10), quint8(1));
        QCOMPARE(shadowD.at(2000), qint16(5));

        // Same values again: nothing changed, so nothing is reported even unsuppressed.
        const McDeviceMapDiff second =
            diffMcDeviceMaps(liveM, shadowM, liveD, shadowD, /*suppressReporting=*/false);
        QVERIFY(second.changedValues.isEmpty());
    }

    /// Tags keep the four-digit zero-padded form the signal mapper binds against. A change here
    /// silently unbinds every commissioned signal map.
    // ── Phase 9 / E1: MC write completion ────────────────────────────────────────────────
    //
    // pushRequest() returning true has only ever meant "queued". A caller could not tell a write
    // that reached the PLC from one dropped by a disconnect, which is why D3's retry policy was
    // unimplementable: there was nothing to retry ON. pushTrackedRequest() resolves exactly once,
    // always — a request that vanishes is worse than one that fails, because the caller waits
    // forever.

    void test_a_tracked_write_that_is_acked_resolves_once_as_ok()
    {
        TestableMcDevice device;
        configureFor3E(&device);
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(finished.isValid());
        QVERIFY(device.deviceConnect());
        device.port->stagedResponse = mc3eWriteAck();

        MCRequest write(MCRequest::WriteBit, 'M', 2001, 1);
        write.buildWriteData_Bit_Device(quint8(0x01));
        const quint64 id = device.pushTrackedRequest(&write);
        QVERIFY2(id != 0, "a valid MC request must be accepted and correlated");

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        const McResult result = qvariant_cast<McResult>(finished.at(0).at(0));
        QCOMPARE(result.correlationId, id);
        QCOMPARE(result.isOk, true);
        QCOMPARE(result.startAddress, 2001);
        QCOMPARE(result.register_type, QStringLiteral("M"));

        // Exactly once: nothing later re-resolves it.
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);

        device.deviceDisconnect();
    }

    void test_a_tracked_write_the_plc_refuses_resolves_once_as_failed_with_the_end_code()
    {
        TestableMcDevice device;
        configureFor3E(&device);
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(device.deviceConnect());
        device.port->stagedResponse = mc3eWriteNak();

        MCRequest write(MCRequest::WriteWord, 'D', 2003, 1);
        write.buildWriteData_Word_Device_Word(qint16(7));
        const quint64 id = device.pushTrackedRequest(&write);
        QVERIFY(id != 0);

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        const McResult result = qvariant_cast<McResult>(finished.at(0).at(0));
        QCOMPARE(result.correlationId, id);
        QCOMPARE(result.isOk, false);
        // The frame's mapped description, not a generic "failed": on a NAK it carries the PLC's
        // own end code, which is the only thing that says why the write was refused.
        QVERIFY2(!result.msg.isEmpty(), "a failure must say why");

        device.deviceDisconnect();
    }

    /// The heartbeat toggle and the round-robin reads share the same queue and the same state
    /// machine as a tracked write. If they were correlated too, every polling round would raise
    /// completion traffic nobody asked for.
    void test_polling_and_the_comm_active_heartbeat_produce_no_completions()
    {
        TestableMcDevice device;
        configureFor3E(&device, /*refreshMs=*/10, /*mAmount=*/8, /*dAmount=*/8);
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(device.deviceConnect());
        device.port->stagedResponse = mc3eWriteAck();

        // Several polling rounds, each of which also queues the comm-active write.
        QTest::qWait(300);
        QVERIFY2(!device.port->sentFrames.isEmpty(), "polling must actually have run");
        QCOMPARE(finished.count(), 0);

        device.deviceDisconnect();
    }

    /// pushRequest() keeps its old meaning — queued, uncorrelated, silent. Existing callers are
    /// unchanged by E1, which is the whole reason the tracked entry point is separate.
    void test_the_untracked_push_still_reports_nothing()
    {
        TestableMcDevice device;
        configureFor3E(&device);
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(device.deviceConnect());
        device.port->stagedResponse = mc3eWriteAck();

        MCRequest write(MCRequest::WriteBit, 'M', 2002, 1);
        write.buildWriteData_Bit_Device(quint8(0x01));
        QVERIFY(device.pushRequest(&write));

        QTest::qWait(300);
        QVERIFY2(!device.port->sentFrames.isEmpty(), "the write must still be sent");
        QCOMPARE(finished.count(), 0);

        device.deviceDisconnect();
    }

    // ── E1(b): every abandon path resolves too ───────────────────────────────────────────

    /// A write queued and then abandoned by a disconnect must fail, not vanish. This is the case
    /// the whole task exists for: "true" from the old pushRequest() was indistinguishable here.
    void test_a_write_abandoned_by_disconnect_resolves_once_as_failed()
    {
        TestableMcDevice device;
        configureFor3E(&device);   // polling parked, so the write stays queued
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(device.deviceConnect());
        device.port->stagedResponse = mc3eWriteAck();

        MCRequest write(MCRequest::WriteBit, 'M', 2004, 1);
        write.buildWriteData_Bit_Device(quint8(0x01));
        const quint64 id = device.pushTrackedRequest(&write);
        QVERIFY(id != 0);

        device.deviceDisconnect();

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        const McResult result = qvariant_cast<McResult>(finished.at(0).at(0));
        QCOMPARE(result.correlationId, id);
        QCOMPARE(result.isOk, false);
        QVERIFY2(result.msg.contains(QStringLiteral("disconnected")),
                 qPrintable(QStringLiteral("the reason must name the disconnect; got: %1")
                                .arg(result.msg)));
    }

    /// setDeviceLostConnect() must fail EVERY outstanding request, not only the one in flight.
    void test_a_lost_link_fails_every_queued_write_not_just_the_one_in_flight()
    {
        TestableMcDevice device;
        configureFor3E(&device);
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(device.deviceConnect());
        // No staged response and sends fail, so the first dispatch takes the send-failure path
        // straight to the retry budget.
        device.port->failSends = true;

        QVector<quint64> ids;
        for (int address = 2010; address < 2013; ++address) {
            MCRequest write(MCRequest::WriteBit, 'M', address, 1);
            write.buildWriteData_Bit_Device(quint8(0x01));
            const quint64 id = device.pushTrackedRequest(&write);
            QVERIFY(id != 0);
            ids.append(id);
        }

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), ids.size(), 5000);

        QSet<quint64> resolved;
        for (const QList<QVariant> &emission : finished) {
            const McResult result = qvariant_cast<McResult>(emission.at(0));
            QCOMPARE(result.isOk, false);
            QVERIFY2(!resolved.contains(result.correlationId), "no id may resolve twice");
            resolved.insert(result.correlationId);
        }
        for (const quint64 id : ids) {
            QVERIFY2(resolved.contains(id), "every submitted write must resolve");
        }
    }

    /// The counting form of the contract, which is what the acceptance criterion asks for:
    /// resolutions == submissions, with no id appearing twice, across a run that mixes an
    /// acked write, tracked and untracked pushes, and a teardown.
    void test_resolutions_match_submissions_exactly()
    {
        TestableMcDevice device;
        configureFor3E(&device);
        QSignalSpy finished(&device, &McProtocolDevice::requestFinished);
        QVERIFY(device.deviceConnect());
        device.port->stagedResponse = mc3eWriteAck();

        QVector<quint64> submitted;
        for (int i = 0; i < 4; ++i) {
            MCRequest tracked(MCRequest::WriteBit, 'M', 2020 + i, 1);
            tracked.buildWriteData_Bit_Device(quint8(0x01));
            submitted.append(device.pushTrackedRequest(&tracked));

            MCRequest untracked(MCRequest::WriteBit, 'M', 2030 + i, 1);
            untracked.buildWriteData_Bit_Device(quint8(0x01));
            QVERIFY(device.pushRequest(&untracked));
        }
        QTest::qWait(200);
        device.deviceDisconnect();
        QTest::qWait(100);

        QSet<quint64> resolved;
        for (const QList<QVariant> &emission : finished) {
            const McResult result = qvariant_cast<McResult>(emission.at(0));
            QVERIFY2(result.correlationId != 0, "an uncorrelated request must never report");
            QVERIFY2(!resolved.contains(result.correlationId), "no id may resolve twice");
            resolved.insert(result.correlationId);
        }
        QCOMPARE(resolved.size(), submitted.size());
        for (const quint64 id : submitted) {
            QVERIFY2(resolved.contains(id), "every tracked write must resolve exactly once");
        }
    }

    // ── Phase 9 / E5: the snapshot must mean "what was just read" ────────────────────────
    //
    // pollingUpdate() used to be emitted at the moment the round's LAST request was selected —
    // before request_handle() sent it, let alone parsed its answer. The first snapshot therefore
    // carried the zero-fill update_d_map() had written, and setup() read nActiveCamera as 0 on a
    // project whose camera 1 was bound and registered. Owner-reported on an MC cell 2026-09-09.

    /// (a) A D-only station: one polling request per round, so the round's only request IS the
    /// last one. The first snapshot must carry what that read returned, not the zero-fill.
    void test_the_first_mc_snapshot_carries_the_values_that_were_read()
    {
        TestableMcDevice device;
        configureFor3E(&device, /*refreshMs=*/20, /*mAmount=*/0, /*dAmount=*/8);

        QList<std::shared_ptr<PlcValueMap>> snapshots;
        QObject::connect(&device, &McProtocolDevice::pollingUpdate,
                         [&snapshots](std::shared_ptr<PlcValueMap> map) {
                             snapshots.append(map);
                         });

        QVERIFY(device.deviceConnect());
        // D2000 = 7 is the value the PLC actually holds; 0 is what the map is pre-filled with, so
        // the two are distinguishable.
        device.port->responseQueue.append(mc3eWordReadReply({7, 0, 0, 0, 0, 0, 0, 0}));

        QTRY_VERIFY_WITH_TIMEOUT(!snapshots.isEmpty(), 3000);

        QVariant value;
        QVERIFY2(snapshots.first()->valueForTag(QStringLiteral("D2000"), &value),
                 "the snapshot must carry the subscribed D range");
        QCOMPARE(value.toInt(), 7);

        device.deviceDisconnect();
    }

    /// (a) An M-only station — the configuration where the round's last request is an M range,
    /// and the one bExecuteTrigger lives on. update_d_map() adds no ranges here.
    void test_an_m_only_station_also_publishes_a_complete_first_snapshot()
    {
        TestableMcDevice device;
        configureFor3E(&device, /*refreshMs=*/20, /*mAmount=*/16, /*dAmount=*/0);

        QList<std::shared_ptr<PlcValueMap>> snapshots;
        QObject::connect(&device, &McProtocolDevice::pollingUpdate,
                         [&snapshots](std::shared_ptr<PlcValueMap> map) {
                             snapshots.append(map);
                         });

        QVERIFY(device.deviceConnect());
        // 16 bits, unpacked LSB-first one byte at a time: bit 0 set makes M2000 true.
        device.port->responseQueue.append(mc3eBitReadReply(QByteArray::fromHex("0100")));

        QTRY_VERIFY_WITH_TIMEOUT(!snapshots.isEmpty(), 3000);

        QVariant value;
        QVERIFY2(snapshots.first()->valueForTag(QStringLiteral("M2000"), &value),
                 "the snapshot must carry the subscribed M range");
        QCOMPARE(value.toBool(), true);

        device.deviceDisconnect();
    }

    /// (b) The first round reports NO changes, for every range including the last.
    ///
    /// The shadow maps are zero-filled, so without the suppression every non-zero power-up value
    /// arrives as a change 0 -> value and reaches handlePlcValues()' edge detection — a PLC
    /// holding bExecuteTrigger high at startup would run a cycle nobody asked for. The runtime is
    /// meant to learn the initial state from the snapshot above, not from the change stream.
    void test_the_first_polling_round_reports_no_changes_but_the_second_does()
    {
        TestableMcDevice device;
        configureFor3E(&device, /*refreshMs=*/20, /*mAmount=*/0, /*dAmount=*/8);

        int snapshotCount = 0;
        int reportedAtFirstSnapshot = -1;
        QList<QMap<QString, QVariant>> reported;
        // Sampled INSIDE the first snapshot rather than by a QTRY afterwards. At a 20 ms refresh
        // the rounds keep coming, so "wait until snapshotCount == 1" is a moving target that has
        // already passed — the first cut of this case failed on snapshotCount == 460. The
        // invariant belongs to the instant the first round ends, so it is captured there.
        QObject::connect(&device, &McProtocolDevice::pollingUpdate,
                         [&](std::shared_ptr<PlcValueMap>) {
                             if (snapshotCount == 0) {
                                 reportedAtFirstSnapshot = reported.size();
                             }
                             ++snapshotCount;
                         });
        QObject::connect(&device, &McProtocolDevice::valueChanged,
                         [&reported](QMap<QString, QVariant> values) { reported.append(values); });

        QVERIFY(device.deviceConnect());
        // Round 1 answers 7; the comm-active write queued at the wrap is acked; round 2 answers 9.
        device.port->responseQueue.append(mc3eWordReadReply({7, 0, 0, 0, 0, 0, 0, 0}));
        device.port->responseQueue.append(mc3eWriteAck());
        device.port->responseQueue.append(mc3eWordReadReply({9, 0, 0, 0, 0, 0, 0, 0}));
        device.port->stagedResponse = mc3eWriteAck();

        QTRY_VERIFY_WITH_TIMEOUT(snapshotCount >= 1, 3000);
        QCOMPARE(reportedAtFirstSnapshot, 0);   // "no changes at all by the end of round 1"

        // And the suppression is for the first round only: round 2's difference is reported.
        QTRY_VERIFY_WITH_TIMEOUT(!reported.isEmpty(), 4000);
        bool sawTheChange = false;
        for (const QMap<QString, QVariant> &values : reported) {
            if (values.value(QStringLiteral("D2000")).toInt() == 9) {
                sawTheChange = true;
            }
        }
        QVERIFY2(sawTheChange, "the second round must report D2000 changing to 9");

        device.deviceDisconnect();
    }

    void test_device_tags_keep_their_zero_padded_form()
    {
        QCOMPARE(mcDeviceTag(QLatin1Char('M'), 10), QStringLiteral("M0010"));
        QCOMPARE(mcDeviceTag(QLatin1Char('D'), 2000), QStringLiteral("D2000"));
        QCOMPARE(mcDeviceTag(QLatin1Char('M'), 0), QStringLiteral("M0000"));
    }
};

QTEST_MAIN(McFrameTest)
#include "main.moc"
