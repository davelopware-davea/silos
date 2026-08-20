# Browser to-do prototype plan

**Status:** Baseline ready for review and initial commit.

**Project context:** [SilOS plan](../../docs/design/SilOS_PLAN.md).

## Goal

Build the smallest Browser-first SilOS prototype that expresses a real uLisp
to-do application, binds language-visible values to screen locations, updates
those locations after values change, accepts semantic input to create, edit,
and delete to-do items, and persists those items across restart.

## Baseline

- Start from the full, unmodified FreeRTOS-Kernel V11.3.0 and uLisp ESP 4.9a
  source snapshots in `third-party/`.
- Commit this state before copying, adapting, or adding any FreeWisp-derived
  runtime support. That commit is the comparison point for every subsequent
  direct change to either upstream source tree.
- Keep experiment-owned code outside `third-party/`; direct edits inside those
  trees must remain reviewable with Git diff.

## Sequence

1. Commit the vendor baseline after review.
2. Re-establish the smallest proven Browser runtime boundary from FreeWisp,
   preserving a clear separation between upstream code and experiment code.
3. Load one active to-do application written in uLisp.
4. Implement the smallest useful Shell boundary: templates/UiRefs, semantic
   input dispatch, and handlers.
5. Add StoreRef-backed to-do rows with create, edit, delete, and restart
   persistence.
6. Test and measure live binding, source-loading, handlers, references, and
   storage; record where their APIs need refinement.

## Non-goals

The experiment does not settle FreeRTOS/uLisp as the final cross-target
architecture, add later SilOS applications, or adopt the current Shell API
sketches unchanged.
