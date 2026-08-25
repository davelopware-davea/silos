# SilOS source

This is the production SilOS implementation, promoted from the successful
Browser to-do prototype. The Browser target starts the cooperative FreeRTOS
port with the pinned FreeRTOS and uLisp vendor trees. It currently:

1. preloads and imports the versioned `store-init/` tree into one
   fixed-capacity in-memory store catalogue;
2. scans `apps/` for `apps/<app-name>/app.lisp` manifests;
3. evaluates each manifest directly, capturing its `shell-app-register` result;
4. streams and evaluates the declared entry store; and
5. proves that `store-bind` returns pending first, then the uLisp task receives
   a bounded completion, snapshots the old pending ref, exposes ready
   StoreRowRefs, and invokes one `store-watch` callback on the uLisp task
   using the public `store-status`, `store-row-count`, `store-row-at`, and
   `store-row-field` accessors;
   and
6. delivers one later-turn `(shell-app-initialise)` event, copies and
   redelivers two app-owned `(init stage)` events without retaining Lisp
   pointers, and proves that binding and mounting occur only in the
   application's requested stages; and
7. declares a UiRef/item-template/bounded-list/mount with `ui-bind`, `ui-type`,
   `ui-template`, `ui-template-list`, `ui-field`, and `ui-text`, then renders
   template-owned literal text followed by both fields for each imported to-do
   after the StoreRef reaches ready, alongside the
   watch's ready status, bounded-row count, named `desc` field read, and
   preserved pending/nil old snapshot; and
8. projects those renderer-resolved list fields into the `#silos-app` Browser
   DOM surface without adding input handling or a browser-to-store path.

This implementation exercises the corresponding subset of the
[UI API](../docs/design/API-UI.md). The FreeRTOS/uLisp, Ref, flow-template,
module-ownership, and platform-seam approach is committed; individual public
operations and bounded representations may still be refined as the to-do
application is completed.

The production baseline retains the prototype's reviewable vendor snapshots:

| Dependency | Upstream revision | Local path |
| --- | --- | --- |
| FreeRTOS-Kernel V11.3.0 | `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5` | `third-party/FreeRTOS-Kernel/` |
| uLisp ESP 4.9a | `aa9b24ca3323159dacadca60ea0e9ffdf00b1a81` | `third-party/ulisp-esp/` |

Runtime code is organized by ownership:

- `SilOS/Store` owns the portable in-memory catalogue.
- `SilOS/Shell/Events.h` owns the bounded, pointer-free event/value contract.
- `SilOS/FreeRTOS/QueueRuntime.*` adapts those contracts to queues and tasks.
- `SilOS/Runtime` owns shared state, app discovery/bootstrap, and completion
  and Shell-event pumping.
- `SilOS/UI/Renderer.*` consumes the module-owned `PlatformSurface.h` seam;
  `SilOS/Platform/Browser/BrowserSurface.cpp` implements it with the DOM.
- `SilOS/Platform/Browser` owns Emscripten/Arduino compatibility, preload
  import, target composition, and the bounded Browser test harness.
- `SilOS/uLisp` owns language translation. Because uLisp keeps its evaluator,
  object representation, and GC internals sketch-private, `Extension.cpp`
  aggregates interface-specific `.inc` fragments into that one translation
  unit. `BuiltinEntries.inc` keeps the vendor registration hook to one line.

The vendored uLisp diff is limited to explicit extension inclusion,
built-in-entry inclusion, and GC-root declaration/call sites. The FreeRTOS
vendor tree requires no modification.

The source loader preserves the current insertion order of its in-memory rows.
That is intentionally only a bootstrap simplification; editable source will
need the later linked-list head/next-row ordering model.

The current implementation is deliberately read-only. It does not yet define
Lisp record literals or implement row creation, updates, deletion, semantic
input, or persistence. Its public Lisp source never traverses StoreRef or
StoreRowRef `meta`/`value` records directly; those compact association lists
remain runtime-private behind the typed Store accessors.

`SilOS/Store/InMemoryStoreBackend.{h,cpp}` contains the initial fixed-capacity
catalogue. Every store has the same generic row representation: stable `id`
and `revision` metadata plus a bounded array of named string fields. Therefore
source rows use a `text` field while to-do rows use `desc` and `status`, without
the backend knowing either shape.

