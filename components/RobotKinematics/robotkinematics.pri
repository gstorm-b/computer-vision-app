# ---------------------------------------------------------------------------
# robotkinematics.pri
#
# Integration include for the RobotKinematics component. Include this from a
# host .pro (e.g. ncr_picking.pro) to compile the library straight into the
# application:
#
#     include(components/RobotKinematics/robotkinematics.pri)
#
# Public headers live under include/RobotKinematics; sources under src/. The
# math dependency is the header-only Eigen relocated to the repository-root
# 3rdparty tree (3rdparty/eigen). The accurate mesh-collision path is wired to
# the optional Coal backend, whose prebuilt install trees (coal, boost, assimp)
# also live under the repository-root 3rdparty tree.
#
# Runtime note: the Coal backend is a shared library. coal.dll (plus the DLLs
# it depends on) must be reachable on PATH next to the application binary, and
# the Nachi MZ04D mesh-collision assets under
# components/RobotKinematics/presets/Nachi/MZ04 must be deployed for the
# in-app collision check to load them. See docs/backlog/later_todo_list.md.
# ---------------------------------------------------------------------------

ROBOTKINEMATICS_DIR = $$PWD
ROBOTKINEMATICS_3RDPARTY = $$PWD/../../3rdparty

INCLUDEPATH += \
    $$ROBOTKINEMATICS_DIR/include \
    $$ROBOTKINEMATICS_3RDPARTY/eigen

# --- Optional accurate mesh-collision backend (Coal) -----------------------
# Enable the Coal mesh-collision backend. The component's own backend .pri adds
# the include paths, the ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND define, and the
# coal import library. Roots point at the repository-root 3rdparty install
# trees rather than the component's local third_party/install layout.
CONFIG += robotkinematics_mesh_collision
MESH_COLLISION_BACKEND = coal
COAL_ROOT   = $$ROBOTKINEMATICS_3RDPARTY/coal
BOOST_ROOT  = $$ROBOTKINEMATICS_3RDPARTY/boost
ASSIMP_ROOT = $$ROBOTKINEMATICS_3RDPARTY/assimp

include($$ROBOTKINEMATICS_DIR/mesh_collision_backend.pri)

# --- Auto-copy mesh-collision runtime DLLs + mesh assets next to the app binary -------
# After the application links, copy the mesh backend's runtime artefacts into
# the target directory so the app runs straight from the build folder without a
# manual PATH/copy step:
#   - DLLs: coal.dll, assimp-vc143-mt.dll, and Coal's Boost runtime
#     dependencies. Per-file "if not exist ... copy".
#   - Mesh assets: the Nachi MZ04D mesh-collision JSON + STL meshes, copied to
#     <target>/robot_assets/Nachi/MZ04 (the runtime resolver looks there first).
#     Recursive (xcopy /E) so both the original-resolution meshes (device runtime
#     check) and the simplified/ voxel meshes (widget tester check) are deployed.
#     xcopy /D copies only missing/newer files.
# Both steps are ON by default. A host that must not run them - a static library
# has no binary to sit next to - opts out with
#
#     CONFIG += robotkinematics_no_copy_dlls robotkinematics_no_copy_assets
#
# *before* this include. Opting out with CONFIG -= robotkinematics_copy_dlls does
# NOT work: the line below would add the flag straight back.
!contains(CONFIG, robotkinematics_no_copy_dlls):   CONFIG += robotkinematics_copy_dlls
!contains(CONFIG, robotkinematics_no_copy_assets): CONFIG += robotkinematics_copy_assets
win32:contains(CONFIG, robotkinematics_mesh_collision) {
    # Target dir = directory of the produced binary. The app uses a
    # debug_and_release layout (<build>/debug, <build>/release); honour an
    # explicit DESTDIR when the host sets one.
    CONFIG(debug, debug|release): ROBOTKINEMATICS_DLL_DEST = $$OUT_PWD/debug
    else:                         ROBOTKINEMATICS_DLL_DEST = $$OUT_PWD/release
    !isEmpty(DESTDIR):            ROBOTKINEMATICS_DLL_DEST = $$DESTDIR

    contains(CONFIG, robotkinematics_copy_dlls) {
        ROBOTKINEMATICS_RUNTIME_DLLS = \
            $$ROBOTKINEMATICS_3RDPARTY/coal/bin/coal.dll \
            $$ROBOTKINEMATICS_3RDPARTY/assimp/bin/assimp-vc143-mt.dll \
            $$ROBOTKINEMATICS_3RDPARTY/boost/lib/boost_serialization-vc143-mt-x64-1_87.dll \
            $$ROBOTKINEMATICS_3RDPARTY/boost/lib/boost_filesystem-vc143-mt-x64-1_87.dll

        for(dll, ROBOTKINEMATICS_RUNTIME_DLLS) {
            exists($$dll) {
                ROBOTKINEMATICS_DLL_TARGET = $$ROBOTKINEMATICS_DLL_DEST/$$basename(dll)
                QMAKE_POST_LINK += if not exist $$shell_quote($$shell_path($$ROBOTKINEMATICS_DLL_TARGET)) \
                    copy /Y $$shell_quote($$shell_path($$dll)) $$shell_quote($$shell_path($$ROBOTKINEMATICS_DLL_TARGET)) \
                    $$escape_expand(\\n\\t)
            }
        }
    }

    contains(CONFIG, robotkinematics_copy_assets) {
        ROBOTKINEMATICS_PRESET_SRC  = $$ROBOTKINEMATICS_DIR/presets/Nachi/MZ04
        ROBOTKINEMATICS_PRESET_DEST = $$ROBOTKINEMATICS_DLL_DEST/robot_assets/Nachi/MZ04
        exists($$ROBOTKINEMATICS_PRESET_SRC) {
            QMAKE_POST_LINK += xcopy /D /E /I /Y /Q \
                $$shell_quote($$shell_path($$ROBOTKINEMATICS_PRESET_SRC)) \
                $$shell_quote($$shell_path($$ROBOTKINEMATICS_PRESET_DEST)) \
                $$escape_expand(\\n\\t)
        }
    }
}

