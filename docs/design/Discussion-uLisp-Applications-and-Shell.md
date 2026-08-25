# Discussion: uLisp Applications and the Shell

Conversation begun on 18 August 2026; general approach adopted on 25 August 2026.

## Purpose

This note records the reasoning behind how the SilOS Shell discovers, loads,
and runs uLisp applications without assuming a conventional filesystem. The
Browser prototype validated store-backed manifests, source loading, one
uLisp-owned workspace, and Shell lifecycle delivery; that general approach is
now committed in the authoritative [SilOS plan](SilOS_PLAN.md). Details and
unimplemented extensions below remain refinement material.

**Related:** [Shell UI](Discussion-Shell-UI.md) defines the Shell's current
input, layout, and framebuffer responsibilities. [Variable Binding and
Templates](Discussion-VariableBindingAndTemplates.md) defines how an app's
uLisp values and templates reach that Shell. [Queue Metaphors](Discussion-QueueMetaphors.md)
defines the storage model, including logical store names and binary objects.
[Shell API](API-Shell.md) collects the current working API sketch.

## Contents

1. [Starting question](#starting-question)
2. [What must exist at boot](#what-must-exist-at-boot)
3. [No filesystem is required](#no-filesystem-is-required)
4. [Discovery and registration options](#discovery-and-registration-options)
5. [Recommended prototype direction](#recommended-prototype-direction)
6. [Loading and running an app](#loading-and-running-an-app)
7. [App declarations and callbacks](#app-declarations-and-callbacks)
8. [App execution and the shared uLisp task](#app-execution-and-the-shared-ulisp-task)
9. [Storage roles](#storage-roles)
10. [Storage backends and CRUD](#storage-backends-and-crud)
11. [uLisp source as a store](#ulisp-source-as-a-store)
12. [uLisp modules and imports](#ulisp-modules-and-imports)
13. [Questions to resolve during implementation](#questions-to-resolve-during-implementation)

## Starting question

How can a mainly native Shell find and run uLisp applications, and how can
applications become visible to it, while avoiding a normal filesystem as much
as possible? Can the storage concept already being discussed hold applications
as well as their data?

## What must exist at boot

Some small, trusted boot path is unavoidable, but it need not resemble a
general-purpose operating-system filesystem or process loader:

```text
firmware / Browser module
  -> native runtime and Shell UI owner
  -> application catalogue
  -> selected uLisp application image
  -> app entry function and Shell-facing descriptors
```

The first two stages are native C++ substrate work: platform start-up,
FreeRTOS/uLisp start-up where used, display/input transport, bounded queues,
and the Shell UI task. This fits the existing separation in which the Shell UI
task owns the framebuffer and templates, while the uLisp task owns evaluation
and the uLisp workspace.

"Shell" should not therefore mean that all its behaviour must be native. The
native portion should be only the reliable host and mechanism. Portable policy
such as an app sequence, labels, and perhaps later menus can be written in the
SilOS language once the bootstrap can load it. This also follows the plan's
goal of putting portable system behaviour in the language where practical.

An app cannot be solely responsible for making itself discoverable: if it is
not already known, nothing knows to load it and execute its registration code.
Self-registration is useful *after* loading, but needs a catalogue, built-in
list, or other root of trust before it.

## No filesystem is required

The Shell needs stable names, metadata, and a way to fetch bytes. It does not
need paths, directories, open file handles, a current working directory, or a
POSIX-compatible API.

The BoundQueueStore discussion already proposes flat, path-like storage names
such as `"todo/items"`; the slash is grouping syntax, not a directory. The
same storage substrate could expose immutable binary objects, for example:

```text
system/apps/catalogue
system/apps/todo/image
system/apps/todo/manifest
todo/items
todo/prefs
```

On an MCU these names may resolve to a compact flash partition, append-only
records, or a small object store. In the Browser they may resolve to embedded
assets, IndexedDB, or another Browser-backed implementation. Those choices
can share the same logical object interface. A physical filesystem might still
be convenient during development, package installation, backup, or an x86
host implementation, but it need not be part of the application or Shell
model.

This distinction matters: application *data* needs ordinary durable mutation,
whereas executable application *images* normally need bounded reads, version
identity, validation, and an atomic replacement story. One generic byte-store
implementation might serve both, but their policies should not be conflated.

## Discovery and registration options

### 1. Build-time catalogue

The firmware or Browser build embeds an array of app descriptors. Each entry
names an app, gives its entry function or embedded uLisp image, and supplies
small Shell-visible metadata.

```text
{ id: "todo", label: "To-do", image: embedded-todo, entry: app-main }
```

This is the smallest and most predictable option. It has no runtime discovery
failure, needs no mutable storage before the Shell appears, and is a strong fit
for the first prototype. Its limitation is that adding or updating an app
requires producing and installing a new system build or image.

### 2. Persistent Shell catalogue

The Shell has a known, fixed catalogue object such as `"system/apps/catalogue"`.
It contains bounded descriptors with an app ID, label, image-object ID,
version, enabled flag, and perhaps the application data-store prefix. At boot
the native bootstrap loads this one known object, validates it, and presents
its entries to the Shell.

This permits installed applications without directory scanning. It needs a
fallback built into firmware, since a corrupted, absent, or incompatible
catalogue must not make the device unusable. It also introduces update,
validation, recovery, and capacity policies that are outside the current
prototype scope.

### 3. Fixed-slot catalogue

Instead of arbitrary object lookup, reserve a small fixed number of app slots
in a flash/image region. A slot contains a manifest and an image. The Shell
enumerates every slot and ignores empty or invalid ones.

This is still filesystem-free and can simplify an MCU implementation. Its
costs are internal fragmentation, a fixed application limit, and a less
natural fit for storage backed by Browser object stores. It is mainly an
implementation option for a later installer rather than a Shell-level concept.

### 4. App self-registration

After an image is loaded, Lisp code calls something like `app-register` to
publish its title, templates, and entry points. This gives application authors
a pleasant declaration mechanism, but it cannot by itself solve discovery.
The Shell has already had to choose and load the image. It is best treated as
the app's activation handshake, not as the source of the installed-app list.

### 5. Scan all stored objects

The Shell could inspect every object looking for an application manifest. This
resembles filesystem directory scanning, gives storage layout accidental
meaning, and makes boot time and failure behaviour depend on unrelated data.
It is not a good fit for bounded MCU resources and should be avoided.

## Recommended prototype direction

Use a **build-time catalogue of embedded application images**, initially with
one built-in to-do app. The catalogue is the only discovery mechanism needed
at boot. An app then performs a small activation handshake to submit its
Shell-facing descriptor and templates.

```text
native bootstrap
  -> built-in catalogue selects "todo"
  -> uLisp loads the embedded todo image
  -> todo app initialises its StoreRefs and UiRefs
  -> todo submits its app descriptor/templates to the Shell
  -> Shell renders and routes semantic input
```

The app should receive a stable app ID chosen by the catalogue; it should not
choose an ID at runtime. The Shell can use that ID to retain per-app layout
preferences, while the app uses it to derive its data names such as
`"todo/items"` and `"todo/prefs"`.

This tests the important question—whether the Shell, a uLisp app, live UI
bindings, and persistent data compose on Browser and MCU—without making
dynamic loading, installation, code updates, or a persistent registry a
prerequisite. The SilOS plan explicitly allows the first prototype to omit
general-purpose APIs and dynamic application loading.

If a later use case requires installed apps, promote the catalogue into a
small, persisted system object with a firmware-resident recovery catalogue.
The public Shell model can remain the same: enumerate catalogue entries, load
the selected image, and activate it. No app-data filesystem is implied.

## Loading and running an app

An application image can initially be either uLisp source held as a bounded
embedded byte array or a uLisp workspace/image prepared at build time. The
loader copies or maps it into the uLisp-owned workspace through a controlled
native operation, evaluates/initialises it, and invokes one agreed entry
function such as `app-main`.

The Shell should not call arbitrary Lisp directly from its UI task. It sends
bounded lifecycle and semantic-input commands to the uLisp task; that task
evaluates app code, mutates UiRefs/StoreRefs, and sends Shell commands or
results back through the existing ownership boundary. An illustrative lifecycle
is:

```text
load -> initialise -> register -> visible
                         |             |
                         v             v
                    registration error  input / storage events
```

The Shell-side app descriptor should stay smaller than a conventional process
manifest. Initially it needs only a stable ID, user-facing label, presentation
requirements, and references to already-registered templates or activation
functions. Capabilities, permissions, background services, and independent
processes are later concerns, not requirements to launch the to-do app.

The promoted runtime keeps a bounded set of applications loaded in one uLisp
workspace. Each has its own descriptor, event handler, Store binding, UI
resources, and generation-tagged event routing. The Shell may render several
apps on a larger surface while tiny mode selects one of those already-loaded
apps; visibility does not determine whether the app remains loaded.

## App declarations and callbacks

The Shell must not look up a common global function such as `app-info`. In the
minimal one-workspace model, every app would redefine it. Instead, separate an
app's small declaration source from its executable source:

```text
apps/todo/app.lisp       one small app declaration
apps/todo/src/main       entry module
apps/todo/src/model      imported module
apps/todo/src/ui         imported module
```

The slash is a logical naming convention, not a requirement for application
code to see directories or use filesystem APIs. An SD/FAT backend may map these
objects to files and directories; IndexedDB and SRAM use different physical
representations.

`apps/todo/app.lisp` contains only the declaration, for example:

```lisp
(shell-app-register
  :name "To-do"
  :ideal-width 24
  :ideal-height 10
  :entry "apps/todo/src/main")
```

The Shell obtains the candidate app ID and declaration-store ID from its small
built-in or persisted app index. It asks the uLisp task to read that one source
store in a restricted **describe** mode. That mode accepts one
`shell-app-register` form with literal, bounded fields; it does not permit
`shell-module-require`, `defun`, or
ordinary app side effects. The native declaration handler validates the fields
and sends a plain fixed-size `AppDescriptor` to the Shell UI task. The Shell
stores it under the candidate app ID. It does not retain a Lisp function pointer
or load the app's source modules merely to display the app in a catalogue.

This makes the initial characteristics a declaration, not a callback. It is
smaller, avoids global name collisions, and gives the Shell the information it
needs before displaying the app. If a real use case makes presentation
requirements dynamic, the loaded app can explicitly send a descriptor update
later.

When the user starts the app, the Shell resets/prepares the uLisp workspace and
loads the manifest's `:entry` store. That entry uses global
`shell-module-require` for shared SilOS APIs and lexical `shell-module-import`
for app-private source modules it
actually needs. Loading every store whose name looks like `apps/todo/src/*`
would need ordering rules and would evaluate unused code, so the entry/import
graph is preferable. A manifest may later list extra eager modules if a
concrete start-up use case needs them.

If a later Shell feature genuinely needs to call application behaviour, the
callback is also registered against the current app ID rather than a shared
global name. The Shell posts an `(app ID, request)` command to the uLisp task;
that task finds the ID's handler and invokes it locally before sending back
only a bounded result. Any stored Lisp handler must be retained through a
uLisp global/rooted registry so garbage collection cannot reclaim it. The
Shell never follows a pointer into the uLisp workspace.

## App execution and the shared uLisp task

Loading `"apps/todo/src/main"` evaluates its top-level forms and calls one
agreed entry function, such as `app-main`. That function should initialise the
app's StoreRefs and UiRefs, register its input/event handlers under the current
app ID, and then **return**. It should not run the application's permanent
event loop.

```text
Shell starts app
  -> uLisp loads main and its required modules
  -> app-main creates state and registers handlers
  -> app-main returns
  -> uLisp task waits for the next queued event
```

The uLisp task owns the one event loop. It receives Shell semantic input,
storage completions, timer events, and lifecycle commands. Each carries an app
ID where applicable. The task selects that app's registered handler, invokes
it to completion, applies any resulting UiRef/StoreRef changes, and then takes
the next event. The Shell UI task continues to render separately through the
existing queue/ownership boundary.

This makes ordinary apps event-driven rather than polling-driven. A to-do app,
for example, needs handlers for Activate/Edit events and StoreRef changes, not
a perpetual `(loop ...)`. A timer-driven app later receives timer events in the
same way.

### What "yield" means here

Multiple apps sharing one uLisp task do **not** execute concurrently. They
share the one workspace and event dispatcher, so one handler must return before
the dispatcher can call another app's handler. A FreeRTOS `taskYIELD()` from a
uLisp safe point can let the Shell, storage, or other FreeRTOS tasks run, but
it resumes the same Lisp evaluation; it cannot switch to another app handler
inside that one uLisp task.

The first rule should therefore be that app handlers are short and bounded.
An app that runs a long `(loop ...)`, performs a blocking wait, or evaluates a
large unbroken computation prevents every other app in that uLisp task from
handling events. The current multi-app runtime relies on that short-handler
rule. If apps must perform long-running work concurrently, SilOS will need
one of these deliberately larger mechanisms:

- resumable uLisp continuations/fibres and a per-app scheduler;
- separate uLisp tasks/workspaces; or
- an application convention that divides work into small queued steps.

The third option is the smallest future extension, but none of these should be
assumed before a concrete multi-app background-work use case exists.

### Reactive scheduling through Refs

UiRefs, StoreRefs, StoreRowRefs, and later MQTT references give applications
the intended way to react without a polling loop. A storage or MQTT task sends
a bounded completion/change message; the uLisp task updates the appropriate
live Ref; the Ref's watch then gives the owning app a small opportunity to
respond. Shell input and timer events use the same dispatcher.

```text
storage / MQTT / Shell input
  -> bounded message to uLisp task
  -> update one Ref or dispatch one app event
  -> short app watch/handler
  -> return to dispatcher
  -> next app's ready event
```

This should allow the task to keep cycling between apps naturally: ordinary
applications spend most of their time inactive, retained only as data, Refs,
templates, and small registered handlers. They run when an observed value or
user/system event changes.

The dispatcher needs one simple fairness rule: a handler or watch gets a
bounded turn and then returns. A change it causes may enqueue further work, but
must not create an unbounded synchronous watch chain that monopolises the
uLisp task. The exact per-app queue/fairness policy can remain an experiment;
a single bounded global queue and short handlers are sufficient to test the
first reactive apps.

## Storage roles

The same broad storage *concept* can hold both kinds of object, but it should
expose distinct roles:

| Role | Example | Needed properties |
|---|---|---|
| System catalogue | `system/apps/catalogue` | Known location, bounded parse, fallback/recovery |
| App image | `system/apps/todo/image` | Immutable read, version/integrity identity, atomic replacement later |
| App data | `todo/items` | BoundQueueStore rows, revisions, durable updates |
| Shell preference | `shell/layout` | Small durable settings keyed by app and display profile |

For the first prototype, the catalogue and app image can simply be compiled
into the platform artifact. Only the to-do data needs the storage API. This
keeps the test of durable application data independent from the harder question
of safely updating executable Lisp.

## Storage backends and CRUD

A normal filesystem is a sensible *platform implementation* when the hardware
offers one. SilOS need not implement a block allocator, FAT tables, directory
traversal, or SD-card driver merely to avoid exposing file paths to apps.
Instead, keep the Storage API logical and put a narrow backend beneath the
storage task:

```text
uLisp Store API / StoreRefs
  -> storage task: record IDs, revisions, CRUD and completion messages
  -> logical-object backend: read, replace, delete, enumerate, sync
  -> RAM | FAT-on-SD | Browser IndexedDB | another target implementation
```

The storage task owns the portable semantics. It maps `store-create`,
`store-bind`, `store-row-add`, field updates, and row deletion to a compact,
versioned representation. It then asks the backend to replace or append the
corresponding logical object. The uLisp task still receives asynchronous
completion or error messages and updates StoreRefs; app code never sees files,
mount points, browser keys, or database transactions.

The minimum backend contract could be deliberately small:

```text
open/inspect
read(object-id) -> bytes and revision/error
replace(object-id, expected-revision, bytes) -> new revision/error
delete(object-id, expected-revision) -> success/error
enumerate(prefix) -> bounded object descriptors
flush -> success/error
```

The common storage task, not every backend, implements row identity and the
BoundQueueStore collection model. This is important: a FAT backend may store a
whole logical collection in one physical file, while IndexedDB may keep one row
per object. Both can provide the same StoreRef behaviour.

### SRAM backend

An SRAM backend is useful for early Browser/MCU tests and for a device without
persistent media. It implements the same CRUD contract in bounded RAM, but
reports itself as volatile and loses data on restart. It is suitable for
testing UI and StoreRef behaviour, not for satisfying the to-do persistence
requirement.

### FAT on an SD card

Using FAT is reasonable and well supported on ESP32. ESP-IDF includes FatFs,
the SDMMC and SDSPI drivers, and a VFS adapter; it provides helpers for mounting
an SD card and then using standard C file operations below a native mount path.
[ESP-IDF's FAT documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html)
describes both SDMMC and SPI-mounted cards.

That mount path must remain a backend detail. For example, the FAT backend
could map the logical `"todo/items"` object to a private file below
`/sdcard/silos/`, using a reversible encoded name or a small internal index.
It could instead keep all logical objects in one append-only data file. The
choice should be driven by the desired interruption guarantee and resource
measurements, not by the API exposed to applications.

FAT solves removable-media access and physical file CRUD, but it does not by
itself define SilOS-level atomic record updates or recovery after power loss.
For the first persistent to-do experiment, a small self-validating record
format is still needed above FAT: for example, write a new version with a
length, revision, checksum, and commit marker, then select the last complete
version after reboot. A later implementation may use temporary files and
replacement, but must test what card removal and interrupted writes actually
guarantee. The StoreRef `silos-error` state naturally represents a missing card,
failed mount, full card, write failure, or failed revision check.

This is a much smaller responsibility than implementing a filesystem. FAT owns
sectors, allocation, and directories; SilOS owns its logical names, record
format, revisions, and observable CRUD semantics.

### Browser backend

`localStorage` is workable only for a very small, window-side prototype. It is
synchronous, string-oriented Web Storage and therefore can block JavaScript;
it is also not available to the dedicated Worker that owns the current
FreeWisp runtime. [MDN's Web Storage documentation](https://developer.mozilla.org/en-US/docs/Web/API/Web_Storage_API)
describes that synchronous model.

For the Browser target, prefer **IndexedDB**. It is asynchronous, transactional,
can store structured data and binary blobs, and is available in Web Workers.
[MDN's IndexedDB documentation](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API)
covers these properties, including Worker availability. A Browser storage
adapter can therefore run beside the Worker bridge and translate IndexedDB
completion callbacks into the same bounded storage-task messages used on the
MCU.

Browser storage is still origin-scoped and subject to browser quota/eviction
policies. The prototype should report persistent-storage failure through the
same Storage API instead of assuming that a successful Browser write is an
unlimited or permanent disk guarantee.

### Recommended first CRUD experiment

Use one logical `"todo/items"` object with a bounded, versioned binary
collection representation. Implement create, read/bind, add row, update row,
and delete row through the storage task. Begin with an SRAM backend to prove
the queues and StoreRefs, then implement the same backend contract using
IndexedDB on the Browser and FAT on an SD card for the ESP32.

This permits a direct comparison of behaviour, error handling, latency, and
memory without prematurely deciding whether the eventual physical layout is
one file per object, one journal file, or an IndexedDB object store.

## uLisp source as a store

Yes. uLisp source should be an **ordinary bound store**. It uses the existing
StoreRef and StoreRowRef model, the same backend and CRUD service, and the same
two-way binding behaviour as application data. A uLisp program editor is then
just another uLisp application: it binds the source rows, displays and edits
them through UiRefs, and observes the normal `silos-saving`, `silos-ready`, and
`silos-error`
states.

```text
ordinary stores
  todo/items                 to-do rows
  apps/todo/source           editable persistent uLisp source rows
  system/apps/catalogue      app records and source-store identities
```

All SilOS code is uLisp in this model. "Code" is consequently not a separate,
privileged storage type or a language-neutral package system. It is a standard
source-store schema and a loader convention applied to ordinary stores. FAT,
IndexedDB, and SRAM still implement the same logical-object backend contract.

An editable source document can contain one ordinary row per line. For example:

```text
store: "apps/todo/source"

row 91: { line: 10, text: "(defvar todos nil)" }
row 92: { line: 20, text: "(defun app-main ()" }
row 93: { line: 30, text: "  (render-todos))" }
```

The row ID is the stable storage identity; `line` is the explicit source-order
key. This is necessary because a BoundQueueStore collection does not promise
that its physical or returned row order is persistent. Sparse line numbers
leave room to insert lines; a small editor may renumber a bounded source store
when it runs out of gaps.

Calling a store a "program" is useful shorthand, but an **app** is a little
larger than one source store. It has a stable catalogue ID and can own source,
an optional manifest, and ordinary data stores. Loading it creates the
separate, temporary in-memory program:

```text
app ID: todo
  apps/todo/source          editable persistent source rows
  system/apps/catalogue     record pointing to the source store
  todo/items                application data
  todo/prefs                application data
```

These names are logical object IDs. A FAT backend can implement them as files,
an IndexedDB backend as object stores/records, and an SRAM backend as bounded
buffers, without changing the uLisp or Shell model.

### Stored source versus loaded code

The persistent source store and loaded uLisp program must be independent. The
Shell streams source from `"apps/todo/source"` through a small bounded reader
buffer into the uLisp workspace only when it is asked to load or reload the
app. Once loaded, its functions, variables, and other workspace objects are
ordinary in-memory uLisp state; they are not live-bound to the source rows.

Consequently, a uLisp editor can update the stored source freely without
disturbing the running app. A Shell **Reload** action is the explicit boundary:

1. the storage task waits for in-flight writes to the source store, or rejects
   Reload as busy;
2. it serialises one stable, ordered view of its current rows to the uLisp
   reader, using bounded chunks rather than a second complete source copy;
3. the runtime stops the current app and releases or resets its uLisp workspace
   state; and
4. the reader loads the current stored source and calls its entry function.

Reload therefore applies all stored edits at once, while ordinary editing never
changes executing code. It also means there is no need to retain multiple
source revisions in persistent storage simply to protect a running app.

The memory cost of retaining the old app while loading a trial replacement can
be unacceptable on the MCU. The smallest first reload path therefore need not
roll back after a load error: it tears down the old app, attempts the new load,
and falls back to a tiny built-in Shell/recovery view that reports the line and
reader error. The user can repair the ordinary source store with the editor and
try Reload again. Retaining an old running app through a failed reload is a
later reliability feature only if measurements justify a second workspace,
image, or source snapshot.

### Line rows are an editor choice, not a language requirement

A source line is convenient for a simple text editor and easy to display using
the existing templates. It is not a natural Lisp semantic unit: one form can
span many lines, and a line may contain several forms. The loader should treat
the assembled snapshot as one character stream and let the uLisp reader
determine form boundaries.

If no on-device source editor is needed in the first prototype, an even
smaller option is one immutable source blob. The editable-row model should be
introduced when its editing benefits justify the extra ordering and reload
rules.

### Same binding model for a uLisp editor

The editor should use the normal Store API rather than receive a special
CodeRef. Its source view is a live collection of ordinary StoreRowRefs:

```lisp
(defvar todo-source
  (store-bind "apps/todo/source" '(line text)))

;; An editor creates, binds, and edits these rows exactly as it would to-do rows.
(store-row-add "apps/todo/source" '(line 40 text "(app-main)"))
(setf (field (field selected-line 'value) 'text) "(app-main)")
```

Its rows have the normal stable IDs, revisions, write status, errors, and
watches. The editor can expose the selected row's `text` field through a UiRef,
so a normal Shell edit session updates the authoritative stored line through
the same path as every other stored value. There is no second, inaccessible
copy of source owned by a special code subsystem.

### Reloading source from storage

A small native `ulisp-load-store` bridge may be needed to stream the selected
ordinary source store into the uLisp reader. It is analogous to storage reads,
not an alternate code store: it obtains its bytes through the same storage task
and asks only for the catalogue-selected source store. The bridge reads in
source order and need retain only its input chunk plus normal reader state.

The editor operations do not have to be line-based forever. A later source
editor could use character ranges, form-oriented edits, or source blobs while
retaining the same StoreRef/StoreRowRef identity and revision semantics. The
first line-oriented schema is attractive only because it makes a constrained
editor small and gives storage bounded records.

If the first prototype only embeds one app and has no source editing or
installing, its source store can be populated at build time and treated as
read-only. It still follows the same representation when an editor is
introduced.

## uLisp modules and imports

The pinned uLisp already supplies `(require 'symbol)`, but it is not a general
file/module import facility. Its implementation first checks whether that
symbol already has a global binding. If it does not, it scans the compiled-in
`LispLibrary` character string until it finds a matching `defun` or `defvar`,
evaluates that one definition, and returns `t`. A later require of the same
symbol returns `nil` because the global binding now exists.

This gives limited, symbol-level deduplication in one uLisp runtime, but it
does not know about SilOS stores and has no callback saying "find module X in
storage". Calling a future `ulisp-load-store` bridge twice would instead read
and evaluate the source twice, potentially repeating initialisation side
effects. SilOS therefore needs one small, opinionated extension rather than
trying to give stores filesystem paths.

### Storage-backed `shell-module-require`

Expose the storage-backed operation under the Shell namespace:

```lisp
(shell-module-require 'silos-store)
```

`shell-module-require` resolves a **uLisp module name** through a small module
catalogue to an ordinary source store:

```text
module name    source store
-----------    ----------------------
silos-store    system/ulisp/silos-store
silos-ui       system/ulisp/silos-ui
shell-forms    system/shell/source
```

The first prototype may compile this bounded catalogue into the native
bootstrap. A later uLisp editor can maintain it as an ordinary
`"system/ulisp/modules"` store. Either way, the values it names are ordinary
editable source stores and the lookup is exact; there is no directory scan.

The storage-aware `shell-module-require` operation runs only in the uLisp task.
It performs the following sequence:

1. Look up the requested module name in a fixed-capacity runtime module table.
   If it is `loaded`, return without reading storage.
2. If it is `loading`, report a cyclic import error rather than recurse.
3. Resolve the name to its source-store ID through the module catalogue.
4. Ask the storage task for a stable ordered read of that source store and
   stream it in bounded chunks through the normal uLisp reader/evaluator.
5. Mark the module `loaded` only after every top-level form succeeds; otherwise
   mark it failed and report the reader/evaluator error.

The runtime table is the actual module-level deduplication mechanism. It is
global to the one running uLisp task, so two separately stored source modules
that both call `shell-module-require` for `todo-model` load it once. It should
be keyed by module name and source-store identity, have a fixed small capacity,
and be cleared when the Shell resets that uLisp workspace during an app reload.
Stored source edits therefore do not alter an already loaded module; the next
app reload starts a fresh table and reads current source again.

The native hook need not keep whole imported files in memory. Apart from the
bounded input chunk and a small module-state entry, uLisp's ordinary workspace
holds the evaluated definitions. This gives the desired source-store model
without a filesystem, package manager, or a second persistent code copy.

There is one consequence of the minimal-memory approach: if a module fails
halfway through evaluation, earlier forms may already have changed the uLisp
workspace. Rolling those back would require a second workspace or an image
snapshot. The first system should therefore load an app's dependency graph
during startup, before entering normal interaction; a module-load failure
returns to the recovery Shell and resets the workspace before the next attempt.
Runtime lazy imports can be introduced later only if their failure semantics
are worth the added recovery machinery.

### Lexical `shell-module-import` for app-private source

Storage-backed `shell-module-require` remains appropriate for **shared system
libraries**: it installs deliberately global names such as the uLisp helpers
in `silos-store`. It is not appropriate for two apps' private `model` or `ui`
files, because top-level `defun` forms from both would share GlobalEnv.

A small native `shell-module-import` primitive evaluates a source store as one
expression that returns a **module factory** or exported closure. It does not
install the module's helper names as globals. A small to-do arrangement could
be:

```lisp
;; apps/todo/src/model: its complete source evaluates to this factory
(lambda ()
  (let ((toggle
         (lambda (row)
           (setf (field (field row 'value) 'status) "done"))))
    (lambda (event)
      ;; model behaviour using private `toggle`
      event)))

;; apps/todo/src/main: the entry imports and instantiates the model
(let ((model ((shell-module-import "apps/todo/src/model"))))
  (shell-app-on-event
    (lambda (event) (model event))))
```

`shell-module-import` reads the named ordinary source store in bounded chunks
and returns the factory closure. The entry calls that factory to create its
private module instance, then keeps the returned closure in a lexical binding.
A module with several exports can instead return a bounded association list of
named closures. Its importing code keeps those values in local bindings; there
is no global `defun` to collide with a calendar's similarly named helpers.

The runtime may cache one successfully read **factory** per source-store ID to
avoid re-reading it. It must not treat a factory's result as a globally shared
module instance: each importing app calls the factory when it needs isolated
state. Cached factories, like app handler closures, are rooted in the uLisp
workspace and are discarded on Shell app reload.

### What a source store represents

With imports, one source store is best thought of as one **uLisp module** rather
than necessarily the complete app. The catalogue names an app's entry module;
its `shell-module-require` forms bring in shared system modules and its
`shell-module-import` forms bring in app-private source stores. The app remains
the catalogue identity plus its source modules and data stores.

### One workspace, redefinition, and reload

The minimal runtime has one uLisp task and one uLisp workspace. Every loaded
app, SilOS API, and imported module shares that workspace's global environment.
There are no module namespaces, separate process heaps, or C++-style function
overloads. One global symbol names one current definition.

Native SilOS built-in names should be a reserved part of that environment.
Applications may redefine their own uLisp globals during development, but must
not override the C-backed storage, UI, Shell module-loading, or recovery
primitives.

The pinned uLisp implementation does permit redefinition: evaluating another
`defun` or `defvar` for an existing global symbol replaces the value in that
symbol's global binding pair. A stored source module can therefore be read
again and its functions will take their new definitions. The last definition
wins. `makunbound` can remove an individual global binding.

This is useful for a developer-facing **hot patch** operation, but it is not a
complete file/module reload:

- a source module loaded twice can repeat initialisation side effects and reset
  its global variables;
- definitions removed from the edited source remain in the workspace unless
  the loader knows and unbinds every symbol the old source owned;
- two modules using the same global name collide; and
- the module table intentionally makes `shell-module-require` skip an already
  loaded module.

The first Shell should therefore expose one simple, predictable operation:
**Reload app**. It stops the app, removes that app's Shell templates, UiRefs,
and storage subscriptions, clears or recreates the one uLisp workspace and its
module table, then loads the app entry module and its imports afresh from their
current stores. This both applies edited code and guarantees that old symbols,
closures, and module state no longer consume workspace memory.

A later explicit `module-reload` command can bypass the loaded check and
re-evaluate one store for development use. It should be labelled as a hot
patch, not expected to undo removed definitions or provide isolation. Proper
module unload/reload would need ownership metadata or separate environments,
which costs more persistent metadata and RAM than the first constrained system
should assume.

### Namespace pressure and app instances

The pinned uLisp provides lexical closures through `lambda`, `let`, and
`let*`, but it has no package/namespace mechanism and no built-in class/object
system. Object-oriented style alone does not fix a global-name collision if two
apps first define their methods with the same top-level `defun` name.

For multiple apps in one workspace, the smallest useful convention is for an
app entry to construct one **app instance** from lexical bindings, then register
its event-handler closure under the current app ID. For example, schematically:

```lisp
;; apps/todo/src/main: evaluated for this app only
(shell-app-on-event
  (let ((items (store-bind "todo/items" '(desc status))))
    (let ((toggle
           (lambda (row) (setf (field (field row 'value) 'status) "done"))))
      (lambda (event)
        ;; Dispatch `event` here; `items` and `toggle` are private.
        event))))
```

The entry's local values and helper functions are captured by the registered
closure rather than published as globals. The app registry retains that closure
as a uLisp garbage-collection root, keyed by the Shell app ID. An equally
shaped calendar app can use local names such as `items`, `toggle`, or `handle`
without colliding with the to-do instance.

This is object-like composition through closures: an app instance owns private
state and exposes only its event-handler capability. It does not require a
class system. The Shell sees only the app ID and sends events; the uLisp task
invokes the closure.

Native SilOS built-ins and small shared uLisp API modules can remain global and
reserved, because they are loaded once and deliberately have system-wide names.
For the first prototype, application-private helpers should stay inside the
entry closure (or be passed explicitly as closures) rather than use top-level
`defun`. A larger app-private module/import system needs a later choice between
lexical module exports, loader-added name mangling, or a real package feature;
prefixing every global function name is workable but a poor primary model.

### Loading SilOS APIs

Applications use the same module-loading mechanism to obtain the uLisp-facing
SilOS APIs. Keep two layers distinct:

```text
native bootstrap primitives              imported uLisp API modules
---------------------------              --------------------------
shell-module-require                     silos/store
storage read needed by module loader     silos/ui
bounded command/queue bridge             silos/app
minimal error/recovery support           later shared helpers
```

The left-hand set is deliberately tiny and compiled into the native runtime.
It exists before any source store can be read, so `shell-module-require` can
resolve a module name and stream it from storage. It also provides the low-level
native operations ultimately used by storage, UI, and Shell bridges.

The right-hand set is ordinary uLisp source stored and loaded exactly like an
application module. For example, an app might begin:

```lisp
(shell-module-require 'silos-store)
(shell-module-require 'silos-ui)
(shell-module-require 'silos-app)
```

`silos-store` can define friendly uLisp functions and constants over the native
storage primitives; `silos-ui` can define template and UiRef helpers; and
`silos-app` can provide the standard app entry conventions. They are loaded
once per workspace through the same runtime module table, so two apps/modules
sharing `silos-store` do not duplicate its definitions in the running uLisp
task.

#### No C signatures are imported

uLisp is interpreted in this design. Reading an app or API source module does
not compile it against C headers or need a declaration of each native function
signature. A native SilOS primitive is compiled into the runtime and registered
in uLisp's built-in table with:

```text
uLisp name       C entry point       runtime argument rule
----------       -------------       ---------------------
store-bind       fn_store_bind       minimum/maximum arity
ui-ref-create    fn_ui_ref_create    minimum/maximum arity
shell-module-require  fn_shell_module_require  minimum/maximum arity
```

The pinned uLisp source's table entries contain exactly a name, a C function
pointer, an encoded minimum/maximum argument count, and optional
documentation. When Lisp evaluates `(store-bind "todo/items" ...)`, it looks
up `store-bind`, checks the supplied argument count at runtime, and calls the
registered C entry point with Lisp objects. Type and value checks are likewise
performed by that C entry point or by its small uLisp wrapper.

Thus `(shell-module-require 'silos-store)` does not make `store-bind` callable
for the first time. The primitive must already exist in the bootstrap table so
that the module loader itself can operate. Instead, it loads the ordinary uLisp
definitions built *on* those primitives: convenient argument shaping, shared
constants, application-level validation, and higher-level operations. A small
call path is:

```text
app source -> (shell-module-require 'silos-store)
           -> storage-backed module loader reads system/ulisp/silos-store
           -> defun definitions become uLisp globals
           -> app calls a wrapper or store-bind directly
           -> uLisp dispatches the registered C primitive
```

The first prototype may expose its minimum Store, UiRef, template, and reload
operations directly as native built-ins and make the imported modules very
thin. This minimises both bootstrap complexity and RAM. More API surface should
move into ordinary uLisp modules only where it removes repeated application
code.

The module catalogue maps these names to ordinary stores such as
`"system/ulisp/silos-store"`. For the first bootstrapping experiment, the
catalogue can be native and the API stores can be populated at build time. A
later uLisp editor may edit those stores too, but changing a SilOS API's source
still takes effect only at the next explicit Shell reload.

## Questions to resolve during implementation

- Does the selected uLisp support a sufficiently small and deterministic
  source/image loading path on both Browser and ESP32?
- What exact app descriptor and activation commands are the minimum needed by
  the current Shell UI design?
- Should one uLisp workspace host all loaded apps, and how are each app's
  globals, templates, StoreRefs, and UiRefs released or reset?
- Is loading source at boot acceptable, or is a build-prepared Lisp image
  materially faster or smaller on the MCU?
- What fixed bounds apply to catalogue entries, image size, names, templates,
  and queued lifecycle/input commands?
- What minimal recovery Shell can report a source-load error and return to the
  editor after an unsuccessful memory-constrained reload?
- What single-object record format and recovery rule are sufficient to retain
  to-do data across reset or an interrupted FAT/SD write?
- Can the Browser IndexedDB adapter and ESP32 FAT adapter both meet the same
  CRUD, revision, and StoreRef error semantics within the available queues and
  RAM?
- Can the storage task serialise a stable ordered source view directly into the
  uLisp reader with a fixed, small buffer while concurrent edits are paused or
  rejected?
- Can the `shell-module-require` implementation safely pause for a
  storage read, stream and evaluate a module, detect cycles, and recover after
  a failed module load?
- What fixed module-catalogue and runtime-module-table capacities are adequate
  for the first Browser and MCU applications?
- What source size, line length, ordering scheme, input buffer, and reader
  error reporting fit the reference MCU?
