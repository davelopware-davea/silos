cmake_minimum_required(VERSION 3.20)

project(silos_browser_todo_boot LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this experiment through emcmake")
endif()

set(FREERTOS_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/FreeRTOS-Kernel")
set(FREERTOS_EMSCRIPTEN_PORT "${CMAKE_CURRENT_SOURCE_DIR}/../../freertos-ulisp-browser/emscripten-freertos-port")
set(ULISP_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/../../freertos-ulisp-browser/standalone-ulisp/compat")
set(ULISP_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/ulisp-esp/ulisp-esp.ino")

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
object *SilosTodoItemsRef = NULL;

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

object *silos_make_todo_row_ref (const TodoRow &row) {
  object *value = nil;
  object *status_text = lispstring(const_cast<char *>(row.status));
  value = silos_prepend_field("status", status_text, value);
  protect(value);
  object *description = lispstring(const_cast<char *>(row.description));
  value = silos_prepend_field("desc", description, value);
  unprotect();

  protect(value);
  object *metadata = nil;
  metadata = silos_prepend_field("error", nil, metadata);
  protect(metadata);
  object *ready = silos_symbol("ready");
  metadata = silos_prepend_field("status", ready, metadata);
  unprotect();
  protect(metadata);
  metadata = silos_prepend_field("revision", number(1), metadata);
  unprotect();
  protect(metadata);
  metadata = silos_prepend_field("id", number(row.id), metadata);
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

object *silos_make_todo_row_refs (const InMemoryStore &store) {
  object *rows = nil;
  for (std::size_t index = store.todo_row_count; index > 0; --index) {
    protect(rows);
    object *row = silos_make_todo_row_ref(store.todo_rows[index - 1]);
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
  if (!symbolp(key)) error("field name is not a symbol", key);
  const char *name = nullptr;
  if (silos_symbol_is(key, "meta")) name = "meta";
  else if (silos_symbol_is(key, "value")) name = "value";
  else if (silos_symbol_is(key, "operation")) name = "operation";
  else if (silos_symbol_is(key, "status")) name = "status";
  else if (silos_symbol_is(key, "error")) name = "error";
  else if (silos_symbol_is(key, "id")) name = "id";
  else if (silos_symbol_is(key, "revision")) name = "revision";
  else if (silos_symbol_is(key, "desc")) name = "desc";
  else {
    error2("unknown field");
    return nil;
  }
  object *entry = silos_find_field(first(args), name);
  if (entry == nil) error2("unknown field");
  return cdr(entry);
}

object *fn_storebind (object *args, object *env) {
  (void) env;
  char name[StoreNameCapacity];
  cstring(checkstring(first(args)), name, sizeof(name));
  object *fields = second(args);
  if (std::strcmp(name, "todo/items") != 0 ||
      fields == nil || !silos_symbol_is(first(fields), "desc") ||
      rest(fields) == nil || !silos_symbol_is(second(fields), "status") ||
      cddr(fields) != nil || checkinteger(third(args)) != 0 ||
      checkinteger(first(cdr(cddr(args)))) <= 0) {
    error2("unsupported store-bind request");
  }
  if (SilosTodoItemsRef != nil) error2("todo/items is already bound");

  SilosTodoItemsRef = silos_make_pending_bind_ref();
  StoreBindStartedPending = true;
  const StorageRequest request{StorageRequestKind::BindTodoItems};
  if (xQueueSend(StorageRequestQueue, &request, 0) != pdPASS) {
    error2("storage request queue is full");
  }
  std::puts("store-bind=todo/items status=pending");
  return SilosTodoItemsRef;
}

object *fn_appstart (object *args, object *env) {
  (void) env;
  SilosActiveHandler = first(args);
  silos_record_app_start();
  return tee;
}

// This is a bootstrap test hook, not a proposed public Shell API.  It proves
// that APP-START retained the app's closure and that events execute on the
// uLisp task through the normal evaluation queue.
object *fn_silostestevent (object *args, object *env) {
  if (SilosActiveHandler == NULL) error2("no active app handler");
  return apply(SilosActiveHandler, args, env);
}

void silos_mark_roots () {
  markobject(SilosActiveHandler);
  markobject(SilosTodoItemsRef);
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
  "  { \":output\", (fn_ptr_type)OUTPUT, PINMODE, NULL },\n  { \"app-declare\", fn_appdeclare, 0207, doc_appdeclare },\n  { \"app-start\", fn_appstart, 0211, doc_appstart },\n  { \"field\", fn_field, 0222, doc_field },\n  { \"store-bind\", fn_storebind, 0244, doc_storebind },\n  { \"silos-test-event\", fn_silostestevent, 0211, NULL },\n};"
  ULISP_GENERATED_SOURCE
  "${ULISP_GENERATED_SOURCE}"
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ulisp-generated.inc" "${ULISP_GENERATED_SOURCE}")

add_executable(silos_todo_boot
  main.cpp
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
)
set_target_properties(silos_todo_boot PROPERTIES OUTPUT_NAME "silos-todo-boot" SUFFIX ".js")

enable_testing()
add_test(NAME silos_todo_boot COMMAND silos_todo_boot)
set_tests_properties(silos_todo_boot PROPERTIES PASS_REGULAR_EXPRESSION "SILOS_TODO_BOOT_PASS")
