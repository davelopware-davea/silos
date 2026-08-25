# SilOS

SilOS is a tiny, text-first operating environment for embedded control and
information systems. It is intended to share a common core, applications, and
interaction model across constrained microcontrollers, Raspberry Pi computers,
web browsers, and larger computers.

The project prioritizes a small code and memory footprint, predictable resource
use, offline operation, and portability over maximum execution speed. Its first
complete system is planned as a compact personal-tools environment containing a
to-do list, alarm, calendar, and chat application.

## Contents

1. [Project status](#project-status)
2. [Key documents](#key-documents)
3. [Discussion records](#discussion-records)
4. [License](#license)

## Project status

SilOS is now in **implementation**. The successful Browser to-do prototype has
been promoted to [`src/`](src/) as the production baseline. FreeRTOS, uLisp,
StoreRefs/UiRefs, flow templates, capability-owned modules, and narrow platform
adapters are the committed architecture; current work is completing to-do CRUD,
persistence, and the ESP32 target. See
[SilOS_PLAN.md](docs/design/SilOS_PLAN.md) for the current decisions and work.

## Key documents

- [SilOS_PLAN.md](docs/design/SilOS_PLAN.md) - the authoritative living project plan,
  including agreed principles, the current milestone, active questions, and
  later areas of work.
- [src/README.md](src/README.md) - the promoted implementation, its current
  capabilities and limits, and Browser build/test instructions.
- [SilOS_DESIGN_BACKLOG.md](docs/design/SilOS_DESIGN_BACKLOG.md) - archived design ideas,
  alternatives, and earlier recommendations retained for future reference.
- [SilOS code layout](docs/design/Discussion-Code-Layout.md) - project-level
  guidance for module ownership, platform interfaces, dependency adapters,
  third-party hooks, and build-file boundaries.
- [Variable binding and templates](docs/design/Discussion-VariableBindingAndTemplates.md)
  - the adopted model covering exposed uLisp variables,
  Shell-owned templates, task ownership, and full-frame rendering.
- [Shell UI](docs/design/Discussion-Shell-UI.md) - the current discussion of
  Shell layout, focus, input, tiny-mode rendering, and bottom-line editing.
- [Queue metaphors](docs/design/Discussion-QueueMetaphors.md) - the exploratory
  model for StoreRefs, storage, MQTT, and their API sketches.
- [BoundQueueStore API](docs/design/API-BoundQueueStore.md) and
  [BoundQueueMQTT API](docs/design/API-BoundQueueMQTT.md) - proposed APIs that
  accompany the queue discussion; neither is committed.
- [FreeWisp spike plan](experiments/freertos-ulisp-browser/plan.md) - the
  completed FreeRTOS + uLisp Browser WebAssembly experiment and its conclusions.
  Its detailed plan, journal, implementation, and measurements are historical
  evidence and should be loaded into context only when FreeWisp is specifically
  requested or referenced.
- [AGENTS.md](AGENTS.md) - project-specific instructions for contributors and
  coding agents.
- [LICENSE](LICENSE) - the MIT License covering the SilOS source code.

## License

SilOS source code is available under the [MIT License](LICENSE).

## Discussion records

- [SilOS code layout](docs/design/Discussion-Code-Layout.md) - the agreed
  project-level code ownership and dependency-layout conventions.
- [Variable binding and templates](docs/design/Discussion-VariableBindingAndTemplates.md)
  - the adopted model for exposing variables once and rendering
  them through reusable, screen-independent flow templates.
- [FreeRTOS, uLisp, and variable-to-UI binding](docs/design/Discussion-FreeRTOS-uLisp-Variable-UI-Binding.md)
  - earlier exploratory background on live binding between Lisp values and the
  UI; consult it only when that earlier reasoning is specifically relevant.
- [Shell UI](docs/design/Discussion-Shell-UI.md) - current exploratory Shell
  layout, navigation, and editing model.
- [Queue metaphors](docs/design/Discussion-QueueMetaphors.md) - current
  exploratory storage and MQTT reference model.
