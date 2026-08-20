# Browser to-do prototype journal

**Experiment plan:** [plan.md](plan.md)

## 2026-08-20 - Fresh upstream baseline

### Question

How can this prototype retain a simple, trustworthy record of every direct
change made to FreeRTOS and uLisp after the experiment begins?

### Method

- Created the `codex/browser-todo-prototype` branch and this separate
  experiment tree.
- Retrieved fresh upstream checkouts of FreeRTOS-Kernel V11.3.0 and uLisp ESP
  4.9a, then verified their checked-out commits as
  `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5` and
  `aa9b24ca3323159dacadca60ea0e9ffdf00b1a81`, respectively.
- Removed the nested Git metadata so both complete source snapshots are normal
  files tracked by the SilOS repository.
- Kept all provenance and experiment documentation outside the upstream source
  directories.

### Result

The full, unmodified source snapshots are ready to be reviewed and committed
as the experiment baseline. No FreeWisp runtime code or source adaptations
have been copied into this tree yet.

### Next question

After the baseline commit, what is the smallest FreeWisp runtime slice that
can support loading and exercising the first real uLisp to-do application?
