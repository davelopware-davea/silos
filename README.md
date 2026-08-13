# SilOS

SilOS is a tiny, text-first operating environment for embedded control and
information systems. It is intended to share a common core, applications, and
interaction model across constrained microcontrollers, Raspberry Pi computers,
web browsers, and larger computers.

The project prioritizes a small code and memory footprint, predictable resource
use, offline operation, and portability over maximum execution speed. Its first
complete system is planned as a compact personal-tools environment containing a
to-do list, alarm, calendar, and chat application.

## Project status

SilOS is currently in the **design and prototyping stage**. The FreeWisp
FreeRTOS + uLisp Browser spike is complete, and work is now moving into the
first end-to-end prototype. See [SilOS_PLAN.md](docs/design/SilOS_PLAN.md) for
the current decisions, open questions, and planned work.

## Files

- [SilOS_PLAN.md](docs/design/SilOS_PLAN.md) - the authoritative living project plan,
  including agreed principles, the current milestone, active questions, and
  later areas of work.
- [SilOS_DESIGN_BACKLOG.md](docs/design/SilOS_DESIGN_BACKLOG.md) - archived design ideas,
  alternatives, and earlier recommendations retained for future reference.
- [Variable binding and templates](docs/design/Discussion-VariableBindingAndTemplates.md)
  - the current prototype discussion covering exposed uLisp variables,
  Shell-owned templates, task ownership, and full-frame rendering.
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

- [Variable binding and templates](docs/design/Discussion-VariableBindingAndTemplates.md)
  - the current high-level hypothesis for exposing variables once and rendering
  them through reusable, screen-independent flow templates.
- [FreeRTOS, uLisp, and variable-to-UI binding](docs/design/Discussion-FreeRTOS-uLisp-Variable-UI-Binding.md)
  - earlier exploratory background on live binding between Lisp values and the
  UI; consult it only when that earlier reasoning is specifically relevant.
