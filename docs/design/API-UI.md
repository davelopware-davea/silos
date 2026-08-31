# Proposed UI API: templates and bounded lists

**Status:** proposed API for the next UI implementation phase.  This is the
normative contract for the small fixed-layout surface described here; the
questions in [Deferred design points](#deferred-design-points) are intentionally
not decided by this document.

**Related:** [variable-binding discussion](Discussion-VariableBindingAndTemplates.md),
[fixed-layout to-do example](Discussion-FreeRTOS-uLisp-Variable-UI-Binding.md#fixed-layout-to-do-example),
and [Store API](API-BoundQueueStore.md).

| Public form | Purpose |
| --- | --- |
| [`ui-bind`](#ui-bind) | Bind one named uLisp location to a stable UI-facing UiRef. |
| [`ui-type`](#ui-type) | Declare an immutable fixed-layout record descriptor. |
| [`ui-template`](#ui-template) | Declare a reusable flow or typed item template. |
| [`ui-template-list`](#ui-template-list) | Declare a bounded visible-window list template. |
| [`ui-field`](#ui-field) | Read an item field or supported UiRef property within a UI template. |
| [`ui-text`](#ui-text) | Stream literal or field text while rendering a UI template. |
| [`ui-date`](#ui-date) | Stream a formatted date field while rendering a UI template. |
| [`ui-mount`](#ui-mount) | Make a template or list available for Shell placement. |
| [`ui-invalidate`](#ui-invalidate) | Mark views depending on a UiRef dirty. |
| [`ui-unmount`](#ui-unmount) | Remove one mount. |
| [`ui-release`](#ui-release) | Release an unused UI resource. |

## 1. Model and ownership

The UI has four distinct objects:

- A **UiRef** is the one stable UI-facing handle for one named uLisp binding.
  The binding's current value remains authoritative in uLisp; a UiRef does not
  copy it.  A UiRef has an ID, generation, declared type, revision, and UI
  status.
- A **template** is an immutable rooted uLisp declaration. It refers to UiRefs
  and typed fields without copying literals or live values into native storage.
- A **list template** names a source UiRef, an item template, and a fixed
  visible window.  It is a template, not a materialised list of row widgets.
- A **mount** makes one template or list template available for Shell
  placement. The Shell decides whether and where it appears, including pixel
  coordinates, fonts, and line height.

The uLisp task alone evaluates Lisp, changes UiRefs and StoreRefs, and invokes
watches. A dedicated UI task obtains synchronised read access for one app at a
time and streams borrowed uLisp declarations and current values through the
platform render interface. The platform owns layout, caching, drawing, and
physical presentation.

Separate UiRefs may refer to independent bindings of the same named Store.
Each traverses its canonical StoreRef projection without copying, while
store-level writability is shared across those StoreRefs. A bounded
`store-wait-until-writable` call releases the uLisp workspace lock, so the UI
task continues rendering the last coherent values during the wait.

### `ui-bind`

`ui-bind` resolves an existing named binding in the current lexical environment
or, if none exists there, in the global environment. It retains that location
and returns its stable UiRef. The ordinary Lisp binding remains usable by app
code; the returned value is what UI declarations use. The binding must already
exist, so normal `let`, `defvar`, and `setq` forms own its scope and
initialisation.

```lisp
;; The UiRef retains this lexical location for as long as the UI resource lives.
(let ((headline "Starting...")
      (headline-ui nil))
  (setq headline-ui (ui-bind headline string))
  ...)
```

A second template must reuse `headline-ui`, not call `ui-bind` again for
`headline`.

The accepted declared types in this increment are `string`, `integer`,
`silos-datetime`, a declared record type, `(ui-list-of TYPE)`, and
`(store-ref (ui-list-of TYPE))`. Assigning a wrong type makes the UiRef
`silos-error`
and renders the applicable error state; it must not reinterpret memory.

## 2. Fixed-layout record types

### `ui-type`

```lisp
(ui-type todo-item
  ;; Field names are compile-time names.  Slot numbers are the actual array
  ;; layout, so an item can be a normal three-element uLisp array.
  (desc   string   0)
  (target silos-datetime 1)
  (status string   2))
```

`ui-type` registers an immutable type descriptor.  Slots must be unique,
non-negative, contiguous, and start at zero.  Thus the example requires an
array of exactly three elements.  `ui-field` in a matching item template compiles
to the numeric slot, not a symbol lookup.

```lisp
;; The application may use ordinary uLisp arrays; ui-type does not make a
;; wrapper object.  Keep these names beside the type declaration so imperative
;; code and declarative templates agree on the fixed layout.
(defvar todo-desc-slot   0)
(defvar todo-target-slot 1)
(defvar todo-status-slot 2)

(defun make-todo (desc target status)
  (let ((item (make-array 3)))
    (setf (aref item todo-desc-slot) desc)
    (setf (aref item todo-target-slot) target)
    (setf (aref item todo-status-slot) status)
    item))
```

### `ui-field`

For a plain record item, `(ui-field item status)` means `(aref item 2)`.  For a
`StoreRowRef`, it means the matching field in that row's live `value` record;
StoreRowRef identity, revision, status, and error remain metadata and are not
record fields.  A list whose declared item type is `todo-item` therefore may
render either local `todo-item` arrays or StoreRowRefs whose bound store schema
is `todo-item`.

In a non-item template, `(ui-field todos-ui count)` projects the live row-count
property of a `(store-ref (ui-list-of TYPE))` UiRef. The template retains the
UiRef and reads its current StoreRef metadata on each refresh; it does not copy
or maintain the count itself.

## 3. Templates, lists, and mounting

### `ui-template`

```lisp
(ui-template todo-row (item todo-item)
  ;; `item` is a transient parameter supplied once per visible row.  It does
  ;; not allocate a variable or retain the current record after the refresh.
  (ui-text "TODO:")
  (ui-text (ui-field item desc)   :width 16 :overflow ui-chop)
  (ui-date (ui-field item target) :format "yyyy-mm-dd")
  (ui-text (ui-field item status) :width 6  :overflow ui-chop))
```

An item template accepts literal text and its declared item parameter's
`(ui-field item field-name)` bindings in this increment. `ui-text` and
`ui-date` stream output while rendering. `:width` is a character-cell maximum
and `ui-chop` clips at that maximum; no padded or formatted string is retained.

A template without an item declaration renders once rather than once per list
row. It may combine literals with supported live UiRef properties:

```lisp
(ui-template todo-count
  (ui-text "Count")
  (ui-text (ui-field todos-ui count) :width 8 :overflow ui-chop))
```

### `ui-text`

`ui-text` streams either a string literal or its `ui-field` value as text while
rendering. A literal uses `(ui-text "text")`; the original declaration is rooted
and the literal is streamed from uLisp memory in declaration order for every
item. Templates impose no public instruction-count or literal-length cap.
A field form accepts the bounded text options defined by the template grammar,
including `:width` and `:overflow ui-chop` in this increment. The supported
non-item projection currently formats the integer StoreRef `count` property.

### `ui-date`

`ui-date` streams its `ui-field` value using its required `:format` option.
The accepted date representation and the complete formatter grammar remain
deferred.

### `ui-template-list`

```lisp
;; `todos-ui` remains the source after application code replaces `todos` with
;; another correctly typed list.
(defvar todos
  (list (make-todo "Buy milk"     20260809 "to do")
        (make-todo "Write report" 20260810 "in progress")
        (make-todo "Call the bank" 20260812 "to do")))
(defvar todos-ui (ui-bind todos (ui-list-of todo-item)))

(ui-template-list todo-list
  :source todos-ui
  :item-template todo-row
  :offset 0
  :limit 5
  :pending "Loading to-dos..."
  :empty "No to-dos."
  :error "To-dos unavailable.")

(defvar todo-list-mount (ui-mount todo-list))
```

`:source` must be a UiRef whose declared type is `(ui-list-of ITEM-TYPE)` or
`(store-ref (ui-list-of ITEM-TYPE))`; `ITEM-TYPE` must equal the item template's
type.  `:offset` and `:limit` are non-negative integer declaration constants,
not mutable values.  A list traverses at most `offset + limit` elements and
renders at most `limit` rows.  It does not allocate row instances, cache row
values, or provide unbounded repeat, filtering, sorting, or virtual scrolling.

For an ordinary list, `nil` selects `:empty`; otherwise the visible window is
rendered.  For a StoreRef source, `silos-pending` selects `:pending`,
`silos-error` selects `:error`, and `silos-ready` selects `:empty` or the visible
window using its current
collection of StoreRowRefs. The supplied state text remains in the rooted list
declaration. Row-level StoreRowRef failure is rendered by the
row's field formatter as an invalid value; this increment does not add a
per-row error template.

### `ui-mount`

`ui-mount` returns a mount handle and makes the template or list available to
the Shell. The app does not name a region or otherwise choose placement; the
Shell decides whether and where each mounted resource appears for the current
display and interaction context.

## 4. Refresh, invalidation, and StoreRefs

The UI task renders mounted views at its platform-configured cadence. A refresh
visits the complete mount and all rows in its visible window. It does
not do field-level or row-level retained invalidation.  This deliberately
catches list membership, ordering, array-slot replacement, and in-place string
changes without a subscription per item.

The following events mark every mount that depends on the UiRef dirty:

1. assigning the bound lexical or global location (which advances the UiRef revision);
2. `(ui-invalidate UIREF)`, for a mutation below the binding such as an
   `aref` slot update; and
3. an internally owned StoreRef or StoreRowRef watch for a StoreRef list
   source.

The list declaration installs two internally owned watches when a StoreRef
becomes its source: one collection watch for StoreRef state/membership and one
`store-rows-watch` watch that follows its current rows.  Their callbacks
run on the uLisp task only, mark the view dirty, and return.  Thus a
`silos-pending -> silos-ready`, row insertion, deletion, field update,
`silos-saving`, or `silos-error`
transition causes a later redraw.  They never render synchronously and must not
start an unbounded watch chain.

```lisp
;; This changes only the second record's status slot.  The list's source
;; binding did not change, so explicitly invalidate the UiRef afterwards.
(setf (aref (nth 1 todos) todo-status-slot) "done")
(ui-invalidate todos-ui)
```

### `ui-invalidate`

`ui-invalidate` marks every mount that depends on its UiRef dirty.  It is the
application-visible operation for reporting an in-place mutation below a bound
location; rendering still occurs later at the Shell's bounded refresh cadence.

For a store-backed list, no application watch is required solely to repaint it:

```lisp
;; The returned StoreRef is the live list source.  The list API owns the
;; bounded StoreRef/row watches needed to schedule a later refresh.
(defvar stored-todos
  (store-bind "todo/items" '(desc target status) 0 5))
(defvar stored-todos-ui
  (ui-bind stored-todos (store-ref (ui-list-of todo-item))))

(ui-template-list stored-todo-list
  :source stored-todos-ui
  :item-template todo-row
  :offset 0 :limit 5
  :pending "Loading to-dos..."
  :empty "No to-dos."
  :error "To-dos unavailable.")
```

## 5. Lifecycle and capacity profile

### `ui-unmount`

`ui-unmount` removes one mount.

### `ui-release`

`ui-release` releases a UiRef, template, or
list only when it has no dependent mounts; otherwise it signals an error.
Releasing a list also removes its internally owned StoreRef/row watches.
Releasing a UiRef invalidates its ID by incrementing its generation, so stale
template handles are rejected rather than reused.  Stopping or reloading an
app performs this cleanup automatically, including mounts, templates, UiRefs,
internal watches and their GC roots.

UiRefs, record types, templates, mounts, instructions, fields, visible rows,
names, literals, and state text have no project-wide compile-time capacity.
Their declarations retain uLisp objects and fail only when the workspace or
catalogue cannot allocate required registration cells. Publication is atomic:
a rejected declaration leaves existing UI state unchanged.

`:limit` bounds how many rows a list traverses and `:width` bounds presentation;
neither reserves a corresponding native array or text buffer. Platform
implementations may impose physical clipping or use target-native formatting
storage without changing the UI data contract.

## Non-goals of this increment

- Arbitrary nested record paths, association-list records, maps, or runtime
  field-name lookup.
- Dynamic list limits or offsets, scrolling controls, sorting, filtering,
  diffing, virtualisation, or row-widget allocation.
- Pixel coordinates, display dimensions, font selection, custom drawing, or
  an application-written render loop.
- Persistent render snapshots, per-field dirty regions, or subscriptions for
  individual local-array fields.
- Editable controls, focus, selection, and the Shell bottom-line editor.
- Public watch-removal or StoreRef cancellation APIs beyond the UI-owned
  cleanup required above.

## Deferred design points

The discussion does not justify a definite choice for the following, so they
are deliberately outside this API contract:

- the exact reader/macro implementation of `ui-bind`, `ui-type`, and template
  declarations, provided they expose the semantics above;
- the complete formatter grammar (including date representation, padding, and
  an alternative to `ui-chop`), and how render errors appear visually;
- whether list `:offset` later becomes a bounded UiRef for scrolling;
- Browser and reference-MCU workspace-lock duration and render cost; and
- StoreRef conflict/retry/deletion semantics and public watch-handle removal.
