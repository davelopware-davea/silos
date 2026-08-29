# RefactorPlan: zero-copy UI rendering pipeline

**Status:** implemented baseline on 29 August 2026.

This is the supporting implementation plan for the UI pipeline refactor. The
project-wide decisions remain authoritative in [SilOS_PLAN.md](SilOS_PLAN.md).

## Intent and decisions

- uLisp objects are canonical for UI bindings, declarations, literals, rows,
  fields, values, and metadata.
- Portable traversal reads borrowed `object*` values without persistent or
  transient value copies. Helpers hide representation details while exposing
  the underlying objects.
- Platform renderers exclusively own drawing, formatting buffers, caching,
  diffing, clipping, and physical output.
- App and UI registry capacity follows available memory rather than global
  compile-time limits.
- A dedicated UI task renders every platform-configured frame. It takes the
  global workspace mutex for one app at a time, making each app coherent while
  allowing apps in one frame to reflect different moments.

## Implemented module seam

- `SilOS/UI/UITemplateEngine.{h,cpp}` owns the dynamically sized collection of
  app bindings, implements declarations behind thin uLisp built-ins, and owns
  binding lifecycle and GC-root integration.
- `SilOS/UI/UIAppBinding.{h,cpp}` owns one app's rooted type, UiRef, template,
  and mount registries and exposes zero-copy traversal.
- `SilOS/UI/UIRenderEngine.{h,cpp}` owns only app renderers, drives frames, and
  visits the catalogue in runtime order.
- `SilOS/UI/UIAppRenderer.{h,cpp}` borrows its stable app binding, traverses one
  locked app, and emits semantic operations.
- `SilOS/UI/IPlatformRenderEngine.h` defines the platform interface receiving
  frame, app, template, and field operations. Borrowed pointers remain valid
  only during the call and app lock.
- `SilOS/uLisp/ULispAccess.h` is the opaque object-navigation interface. Its
  implementation and the thin built-in/root adapters remain in the uLisp
  translation unit because the vendored runtime keeps its object model private.
- `SilOS/uLisp/UIBuiltins.inc` contains no UI policy; it only maps built-ins to
  `UITemplateEngine` using the current app binding.

The Browser currently rebuilds its DOM projection each frame. Other targets may
draw directly, retain a framebuffer, or emit only changes without altering
portable UI code.

## Memory and lifecycle rules

- Runtime storage is sized from the discovered app catalogue at bootstrap.
- Store collections, row fields, names, values, UI registries, and declarations
  have no arbitrary compile-time data capacity.
- Template `:limit` and field `:width` bound traversal and presentation; they do
  not allocate corresponding arrays or buffers.
- GC marks retained roots. Compaction reports pointer moves so external root
  heads remain valid.
- Catalogue reload or `load-image` integration must stop rendering, clear
  registrations, replace runtime storage, and rebootstrap before rendering.

## Acceptance criteria

- Steady-state portable rendering performs no native allocation or value copy.
- Strings stream directly from uLisp cells into the platform renderer.
- Mutations faster than the frame cadence coalesce into the latest app sample.
- Data exceeding former app, store, field-count, name, literal, visible-row,
  and rendered-text limits is accepted subject to available memory.
- GC, compaction, stale generations, reload, allocation failure, lock duration,
  frame time, stack use, and heap stability remain explicit verification targets.

Input, focus, controls, hit-testing, platform display strategies, and bounded
inter-task transport payloads are outside this refactor.
