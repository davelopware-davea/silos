# Shell API

Draft recorded on 18 August 2026.

## Status

This document collects the current proposed boundary between the SilOS Shell,
uLisp applications, and the native runtime. It summarises [uLisp Applications
and the Shell](Discussion-uLisp-Applications-and-Shell.md) and [Shell
UI](Discussion-Shell-UI.md). It is a working API sketch, not a committed SilOS
interface.

The companion [BoundQueueStore](API-BoundQueueStore.md) and
[BoundQueueMQTT](API-BoundQueueMQTT.md) APIs define the live Refs through
which an app normally receives storage and network changes.
The proposed [UI API](API-UI.md) defines the bounded templates, UiRefs, lists,
and mounts that the Shell renders from those live values.

| Public form | Purpose |
| --- | --- |
| [`shell-app-register`](#shell-app-register) | Register an app descriptor with the Shell in restricted declaration mode. |
| [`shell-app-on-event`](#shell-app-on-event) | Register the active app's bounded event handler. |
| [`shell-event-type`](#shell-event-type) | Return the type symbol at the start of an app event. |
| [`shell-request-poke`](#shell-request-poke) | Request one later-turn application-defined event. |
| [`shell-module-require`](#shell-module-require) | Load a shared system API module once. |
| [`shell-module-import`](#shell-module-import) | Load an app-private source module as a factory or bounded export. |

## Contents

1. [Model](#model)
2. [App index and declaration source](#app-index-and-declaration-source)
3. [`shell-app-register`](#shell-app-register)
4. [Loading and starting an app](#loading-and-starting-an-app)
5. [Events, handlers, and Refs](#events-handlers-and-refs)
6. [Reload](#reload)
7. [Shared and private source modules](#shared-and-private-source-modules)
8. [Ownership and memory](#ownership-and-memory)
9. [Deferred decisions](#deferred-decisions)

## Model

The Shell and its UI task are native code. They own the display, input
normalisation, app catalogue, layout, and Shell-owned descriptor/template
storage. The uLisp task owns evaluation, the uLisp workspace, application
closures, Refs, and watch invocation. Queues carry only bounded messages and
plain descriptors across that boundary.

The first prototype uses one active uLisp application. A later dispatcher may
retain several app instances in one workspace, but they do not execute
concurrently.

```text
Shell UI task <-> bounded commands/events <-> uLisp task
      owns display                         owns Lisp state and handlers
```

## App index and declaration source

The Shell needs a small known app index. Initially it may be compiled into the
platform artifact; later it may be a persisted system store. An index entry
contains a stable app ID and the logical store name for that app's declaration
source:

```text
todo -> apps/todo/app.lisp
```

The declaration source is separate from executable source modules:

```text
apps/todo/app.lisp       app characteristics and entry-store ID
apps/todo/src/main       executable entry source
apps/todo/src/model      app-private source module
```

These are logical store names. A FAT backend may map them to files; IndexedDB
or SRAM may not. The Shell does not infer apps by scanning arbitrary stores.

To catalogue an app, the Shell asks the uLisp task to read only `app.lisp` in a
restricted **describe** mode. Describe mode permits one `shell-app-register`
form with literal bounded values. It rejects `shell-module-require`,
`shell-module-import`, `defun`, and ordinary
application side effects. Thus the Shell learns presentation requirements
without loading executable code.

## `shell-app-register`

```lisp
(shell-app-register
  :name "To-do"
  :ideal-width 24
  :ideal-height 10
  :entry "apps/todo/src/main")
```

`shell-app-register` is a native uLisp primitive. During describe mode it
validates the declaration and emits a fixed-size native descriptor associated
with the current index app ID:

```text
{
  id: "todo",
  name: "To-do",
  ideal_width: 24,
  ideal_height: 10,
  entry: "apps/todo/src/main"
}
```

The Shell owns this descriptor. It does not look up a common Lisp function,
retain a Lisp pointer, or load the entry source merely to show the app. Field
limits and the exact layout values remain for the prototype to establish.

## Loading and starting an app

When the user starts an app, the Shell sends its app ID and entry-store ID to
the uLisp task. The task clears/prepares the active app workspace, streams the
entry store through the uLisp reader using a bounded input buffer, and evaluates
it.

The entry source initialises state and finishes by registering an app instance.

### `shell-app-on-event`

```lisp
(shell-app-on-event
  (lambda (event)
    ;; Handle one bounded event and return.
    event))
```

`shell-app-on-event` associates the closure with the current app ID and retains
it as a uLisp GC root. This is a working proposal; the exact callback
registration form remains open. The entry must return after setup; it must not
take ownership of the uLisp task with a permanent `(loop ...)`.

## Events, handlers, and Refs

The uLisp task is the event loop. The Shell, storage, MQTT, and timing services
send bounded events. Each event identifies an app where relevant. The task
invokes that app's handler or updates a live Ref and runs its watches. Each
handler/watch performs bounded work and returns.

```text
Shell input / storage / MQTT / timer
  -> event or Ref update in uLisp task
  -> short app handler or watch
  -> next queued event
```

StoreRefs, StoreRowRefs, UiRefs, and later MQTT Refs provide the normal
reactive work items. A change may enqueue more work, but must not create an
unbounded synchronous watch chain. A FreeRTOS yield cannot switch to another
app handler inside the same uLisp task, so multi-app fairness begins with short
handlers and a bounded event queue.

### Event values

Every event delivered to an app is a proper list with this shape:

```lisp
(event-type parameter ...)
```

`event-type` is a symbol. Its parameters and their positions are defined by
that event type. Apps may use ordinary list operations such as `nth` to read
them.

### `shell-event-type`

```lisp
(shell-event-type event)
```

`shell-event-type` returns the first item in a valid app event. It makes the
role of that item explicit at call sites and rejects values that are not proper
event lists beginning with a symbol.

### Minimal lifecycle and later-turn event contract

The first Shell event is deliberately small and explicit. After a successful
`shell-app-on-event` registration, the Shell enqueues exactly one
`(shell-app-initialise)` event for that app. It is delivered on a later uLisp
event turn, never by calling the handler synchronously from
`shell-app-on-event`. A
stopped or reloaded app does not receive an event from its former generation;
a new successful registration receives its own one `(shell-app-initialise)` event.

### `shell-request-poke`

An app may ask the Shell to schedule one later-turn event with:

```lisp
(shell-request-poke type arg ...)
```

This is a **general**, variadic request. `type` must be a symbol, but the Shell
assigns no meaning to its value or to the remaining arguments. On success the
Shell copies the complete event across the boundary and later calls the same
app handler with a fresh event of this shape:

```lisp
(type arg ...)
```

The poke is only the internal mechanism used to queue this later turn; it is
not part of the app-facing event value. Every event type and parameter
convention supplied through `shell-request-poke` is application owned. In
particular `init` and numeric stages have no native meaning and must not become
lifecycle special cases.

For example, `(shell-request-poke 'init 1)` later delivers `(init 1)`. For that
event, `(shell-event-type event)` returns the symbol `init` and `(nth 1 event)`
returns the stage number `1`; `nth` uses zero-based positions.

The initial bounded profile accepts only serialisable values: `nil`, booleans,
integers, symbols, strings of at most 48 UTF-8 bytes, and proper nested lists
of those values.  Lists have at most depth 4, at most 8 elements per list, and
at most 16 values in the whole payload.  Symbols have at most 16 bytes.  The
Shell stores a bounded flat native copy, then reconstructs fresh Lisp values for
delivery.  It never puts a Lisp heap pointer, closure, Ref, array, or other
runtime object in a native queue.

There is one outstanding poke per app.  A second request before delivery fails
deterministically with `poke-pending`; it does not overwrite or merge the
first payload.  Accepted pokes retain FIFO order with other Shell events and
are always delivered after the requesting handler returns.  If the bounded
request or event queue is full, the request fails deterministically without
leaving a pending flag.  Stopping or reloading an app removes queued lifecycle
and poke events for its generation and clears its pending-poke state before
releasing its handler and other roots.

## Reload

Persistent source and the loaded uLisp program are independent. Editing source
does not alter the current workspace. Shell **Reload app** is the explicit
boundary:

1. wait for writes to the app's source store, or reject Reload as busy;
2. stop the app and release its Shell templates, UiRefs, StoreRefs, watches,
   subscriptions, and rooted callbacks;
3. clear/recreate the uLisp workspace and module state; and
4. stream and evaluate the current entry source and its dependencies.

The smallest path does not retain the old running app while trying the new one.
A source/load failure enters a small native recovery Shell that reports the
error and can return to an editor. Rollback needs additional workspace or image
memory and is deferred.

## Shared and private source modules

Native SilOS primitives are registered in uLisp's built-in table at the C/C++
build. uLisp is interpreted, so application source does not import C headers or
compile against C signatures. It calls built-in names directly; uLisp checks
builtin arity at runtime.

Two provisional source-loading forms have distinct roles:

```lisp
(shell-module-require 'silos-store)               ; shared system API module
(shell-module-import "apps/todo/src/model")       ; app-private source module
```

### `shell-module-require`

`shell-module-require` loads a shared system library once into deliberately
global API names. It uses a bounded module catalogue mapping library names to
stores, and must detect cycles and avoid repeat loads.

### `shell-module-import`

`shell-module-import` is a proposed native primitive for app-private source.
Its store evaluates to a factory closure or bounded export value instead of
top-level global `defun` definitions. The entry keeps that value in a lexical
binding, so separate apps may use the same helper names without collision. The
runtime may cache one factory per store ID, but each app instantiates private
state separately.

## Ownership and memory

- The Shell UI task never traverses or retains pointers into the uLisp heap.
- The uLisp task alone evaluates Lisp, updates Refs, invokes watches, and calls
  application closures.
- Native registries retaining a uLisp binding or closure must be visible to
  garbage collection and removed on app reload.
- Source is streamed from storage with a bounded reader buffer; it is not kept
  in a second native source copy.
- One workspace has no user-defined namespaces. App-private functions and
  state use lexical closures; native builtins and shared system API names are
  reserved globals.

## Deferred decisions

- descriptor fields, sizes, and validation rules;
- app-index representation, update, corruption recovery, and capacity;
- restricted declaration-reader implementation and error reporting;
- exact event vocabulary, handler registration, ordering, and fairness;
- lifecycle states, stop/close policy, and release/cancellation APIs;
- source-editor, module-factory, lexical-export, and import-error semantics;
- long-running/background work and whether it needs continuations or separate
  uLisp workspaces; and
- package/namespace support if closure-based isolation becomes insufficient.
