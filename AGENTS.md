# Project instructions

## Planning

- [`docs/design/SilOS_PLAN.md`](docs/design/SilOS_PLAN.md) is the authoritative **project-wide SilOS plan**. Consult it for all project planning and update it only when SilOS-level decisions, milestones, requirements, or plans change.
- [`docs/design/Spec-ShellUI.md`](docs/design/Spec-ShellUI.md) is the authoritative **Shell UI interaction and layout specification**. Consult it when planning, implementing, or reviewing Shell UI behavior.
- [`docs/design/ImplementationPlan-ShellUI.md`](docs/design/ImplementationPlan-ShellUI.md) is the authoritative **Shell UI implementation architecture and sequence**. Consult it when changing Shell UI classes, task ownership, scene construction, input flow, or platform display adapters.

## Build and test

- Use [`src/build.sh`](src/build.sh) as the repository build entry point.
- Use [`src/test.sh`](src/test.sh) as the repository test entry point; it invokes `build.sh` itself.
- Run these scripts instead of constructing direct CMake, Ninja, or CTest commands. They select the supported build configuration and run `check-setup.sh`, which reports missing dependencies and their fixes.
- Lower-level build commands are appropriate only when the task is specifically diagnosing or changing the build scripts themselves. Finish verification through `build.sh` or `test.sh` as applicable.

## Communication style

- Keep responses concise and under 500 words unless the task clearly requires more detail or the user requests it.
- When additional context could be useful, summarize it as a short list of points instead of explaining every point in depth. Allow the user to request expansion on the items that interest them.

## Alive API metadata

- When changing `docs/design/API-*.md` or SilOS's public uLisp built-ins, review and update `.vscode/silos-api-stubs.lisp`, then run `src/check_alive_api_stubs.py --update`. The default check runs under CTest and must pass after the review.
