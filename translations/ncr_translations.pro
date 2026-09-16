# The one project that owns the translation file.
#
# Its only job is to give lupdate a project whose source set is the whole product, so
# `lupdate` — from Qt Creator's *Update Translations* or from
# scripts/update_translations.ps1 — sees every tr() string there is.
#
# ── It must never be built, and TEMPLATE alone cannot express that ────────────────────
#
# This project lists every source in src/ plus both shells. Building it would be a third
# compilation of the entire product.
#
# TEMPLATE = aux does NOT prevent that — measured, not assumed: qmake still emits OBJECTS
# and a link target for it. The one template that builds nothing, `subdirs`, is also the one
# lupdate collects no sources from (27 strings vs 0 in the same probe). There is no template
# that is both.
#
# So the exclusion lives in the umbrella instead: ncr_picking_all.pro lists this project with
# `no_default_target`, which leaves qmake's `make_first:` rule with no dependencies. lupdate
# still recurses into it, because recursion does not care about build targets.
#
# Consequence to know about: `nmake all` ignores no_default_target and will try to compile
# this — and fail, because an aux project gets no Qt include paths. Use the default target,
# which is what Qt Creator and every recipe in docs/rules/build_and_verification.md do.
#
# ── Why this file exists ──────────────────────────────────────────────────────────────
#
# lupdate updates only the .ts files a project lists in TRANSLATIONS, and it scans only that
# project's own SOURCES/HEADERS/FORMS. Before this file existed, TRANSLATIONS was declared by
# the two application shells — and after Phase 6 / E7a the shells compile a handful of files
# each, because all of src/ moved into the ncr_shared static library. So an update run saw
# ~40 strings out of ~974 and marked the other 861 as vanished. Nothing failed; the build was
# green and the Japanese UI simply reverted to English.
#
# The shells now declare EXTRA_TRANSLATIONS instead, which lrelease still releases and
# embed_translations still embeds, but which lupdate never touches. See components/app/app.pri.
#
# ── The rule ──────────────────────────────────────────────────────────────────────────
#
# The scan set is assembled by including the SAME .pri files the build uses. Never by listing
# sources here. A hand-written list is the identical failure one module later: it would be
# correct on the day it was written and quietly incomplete afterwards, with no error to say
# so. Including the .pri files means a new module joins the translation scan the moment it
# joins the build.

TEMPLATE = aux

QT += core gui widgets

# --- The scan set -----------------------------------------------------------------------
#
# Everything that can contain a translatable string:
#   src/            the seven modules, compiled into ncr_shared
#   3rdparty/       the vendored Qt Solutions property browser — ~17 translatable contexts
#                   (QtBoolEdit, QtColorEditWidget, QtCursorDatabase, …). It is compiled into
#                   the library but belongs to no module .pri, so it has to be named here or
#                   that whole group stays vanished.
#   components/app/ commissioning shell
#   runtime_app/    operator runtime shell
include($$PWD/../src/core/core.pri)
include($$PWD/../src/device/device.pri)
include($$PWD/../src/calibration/calibration.pri)
include($$PWD/../src/matching/matching.pri)
include($$PWD/../src/model/model.pri)
include($$PWD/../src/runtime/runtime.pri)
include($$PWD/../src/ui/ui.pri)

include($$PWD/../3rdparty/qtpropertybrowser/qtpropertybrowser_vendor.pri)

include($$PWD/../components/app/app.pri)
include($$PWD/../runtime_app/runtime_app.pri)

# --- Take back everything an lupdate-only project must not do ---------------------------
#
# The .pri files above are written for building. They bring resources, link flags and the
# lrelease/embed_translations steps with them; none of that belongs here, and running
# lrelease from two places would produce the .qm twice.
#
# EXTRA_TRANSLATIONS is cleared for a different reason: the shells declare the .ts there, and
# leaving it set would make this project release a .qm nobody uses. lupdate ignores it either
# way — only the TRANSLATIONS line below drives the update.
CONFIG -= lrelease embed_translations
RESOURCES =
EXTRA_TRANSLATIONS =
LIBS =
PRE_TARGETDEPS =
QMAKE_POST_LINK =

# --- The file this project owns ---------------------------------------------------------
#
# One file for the whole product: both shells load a single :/i18n/ncr_picking_ja_JP through
# one QTranslator, so one .ts is what they can actually consume.
#
# NEVER update this with lupdate's -no-obsolete. That flag deletes vanished entries outright,
# and vanished entries still carry their Japanese — running it on 2026-08-24 would have
# destroyed 835 recoverable translations. Vanished is recoverable; deleted is not.
TRANSLATIONS = $$PWD/../components/app/translations/ncr_picking_ja_JP.ts
