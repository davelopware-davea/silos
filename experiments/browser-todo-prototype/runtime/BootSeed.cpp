#include "BootSeed.h"

#include "InMemoryStoreBackend.h"

namespace {
// Every string is a single source-row value.  Separating these from the
// runtime makes it clear that the loader obtains its program from a store,
// rather than from C++ code that directly evaluates a compiled-in string.
constexpr const char *TodoManifestRows[] = {
    "#| The bootstrap currently trusts this small manifest to contain only APP-DECLARE. |#",
    "(app-declare :name \"To-do\" :ideal-width 24 :ideal-height 10 :entry \"apps/todo/src/main\")",
};

constexpr const char *TodoMainRows[] = {
    "#| The entry source creates one lexical app instance and then returns. |#",
    "#| APP-START retains the returned event-handler closure for the Shell. |#",
    "(app-start",
    "  (let ((todo-items",
    "#| STORE-BIND returns a live StoreRef immediately, before its rows are ready. |#",
    "         (store-bind \"todo/items\" '(desc status) 0 8)))",
    "#| The handler is deliberately short: one event, one result, then return. |#",
    "    (lambda (event)",
    "      (cond",
    "#| A StoreRef separates its request metadata from its current row collection. |#",
    "        ((eq event 'binding-status)",
    "         (field (field todo-items 'meta) 'status))",
    "#| COUNT reads the live row collection. It is NIL while the bind is pending. |#",
    "        ((eq event 'count)",
    "         (let ((rows (field todo-items 'value)))",
    "           (if rows (length rows) 0)))",
    "#| This observes one bound StoreRowRef's named application field. |#",
    "#| It makes no ordering promise; ordering is a separate future Store API. |#",
    "        ((eq event 'sample-description)",
    "         (let ((rows (field todo-items 'value)))",
    "           (if rows",
    "               (field (field (car rows) 'value) 'desc)",
    "               nil)))",
    "#| Unknown events are returned unchanged while the event API is still small. |#",
    "        (t event)))))",
};

bool seed_source_store(InMemoryStoreBackend &stores, const char *name,
                       const char *const rows[], std::size_t row_count) {
  if (!stores.create_store(name)) return false;
  for (std::size_t index = 0; index < row_count; ++index) {
    const InMemoryStoreFieldInput field{"text", rows[index]};
    // Source ordering is only this bootstrap's insertion ordering.  The IDs
    // still make rows look like ordinary records, ready for the later linked
    // source-order model without encoding it into the backend.
    if (!stores.append_row(name, static_cast<std::uint32_t>(index + 1), 1,
                           &field, 1)) {
      return false;
    }
  }
  return true;
}
}

bool seed_boot_stores(InMemoryStoreBackend &stores) {
  if (!seed_source_store(stores, "apps/todo/app.lisp", TodoManifestRows,
                         sizeof(TodoManifestRows) / sizeof(TodoManifestRows[0])) ||
      !seed_source_store(stores, "apps/todo/src/main", TodoMainRows,
                         sizeof(TodoMainRows) / sizeof(TodoMainRows[0])) ||
      !stores.create_store("todo/items")) {
    return false;
  }

  constexpr InMemoryStoreFieldInput FirstTodo[] = {
      {"desc", "Learn how SilOS loads Lisp from a store"},
      {"status", "to do"},
  };
  constexpr InMemoryStoreFieldInput SecondTodo[] = {
      {"desc", "Build the first live screen binding"},
      {"status", "in progress"},
  };
  return stores.append_row("todo/items", 1, 1, FirstTodo,
                           sizeof(FirstTodo) / sizeof(FirstTodo[0])) &&
         stores.append_row("todo/items", 2, 1, SecondTodo,
                           sizeof(SecondTodo) / sizeof(SecondTodo[0]));
}
