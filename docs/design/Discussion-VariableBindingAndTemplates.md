# Discussion: Variable Binding and Templates

Conversation begun on 13 August 2026; updated on 17 August 2026.

## Purpose

This note summarises the current high-level idea for exposing uLisp variables to
the SilOS Shell and rendering them through reusable, display-independent
templates. It is discussion material to guide the next prototype; it does not
yet promote these details into the authoritative SilOS plan.

**Related:** [Shell UI](Discussion-Shell-UI.md) applies UiRefs to focus and
bottom-line editing. [Earlier binding discussion](Discussion-FreeRTOS-uLisp-Variable-UI-Binding.md)
preserves the reasoning that led to this model.

## Contents

1. [Core model](#core-model)
2. [UiRef value, status, and watches](#uiref-value-status-and-watches)
3. [Task and memory ownership](#task-and-memory-ownership)
4. [Rendering and synchronisation](#rendering-and-synchronisation)
5. [Flow-based templates](#flow-based-templates)
6. [Per-occurrence formatting](#per-occurrence-formatting)
7. [Prototype questions still to answer](#prototype-questions-still-to-answer)

## Core model

The design separates three concepts:

1. A **uLisp variable** contains the authoritative application value in the
   uLisp workspace.
2. A **UiRef** exposes that variable once to the Shell and gives it a stable,
   language-visible and Shell-side identity. Earlier sketches called this a
   binding; `UiRef` distinguishes it from a template's `Binding` instruction.
3. A **template** is an ordered list of instructions describing how literals
   and already-bound variables flow into the UI.

A UiRef may appear in multiple templates, or multiple times in the same
template. Templates refer to UiRefs; they do not own variables or create a new
UiRef for each occurrence.

```text
uLisp variable -> one UiRef -> many template occurrences
```

## UiRef value, status, and watches

`defui` creates the UiRef for an exposed variable. The exact Lisp-facing
syntax remains to be designed, but the result is a stable live reference with a
conceptual shape parallel to a StoreRef:

```text
{
  meta: {
    status: ready | editing | stale | error,
    type: string | integer | ...,
    editable: true | false,
    revision: ...
  },
  value: current value of the exposed uLisp variable
}
```

This is language semantics, not a requirement to copy the uLisp value into a
second native object. The authoritative value remains in the uLisp variable;
the UiRef provides its stable identity and metadata.

`meta.status` describes the UI lifecycle. In particular, the Shell sets it to
`editing` while its bottom-line editor is active for that UiRef. It changes to
`stale` if an external update conflicts with the active edit. When editing ends,
application code can observe the transition and carry out post-edit work. This
status is distinct from the persistence state of a StoreRef or StoreRowRef that
may supply the variable's value.

An application can watch a UiRef, using the same live-reference convention as
StoreRefs:

```lisp
(ui-ref-watch title-ref
  (lambda (live old-value)
    ...))
```

The callback receives the still-live UiRef and a bounded, non-live old-value or
metadata snapshot. It runs in the uLisp task after the appropriate UiRef state
change, never directly in the Shell task. A Shell-originated change is therefore
sent to the uLisp task, which updates the UiRef and invokes watches safely.

## Task and memory ownership

The initial design uses separate FreeRTOS tasks for uLisp and the Shell UI:

- The **uLisp task** owns evaluation, mutation, allocation, garbage collection,
  and compaction of the uLisp workspace.
- The **Shell UI task** owns the binding registry, templates, layout policy, and
  framebuffer. No other task writes to the framebuffer.
- Browser or platform integration transports input and completed frames, but
  does not inspect the uLisp workspace or render application values.
- Queues carry bounded commands, input, results, and completed-frame messages
  between owners. They do not carry pointers into the uLisp workspace.

The UiRef registry associates a stable `UiRefId` with a uLisp global variable.
An entry may cache the variable's global environment binding-pair pointer,
while templates contain only `UiRefId` references. Consequently, if a cached
pair moves, only the registry entry needs repair.

## Rendering and synchronisation

The first implementation should favour predictable behaviour over change
tracking or partial rendering:

1. The Shell UI task wakes at its selected refresh rate.
2. It locks access to the uLisp workspace, preventing evaluation, mutation, GC,
   and compaction.
3. It traverses the active template and follows each referenced UiRef's binding
   pair to its current value.
4. It clears and redraws the complete framebuffer.
5. It unlocks the uLisp workspace.
6. It sends the completed framebuffer to the platform display or Browser
   bridge, then waits for its next refresh.

Rendering while holding the workspace lock lets the Shell stream authoritative
values directly from uLisp memory and avoids persistent native copies. If later
measurements show that this blocks the uLisp task for too long, a bounded,
per-refresh snapshot could shorten the lock interval. That optimisation is not
part of the initial design.

There is initially no variable change tracking, dirty-region tracking, or
framebuffer revision mechanism. Every refresh reads all referenced bindings and
renders the whole active template.

Ordinary uLisp mark-and-sweep garbage collection is non-moving and therefore
does not itself require remapping cached binding-pair pointers. Operations that
can move or remove pairs--notably workspace compaction, image loading, or
unbinding--must repair or invalidate registry entries while workspace access is
synchronised. An initial implementation may instead re-resolve pairs by symbol
to reduce pointer-lifetime risk; the prototype should determine the simplest
safe approach.

## Flow-based templates

Application code does not specify pixels, coordinates, screen dimensions,
fonts, or line height. A template behaves like a small sequence of inline
spans. The Shell chooses the template's origin and lays its instructions out
within the available display area.

The minimum instruction set is:

- `Literal`: render fixed text and advance the cursor;
- `Binding`: render the current value of a referenced UiRef and advance the
  cursor; and
- `NewLine`: move to the beginning of the next Shell-defined line.

For example:

```text
Literal("Desc:")
Binding(description, "%.16s")
NewLine
Literal("Status:")
Binding(status, "%.5s")
Literal(" ")
Binding(count, "%ld")
```

Adjacency is explicit: consecutive literal and binding instructions render
next to one another. If a space or separator is wanted, it is represented by a
literal instruction. The Shell controls clipping to the available region and
the physical interpretation of a line, allowing the same application template
to be used on different screen sizes.

## Per-occurrence formatting

Formatting belongs to a `Binding` instruction, not to the UiRef registry. The
same UiRef can therefore be clipped or formatted differently in each occurrence.

The initial representation should use a restricted C `printf`-style format
string directly in the instruction rather than introduce format objects or
format identifiers. Examples include `"%.16s"`, `"%5ld"`, and `"%.2f"`.

Template creation must validate these strings before the Shell stores them:

- permit exactly one supported conversion for each binding occurrence;
- require the conversion to match the Lisp value type passed to the formatter;
- reject `%n`, dynamic `*` width or precision, positional arguments, and
  unsupported length modifiers or conversions;
- enforce a bounded maximum formatted output length; and
- copy the validated format into Shell-owned template storage.

uLisp strings are not contiguous C strings. To use `%s`, the renderer must copy
the bounded content into a temporary render buffer or implement the supported
string formatting while traversing uLisp string cells. Such temporary storage
exists only during rendering and is not a second persistent application value.

The format language can become richer only when concrete application needs
justify it. If C formatting later proves too large for the MCU or insufficient
for values such as dates, its internal representation can change without
altering the variable-binding or flow-template model.

## Prototype questions still to answer

The next experiment should establish:

- the Lisp-facing operations for creating, watching, and releasing a UiRef;
- whether UiRef and template commands are queued to the Shell task or modify
  Shell-owned structures through a short synchronised call;
- the fixed capacities and memory cost of bindings, templates, instructions,
  literals, and format strings;
- the safe handling of missing, unbound, expired, or wrong-type values;
- whether cached binding-pair relocation or per-refresh symbol resolution is
  simpler on the selected uLisp implementation; and
- the measured workspace-lock duration and full-frame rendering latency on the
  Browser and reference MCU.
