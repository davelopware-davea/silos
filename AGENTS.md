# Project instructions

## Planning

- [`docs/design/SilOS_PLAN.md`](docs/design/SilOS_PLAN.md) is the authoritative **project-wide SilOS plan**. Consult it for all project planning and update it only when SilOS-level decisions, milestones, requirements, or plans change.

## FreeWisp spike

- **FreeWisp** is the short name for the FreeRTOS + uLisp browser WebAssembly spike. Treat references to "FreeWisp" as references to this experiment.
- FreeWisp lives in [`experiments/freertos-ulisp-browser/`](experiments/freertos-ulisp-browser/). Read its local [`plan.md`](experiments/freertos-ulisp-browser/plan.md) and [`spike.md`](experiments/freertos-ulisp-browser/spike.md) before working on it.
- [`experiments/freertos-ulisp-browser/plan.md`](experiments/freertos-ulisp-browser/plan.md) is the **FreeWisp-only plan**. Keep it short and current; update it when this spike's agreed scope, approach, sequence, success criteria, or non-goals change. It does not supersede the project-wide SilOS plan.
- [`experiments/freertos-ulisp-browser/spike.md`](experiments/freertos-ulisp-browser/spike.md) is the **FreeWisp experimental journal**. Append a dated entry after meaningful investigation or implementation, recording what was tried, why, the observed result (including failures), supporting measurements or evidence, and the next question when useful.
- If FreeWisp evidence leads to a SilOS-level decision, record the evidence in `spike.md`, update the FreeWisp `plan.md` if its direction changes, and promote the resulting project decision to `docs/design/SilOS_PLAN.md`.
- Do not turn either plan into a work log or rewrite journal history. This separation preserves project direction, spike intent, and experimental evidence without conflating them.

## Communication style

- Keep responses concise and generally under 500 words unless the task clearly requires more detail or the user requests it.
- When additional context could be useful, summarize it as a short list of points instead of explaining every point in depth. Allow the user to request expansion on the items that interest them.
