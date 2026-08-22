# BoundQueueStore API

Draft recorded on 14 August 2026.

## Public API

| Function/form | Purpose |
| --- | --- |
| [`store-create`](#store-create) | Create an exact store. |
| [`store-list`](#store-list) | List store descriptors. |
| [`store-meta`](#store-meta) | Read store metadata. |
| [`store-delete`](#store-delete) | Delete one exact store. |
| [`store-get`](#store-get) | Read a bounded row snapshot. |
| [`store-bind`](#store-bind) | Bind a bounded live row window. |
| [`store-status`](#store-status) | Read a StoreRef operation status. |
| [`store-error`](#store-error) | Read a StoreRef error. |
| [`store-value`](#store-value) | Read a ready StoreRef's operation result. |
| [`store-row-count`](#store-row-count) | Count rows in a ready row-result StoreRef. |
| [`store-row-at`](#store-row-at) | Select a row by bounded-window index. |
| [`store-row-id`](#store-row-id) | Read a StoreRowRef's stable ID. |
| [`store-row-revision`](#store-row-revision) | Read a StoreRowRef's revision. |
| [`store-row-status`](#store-row-status) | Read a StoreRowRef status. |
| [`store-row-error`](#store-row-error) | Read a StoreRowRef error. |
| [`store-row-field`](#store-row-field) | Read or update one schema field of a StoreRowRef. |
| [`store-row-add`](#store-row-add) | Add a row. |
| [`store-row-delete`](#store-row-delete) | Delete a bound row. |
| [`store-row-delete-id`](#store-row-delete-id) | Delete a row by stable ID. |
| [`store-ref-watch`](#store-ref-watch) | Watch a StoreRef. |
| [`store-row-ref-watch`](#store-row-ref-watch) | Watch one StoreRowRef. |
| [`store-ref-watch-rows`](#store-ref-watch-rows) | Watch the rows of a bound StoreRef. |

## Status

This document collects the current proposed uLisp API for the exploratory
BoundQueueStore model described in
[Discussion-QueueMetaphors.md](Discussion-QueueMetaphors.md). It is a working
API specification, not yet a committed SilOS interface. Examples use
JSON-like notation where the exact uLisp representation remains undecided.

The companion [API-BoundQueueMQTT.md](API-BoundQueueMQTT.md) applies the same
asynchronous Ref conventions to MQTT while preserving the different semantics
of immutable network messages.

## Contents

1. [Status](#status)
2. [Model](#model)
3. [Names](#names)
4. [Common values](#common-values)
5. [Store management](#store-management)
6. [Reading and binding rows](#reading-and-binding-rows)
7. [Changing rows](#changing-rows)
8. [Watches](#watches)
9. [UI composition](#ui-composition)
10. [Deferred extensions](#deferred-extensions)

## Model

BoundQueueStore exposes storage through asynchronous operations and live,
two-way bindings. Queues, task messages, and native buffers are implementation
details; application code uses `store-*` functions.

Every asynchronous operation returns a **StoreRef** immediately. A live
`store-bind` result contains **StoreRowRefs**, which can remain synchronised with
stored rows. Only the uLisp task invokes Lisp code or changes these values.

## Names

A store name is a flat string containing `/`-separated segments:

```text
todo/items
todo/prefs
alarm/items
```

Rules currently proposed:

- no leading or trailing `/`;
- the first segment identifies the application or owner;
- `/` provides prefix grouping only; parent stores do not need to exist;
- creation and deletion always use an exact name;
- lowercase letters, digits, `-`, and `/` are allowed; and
- empty segments, `.` and `..` are not allowed.

## Common values

### StoreRef

```text
{
  meta: {
    operation: bind,
    status: pending,
    error: nil
  },
  value: nil
}
```

`meta.operation` is immutable and identifies the request:

```text
create | list | meta | delete | get | bind | add-row | delete-row
```

The basic StoreRef status progression is:

```text
pending -> ready
pending -> error
```

The meaning of `value` depends on the operation.

### StoreRowRef

```text
{
  meta: {
    id: 41,
    revision: 7,
    status: ready,
    error: nil
  },
  value: {
    desc: "Buy milk",
    target: 20260820,
    status: "to do"
  }
}
```

System metadata and application data are separated to avoid field-name
collisions. A compact fixed-layout representation is expected internally and
is not public API. Application code must use the typed StoreRef and
StoreRowRef accessors below, rather than traversing `meta` or `value` with a
generic field operation.

## Store management

### `store-create`

```lisp
(store-create name kind [schema])
```

Creates an exact store name. Initial kinds are `rows` and `blob`; a row schema
describes its fixed fields.

```lisp
(store-create "todo/items"
  'rows
  '((desc string) (target datetime) (status string)))

(store-create "todo/icon" 'blob)
```

Returns a StoreRef with `operation: create`.

### `store-list`

```lisp
(store-list [prefix])
```

Lists exact store descriptors whose names begin with the optional prefix.

```lisp
(store-list "todo/")
```

Its ready value is conceptually:

```text
[
  {name: "todo/items", kind: rows, count: 12, revision: 8},
  {name: "todo/prefs", kind: rows, count: 3, revision: 2}
]
```

Returns a StoreRef with `operation: list`.

### `store-meta`

```lisp
(store-meta name)
```

Returns store-level metadata through a StoreRef with `operation: meta`.

### `store-delete`

```lisp
(store-delete name)
```

Deletes one exact store. It never recursively deletes a prefix. Returns a
StoreRef with `operation: delete`. The behaviour of existing live bindings to a
deleted store remains to be specified.

## Reading and binding rows

### `store-get`

```lisp
(store-get name fields start count)
```

Returns an independent, bounded snapshot through a StoreRef with `operation:
get`. `start` plus `count` avoids inclusive/exclusive end ambiguity.

### `store-bind`

```lisp
(store-bind name fields start count)
```

Immediately returns a StoreRef with `operation: bind` and `status: pending`.
When ready, its value is an array of live StoreRowRefs:

```lisp
(defvar todos-ref
  (store-bind "todo/items" '(desc target status) 0 10))
```

The StoreRef's metadata describes the request, errors, and result membership.
Each StoreRowRef has its own stable ID, revision, status, error, and value.
Record identity never depends on the row's current array index.

### Store accessors

These accessors apply to a StoreRef returned by an operation. They make its
asynchronous state explicit and do not expose its internal representation.

#### `store-status`

```lisp
(store-status ref)
```

Returns the StoreRef's status, initially `pending` and subsequently `ready` or
`error`. Code must check for `ready` before reading an operation result.

#### `store-error`

```lisp
(store-error ref)
```

Returns the operation error when `(store-status ref)` is `error`; otherwise it
returns `nil`. It does not itself imply that a result is ready.

#### `store-value`

```lisp
(store-value ref)
```

Returns the operation-specific result only when the StoreRef is `ready`.
Calling it for a `pending` or `error` StoreRef is a state error. The result
shape depends on the operation: for example, `store-list` yields descriptors,
`store-row-add` yields one StoreRowRef, `store-get` yields an independent
bounded snapshot, and `store-bind` yields a bounded live row result. The
snapshot's public record representation remains to be specified.

#### `store-row-count`

```lisp
(store-row-count ref)
```

Returns the number of StoreRowRefs in a ready `store-bind` StoreRef. Calling
it for a non-ready StoreRef, or for a ready ref whose operation does not yield
a live row result, is a state/type error.

#### `store-row-at`

```lisp
(store-row-at ref index)
```

Returns the StoreRowRef at zero-based `index` in a ready `store-bind`
StoreRef.
`index` selects the current bounded result window, not a record identity;
callers must use `store-row-id` when they need a stable identity. A non-ready
or non-row result is a state/type error, and an index below zero or greater
than or equal to `(store-row-count ref)` is a bounds error.

For example, the first bound row's description is read without exposing any
raw list or record traversal:

```lisp
(store-row-field (store-row-at todos-ref 0) 'desc)
```

### Store row accessors

These accessors apply to a StoreRowRef. A row can be `ready`, `saving`,
`deleting`, `deleted`, or `error` as its lifecycle changes. Its application
fields are readable only while it is `ready` or `saving`; reading a deleted,
deleting, or error row is a state error unless an accessor says otherwise.

#### `store-row-id`

```lisp
(store-row-id row)
```

Returns the row's immutable stable ID. It remains available for a deleted row
so callers can identify its tombstone.

#### `store-row-revision`

```lisp
(store-row-revision row)
```

Returns the row's current revision. A successful write advances it. The last
known revision remains available for a deleted row.

#### `store-row-status`

```lisp
(store-row-status row)
```

Returns the row lifecycle status. Callers must use it to decide whether a
field can be read or changed.

#### `store-row-error`

```lisp
(store-row-error row)
```

Returns the row error when `(store-row-status row)` is `error`; otherwise it
returns `nil`. It remains available without allowing field access to an error
row.

#### `store-row-field`

```lisp
(store-row-field row field)
```

Returns application field `field` from a `ready` or `saving` StoreRowRef.
`field` must be a field declared by that row's store schema; an unknown field
is a schema error. It never reads system metadata such as ID, revision,
status, or error, which have their own accessors.

## Changing rows

### Bound update

`store-row-field` has corresponding `setf` setter semantics. Setting a schema
field on a `ready` row updates it optimistically and queues the storage write:

```lisp
(setf
  (store-row-field row 'status)
  "done")
```

The setter rejects a row that is not `ready`, an unknown schema field, or a
value that does not satisfy the schema. `setf` is deliberately supported here:
the operation is a row-field update, not mutation of the underlying
representation, and the setter is where the asynchronous write and validation
semantics are defined.

The row state progresses as follows:

```text
ready -> saving -> ready
ready -> saving -> error
```

A successful write advances its revision. Conflict and failed-write recovery
policies remain open.

### `store-row-add`

```lisp
(store-row-add name record)
```

Adds a row without assigning it a positional ordering. It returns a StoreRef
with `operation: add-row`; when ready, its value is the newly assigned
StoreRowRef. Matching bound collections acquire the new row.

### `store-row-delete`

```lisp
(store-row-delete row-ref)
```

Deletes a bound row. The StoreRowRef follows:

```text
ready -> deleting -> deleted
ready -> deleting -> error
```

After success it retains tombstone metadata while its value becomes nil. Its
parent StoreRef subsequently removes it from the result array.

### `store-row-delete-id`

```lisp
(store-row-delete-id name id)
```

Deletes a row without first binding it. Returns a StoreRef with `operation:
delete-row`.

## Watches

Watch callbacks run only on the uLisp task after their corresponding value has
changed.

### `store-ref-watch`

```lisp
(store-ref-watch store-ref
  (lambda (live old-value)
    ...))
```

Watches top-level request metadata and result membership. It does not fire for
every field change inside every row.

### `store-row-ref-watch`

```lisp
(store-row-ref-watch row-ref
  (lambda (live-row old-value)
    ...))
```

Watches one StoreRowRef.

### `store-ref-watch-rows`

```lisp
(store-ref-watch-rows store-ref
  (lambda (live-row old-value)
    ...))
```

Watches all rows in a bound result and automatically follows rows entering or
leaving it.

The live argument remains storage-backed; `old-value` is a non-live snapshot.
Snapshot size must be bounded. Insertion/deletion event detail and watch
removal/lifetime APIs remain open.

## UI composition

StoreRefs and StoreRowRefs are uLisp-visible values and may be exposed through
SilOS UI bindings:

```text
storage <-> StoreRef/StoreRowRefs in uLisp -> UI binding -> template
```

Templates can display request state, errors, row metadata, and row values. UI
input may deliberately modify a StoreRowRef, producing the two-way write path
above.

The proposed [UI API](API-UI.md) specifies the bounded UiRef, template, and
fixed-window list contract for displaying these StoreRefs and StoreRowRefs.

## Deferred extensions

- `store-search`, returning stable IDs matching a future query language;
- binary range reads and writes and their buffer representation;
- rename, recursive deletion, and hierarchical permissions;
- ordering or insertion-before semantics;
- cancellation, retry, timeout, and watch-removal APIs;
- conflict handling and failed optimistic-write recovery; and
- exact schemas, field types, limits, and persistence guarantees.
