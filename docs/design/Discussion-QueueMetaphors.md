# Discussion: Queue Metaphors

Conversation begun on 14 August 2026; updated on 15 August 2026.

## Purpose

This note records exploratory queue-and-binding models for SilOS storage and
MQTT networking. It is discussion material, not an agreed design or a
replacement for the authoritative SilOS plan.

## BoundQueueStore model

The proposed architecture is called the **BoundQueueStore model**. It
supersedes the earlier, simpler **QueueStore model** in this discussion.

QueueStore treated storage as an asynchronous request-and-response service.
BoundQueueStore retains that mechanism but makes a live **StoreRef** the primary
result: a uLisp-visible value that represents storage request state and, once
available, the requested stored data. It can remain synchronised with that data
in both directions. `store-bind` is the operation that creates a StoreRef; the
operation and resulting value deliberately have different names.

Queues are an implementation detail. Developer-facing code uses the **Store
API** and does not manipulate queues directly.

The current API sketch is collected separately in
[API-BoundQueueStore.md](API-BoundQueueStore.md).

## StoreRef shape

A binding is returned immediately. Even a request selecting one row
provisionally returns a collection-level StoreRef whose value will be an array
of rows:

```lisp
(defvar todos-ref
  (store-bind "todos" '(desc target status) 0 2))
```

The StoreRef has metadata for the request and binding as a whole. Each returned
row is a live **StoreRowRef** with separate metadata for that record:

```text
{
  meta: {
    operation: bind,
    status: ready,
    error: nil,
    count: 2
  },
  value: [
    {
      meta: {id: 41, revision: 7, status: ready, error: nil},
      value: {desc: "Buy milk", target: 20260820, status: "to do"}
    },
    {
      meta: {id: 58, revision: 3, status: saving, error: nil},
      value: {desc: "Call bank", target: nil, status: "done"}
    }
  ]
}
```

Before the request completes, the StoreRef has `pending` status and an empty or
nil value. Collection metadata describes request state, errors, and result
membership. Row metadata contains the stable record ID, revision, and that
row's binding/write state. Keeping application fields below `value` avoids a
collision between, for example, a to-do's `status` and `meta.status`.

Every StoreRef has an immutable `meta.operation` identifying what produced it,
such as `create`, `list`, `meta`, `delete`, `get`, `bind`, or `add-row`.
`meta.status` independently describes its current progress.

The proposed generic `field` operation exposes this without requiring a
different accessor for every property:

```lisp
(field (field todos-ref 'meta) 'status)  ; pending or ready
(field todos-ref 'value)                 ; nil or StoreRowRef array
```

This JSON-like form describes language semantics only. A compact fixed-layout
native object is likely more appropriate than association lists internally.

Bindings should identify records by stable record ID rather than by a mutable
row index. Ranged queries may still use indexes when returning snapshots or
collections.

## Two-way synchronisation

Changing a field of a StoreRowRef can update its visible value immediately and
queue a corresponding storage write:

```lisp
(defvar row1 (aref (field todos-ref 'value) 0))

(setf
  (field (field row1 'value) 'status)
  "done")
```

The binding metadata exposes progress and failure:

```text
pending -> ready
ready -> saving -> ready
ready -> saving -> error
```

A successful commit updates the binding's revision or `changed` value. External
changes to the same stored record are delivered back to the uLisp task and
update the binding. Revision numbers are needed to detect conflicts and avoid
treating a binding's own write notification as a new edit.

The precise failure policy remains open: an unsuccessful optimistic write could
retain the edited value as dirty data, restore the last committed value, or
retain both.

## Store and row lifecycle

Stores use flat path-like names with `/` as a grouping convention, for example
`"todo/items"` and `"todo/prefs"`. These are names and prefixes, not directories.
The initial management operations are `store-create`, `store-list`,
`store-meta`, and `store-delete`. Creation and deletion affect exact names;
prefix deletion is not implicit.

Rows can be added without implying a persistent positional order:

```lisp
(store-row-add "todo/items" new-item)
```

The operation StoreRef initially reports `operation: add-row` and `status:
pending`; on success its value is the newly assigned StoreRowRef. Relevant live
collection bindings then acquire that row.

A bound row can be deleted directly:

```lisp
(store-row-delete row1)
```

Its state progresses from `ready` through `deleting` to `deleted` or `error`.
A deleted StoreRowRef retains tombstone metadata, including its ID and final
revision, while its value becomes nil. Its parent collection subsequently
removes it and notifies the relevant watches. `store-row-delete-id` is the
direct alternative when no StoreRowRef is available.

## Watching StoreRefs and rows

Three watch scopes are proposed:

```lisp
(store-ref-watch todos-ref
  (lambda (live old-value)
    ...))

(store-row-ref-watch row1
  (lambda (live-row old-value)
    ...))

(store-ref-watch-rows todos-ref
  (lambda (live-row old-value)
    ...))
```

`store-ref-watch` observes top-level request metadata and result membership, but
does not fire for every field change within every row. `store-row-ref-watch`
observes one StoreRowRef. `store-ref-watch-rows` observes changes to all rows in
the collection and automatically follows rows entering or leaving the result
set.

