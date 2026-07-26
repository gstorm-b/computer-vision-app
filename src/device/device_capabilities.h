#ifndef DEVICE_CAPABILITIES_H
#define DEVICE_CAPABILITIES_H

#include <QStringList>

/// Device-capability mix-in interfaces: optional facets a concrete device (e.g. a PLC or
/// camera) can implement in addition to its base IDevice/IDeviceCfg contract, so callers can
/// query "does this device support X" via dynamic_cast instead of downcasting to a concrete type.
namespace vc::device {

/// Capability mix-in for devices that expose named digital (bit) I/O tags/lines.
class IDigitalIoProvider {
public:
    virtual ~IDigitalIoProvider() = default;

    /// Returns the names of all digital I/O tags/lines currently available on the device.
    virtual QStringList availableDigitalIoNames() const = 0;
};

/// Capability mix-in for devices that expose named word (register-sized) I/O tags.
class IWordIoProvider {
public:
    virtual ~IWordIoProvider() = default;

    /// Returns the names of all word I/O tags currently available on the device.
    virtual QStringList availableWordIoNames() const = 0;
};

/// Combined capability for PLC-like devices that expose both digital and word tags by name.
class IPlcTagProvider : public IDigitalIoProvider, public IWordIoProvider {
public:
    ~IPlcTagProvider() override = default;
};

/// Capability mix-in for devices that accept writes to named PLC I/O tags.
class IPlcIoWriter {
public:
    virtual ~IPlcIoWriter() = default;

    /// Writes `value` to the digital I/O tag identified by `tag`.
    /// @return true if the write succeeded.
    virtual bool writeDigitalIoByName(const QString &tag, bool value) = 0;

    /// Writes `value` to the word I/O tag identified by `tag`.
    /// @return true if the write succeeded.
    virtual bool writeWordIoByName(const QString &tag, qint16 value) = 0;
};

/// Marker capability for devices that can act as a source of images (e.g. cameras).
class IImageSourceDevice {
public:
    virtual ~IImageSourceDevice() = default;
};

/// Marker capability for devices that can output/consume processing results.
class IResultOutputDevice {
public:
    virtual ~IResultOutputDevice() = default;
};

} // namespace vc::device

#endif // DEVICE_CAPABILITIES_H
