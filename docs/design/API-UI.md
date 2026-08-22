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
| [`ui-bind`](#ui-bind) | Bind one uLisp global to a stable UI-facing UiRef. |
| [`ui-type`](#ui-type) | Declare an immutable fixed-layout record descriptor. |
| [`ui-template`](#ui-template) | Declare a reusable typed item template. |
| [`ui-template-list`](#ui-template-list) | Declare a bounded visible-window list template. |
| [`ui-field`](#ui-field) | Read a declared item field within a UI template. |
| [`ui-text`](#ui-text) | Stream literal or field text while rendering a UI template. |
| [`ui-date`](#ui-date) | Stream a formatted date field while rendering a UI template. |
| [`ui-mount`](#ui-mount) | Place a template or list in a Shell-defined region. |
| [`ui-invalidate`](#ui-invalidate) | Mark views depending on a UiRef dirty. |
| [`ui-unmount`](#ui-unmount) | Remove one mount. |
| [`ui-release`](#ui-release) | Release an unused UI resource. |

## 1. Model and ownership

The UI has four distinct objects:

- A **UiRef** is the one stable UI-facing handle for one uLisp global binding.
  The binding's current value remains authoritative in uLisp; a UiRef does not
  copy it.  A UiRef has an ID, generation, declared type, revision, and UI
  status.
- A **template** is immutable Shell-owned formatting/layout metadata.  It holds
  UiRef IDs or typed item-field slot IDs, never Lisp values or heap pointers.
- A **list template** names a source UiRef, an item template, and a fixed
  visible window.  It is a template, not a materialised list of row widgets.
- A **mount** places one template or list template in a Shell-defined region.
  The Shell decides pixel coordinates, fonts, and line height.

The uLisp task alone evaluates Lisp, changes UiRefs and StoreRefs, and invokes
watches.  The Shell owns templates, mounts, layout, and the framebuffer.  A
Shell refresh obtains synchronised read access to the workspace and streams
current values directly into the framebuffer; it neither retains pointers into
the Lisp heap nor creates persistent formatted-value copies.

### `ui-bind`

`ui-bind` defines the named uLisp global and returns its stable UiRef.  The
ordinary global remains usable by Lisp code; the returned value is what UI
declarations use.

```lisp
;; `headline` is an ordinary global value.  `headline-ui` is its one UiRef.
;; A second template must reuse headline-ui, not call ui-bind again for headline.
(defvar headline-ui
  (ui-bind headline string "Starting..."))
```

The accepted declared types in this increment are `string`, `integer`,
`datetime`, a declared record type, `(list-of TYPE)`, and
`(store-ref (list-of TYPE))`.  Assigning a wrong type makes the UiRef `error`
and renders the applicable error state; it must not reinterpret memory.

## 2. Fixed-layout record types

### `ui-type`

```lisp
(ui-type todo-item
  ;; Field names are compile-time names.  Slot numbers are the actual array
  ;; layout, so an item can be a normal three-element uLisp array.
  (desc   string   0)
  (target datetime 1)
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

## 3. Templates, lists, and mounting

### `ui-template`

```lisp
(ui-template todo-row (item todo-item)
  ;; `item` is a transient parameter supplied once per visible row.  It does
  ;; not allocate a variable or retain the current record after the refresh.
  (ui-text "TODO:")
  (ui-text (ui-field item desc)   :width 16 :overflow chop)
  (ui-date (ui-field item target) :format "yyyy-mm-dd")
  (ui-text (ui-field item status) :width 6  :overflow chop))
```

An item template accepts literal text and its declared item parameter's
`(ui-field item field-name)` bindings in this increment. `ui-text` and
`ui-date` stream output while rendering. `:width` is a character-cell maximum
and `chop` clips at that maximum; no padded or formatted string is retained.

### `ui-text`

`ui-text` streams either a string literal or its `ui-field` value as text while
rendering. A literal uses `(ui-text "text")`; it is copied once into
exact-length, immutable template-owned storage and emitted in declaration order
for every item. Templates do not impose a public instruction-count or literal-
length cap: declaration allocates exactly the descriptor count and string bytes
required by the form.
A field form accepts the bounded text options defined by the template grammar,
including `:width` and `:overflow chop` in this increment.

### `ui-date`

`ui-date` streams its `ui-field` value using its required `:format` option.
The accepted date representation and the complete formatter grammar remain
deferred.

### `ui-template-list`

```lisp
;; The value begins as an ordinary list.  `todos-ui` is still the source even
;; after application code replaces `todos` with another correctly typed list.
(defvar todos-ui
  (ui-bind todos (list-of todo-item)
    (list (make-todo "Buy milk"     20260809 "to do")
          (make-todo "Write report" 20260810 "in progress")
          (make-todo "Call the bank" 20260812 "to do"))))

(ui-template-list todo-list
  :source todos-ui
  :item-template todo-row
  :offset 0
  :limit 5
  :pending "Loading to-dos..."
  :empty "No to-dos."
  :error "To-dos unavailable.")

;; Region names are Shell-owned; app code declares semantic placement only.
(defvar todo-list-mount (ui-mount todo-list :region 'main))
```

`:source` must be a UiRef whose declared type is `(list-of ITEM-TYPE)` or
`(store-ref (list-of ITEM-TYPE))`; `ITEM-TYPE` must equal the item template's
type.  `:offset` and `:limit` are non-negative integer declaration constants,
not mutable values.  A list traverses at most `offset + limit` elements and
renders at most `limit` rows.  It does not allocate row instances, cache row
values, or provide unbounded repeat, filtering, sorting, or virtual scrolling.

For an ordinary list, `nil` selects `:empty`; otherwise the visible window is
rendered.  For a StoreRef source, `pending` selects `:pending`, `error` selects
`:error`, and `ready` selects `:empty` or the visible window using its current
collection of StoreRowRefs.  The supplied state text is a bounded literal
owned by the list template.  Row-level StoreRowRef failure is rendered by the
row's field formatter as an invalid value; this increment does not add a
per-row error template.

### `ui-mount`

`ui-mount` returns a mount handle.  A template or list may have multiple
mounts, each with its own region; each mount renders the same live source.

## 4. Refresh, invalidation, and StoreRefs

The Shell refreshes dirty mounted views at its configured bounded cadence.  A
refresh redraws the complete mount and all rows in its visible window.  It does
not do field-level or row-level retained invalidation.  This deliberately
catches list membership, ordering, array-slot replacement, and in-place string
changes without a subscription per item.

The following events mark every mount that depends on the UiRef dirty:

1. assigning the global created by `ui-bind` (which advances the UiRef revision);
2. `(ui-invalidate UIREF)`, for a mutation below the global binding such as an
   `aref` slot update; and
3. an internally owned StoreRef or StoreRowRef watch for a StoreRef list
   source.

The list declaration installs two internally owned watches when a StoreRef
becomes its source: one collection watch for StoreRef state/membership and one
`store-rows-watch` watch that follows its current rows.  Their callbacks
run on the uLisp task only, mark the view dirty, and return.  Thus a
`pending -> ready`, row insertion, deletion, field update, `saving`, or `error`
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
global; rendering still occurs later at the Shell's bounded refresh cadence.

For a store-backed list, no application watch is required solely to repaint it:

```lisp
;; The returned StoreRef is the live list source.  The list API owns the
;; bounded StoreRef/row watches needed to schedule a later refresh.
(defvar stored-todos-ui
  (ui-bind stored-todos (store-ref (list-of todo-item))
    (store-bind "todo/items" '(desc target status) 0 5)))

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
internal watches, their GC roots, dynamically allocated instruction arrays,
and template-owned literal strings.

The first implementation uses these compile-time capacities per app:

| Resource | Capacity | Failure behaviour |
| --- | ---: | --- |
| UiRefs | 16 | declaration signals `ui-capacity` |
| record types | 8 | declaration signals `ui-capacity` |
| templates, including lists | 12 | declaration signals `ui-capacity` |
| instructions per template | dynamically allocated; no API cap | declaration fails atomically if native allocation fails |
| mounts | 8 | `ui-mount` signals `ui-capacity` |
| StoreRef watch entries owned by lists | 16 (two per store list) | list declaration signals `ui-capacity` |
| visible rows per list (`:limit`) | 5 | declaration rejects the limit |
| list state text | 48 bytes in the first implementation | declaration rejects the text |
| `ui-text` literal text | exact-length dynamic allocation; no API cap | declaration fails atomically if native allocation fails |
| temporary formatted field output | 32 bytes | formatter clips and reports a render error |

Fixed capacities in this table are hard bounds, not starting allocation sizes.
`ui-template` is the exception: it validates a complete declaration into
temporary, dynamically owned storage, then publishes the immutable template in
one step. Failure releases every candidate descriptor and string, leaving the
existing UI unchanged.

Removing the two per-template caps does not make template resource use free or
literally infinite. Developers must keep declarations moderate: instruction
descriptors consume native memory, literal bytes consume native memory, and
each instruction adds work to every rendered row. Available memory, allocation
fragmentation, uLisp's ability to hold the declaration, and the source/store
transport can still reject or prevent a very large form. Platform profiles may
diagnose or budget those risks, but they must not introduce a public semantic
limit on instruction count or `ui-text` literal length.

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
  an alternative to `chop`), and how render errors appear visually;
- whether list `:offset` later becomes a bounded UiRef for scrolling;
- capacity values after Browser and reference-MCU measurements, especially
  watch-table cost and workspace-lock duration; and
- StoreRef conflict/retry/deletion semantics and public watch-handle removal.
