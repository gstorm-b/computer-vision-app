# Documentation Map

This folder is organized by authority. Read the smallest set that matches the
task; do not load the whole tree by default.

## Source Of Truth

| Area | Read |
|---|---|
| Agent entrypoint and authority map | [../AGENT.md](../AGENT.md) |
| Module boundaries and invariants | `AGENTS.md` scope card inside each module folder (`src/<module>/`, `app/`) |
| Engineering and architecture rules | [rules/design_rules.md](rules/design_rules.md) |
| UI/QSS implementation rules | [rules/ui_design_rules.md](rules/ui_design_rules.md) |
| UI token palette and colour migration | [rules/ui_theme_tokens.md](rules/ui_theme_tokens.md) |
| qmake/MSVC build and verification | [rules/build_and_verification.md](rules/build_and_verification.md) |
| Doxygen/Graphviz/PlantUML docs build | [rules/documentation_build.md](rules/documentation_build.md) |
| Current structural architecture | [../uml/](../uml/) |

## Current Domain Specs

| Domain | Read |
|---|---|
| Task Localization | [domains/task_localization/README.md](domains/task_localization/README.md) |
| Operator runtime shell (`ncr_runtime.exe`) | [domains/runtime_app/runtime_shell.md](domains/runtime_app/runtime_shell.md) |
| Virtual (hardware-free) devices | [domains/virtual_devices/virtual_devices.md](domains/virtual_devices/virtual_devices.md) |
| Pick geometry &amp; vision output contract | [domains/task_localization/pick_geometry_and_output_contract.md](domains/task_localization/pick_geometry_and_output_contract.md) |
| Signal map widgets | [domains/signal_map/](domains/signal_map/) |
| Calibration | [domains/calibration/calibration_module.md](domains/calibration/calibration_module.md) |
| Calibration points table UI | [domains/calibration/calibration_tables_point.md](domains/calibration/calibration_tables_point.md) |
| RobotKinematics integration | [architecture/robot_kinematics_module.md](architecture/robot_kinematics_module.md) |
| Device type notes | [architecture/device_type.md](architecture/device_type.md) |

## Active Planning And Backlog

| Purpose | Read |
|---|---|
| Active technical-debt backlog | [backlog/technical_debt_and_next_steps.md](backlog/technical_debt_and_next_steps.md) |
| Deferred TODO list | [backlog/later_todo_list.md](backlog/later_todo_list.md) |
| Long-form architecture backlog | [backlog/architecture_improvement_todo.md](backlog/architecture_improvement_todo.md) |
| Vision widget implementation handoff | [backlog/vision_widgets_implementation_plan.md](backlog/vision_widgets_implementation_plan.md) |
| Closed restructure roadmap and source-of-truth hierarchy | [architecture/project_restructure_plan.md](architecture/project_restructure_plan.md) |

## Product And Packaging

**Phase 4 is on hold as of 2026-07-28.** Both documents below are recorded plans,
not active work — read them for the release shape decision, not as a task list.

| Purpose | Read |
|---|---|
| Install image manifest (`dist/`) | [product/install_image.md](product/install_image.md) |
| Product/release shape (on hold) | [product/phase4_product_release_plan.md](product/phase4_product_release_plan.md) |
| Customer installer risk and checklist (on hold) | [product/customer_installer_packaging.md](product/customer_installer_packaging.md) |

## Traceability Only

These files are valuable history, but they are not current source of truth.

| Group | Contents |
|---|---|
| [history/restructure/](history/restructure/) | Phase verification notes and closeout records |
| [history/handoffs/](history/handoffs/) | Completed agent handoff/rework requests |
| [history/old_session/](history/old_session/) | Legacy session notes |
| [history/request/](history/request/) | Original user/request prompts and sample assets |

## Generated And Artifacts

These files support inspection, demos, or generated reference. Prefer source
docs and code when behavior matters.

| Group | Contents |
|---|---|
| [generated/architecture_docs/](generated/architecture_docs/) | Generated or drift-prone class/API reference |
| [generated/html/](generated/html/) | HTML generated from markdown docs |
| [generated/doxygen/](generated/doxygen/) | Full Doxygen API reference built from source `///` comments (see [rules/documentation_build.md](rules/documentation_build.md)) |
| [artifacts/ui/](artifacts/ui/) | UI mockups and previews |

## Cleanup Policy

Do not delete a doc just because it is old. First move it under `history/`,
`generated/`, or `artifacts/`, update references, and only delete it when it has
no remaining traceability value and the source-of-truth equivalent is listed in
this index.
