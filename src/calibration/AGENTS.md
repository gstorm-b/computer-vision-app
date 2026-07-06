# Module: calibration (level 1)

**Purpose.** Camera-to-robot calibration: calibration boards (Fanuc iRVision
board implemented), board factory, and the homography/plane `Calibrator`
(OpenCV).

**Public surface.** `calibration_board.h`, `calibration_board_factory.h`,
`calibrator.h`, `fanuc_irvision_board.h`.

**May include.** Qt, OpenCV, `core/`, other `calibration/` headers.
**Must NOT include.** UI, `model/`, `runtime/`, `device/`, `matching/`.
Enforced by the architecture contract test. (Consumed by `device/camera`
— cameras own a `Calibrator` — and by `model`; never depend back on them.)

**Invariants.**
- `Calibrator` persists via YAML and JSON round-trips that preserve H and
  plane bit-exact (covered by tests).
- Custom calibration-board authoring is a deferred integration — do not add
  speculative board types without a concrete requirement.

**Verify.** `tests/calibration_test` (qmake + `nmake /nologo -f
Makefile.Debug compiler_moc_source_make_all` + `nmake /nologo`; run
`calibration_test.exe`), plus root app build.

**Build registration.** `src/calibration/calibration.pri` only.

**Docs.** `docs/domains/calibration/calibration_module.md`.
