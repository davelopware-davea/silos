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

SilOS is currently in the **design and planning stage**; see
[SilOS_PLAN.md](docs/design/SilOS_PLAN.md) for the current decisions, open questions, and
planned work.

## Files

- [SilOS_PLAN.md](docs/design/SilOS_PLAN.md) - the authoritative living project plan,
  including agreed principles, the current milestone, active questions, and
  later areas of work.
- [SilOS_DESIGN_BACKLOG.md](docs/design/SilOS_DESIGN_BACKLOG.md) - archived design ideas,
  alternatives, and earlier recommendations retained for future reference.
- [FreeWisp spike plan](experiments/freertos-ulisp-browser/plan.md) - the active
  FreeRTOS + uLisp browser WebAssembly experiment.
- [AGENTS.md](AGENTS.md) - project-specific instructions for contributors and
  coding agents.
- [LICENSE](LICENSE) - the MIT License covering the SilOS source code.

## License

SilOS source code is available under the [MIT License](LICENSE).

## Discussion records

- [FreeRTOS, uLisp, and variable-to-UI binding](docs/design/Discussion-FreeRTOS-uLisp-Variable-UI-Binding.md)
  - an exploratory discussion about live binding between Lisp values and the
  UI; this is background material rather than the active FreeWisp spike.
