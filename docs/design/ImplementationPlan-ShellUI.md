# Implementation plan: Shell UI

**Status:** active implementation plan.

This document defines the implementation architecture and delivery sequence for
the adopted [Shell UI specification](Spec-ShellUI.md). That specification is
authoritative for visible behaviour; the project-wide
[SilOS plan](SilOS_PLAN.md) is authoritative for priorities and milestones.
The existing [UI rendering refactor](RefactorPlan-UI-Rendering-Pipeline.md)
describes the implemented baseline from which this work starts.
Public application-facing contracts remain in the proposed
[UI API](API-UI.md) and [Shell API](API-Shell.md); module placement follows the
[code-layout decision](Discussion-Code-Layout.md).

## Intent

Implement the complete Shell interaction model first on the Browser target,
using Canvas rather than HTML layout. Portable code owns interaction and
relative layout. Target adapters own pixels and physical drawing while
preserving the same hierarchy, navigation, and semantic visual roles.

The implementation favours simple full-frame reconstruction. It must not add
caching, dirty-region tracking, or retained presentation state until
measurement demonstrates a need.

## Module architecture

### `ShellUIRuntime`

`ShellUIRuntime` is the runtime integration module for the Shell UI task. It:

- owns the render cadence and bounded semantic-input queue;
- owns the `ShellInteractionEngine` instance and its persistent state;
- coordinates app-catalogue access and one-app-at-a-time uLisp workspace locks;
- invokes scene construction and display once per frame; and
- participates in start, stop, reload, and shutdown sequencing.

It does not interpret navigation policy, calculate layout, traverse uLisp UI
templates, or draw. Keeping interaction state on the render task avoids
cross-task UI-state snapshots.

### `ShellInteractionEngine`

`ShellInteractionEngine` is the portable interaction state machine. Its small
interface accepts one semantic input event and exposes the resulting state to
scene construction. Its implementation owns:

- Shell-level versus app-level focus and Back toggling between them;
- hierarchy traversal through apps, mounts, rows, fields, and data entry;
- Single App and Multi App state;
- Shell menus and pages, app menus, move, resize, settings, and editing modes;
- double-Enter recognition from press timing, excluding repeat events;
- focus identity, selection, scroll positions, and remembered app dimensions;
- commands or app events produced by an interaction; and
- every state transition required by `Spec-ShellUI.md`.

Menus, pages, focus variants, and editors begin as internal state variants, not
separate public classes. A new module seam is warranted only if it hides
substantial policy behind a smaller interface.

### `ShellLayoutEngine`

`ShellLayoutEngine` converts interaction state, display capabilities, and the
app catalogue into a frame-local semantic scene. Common code owns relative
structure for the SilOS menu, workspace, bottom row, replacement pages, app
columns, mounts, rows, and fields.

App tiling uses relative column widths and per-app height weights. A sole app
in a column occupies its full height without changing its remembered
multi-app height. The engine precomputes directional navigation neighbours:
vertical movement stays within a column, while horizontal movement selects the
app spanning the previous app's normalized top position. No pixel feedback or
renderer hit-testing is required for keyboard navigation.

The scene contains stable native identities, hierarchy, relative extents,
semantic style roles, clipping relationships, and navigation links. It does
not persist borrowed uLisp objects or rendered values.

### `UITemplatePopulator`

`UITemplatePopulator` is the portable app-content module. It replaces the
ambiguous `UIRenderEngine` role and absorbs the existing per-app traversal
behind one interface. For a requested app region it:

- traverses that app's `UIAppBinding` mounts, templates, lists, rows, and fields;
- supplies semantic content and field metadata to scene construction; and
- streams literals and current values while that app's workspace lock is held.

Borrowed uLisp pointers remain valid only during the population call and must
never be retained in the scene. Existing `UIAppRenderer` logic may remain as a
private helper during migration; it is not a separate architectural seam.

### `ShellRenderEngine`

`ShellRenderEngine` is the portable frame-composition module. One render call
uses `ShellLayoutEngine` and `UITemplatePopulator` to compose Shell chrome,
replacement pages, app regions, content, bottom-row prompts, and focus styles,
then submits the completed semantic frame to `IPlatformDisplay`.

