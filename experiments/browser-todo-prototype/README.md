# Browser to-do prototype

Completed and frozen on 25 August 2026. This Browser-first to-do experiment
validated the architecture now adopted in the
[SilOS plan](../../docs/design/SilOS_PLAN.md). Its implementation was promoted
to [`src/`](../../src/); continue production work there.

This tree deliberately begins with fresh, complete upstream source snapshots:

| Dependency | Upstream revision | Local path |
| --- | --- | --- |
| FreeRTOS-Kernel V11.3.0 | `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5` | `third-party/FreeRTOS-Kernel/` |
| uLisp ESP 4.9a | `aa9b24ca3323159dacadca60ea0e9ffdf00b1a81` | `third-party/ulisp-esp/` |

The initial commit on `codex/browser-todo-prototype` is intended as the clean
vendor baseline. Once it exists, use Git to inspect any direct upstream-source
changes, for example:

```powershell
git diff <baseline-commit> -- third-party/FreeRTOS-Kernel
git diff <baseline-commit> -- third-party/ulisp-esp
```

Experiment-owned configuration, ports, generated adapters, application code,
and browser assets belong outside `third-party/`. Keeping that boundary makes
both direct modifications and supporting adaptations explicit.

See [plan.md](plan.md) for the historical scope and [spike.md](spike.md) for
evidence gathered during the experiment.
