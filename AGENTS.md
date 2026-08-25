# Project instructions

## Planning

- [`docs/design/SilOS_PLAN.md`](docs/design/SilOS_PLAN.md) is the authoritative **project-wide SilOS plan**. Consult it for all project planning and update it only when SilOS-level decisions, milestones, requirements, or plans change.


## Communication style

- Keep responses concise and under 500 words unless the task clearly requires more detail or the user requests it.
- When additional context could be useful, summarize it as a short list of points instead of explaining every point in depth. Allow the user to request expansion on the items that interest them.

## Alive API metadata

- When changing `docs/design/API-*.md` or SilOS's public uLisp built-ins, review and update `.vscode/silos-api-stubs.lisp`, then run `src/check_alive_api_stubs.py --update`. The default check runs under CTest and must pass after the review.
