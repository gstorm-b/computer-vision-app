#ifndef PROJECT_REPOSITORY_H
#define PROJECT_REPOSITORY_H

#include "model/project.h"
#include <QString>

/// Project file schema/software version string written into a new project's project_info
/// table by ProjectRepository::createNew.
#define SOFTWARE_VERSION    "0.01"

namespace vc::model {

/// Static repository that persists a Project to/from a SQLite project file: creates the
/// schema for new projects, and upserts/reads project metadata, the serialized task JSON,
/// and per-task image BLOBs on save/load.
class ProjectRepository {
public:
    /// Creates a new project SQLite file at `path`: opens/creates the database, builds the
    /// schema, and seeds project_info with `projectName`, SOFTWARE_VERSION, and `str_time`
    /// as both the created_at and updated_at values.
    /// @param path filesystem path of the SQLite database file to create
    /// @param projectName initial project name stored in project_info
    /// @param str_time timestamp string stored as both created_at and updated_at
    /// @return true on success; on failure call lastMsg() for the SQL error text
    static bool createNew(const QString& path,
                          const QString& projectName,
                          const QString& str_time);
    /// Loads a project from the SQLite file at `path` into `out`: reads the project_info
    /// key/value rows, parses the project_data JSON blob via Project::fromJson, and injects
    /// per-task image BLOBs.
    /// @param path filesystem path of the SQLite database file to read
    /// @param out project instance populated with the loaded data
    /// @return true on success; false if the file can't be opened, project_data is empty, or
    ///         image loading fails (see lastMsg())
    static bool load(const QString& path, Project& out);
    /// Saves `project` to the SQLite file at `path` inside a single transaction: upserts
    /// project_info, serializes the project to JSON into project_data, and writes per-task
    /// image BLOBs; rolls back the transaction on any failure.
    /// @param path filesystem path of the SQLite database file to write
    /// @param project project instance to persist
    /// @return true if the transaction committed; false if it was rolled back (see lastMsg())
    static bool save(const QString& path, const Project& project);
    /// Returns the last SQL/database error message recorded by a create/load/save call.
    static QString lastMsg();

private:
    /// Executes the kSchema DDL statements (split on ';') on `connName` to create the
    /// project_info, project_data, and project_images tables if they don't already exist.
    /// @param connName name of the already-open QSqlDatabase connection to run the statements on
    /// @return true if every statement executed successfully; false on the first failure
    static bool createSchema(const QString& connName);
    /// Replaces the project_images table contents with the current in-memory image map (via
    /// ITask::getTaskImageMap) of every task in `tasks`, serializing each cv::Mat as a
    /// BMP-encoded BLOB.
    /// @param connName name of the already-open QSqlDatabase connection to write to
    /// @param tasks tasks whose images are persisted
    /// @return true on success; false on the first SQL failure (see m_lastMsg)
    static bool saveImages(const QString& connName,
                           const QVector<ITask*>& tasks);
    /// Reads the project_images rows for each task's id in `tasks` and injects the decoded
    /// images back into the task via ITask::loadTaskImageMap.
    /// @param connName name of the already-open QSqlDatabase connection to read from
    /// @param tasks tasks to populate with their loaded image maps (modified in place)
    /// @return true on success; false on the first SQL failure (see m_lastMsg)
    static bool loadImages(const QString& connName,
                           QVector<ITask*>& tasks);
    static QString m_lastMsg;  ///< Last error/status message set by a create/load/save call; returned by lastMsg().
};

} // namespace vc::model

#endif // PROJECT_REPOSITORY_H
