# Opt-in parallel compilation (MSVC `/MP`).
#
# OFF by default, and deliberately so. `/MP` spawns one `cl.exe` per job and each carries
# its own working set; on a machine with less RAM than cores it starts swapping, which is
# slower than a serial build rather than faster. Whether it pays off is a property of the
# machine, not of the project, so it is a switch rather than a setting in the repository.
#
#   qmake ... CONFIG+=multicore                 use every logical processor
#   qmake ... CONFIG+=multicore NCR_JOBS=8      cap at 8 parallel compilations
#
# NCR_JOBS may also come from the environment, so a workstation can pick its own ceiling
# once instead of on every qmake line.
#
# Why this works here
# ---------------------------------------------------------------------------------------
# qmake's win32-msvc makefiles use MSVC batch rules: one `cl.exe` invocation receives many
# .cpp files (that is the "Compiling... / Generating Code..." pattern in the build log).
# `/MP` parallelises *within* such an invocation. A makefile that compiled one file per
# invocation would gain nothing from this flag, which is why it belongs here and not in a
# make-tool argument.
#
# Interaction with jom — read this before turning it on in Qt Creator
# ---------------------------------------------------------------------------------------
# `jom` (Qt Creator's default make tool for MSVC kits) already parallelises at the
# *makefile* level: it runs several compile jobs at once. Adding `/MP` on top multiplies
# the two, and the product can exceed the core count several times over — at which point
# the processes fight for cache and memory and the build gets slower.
#
#   nmake  : turn this on, leave NCR_JOBS unset.
#   jom    : leave it off, or set NCR_JOBS to a small number (2-4). jom is already doing
#            the parallelising.
#
# Compatibility: `/MP` is silently ignored alongside `/Gm` (removed in modern MSVC) and
# `/Yc` precompiled-header creation. This project uses neither — checked, not assumed.
#
# Included by qmake/common_deps.pri (the shared library and both shells) and by
# tests/architecture_contract_test/architecture_contract_test.pro, which are the four
# builds large enough for this to matter.

win32-msvc*:contains(CONFIG, multicore) {

    # qmake command line first, then the environment. Same precedence rule as the
    # dependency paths in qmake/local_paths.pri.
    isEmpty(NCR_JOBS): NCR_JOBS = $$(NCR_JOBS)

    isEmpty(NCR_JOBS) {
        QMAKE_CXXFLAGS += /MP
        QMAKE_CFLAGS   += /MP
        message("multicore: /MP enabled (all logical processors)")
    } else {
        QMAKE_CXXFLAGS += /MP$$NCR_JOBS
        QMAKE_CFLAGS   += /MP$$NCR_JOBS
        message("multicore: /MP$$NCR_JOBS enabled")
    }
}
