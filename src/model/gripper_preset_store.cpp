#include "model/gripper_preset_store.h"

namespace vc::model {

int GripperPresetStore::indexOf(const QString &name) const {
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets.at(i).name == name)
            return i;
    }
    return -1;
}

bool GripperPresetStore::contains(const QString &name) const {
    return indexOf(name) >= 0;
}

const GripperPreset* GripperPresetStore::find(const QString &name) const {
    const int idx = indexOf(name);
    return idx >= 0 ? &m_presets.at(idx) : nullptr;
}

bool GripperPresetStore::add(const GripperPreset &preset) {
    if (preset.name.trimmed().isEmpty() || contains(preset.name))
        return false;
    m_presets.append(preset);
    return true;
}

bool GripperPresetStore::setBoxes(const QString &name, const mtc::GripperBoxes &boxes) {
    const int idx = indexOf(name);
    if (idx < 0)
        return false;
    m_presets[idx].boxes = boxes;
    return true;
}

bool GripperPresetStore::rename(const QString &from, const QString &to) {
    const int idx = indexOf(from);
    if (idx < 0)
        return false;
    if (from == to)
        return true;   // no-op rename is not an error
    if (to.trimmed().isEmpty() || contains(to))
        return false;
    m_presets[idx].name = to;
    return true;
}

bool GripperPresetStore::remove(const QString &name) {
    const int idx = indexOf(name);
    if (idx < 0)
        return false;
    m_presets.removeAt(idx);
    return true;
}

QJsonArray GripperPresetStore::toJson() const {
    QJsonArray arr;
    for (const GripperPreset &p : m_presets) {
        arr.append(QJsonObject{
            { "name",     p.name },
            { "w",        p.boxes.size.width },
            { "h",        p.boxes.size.height },
            { "distance", p.boxes.distance },
        });
    }
    return arr;
}

void GripperPresetStore::fromJson(const QJsonArray &arr) {
    m_presets.clear();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();

        GripperPreset p;
        p.name = o["name"].toString();
        p.boxes.size = cv::Size2f(static_cast<float>(o["w"].toDouble(0.0)),
                                  static_cast<float>(o["h"].toDouble(0.0)));
        // An "angle" key written by an earlier build is deliberately ignored: the jaw
        // angle is per-pattern now, so a preset must not carry one back in.
        p.boxes.distance = o["distance"].toDouble(0.0);

        // add() enforces the non-blank/unique invariant; a rejected entry is dropped so a
        // hand-edited file degrades to a shorter list instead of failing the task load.
        add(p);
    }
}

} // namespace vc::model
