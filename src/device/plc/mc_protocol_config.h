#ifndef MC_PROTOCOL_CONFIG_H
#define MC_PROTOCOL_CONFIG_H

#include "device/plc/plc_device.h"
#include "device/plc/mc_context.h"
#include "device/plc/mc_msg_interface.h"
#include "device/plc/mc_context_factory.h"

/// PLC device family (config, protocol devices, and MC-protocol support types).
namespace vc::device {

/// MC-protocol device configuration: holds a cloned McContext (frame type plus its
/// addressing/timing parameters) and adds MC-specific JSON persistence on top of PlcCfg.
class McProtocolConfig : public PlcCfg {
    Q_GADGET

public:
    /// Constructs an MC-protocol config with no context set yet.
    explicit McProtocolConfig()
        : PlcCfg() {}

    /// Returns the static QMetaObject for McProtocolConfig, so Q_GADGET-based reflection
    /// (e.g. the property browser) resolves the runtime type through a PlcCfg/IDeviceCfg pointer.
    const QMetaObject &getMetaObject() const override {
        return vc::device::McProtocolConfig::staticMetaObject;
    }

    /// Returns PlcType::MitsubishiMc, identifying this as the Mitsubishi MC PLC family config.
    PlcType plcType() const override {
        return PlcType::MitsubishiMc;
    }

    /// Returns the frame type of the current context, or McFrameType::Frame_User if no
    /// context has been set yet.
    McFrameType currentFrameType() const {
        if (!m_context) {
            return McFrameType::Frame_User;
        }
        return m_context->frameType();
    }

    /// Replaces the current context with a clone of `ctx`; this config does not take
    /// ownership of `ctx` itself.
    /// @param ctx source context to clone; must be non-null
    /// @return false if `ctx` is null (context left unchanged), true otherwise
    bool setContext(McContext *ctx) {
        if (!ctx) {
            return false;
        }
        m_context.reset(ctx->clone());
        return true;
    }

    /// Returns the current context, or nullptr if none has been set. Ownership stays with
    /// this config.
    McContext* context() {
        return m_context.get();
    }

    /// Builds a fresh context for `frame_type`/`code` via Factory::contextFactory() and
    /// installs it, discarding any previous context.
    /// @param frame_type MC frame variant to configure (e.g. Frame_3E)
    /// @param code data code (binary/ASCII) the frame should use
    /// @return false if the factory could not produce a context for the given type/code
    bool configMcProtocol(McFrameType frame_type, McDataCode code = McDataCode::Binary) {
        std::shared_ptr<McContext> temp = Factory::contextFactory(frame_type, code);
        if (!temp) {
            return false;
        }
        m_context = temp;
        return true;
    }

    /// Serializes the base PlcCfg fields plus the MC frame type and context (if any) to JSON.
    /// @return JSON object with DEVICE_JSK_MC_FRAME (empty string when no context) and,
    /// when a context is set, DEVICE_JSK_MC_CONTEXT holding the context's own JSON.
    QJsonObject toJson() const override {
        QJsonObject obj = PlcCfg::toJson();
        if (m_context) {
            obj[DEVICE_JSK_MC_FRAME] = McFrameTypeToString(m_context->frameType());
            obj[DEVICE_JSK_MC_CONTEXT] = m_context->toJson();
        } else {
            obj[DEVICE_JSK_MC_FRAME] = "";
        }
        return obj;
    }

    /// Restores base PlcCfg fields, then rebuilds the context from the JSON's MC frame type
    /// via Factory::contextFactory() and loads the context's own fields from
    /// DEVICE_JSK_MC_CONTEXT.
    /// @param obj JSON object previously produced by toJson()
    /// @return false if the base parse fails or the factory can't build a context for the
    /// stored frame type
    bool fromJson(const QJsonObject &obj) override {
        if (!PlcCfg::fromJson(obj)) return false;
        McFrameType frame_type = McFrameTypeFromString(obj[DEVICE_JSK_MC_FRAME].toString());
        m_context = Factory::contextFactory(frame_type);
        if (m_context) {
            m_context->fromJson(obj[DEVICE_JSK_MC_CONTEXT].toObject());
            return true;
        }
        return false;
    }

    /// Deep-copies this config, including a clone of the current context if one is set.
    /// @return heap-allocated copy; caller takes ownership
    IDeviceCfg* clone() override {
        McProtocolConfig* new_ptr = new McProtocolConfig(*this);
        if (this->m_context) {
            McContext* ctx = this->m_context.get();
            new_ptr->setContext(ctx);
        }
        return new_ptr;
    }

private:
    std::shared_ptr<McContext> m_context;  ///< Current frame type + addressing/timing context, or null if unset.
};

}

#endif // MC_PROTOCOL_CONFIG_H