It rebuilds the whole semantic scene on every frame. Only interaction and
layout preferences survive between frames. The scene-construction seam should
carry a short implementation note explaining that a future measured
optimisation may retain an **unpopulated** structural scene and invalidate it
when catalogue, layout, hierarchy, or capability inputs change. Dynamic uLisp
values would still be refreshed under their app lock.

### `IPlatformDisplay`

`IPlatformDisplay` is the Shell-owned platform seam. It receives one semantic
frame plus display capabilities such as size class and usable logical rows and
columns. The adapter chooses pixel coordinates, font metrics, border strokes,
clipping, and physical output.

The interface exposes semantic roles rather than Browser or device concepts:
normal content, app focus, app-menu/layout ownership, active selection,
disabled content, and editing state. Adapters must keep these roles visually
distinguishable within the target's monochrome or two-colour capabilities.

`BrowserCanvasDisplay` is the first adapter. It uses a deliberately small
Canvas drawing vocabulary: clear/fill, text, lines or rectangles, clipping,
and semantic foreground/background styles. DOM elements must not perform Shell
layout or represent app windows.

## Data and event flow

```text
platform or future I/O adapter
  -> bounded semantic input queue
  -> ShellUIRuntime
  -> ShellInteractionEngine
  -> ShellLayoutEngine + UITemplatePopulator
  -> ShellRenderEngine
  -> IPlatformDisplay
```

`ShellInputEvent` is pointer-free and carries a semantic action, press/repeat
information, and a monotonic timestamp where timing is relevant. Its action
set covers Back, Enter, four directions, and the text-editing operations needed
by the specification. Physical key codes, GPIO identifiers, DOM events, and
target SDK types remain inside adapters.

Effects intended for applications cross the existing bounded Shell/uLisp event
transport and retain app identity and generation checks. Shell UI input and
app-directed input are independently routable; the Shell is not an implicit
relay for every input.

## Input/Output architectural direction

Input/Output is a top-level SilOS capability alongside Shell, UI, Store, and
Runtime. A future design must support:

- target adapters for GPIO and other physical or logical channels;
- bidirectional binding between channels and application-visible variables;
- configured events routed to the Shell or directly to a selected app; and
- bounded, pointer-free transport across task ownership seams.

This milestone defines only the semantic Shell input seam. GPIO discovery,
configuration, output scheduling, electrical policy, and the public uLisp I/O
interface remain deferred until concrete use cases are planned.

## Implementation sequence

1. Introduce semantic input, display-capability, semantic-scene, and
   `IPlatformDisplay` types with fake-adapter tests.
2. Implement `ShellInteractionEngine` state transitions and
   `ShellLayoutEngine` relative geometry/navigation against the specification.
3. Adapt the existing renderer into `UITemplatePopulator`, preserving
   one-app-at-a-time locking and zero-copy borrowed-value lifetimes.
4. Implement `ShellRenderEngine` and integrate it through `ShellUIRuntime`.
5. Replace Browser DOM presentation with `BrowserCanvasDisplay` and remove the
   obsolete platform-render interface after all callers migrate.
6. Complete the specification conformance matrix and measure frame time,
   input-to-display latency, stack use, and steady-state heap use.

Each step must leave `src/test.sh` passing. Public uLisp changes require the
applicable `API-*.md` and Alive API metadata updates in the same step.

## Verification and completion

Portable unit tests use semantic events, deterministic timestamps, display
capabilities, and fake app catalogues. They cover hierarchy traversal,
double-Enter, focus restoration, menu/page replacement, editing, auto-hide,
Single/Multi App transitions, column navigation, movement, resizing, and
remembered heights.

Scene tests assert relative geometry, navigation links, clipping ownership,
bottom-row reservation, and semantic focus roles without asserting pixels.
Population tests assert traversal order, per-app locking, coherent reads, and
that borrowed uLisp values are not retained after a call. Display contract
tests assert balanced frame/region operations and safe clipping.

Browser integration tests exercise Canvas output at small and larger viewport
profiles. Visual checks cover the monochrome/two-colour style roles, left-side
Nested Frame menu, non-overlapping bottom row, app tiling, and absence of
pop-over windows or DOM-based layout.

The milestone is complete when the Browser Canvas implementation satisfies
every conformance requirement in `Spec-ShellUI.md`, all repository tests pass,
and the measurements above are recorded. ESP32 adoption is the next validation
of the portable architecture, not a completion condition for this milestone.