`SilOS/Platform/Browser/BrowserStoreInitLoader.cpp` traverses the
Emscripten-preloaded `/store-init` virtual
directory before FreeRTOS starts. A store name is the exact normalized relative
file path, including the extension:

- `store-init/apps/todo/app.lisp` -> `apps/todo/app.lisp`
- `store-init/apps/todo/src/main.lisp` -> `apps/todo/src/main.lisp`
- `store-init/todo/items.csv` -> `todo/items.csv`

The manifest consequently declares `apps/todo/src/main.lisp`, and the app
binds `todo/items.csv`. Each `.lisp` line becomes an ordered generic row with a
`text` field; the streaming reader re-emits a newline between rows, so normal
`;` Lisp comments terminate at their original source-line boundary. A `.csv`
header becomes named application fields; every data row
receives sequential loader-assigned IDs and revision `1`.

The importer is deliberately bounded: at most 8 stores, 64 rows per store, 4
fields per row, 15-character names, 255-character field values/source lines,
and 4096 bytes per input file. CSV accepts LF or CRLF records, quoted commas,
and doubled quotes; it rejects malformed quotes, lone CR, quoted line breaks,
NUL bytes, unsupported extensions, and all capacity overflows.

The item-template implementation has no instruction-count or `ui-text`
literal-length cap. A successful `ui-template` declaration owns one exact-size
native instruction allocation plus one exact-size allocation for each literal.
The candidate is validated completely before publication; failure frees all
candidate storage. App cleanup/reload frees every installed literal and then
the instruction array. This demonstrates dynamic ownership, not infinite
resources: native memory exhaustion/fragmentation, uLisp heap pressure, and the
independent startup importer limits above remain practical risks. Field-name,
field-output, list-window, and list-state-text bounds remain unchanged.

The entry registers two deliberately different callbacks. `store-watch`
is the documented storage callback: it receives `(live old-value)` only after
the live StoreRef has changed. `shell-app-on-event` separately retains an
app-level handler. It receives `(shell-app-initialise)` later and uses
`shell-request-poke` to request two generic `(init stage)` events that choose
when to bind storage and when to mount the list. The Shell never interprets
that app-owned event type or stage data. Only one StoreRef watch is supported
now, and its callback is rooted until the future app-stop/reload lifecycle
releases it. There is no watch removal operation in this increment.

The entry's template and live handles are private lexical bindings captured by
the app event-handler closure. `ui-bind` resolves and roots the `todo-items`
binding pair itself, so later redraws follow that location without promoting
the app instance's state to uLisp globals.

From any working directory, configure when needed and build with:

```bash
bash /path/to/SilOS/src/build.sh
```

`build.sh` activates no SDK itself: when first configuring, run it from a Bash
environment (such as Git Bash or WSL) in which the Emscripten SDK has been
activated and `emcmake`, CMake, and Ninja are on `PATH`. It refuses to overwrite
an existing build configured with a non-Emscripten toolchain.

Build and run CTest with:

```bash
bash /path/to/SilOS/src/test.sh
```

To build and view the current Browser target:

```bash
bash ./src/view-browser.sh
```

It calls `build.sh`, serves only on `127.0.0.1:8765`, and prints
the URL to open. Pass a different local port as the first argument if needed;
use Ctrl-C to stop and clean up the server.

To change the to-do UI, edit `src/store-init/apps/todo/src/main.lisp`,
stop an already running `view-browser.sh` with Ctrl-C, and run it again. It
invokes `build.sh`, so `test.sh` is optional during UI iteration. Refresh the
printed URL after the rebuild; use a hard refresh if an already-open tab still
shows the prior JavaScript/Wasm/data bundle. The promoted baseline deliberately
keeps its original three instructions (`TODO:`, description, status), so the
normal CTest remains a stable baseline rather than a synthetic capacity demo.

The page loads adjacent Emscripten JavaScript, WASM, and preloaded data files.
When its StoreRef becomes ready, `#silos-todo-list` contains the two bound rows.
Each row begins with a `.silos-template-literal` containing `TODO:`, followed
by `.silos-template-field` elements that retain the declared field name in
`data-field` and display the template-width-chopped value. The page is a
one-way display adapter: it does not handle input or call back into the Store.
