# Module: libwg (UI level)

**Purpose.** Tiny reusable widget primitives: `GroupFrame`,
`ValidatingLineEdit`.

**May include.** Everything except `app/` (same rule as the other UI
modules). In practice this module should stay close to Qt-only.

**Status.** Merge candidate: consolidating `libwg/` into `src/widgets/`
(or a future `src/ui/`) is backlog item C6 of the 2026-07 restructure.
Do not add new files here — put new reusable widgets in `src/widgets/`.

**Verify.** Root app build.

**Build registration.** `src/libwg/libwg.pri` only.
