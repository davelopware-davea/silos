# Discussion: Variable Binding and Templates

Conversation recorded on 13 August 2026.

## Purpose

This note summarises the current high-level idea for exposing uLisp variables to
the SilOS Shell and rendering them through reusable, display-independent
templates. It is discussion material to guide the next prototype; it does not
yet promote these details into the authoritative SilOS plan.

## Core model

The design separates three concepts:

1. A **uLisp variable** contains the authoritative application value in the
   uLisp workspace.
2. A **binding** exposes that variable once to the Shell and gives it a stable
   Shell-side identity.
3. A **template** is an ordered list of instructions describing how literals
   and already-bound variables flow into the UI.

A bound variable may appear in multiple templates, or multiple times in the
same template. Templates refer to bindings; they do not own variables or create
a new binding for each occurrence.

```text
uLisp variable -> one Shell binding -> many template occurrences
```

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

The binding registry associates a stable `BindingId` with a uLisp global
variable. An entry may cache the variable's global environment binding-pair
pointer, while templates contain only `BindingId` references. Consequently, if
a cached pair moves, only the registry entry needs repair.

## Rendering and synchronisation

The first implementation should favour predictable behaviour over change
tracking or partial rendering:

1. The Shell UI task wakes at its selected refresh rate.
2. It locks access to the uLisp workspace, preventing evaluation, mutation, GC,
   and compaction.
3. It traverses the active template and follows each referenced binding pair to
   its current value.
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
- `Binding`: render the current value of an exposed variable and advance the
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

Formatting belongs to a `Binding` instruction, not to the binding registry.
The same exposed variable can therefore be clipped or formatted differently in
each occurrence.

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

- the Lisp-facing operations for exposing and unexposing a global variable;
- whether binding and template commands are queued to the Shell task or modify
  Shell-owned structures through a short synchronised call;
- the fixed capacities and memory cost of bindings, templates, instructions,
  literals, and format strings;
- the safe handling of missing, unbound, expired, or wrong-type values;
- whether cached binding-pair relocation or per-refresh symbol resolution is
  simpler on the selected uLisp implementation; and
- the measured workspace-lock duration and full-frame rendering latency on the
  Browser and reference MCU.