HEADERS += \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Adapters/DhAdapter.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Adapters/UrdfAdapter.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Core/Ids.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Core/JointVector.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Core/Pose.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Core/Result.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Core/Units.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/CollisionBackend.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/CollisionChecker.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/CollisionGeometry.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/CollisionProfile.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/BuiltInCollisionProfiles.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/CollisionProfileJsonLoader.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/CollisionProfileValidator.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/MeshCollisionProfile.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/MeshCollisionProfileValidator.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/StlMeshLoader.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/StlPrimitiveAuthoringHelper.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Collision/TriangleMesh.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Kinematics/ForwardKinematics.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Kinematics/InverseKinematics.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Kinematics/JointLimitValidator.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Kinematics/RobotKinematics.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Kinematics/SerialRobotKinematics.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Model/FrameRegistry.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Model/RobotModelConfig.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Model/RobotModelValidator.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Model/SerialRobotConfigBuilder.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Model/ToolRegistry.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Posture/ArmPosture.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Posture/PostureResolver.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Presets/PresetJsonLoader.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Presets/Virtual6DofTestArm.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Presets/NachiMZ04D.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Solvers/IKSolutionRanker.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Solvers/IKSolver.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Solvers/NumericalIKSolver.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Solvers/AnalyticIKSolver.h \
    $$ROBOTKINEMATICS_DIR/include/RobotKinematics/Solvers/Analytic6DofSphericalWristSolver.h

SOURCES += \
    $$ROBOTKINEMATICS_DIR/src/Adapters/DhAdapter.cpp \
    $$ROBOTKINEMATICS_DIR/src/Adapters/UrdfAdapter.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/CollisionBackend.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/BuiltInCollisionProfiles.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/CollisionChecker.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/CollisionProfileJsonLoader.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/CollisionProfileValidator.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/MeshCollisionProfile.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/MeshCollisionProfileJsonLoader.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/MeshCollisionProfileValidator.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/StlMeshLoader.cpp \
    $$ROBOTKINEMATICS_DIR/src/Collision/StlPrimitiveAuthoringHelper.cpp \
    $$ROBOTKINEMATICS_DIR/src/Core/LibraryAnchor.cpp \
    $$ROBOTKINEMATICS_DIR/src/Core/Pose.cpp \
    $$ROBOTKINEMATICS_DIR/src/Core/Units.cpp \
    $$ROBOTKINEMATICS_DIR/src/Kinematics/ForwardKinematics.cpp \
    $$ROBOTKINEMATICS_DIR/src/Kinematics/JointLimitValidator.cpp \
    $$ROBOTKINEMATICS_DIR/src/Kinematics/SerialRobotKinematics.cpp \
    $$ROBOTKINEMATICS_DIR/src/Model/SerialRobotConfigBuilder.cpp \
    $$ROBOTKINEMATICS_DIR/src/Model/RobotModelValidator.cpp \
    $$ROBOTKINEMATICS_DIR/src/Posture/PostureResolver.cpp \
    $$ROBOTKINEMATICS_DIR/src/Presets/PresetJsonLoader.cpp \
    $$ROBOTKINEMATICS_DIR/src/Presets/Virtual6DofTestArm.cpp \
    $$ROBOTKINEMATICS_DIR/src/Presets/NachiMZ04D.cpp \
    $$ROBOTKINEMATICS_DIR/src/Solvers/IKSolutionRanker.cpp \
    $$ROBOTKINEMATICS_DIR/src/Solvers/NumericalIKSolver.cpp \
    $$ROBOTKINEMATICS_DIR/src/Solvers/Analytic6DofSphericalWristSolver.cpp

contains(CONFIG, robotkinematics_mesh_collision):equals(MESH_COLLISION_BACKEND, coal) {
    HEADERS += $$ROBOTKINEMATICS_DIR/src/Collision/CoalMeshCollisionBackend.h
    SOURCES += $$ROBOTKINEMATICS_DIR/src/Collision/CoalMeshCollisionBackend.cpp
}
