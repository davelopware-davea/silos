cmake_minimum_required(VERSION 3.20)

project(silos_browser_todo_boot LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this experiment through emcmake")
endif()

set(FREERTOS_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/FreeRTOS-Kernel")
set(FREERTOS_EMSCRIPTEN_PORT "${CMAKE_CURRENT_SOURCE_DIR}/../../freertos-ulisp-browser/emscripten-freertos-port")
set(ULISP_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/../../freertos-ulisp-browser/standalone-ulisp/compat")
set(ULISP_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/ulisp-esp/ulisp-esp.ino")
# Link dependencies make edits below store-init rebuild Emscripten's preloaded
# virtual-filesystem data, rather than leaving a stale browser artifact.
file(GLOB_RECURSE STORE_INIT_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/store-init/*"
)

# Arduino normally inserts forward declarations while preprocessing a sketch.
# Generate those declarations in an adapter, keeping the vendored uLisp source
# byte-for-byte unchanged.
file(READ "${ULISP_UPSTREAM}" ULISP_SOURCE)
string(REGEX MATCHALL
  "\n[A-Za-z_][A-Za-z0-9_ *]*[(][^\n;]*[)] *[{]"
  ULISP_FUNCTIONS
  "${ULISP_SOURCE}"
)
set(ULISP_PROTOTYPES "")
foreach(SIGNATURE IN LISTS ULISP_FUNCTIONS)
  string(REGEX REPLACE "^\n" "" PROTOTYPE "${SIGNATURE}")
  string(REGEX REPLACE "[{]$" ";" PROTOTYPE "${PROTOTYPE}")
  string(APPEND ULISP_PROTOTYPES "${PROTOTYPE}\n")
endforeach()
string(REPLACE
  "// Forward references"
  "// Forward references\nvoid silos_mark_roots ();\n${ULISP_PROTOTYPES}"
  ULISP_GENERATED_SOURCE
  "${ULISP_SOURCE}"
)
string(REPLACE
  "  markobject(GCStack);"
  "  markobject(GCStack);\n  silos_mark_roots ();"
  ULISP_GENERATED_SOURCE
  "${ULISP_GENERATED_SOURCE}"
)
set(SILOS_APP_EXTENSION [[
const char doc_appdeclare[] = "(app-declare :name string :ideal-width integer :ideal-height integer :entry string)\nRegisters the current app declaration during bootstrap.";
const char doc_appstart[] = "(app-start handler)\nRegisters the current app event handler.";
const char doc_storebind[] = "(store-bind name fields start count)\nReturns a live StoreRef whose rows become ready asynchronously.";
const char doc_storerefwatch[] = "(store-ref-watch store-ref handler)\nCalls handler with live and old StoreRef values after a top-level ref change.";
const char doc_field[] = "(field record name)\nReads one named field from a StoreRef, StoreRowRef, or its metadata/value record.";

// The initial bootstrap deliberately trusts its one manifest.  It reads the
// documented positional keyword/value shape and leaves manifest validation for
// the later, bounded describe-mode experiment.
object *fn_appdeclare (object *args, object *env) {
  (void) env;
  char name[32];
  char entry[48];
  args = cdr(args);                         // Skip :name.
  cstring(checkstring(first(args)), name, sizeof(name));
  args = cdr(cdr(args));                    // Skip name and :ideal-width.
  int ideal_width = checkinteger(first(args));
  args = cdr(cdr(args));                    // Skip width and :ideal-height.
  int ideal_height = checkinteger(first(args));
  args = cdr(cdr(args));                    // Skip height and :entry.
  cstring(checkstring(first(args)), entry, sizeof(entry));
  silos_capture_app_declaration(name, ideal_width, ideal_height, entry);
  return tee;
}

object *SilosActiveHandler = NULL;
object *SilosBoundStoreRef = NULL;
object *SilosBoundStoreWatch = NULL;
char SilosBoundStoreName[StoreNameCapacity]{};
char SilosBoundFieldNames[InMemoryFieldsPerRowCapacity][InMemoryFieldNameCapacity]{};
std::size_t SilosBoundFieldCount = 0;
std::size_t SilosBoundStart = 0;
std::size_t SilosBoundCount = 0;

// Refs are ordinary, compact association lists in this experiment. FIELD is
// the sole application-facing accessor, so a later packed representation need
// not change the Lisp source that uses it.
bool silos_symbol_is (object *value, const char *name) {
  if (!symbolp(value)) return false;
  // uLisp stores ordinary six-character-or-shorter symbols in its packed
  // radix-40 form, while longer names are linked character cells.  Never use
  // eqsymbols on a packed symbol: it expects that linked representation.
  if (!longsymbolp(value) && std::strlen(name) <= 6 &&
      valid40(const_cast<char *>(name))) {
    return value->name == twist(pack40(const_cast<char *>(name)));
  }
  return longsymbolp(value) && eqsymbols(value, const_cast<char *>(name));
}

object *silos_symbol (const char *name) {
  return internlong(const_cast<char *>(name));
}

// FIELD receives a Lisp symbol, but the generic store keeps its field names
// as bounded C strings.  PRINTSYMBOL is the one reader-independent bridge:
// it works for uLisp's compact radix-40 symbols as well as long symbols.
char SilosPrintedSymbol[InMemoryFieldNameCapacity]{};
std::size_t SilosPrintedSymbolLength = 0;
bool SilosPrintedSymbolOverflow = false;

void silos_collect_symbol_character (char character) {
  if (SilosPrintedSymbolLength + 1 >= sizeof(SilosPrintedSymbol)) {
    SilosPrintedSymbolOverflow = true;
    return;
  }
  SilosPrintedSymbol[SilosPrintedSymbolLength++] = character;
}

bool silos_copy_symbol_name (object *symbol, char name[], std::size_t capacity) {
  if (!symbolp(symbol) || capacity == 0) return false;
  SilosPrintedSymbolLength = 0;
  SilosPrintedSymbolOverflow = false;
  SilosPrintedSymbol[0] = '\0';
  printsymbol(symbol, silos_collect_symbol_character);
  if (SilosPrintedSymbolOverflow || SilosPrintedSymbolLength + 1 > capacity) {
    return false;
  }
  SilosPrintedSymbol[SilosPrintedSymbolLength] = '\0';
  std::snprintf(name, capacity, "%s", SilosPrintedSymbol);
  return true;
}

object *silos_prepend_field (const char *name, object *value, object *tail) {
  // myalloc may run GC. Protect every live intermediate before asking uLisp
  // for another object, because C++ locals are not automatically GC roots.
  protect(value);
  protect(tail);
  object *key = silos_symbol(name);
  protect(key);
  object *entry = cons(key, value);
  protect(entry);
  object *result = cons(entry, tail);
  unprotect();
  unprotect();
  unprotect();
  unprotect();
  return result;
}

object *silos_find_field (object *record, const char *name) {
  while (record != nil) {
    if (!consp(record)) return nil;
    object *entry = first(record);
    if (consp(entry) && silos_symbol_is(first(entry), name)) return entry;
    record = rest(record);
  }
  return nil;
}

object *silos_make_pending_bind_ref () {
  object *metadata = nil;
  metadata = silos_prepend_field("error", nil, metadata);
  protect(metadata);
  object *pending = silos_symbol("pending");
  metadata = silos_prepend_field("status", pending, metadata);
  unprotect();
  protect(metadata);
  object *bind = silos_symbol("bind");
  metadata = silos_prepend_field("operation", bind, metadata);
  unprotect();

  protect(metadata);
  object *ref = nil;
  ref = silos_prepend_field("value", nil, ref);
  protect(ref);
  ref = silos_prepend_field("meta", metadata, ref);
  unprotect();
  unprotect();
  return ref;
}

// The only transition in this increment is PENDING to READY, so VALUE is NIL
// when this snapshot is taken. Build fresh association-list cells nonetheless:
// OLD-VALUE must not be another name for the StoreRef that will be mutated.
object *silos_snapshot_store_ref (object *live_ref) {
  object *value_entry = silos_find_field(live_ref, "value");
  object *metadata_entry = silos_find_field(live_ref, "meta");
  if (value_entry == nil || metadata_entry == nil) error2("invalid StoreRef");
  object *operation_entry = silos_find_field(cdr(metadata_entry), "operation");
  object *status_entry = silos_find_field(cdr(metadata_entry), "status");
  object *error_entry = silos_find_field(cdr(metadata_entry), "error");
  if (operation_entry == nil || status_entry == nil || error_entry == nil) {
    error2("invalid StoreRef metadata");
  }

  object *metadata = nil;
  metadata = silos_prepend_field("error", cdr(error_entry), metadata);
  protect(metadata);
  metadata = silos_prepend_field("status", cdr(status_entry), metadata);
  unprotect();
  protect(metadata);
  metadata = silos_prepend_field("operation", cdr(operation_entry), metadata);
  unprotect();

  protect(metadata);
  object *snapshot = nil;
  snapshot = silos_prepend_field("value", cdr(value_entry), snapshot);
  protect(snapshot);
  snapshot = silos_prepend_field("meta", metadata, snapshot);
  unprotect();
  unprotect();
  return snapshot;
}

object *silos_make_store_row_ref (const InMemoryStoreRow &row,
                                  const char field_names[][InMemoryFieldNameCapacity],
                                  std::size_t field_count) {
  object *value = nil;
  // Build only the fields named by STORE-BIND.  The row itself is generic:
  // source rows and to-do rows are both read through this same lookup path.
  for (std::size_t index = field_count; index > 0; --index) {
    const InMemoryStoreField *field = SourceStores.find_field(
        row, field_names[index - 1]);
    if (field == nullptr) continue;
    protect(value);
    object *field_value = lispstring(const_cast<char *>(field->value));
    value = silos_prepend_field(field->name, field_value, value);
    unprotect();
  }

  protect(value);
  object *metadata = nil;
  metadata = silos_prepend_field("error", nil, metadata);
  protect(metadata);
  object *ready = silos_symbol("ready");
  metadata = silos_prepend_field("status", ready, metadata);
  unprotect();
  protect(metadata);
  metadata = silos_prepend_field("revision", number(static_cast<int>(row.revision)), metadata);
  unprotect();
  protect(metadata);
  metadata = silos_prepend_field("id", number(static_cast<int>(row.id)), metadata);
  unprotect();

  protect(metadata);
  object *ref = nil;
  ref = silos_prepend_field("value", value, ref);
  protect(ref);
  ref = silos_prepend_field("meta", metadata, ref);
  unprotect();
  unprotect();
  unprotect();
  return ref;
}

object *silos_make_store_row_refs (const InMemoryStore &store,
                                   const char field_names[][InMemoryFieldNameCapacity],
                                   std::size_t field_count, std::size_t start,
                                   std::size_t count) {
  object *rows = nil;
  const std::size_t first = start < store.row_count ? start : store.row_count;
  const std::size_t available = store.row_count - first;
  const std::size_t selected = count < available ? count : available;
  for (std::size_t index = first + selected; index > first; --index) {
    protect(rows);
    object *row = silos_make_store_row_ref(store.rows[index - 1], field_names,
                                            field_count);
    protect(row);
    rows = cons(row, rows);
    unprotect();
    unprotect();
  }
  return rows;
}

object *fn_field (object *args, object *env) {
  (void) env;
  object *key = second(args);
  char name[InMemoryFieldNameCapacity];
  if (!silos_copy_symbol_name(key, name, sizeof(name)))
    error("field name is not a supported symbol", key);
  object *entry = silos_find_field(first(args), name);
  if (entry == nil) error2("unknown field");
  return cdr(entry);
}

object *fn_storebind (object *args, object *env) {
  (void) env;
  char name[StoreNameCapacity];
  cstring(checkstring(first(args)), name, sizeof(name));
  object *fields = second(args);
  const int start = checkinteger(third(args));
  const int count = checkinteger(first(cdr(cddr(args))));
  if (SourceStores.get(name) == nullptr || start < 0 || count <= 0) {
    error2("unsupported store-bind request");
  }
  SilosBoundFieldCount = 0;
  while (fields != nil) {
    if (!consp(fields) || SilosBoundFieldCount == InMemoryFieldsPerRowCapacity ||
        !silos_copy_symbol_name(first(fields),
                                SilosBoundFieldNames[SilosBoundFieldCount],
                                InMemoryFieldNameCapacity)) {
      error2("unsupported store-bind fields");
    }
    ++SilosBoundFieldCount;
    fields = rest(fields);
  }
  if (SilosBoundStoreRef != nil) error2("a store is already bound");

  SilosBoundStoreRef = silos_make_pending_bind_ref();
  std::snprintf(SilosBoundStoreName, sizeof(SilosBoundStoreName), "%s", name);
  SilosBoundStart = static_cast<std::size_t>(start);
  SilosBoundCount = static_cast<std::size_t>(count);
  StoreBindStartedPending = true;
  StorageRequest request{};
  request.kind = StorageRequestKind::BindStore;
  std::snprintf(request.store_name, sizeof(request.store_name), "%s", name);
  if (xQueueSend(StorageRequestQueue, &request, 0) != pdPASS) {
    error2("storage request queue is full");
  }
  std::printf("store-bind=%s status=pending\n", name);
  return SilosBoundStoreRef;
}

object *fn_storerefwatch (object *args, object *env) {
  (void) env;
  if (first(args) != SilosBoundStoreRef) error2("unknown StoreRef");
  if (SilosBoundStoreWatch != NULL) error2("StoreRef already has a watch");
  // The callback remains rooted until the future app-stop/reload lifecycle
  // releases it. This one-watch lifetime is deliberately the current limit.
  SilosBoundStoreWatch = second(args);
  StoreRefWatchRegistered = true;
  return tee;
}

object *fn_appstart (object *args, object *env) {
  (void) env;
  SilosActiveHandler = first(args);
  silos_record_app_start();
  return tee;
}

// This is a test-only observation sink, not a proposed Store or Shell API.
// The Lisp watch has already read LIVE and OLD-VALUE; this hook just records
// that bounded proof data for the native CTest client without invoking the
// app-level handler.
object *fn_silostestwatchobservation (object *args, object *env) {
  (void) env;
  object *old_status = first(cdr(cdr(cdr(args))));
  object *old_value = first(cdr(cdr(cdr(cdr(args)))));
  if (!silos_symbol_is(first(args), "ready") || checkinteger(second(args)) != 2 ||
      !silos_symbol_is(old_status, "pending") || old_value != nil) {
    error2("unexpected StoreRef watch observation");
  }
  cstring(checkstring(third(args)), StoreRefWatchObservedDescription,
          sizeof(StoreRefWatchObservedDescription));
  ++StoreRefWatchObservationCount;
  StoreRefWatchObservedReady = true;
  StoreRefWatchObservedCount = true;
  StoreRefWatchObservedOldPending = true;
  StoreRefWatchObservedOldValueNil = true;
  return tee;
}

void silos_mark_roots () {
  markobject(SilosActiveHandler);
  markobject(SilosBoundStoreRef);
  markobject(SilosBoundStoreWatch);
}

]])
string(REPLACE
  "// Built-in symbol lookup table"
  "${SILOS_APP_EXTENSION}// Built-in symbol lookup table"
  ULISP_GENERATED_SOURCE
  "${ULISP_GENERATED_SOURCE}"
)
string(REPLACE
  "  { \":output\", (fn_ptr_type)OUTPUT, PINMODE, NULL },\n};"
  "  { \":output\", (fn_ptr_type)OUTPUT, PINMODE, NULL },\n  { \"app-declare\", fn_appdeclare, 0207, doc_appdeclare },\n  { \"app-start\", fn_appstart, 0211, doc_appstart },\n  { \"field\", fn_field, 0222, doc_field },\n  { \"store-bind\", fn_storebind, 0244, doc_storebind },\n  { \"store-ref-watch\", fn_storerefwatch, 0222, doc_storerefwatch },\n  { \"silos-test-watch-observation\", fn_silostestwatchobservation, 0255, NULL },\n};"
  ULISP_GENERATED_SOURCE
  "${ULISP_GENERATED_SOURCE}"
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ulisp-generated.inc" "${ULISP_GENERATED_SOURCE}")

add_executable(silos_todo_boot
  main.cpp
  InMemoryStoreBackend.cpp
  StoreInitLoader.cpp
  "${FREERTOS_EMSCRIPTEN_PORT}/port.c"
  "${FREERTOS_KERNEL}/list.c"
  "${FREERTOS_KERNEL}/queue.c"
  "${FREERTOS_KERNEL}/tasks.c"
  "${FREERTOS_KERNEL}/portable/MemMang/heap_4.c"
)
target_compile_features(silos_todo_boot PRIVATE c_std_11 cxx_std_17)
target_compile_options(silos_todo_boot PRIVATE
  -Wall -Wextra -Werror
  -Wno-unused-parameter
  -Wno-unused-but-set-variable
)
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS
  "-Wno-sometimes-uninitialized;-Wno-implicit-const-int-float-conversion"
)
target_include_directories(silos_todo_boot PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${ULISP_COMPAT}"
  "${FREERTOS_EMSCRIPTEN_PORT}"
  "${FREERTOS_KERNEL}/include"
)
target_link_options(silos_todo_boot PRIVATE
  "-sASYNCIFY=1"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sASSERTIONS=1"
  "--preload-file"
  "${CMAKE_CURRENT_SOURCE_DIR}/store-init@/store-init"
)
set_property(TARGET silos_todo_boot APPEND PROPERTY LINK_DEPENDS "${STORE_INIT_FILES}")
set_target_properties(silos_todo_boot PROPERTIES OUTPUT_NAME "silos-todo-boot" SUFFIX ".js")

enable_testing()
add_test(NAME silos_todo_boot COMMAND silos_todo_boot)
set_tests_properties(silos_todo_boot PROPERTIES PASS_REGULAR_EXPRESSION "SILOS_TODO_BOOT_PASS")
