#pragma once

#include <QObject>

/// QtTest suite covering MeshCollisionProfileJsonLoader and MeshCollisionProfileValidator:
/// exercises JSON parsing of mesh-collision profiles (valid and malformed) and validation of
/// a parsed profile against a robot config.
class MeshCollisionProfileJsonTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies loadJson() fails with InvalidRobotConfig for an unrecognized top-level field
    /// and for a mesh entry missing the required scaleToMeters value.
    void loaderRejectsUnknownTopLevelFieldsAndMissingScale();
    /// Verifies loadJson() on a well-formed profile correctly parses backendPreference order,
    /// mesh fields (link id, scale, quality mode), and free-form metadata.
    void loaderParsesBackendPreferenceAndMetadata();
    /// Verifies loadFile() resolves a mesh's relative "path" against the directory containing
    /// the profile JSON file, producing an absolute, cleaned path.
    void loaderResolvesRelativeMeshPathsFromProfileFile();
    /// Verifies loadJson() rejects meshes whose transform components, scaleToMeters, or
    /// margin_m are non-numeric (e.g. strings), each with InvalidRobotConfig.
    void loaderRejectsNonNumericMeshTransformsAndOptions();
    /// Verifies MeshCollisionProfileValidator::validate() reports InvalidRobotConfig with
    /// multiple issues when a mesh references an unknown link and has inconsistent
    /// "simplified" quality metadata (missing simplifiedFrom, negative error bound).
    void validatorRejectsInvalidLinkAndBrokenSimplifiedMetadata();
    /// Verifies loadJson() rejects a profile with duplicate mesh ids and a disabledPairs
    /// entry referencing a mesh id that does not exist.
    void loaderRejectsDuplicateMeshIdsAndInvalidDisabledPairs();
};
