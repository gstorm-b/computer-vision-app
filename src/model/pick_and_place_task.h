/// @file
/// Disabled legacy header: the PickPlaceTask class, its header guard, and every declaration
/// below are commented out in their entirety (dead code, excluded from the build). Kept for
/// reference only — do not uncomment without reconciling it against the current ITask-derived
/// task model.
// #ifndef PICK_AND_PLACE_TASK_H
// #define PICK_AND_PLACE_TASK_H

// #include "model/itask.h"
// #include "model/camera_map_entry.h"

// #include <QJsonArray>

// namespace vc::model {

// class PickPlaceTask : public ITask {
//     Q_OBJECT

//     Q_PROPERTY(QString outputDeviceId
//                    READ outputDeviceId
//                        WRITE setOutputDeviceId
//                            NOTIFY outputDeviceIdChanged)

// public:
//     explicit PickPlaceTask(QObject* parent = nullptr)
//         : ITask(parent) {}

//     // ── PickPlace-specific getters ─────────────────────
//     QString                 outputDeviceId()       const { return m_outputDeviceId;       }
//     QString                 cameraSignalDeviceId() const { return m_cameraSignalDeviceId; }
//     QString                 cameraSignalType()     const { return m_cameraSignalType;     }
//     int                     cameraSignalAddress()  const { return m_cameraSignalAddress;  }
//     QVector<CameraMapEntry> cameraMap()            const { return m_cameraMap;            }

//     // ── PickPlace-specific setters ─────────────────────
//     void setOutputDeviceId(const QString& v) {
//         if (m_outputDeviceId == v) return;
//         m_outputDeviceId = v;
//         emit outputDeviceIdChanged();
//     }
//     void setCameraSignalDeviceId(const QString& v) { m_cameraSignalDeviceId = v; }
//     void setCameraSignalType(const QString& v)     { m_cameraSignalType     = v; }
//     void setCameraSignalAddress(int v)             { m_cameraSignalAddress  = v; }
//     void setCameraMap(const QVector<CameraMapEntry>& map) {
//         m_cameraMap = map;
//         emit cameraMapChanged();
//     }

//     // ── Camera map helpers ─────────────────────────────
//     QString cameraIdBySignal(int sv) const {
//         for (const auto& e : m_cameraMap)
//             if (e.signalValue() == sv)
//                 return e.cameraDeviceId();
//         return {};
//     }

//     // ── Task interface ─────────────────────────────────
//     TaskType taskType() const override {
//         return TaskType::PickPlaceTask;
//     }

//     bool isValid() const override {
//         if (id().isEmpty() || name().isEmpty())        return false;
//         if (isOwned()  && ownedCameraId().isEmpty())   return false;
//         if (isShared() && sourceTaskId().isEmpty())    return false;
//         if (m_outputDeviceId.isEmpty())                return false;
//         return true;
//     }

//     // ── Serialize: base + pick-specific fields ─────────
//     QJsonObject toJson() const override {
//         QJsonObject o = ITask::toJson();  // base fields

//         QJsonArray cmArr;
//         for (const auto& e : m_cameraMap)
//             cmArr.append(e.toJson());

//         // Merge pick-specific fields to base JSON
//         o["cameraSignalDeviceId"]  = m_cameraSignalDeviceId;
//         o["cameraSignalType"]      = m_cameraSignalType;
//         o["cameraSignalAddress"]   = m_cameraSignalAddress;
//         o["outputDeviceId"]        = m_outputDeviceId;
//         o["cameraMap"]             = cmArr;
//         return o;
//     }

//     static PickPlaceTask* fromJson(const QJsonObject& o,
//                                    QObject* parent = nullptr) {
//         auto* t = new PickPlaceTask(parent);
//         ITask::baseFromJson(o, t);  // parse common fields

//         // Parse pick-specific fields
//         t->m_cameraSignalDeviceId = o["cameraSignalDeviceId"]
//                                         .toString();
//         t->m_cameraSignalType     = o["cameraSignalType"]
//                                     .toString();
//         t->m_cameraSignalAddress  = o["cameraSignalAddress"]
//                                        .toInt();
//         t->m_outputDeviceId       = o["outputDeviceId"]
//                                   .toString();

//         for (const auto& v : o["cameraMap"].toArray())
//             t->m_cameraMap.append(
//                 CameraMapEntry::fromJson(v.toObject()));

//         return t;
//     }

// signals:
//     void outputDeviceIdChanged();
//     void cameraMapChanged();

// private:
//     QString                 m_outputDeviceId;
//     QString                 m_cameraSignalDeviceId;
//     QString                 m_cameraSignalType;
//     int                     m_cameraSignalAddress = 0;
//     QVector<CameraMapEntry> m_cameraMap;
// };

// } // namespace vc::model

// #endif // PICK_AND_PLACE_TASK_H