The live parameter remains connected to storage; `old-value` is a non-live
snapshot. Watches run on the uLisp task after the corresponding value changes.
Old-value snapshots must have bounded memory costs, especially at collection
scope. The precise snapshots and events supplied for row insertion and deletion
remain open.

## Composition with UI binding

A StoreRef is a uLisp-visible value and can also be exposed through the
existing UI-binding idea:

```text
storage <-> StoreRef/StoreRowRefs in uLisp -> UI binding -> template
```

A template can display `pending`, an error, or fields of the ready value. When
storage completes a request or reports a later change, the uLisp task updates
the appropriate StoreRef or StoreRowRef and the UI observes it on a subsequent
refresh. The storage and UI binding mechanisms remain separate but compose
through the uLisp value.

## Other Store API operations

The live-binding model need not replace every storage operation:

- `store-bind` returns a StoreRef containing live, potentially two-way
  StoreRowRefs;
- `store-get` returns an independent snapshot;
- `store-meta` requests object metadata; and
- binary reads and writes may use stable native buffers rather than record
  bindings.

A possible future `store-search` operation could return the stable record IDs
matching a query. Its query language, indexing, and result-binding behaviour are
deliberately deferred.

A storage ID such as `"todos"` plays a role somewhat like a filename, but names
a stored data object rather than an opened stream. A structured object may
contain fixed-layout records; a binary object may contain uninterpreted bytes.

## MQTT networking extension

The same asynchronous Ref and watch conventions could support an intentionally
opinionated networking API that exposes MQTT only to uLisp applications. TCP,
UDP, HTTP, and MQTT client internals would remain platform implementation
details.

This MQTT application of the approach is provisionally called
**BoundQueueMQTT**.

The current MQTT API sketch is collected separately in
[API-BoundQueueMQTT.md](API-BoundQueueMQTT.md).

Publishing returns an operation Ref whose status records progress:

```lisp
(mqtt-publish "chat/outbox/alice" "Hello" 1)
```

```text
pending -> queued -> sent -> acknowledged
                         -> error
```

The exact progression depends on MQTT QoS. Subscribing binds a bounded window
of immutable messages rather than binding each message independently:

```lisp
(defvar inbox
  (mqtt-subscribe "chat/inbox/+" 'fifo 5))
```

Its conceptual shape is:

```text
{
  meta: {
    operation: subscribe,
    status: ready,
    mode: fifo,
    limit: 5,
    waiting: 3,
    dropped: 0
  },
  value: [up to five immutable MessageRefs]
}
```

A MessageRef contains MQTT metadata such as topic, QoS, retained flag, and a
local sequence or receipt identity, plus the immutable payload. It does not
have the two-way row and revision semantics of a StoreRowRef.

New arrivals do not change the current window while application code may still
be processing it. Instead, `meta.waiting` counts messages received since the
window was last updated, and a watch reports that count:

```lisp
(mqtt-ref-watch inbox
  (lambda (live waiting)
    ...))
```

The application advances the window explicitly when ready:

```lisp
(mqtt-ref-update inbox)
```

Two initial modes cover different uses:

- `fifo` consumes the current window and loads the next oldest messages, making
  it suitable for chat, commands, and other ordered work;
- `latest` replaces the window with the newest messages and skips superseded
  arrivals, making it suitable for telemetry and dashboards.

The subscription retains only the current window plus a bounded backlog.
Transient `latest` subscriptions can use a small RAM ring, potentially only one
pending payload. Reliable chat or command subscriptions can spool directly into
a BoundQueueStore store, ideally before acknowledging a QoS 1 message. The first
SilOS chat use case would most likely use a FIFO window backed by its existing
durable message store rather than an unbounded MQTT inbox in RAM.

MQTT retained topics support a distinct, closer form of live binding:

```lisp
(mqtt-bind "house/temperature")
```

An MqttValueRef represents the latest retained value and can optionally be
two-way, with a local update publishing a new retained value. This behaviour is
appropriate for state topics, but must not be applied to ordinary immutable
event messages.

## Scheduling and ownership

Only the uLisp task executes Lisp code or modifies the uLisp workspace. The
storage and MQTT tasks perform requests and send completion, change, or arrival
messages; they do not invoke Lisp closures or mutate uLisp-visible Refs
directly.

The uLisp task receives each message and safely updates the associated binding.
SilOS therefore needs a small, bounded binding table whose handles are visible
as roots to uLisp garbage collection. Releasing a binding removes its storage
or MQTT subscription and table entry.

Bulk data need not be copied into queue messages. Messages can carry bounded
descriptors, buffer handles, status, revisions, and correlation IDs. Stable
native buffers may carry large or binary results; messages must not expose
unowned pointers into the uLisp workspace.

## Questions to explore

- What are the smallest StoreRef, StoreRowRef, and `field` representations?
- How are bindings released, refreshed, cancelled, or retried?
- What happens when the bounded binding or request table is full?
- What optimistic-write and conflict policy is simplest and least surprising?
- Should stored objects be record collections, binary blobs, or both?
- What bounded MQTT backlog and overflow policies apply to each QoS level?
- When must a durable inbound MQTT message be committed before acknowledgement?
- Which Ref machinery can storage and MQTT share without giving immutable
  messages inappropriate StoreRowRef semantics?
