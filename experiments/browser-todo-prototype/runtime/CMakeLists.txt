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

# Keep the executable JavaScript target for CTest, and stage a small HTML
# launcher alongside it for the Browser proof. The launcher owns only the
# document surface; the runtime continues to own all UI/template data.
configure_file(browser-surface.html.in browser-surface.html @ONLY)

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
const char doc_storestatus[] = "(store-status store-ref)\nReads a StoreRef operation status.";
const char doc_storeerror[] = "(store-error store-ref)\nReads a StoreRef error.";
const char doc_storevalue[] = "(store-value store-ref)\nReads a ready StoreRef result.";
const char doc_storerowcount[] = "(store-row-count store-ref)\nCounts rows in a ready bound StoreRef.";
const char doc_storerowat[] = "(store-row-at store-ref index)\nReads a row from a ready bound StoreRef.";
const char doc_storerowid[] = "(store-row-id store-row-ref)\nReads a StoreRowRef stable ID.";
const char doc_storerowrevision[] = "(store-row-revision store-row-ref)\nReads a StoreRowRef revision.";
const char doc_storerowstatus[] = "(store-row-status store-row-ref)\nReads a StoreRowRef status.";
const char doc_storerowerror[] = "(store-row-error store-row-ref)\nReads a StoreRowRef error.";
const char doc_storerowfield[] = "(store-row-field store-row-ref field)\nReads an application field from a ready StoreRowRef.";

const char doc_apppoke[] = "(app-request-poke arg...)";
const char doc_uitype[] = "(ui-type name (field type slot)...)";
const char doc_uibind[] = "(ui-bind name type value)";
const char doc_uitemplate[] = "(ui-template name (item type) (ui-text (ui-field item field) :width integer :overflow chop)...)";
const char doc_uitemplatelist[] = "(ui-template-list name :source ui-ref :item-template template :offset integer :limit integer :pending string :empty string :error string)";
const char doc_uimount[] = "(ui-mount list :region region)";

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
bool SilosPokePending = false;
char SilosBoundStoreName[StoreNameCapacity]{};
char SilosBoundFieldNames[InMemoryFieldsPerRowCapacity][InMemoryFieldNameCapacity]{};
std::size_t SilosBoundFieldCount = 0;
std::size_t SilosBoundStart = 0;
std::size_t SilosBoundCount = 0;

// This is intentionally a small real template registry, not a Todo-specific
// print path. It contains identifiers and bounded field descriptors only;
// values remain in the uLisp StoreRef and are streamed during a Shell redraw.
constexpr std::size_t SilosUiFieldCapacity = 3;
struct SilosUiTypeField {
  char name[InMemoryFieldNameCapacity]{};
  int slot = -1;
};
struct SilosUiField {
  char name[InMemoryFieldNameCapacity]{};
  int width = 0;
};
object *SilosUiSourceRef = NULL;
char SilosUiTypeName[InMemoryFieldNameCapacity]{};
SilosUiTypeField SilosUiTypeFields[SilosUiFieldCapacity]{};
std::size_t SilosUiTypeFieldCount = 0;
SilosUiField SilosUiFields[SilosUiFieldCapacity]{};
std::size_t SilosUiFieldCount = 0;
bool SilosUiTypeDeclared = false;
bool SilosUiRefDeclared = false;
bool SilosUiItemTemplateDeclared = false;
bool SilosUiListDeclared = false;
bool SilosUiMounted = false;
int SilosUiRefHandle = 0;
int SilosUiTemplateHandle = 0;
int SilosUiListHandle = 0;
char SilosUiPendingText[PokeStringCapacity]{};
char SilosUiEmptyText[PokeStringCapacity]{};
char SilosUiErrorText[PokeStringCapacity]{};

// Refs are ordinary, compact association lists in this experiment. The typed
// StoreRef and StoreRowRef accessors below are the only application-facing
// view, so a later packed representation need not change Lisp source.
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
  if (std::strlen(name) <= 6 && valid40(const_cast<char *>(name))) {
    return intern(twist(pack40(const_cast<char *>(name))));
  }
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

bool silos_symbol_name_is(object *value, const char *name) {
  char value_name[InMemoryFieldNameCapacity]{};
  return silos_copy_symbol_name(value, value_name, sizeof(value_name)) &&
      std::strcmp(value_name, name) == 0;
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

object *silos_store_ref_metadata(object *ref) {
  object *metadata = silos_find_field(ref, "meta");
  if (metadata == nil || silos_find_field(cdr(metadata), "operation") == nil ||
      silos_find_field(cdr(metadata), "status") == nil ||
      silos_find_field(cdr(metadata), "error") == nil ||
      silos_find_field(ref, "value") == nil) error2("invalid StoreRef");
  return cdr(metadata);
}

object *silos_store_row_metadata(object *row) {
  object *metadata = silos_find_field(row, "meta");
  if (metadata == nil || silos_find_field(cdr(metadata), "id") == nil ||
      silos_find_field(cdr(metadata), "revision") == nil ||
      silos_find_field(cdr(metadata), "status") == nil ||
      silos_find_field(cdr(metadata), "error") == nil ||
      silos_find_field(row, "value") == nil) error2("invalid StoreRowRef");
  return cdr(metadata);
}

object *silos_store_ref_status(object *ref) {
  object *status = silos_find_field(silos_store_ref_metadata(ref), "status");
  return cdr(status);
}

object *silos_store_row_status(object *row) {
  object *status = silos_find_field(silos_store_row_metadata(row), "status");
  return cdr(status);
}

object *silos_store_ref_ready_value(object *ref) {
  if (!silos_symbol_is(silos_store_ref_status(ref), "ready")) error2("StoreRef is not ready");
  object *value = silos_find_field(ref, "value");
  return cdr(value);
}

object *silos_store_bound_rows(object *ref) {
  object *metadata = silos_store_ref_metadata(ref);
  object *operation = silos_find_field(metadata, "operation");
  if (!silos_symbol_is(cdr(operation), "bind")) error2("StoreRef does not contain bound rows");
  object *rows = silos_store_ref_ready_value(ref);
  if (rows != nil && !consp(rows)) error2("invalid bound StoreRef rows");
  return rows;
}

object *fn_storestatus (object *args, object *env) {
  (void) env;
  return silos_store_ref_status(first(args));
}

object *fn_storeerror (object *args, object *env) {
  (void) env;
  object *error = silos_find_field(silos_store_ref_metadata(first(args)), "error");
  return cdr(error);
}

object *fn_storevalue (object *args, object *env) {
  (void) env;
  return silos_store_ref_ready_value(first(args));
}

object *fn_storerowcount (object *args, object *env) {
  (void) env;
  return number(listlength(silos_store_bound_rows(first(args))));
}

object *fn_storerowat (object *args, object *env) {
  (void) env;
  int index = checkinteger(second(args));
  if (index < 0) error2("StoreRowRef index is negative");
  object *rows = silos_store_bound_rows(first(args));
  while (index > 0 && rows != nil) {
    rows = rest(rows);
    --index;
  }
  if (rows == nil) error2("StoreRowRef index out of bounds");
  object *row = first(rows);
  (void) silos_store_row_metadata(row);
  return row;
}

object *fn_storerowid (object *args, object *env) {
  (void) env;
  object *id = silos_find_field(silos_store_row_metadata(first(args)), "id");
  return cdr(id);
}

object *fn_storerowrevision (object *args, object *env) {
  (void) env;
  object *revision = silos_find_field(silos_store_row_metadata(first(args)), "revision");
  return cdr(revision);
}

object *fn_storerowstatus (object *args, object *env) {
  (void) env;
  return silos_store_row_status(first(args));
}

object *fn_storerowerror (object *args, object *env) {
  (void) env;
  object *error = silos_find_field(silos_store_row_metadata(first(args)), "error");
  return cdr(error);
}

object *fn_storerowfield (object *args, object *env) {
  (void) env;
  object *row = first(args);
  if (!silos_symbol_is(silos_store_row_status(row), "ready") &&
      !silos_symbol_is(silos_store_row_status(row), "saving")) {
    error2("StoreRowRef fields are not readable");
  }
  char name[InMemoryFieldNameCapacity];
  object *key = second(args);
  if (!silos_copy_symbol_name(key, name, sizeof(name)))
    error("StoreRowRef field name is not a supported symbol", key);
  object *value = silos_find_field(row, "value");
  object *entry = silos_find_field(cdr(value), name);
  if (entry == nil) error2("unknown StoreRowRef field");
  return cdr(entry);
}

// Copy one admissible Lisp value into a bounded flat native arena. The queue
// therefore owns data, not a uLisp pointer which could be reclaimed or mutate
// before the Shell's later delivery turn.
bool silos_serialise_value(object *value, SerializedPayload &payload,
                            std::size_t depth, std::uint8_t &index) {
  if (depth > PokeDepthCapacity || payload.value_count == PokeValueCapacity) return false;
  index = payload.value_count++;
  SerializedValue &out = payload.values[index];
  out = SerializedValue{};
  if (value == nil) { out.kind = SerializedValueKind::Nil; return true; }
  if (value == tee) { out.kind = SerializedValueKind::Boolean; return true; }
  if (integerp(value)) {
    out.kind = SerializedValueKind::Integer;
    out.integer = value->integer;
    return true;
  }
  if (symbolp(value)) {
    out.kind = SerializedValueKind::Symbol;
    return silos_copy_symbol_name(value, out.text, PokeSymbolCapacity);
  }
  if (stringp(value)) {
    out.kind = SerializedValueKind::String;
    cstring(value, out.text, sizeof(out.text));
    return true;
  }
  if (!consp(value)) return false;
  out.kind = SerializedValueKind::List;
  out.child_count = 0;
  while (value != nil) {
    std::uint8_t child = InvalidSerializedValueIndex;
    if (!consp(value) || out.child_count == PokeListCapacity ||
        !silos_serialise_value(first(value), payload, depth + 1, child)) return false;
    out.children[out.child_count] = child;
    ++out.child_count;
    value = rest(value);
  }
  return true;
}

object *silos_deserialise_value(const SerializedPayload &payload, std::uint8_t index) {
  if (index >= payload.value_count) error2("invalid serial event");
  const SerializedValue &value = payload.values[index];
  switch (value.kind) {
    case SerializedValueKind::Nil: return nil;
    case SerializedValueKind::Boolean: return tee;
    case SerializedValueKind::Integer: return number(value.integer);
    case SerializedValueKind::Symbol: return silos_symbol(value.text);
    case SerializedValueKind::String: return lispstring(const_cast<char *>(value.text));
    case SerializedValueKind::List: {
      object *result = nil;
      for (std::size_t index = value.child_count; index > 0; --index) {
        protect(result);
        object *item = silos_deserialise_value(payload, value.children[index - 1]);
        protect(item);
        result = cons(item, result);
        unprotect();
        unprotect();
      }
      return result;
    }
  }
  error2("invalid serial event");
}

void silos_render_ui() {
  if (!SilosUiMounted || SilosUiSourceRef == nil) return;
  object *metadata = silos_find_field(SilosUiSourceRef, "meta");
  object *status = metadata == nil ? nil : silos_find_field(cdr(metadata), "status");
  if (status == nil || silos_symbol_is(cdr(status), "pending")) {
    silos_browser_surface_begin("pending", SilosUiPendingText);
    std::printf("ui-list region=main state=pending text=%s\\n", SilosUiPendingText);
    UiPendingRendered = true;
    return;
  }
  if (!silos_symbol_is(cdr(status), "ready")) {
    silos_browser_surface_begin("error", SilosUiErrorText);
    std::printf("ui-list region=main state=error text=%s\\n", SilosUiErrorText);
    return;
  }
  object *value = silos_find_field(SilosUiSourceRef, "value");
  object *rows = value == nil ? nil : cdr(value);
  if (rows == nil) {
    silos_browser_surface_begin("empty", SilosUiEmptyText);
    std::printf("ui-list region=main state=empty text=%s\\n", SilosUiEmptyText);
    return;
  }
  silos_browser_surface_begin("ready", "Bound to-do items");
  std::printf("ui-list region=main state=ready\\n");
  std::size_t row_index = 0;
  while (rows != nil && row_index < 5) {
    if (!consp(rows)) { std::printf("ui-list render-error=improper-rows\\n"); return; }
    object *row_value = silos_find_field(first(rows), "value");
    std::printf("ui-row index=%zu", row_index);
    for (std::size_t field_index = 0; field_index < SilosUiFieldCount; ++field_index) {
      object *field = row_value == nil ? nil : silos_find_field(cdr(row_value), SilosUiFields[field_index].name);
      if (field == nil || !stringp(cdr(field))) {
        std::printf(" %s=<invalid>", SilosUiFields[field_index].name);
      } else {
        char rendered[InMemoryFieldValueCapacity]{};
        cstring(cdr(field), rendered, sizeof(rendered));
        rendered[SilosUiFields[field_index].width < static_cast<int>(sizeof(rendered)) ?
                 SilosUiFields[field_index].width : static_cast<int>(sizeof(rendered)) - 1] = '\0';
        std::printf(" %s=%s", SilosUiFields[field_index].name, rendered);
        silos_browser_surface_add_field(static_cast<int>(row_index),
                                        SilosUiFields[field_index].name, rendered);
      }
    }
    std::printf("\\n");
    ++row_index;
    rows = rest(rows);
  }
  UiReadyRendered = row_index == 2;
}

bool silos_ui_is_declared_field(object *field) {
  if (!symbolp(field)) return false;
  for (std::size_t index = 0; index < SilosUiTypeFieldCount; ++index) {
    if (silos_symbol_name_is(field, SilosUiTypeFields[index].name)) return true;
  }
  return false;
}

bool silos_ui_is_list_of_declared_type(object *declaration) {
  return consp(declaration) && listlength(declaration) == 2 &&
      silos_symbol_name_is(first(declaration), "list-of") &&
      symbolp(second(declaration)) &&
      silos_symbol_name_is(second(declaration), SilosUiTypeName);
}

bool silos_ui_is_store_ref_list_of_declared_type(object *declaration) {
  return consp(declaration) && listlength(declaration) == 2 &&
      silos_symbol_name_is(first(declaration), "store-ref") &&
      silos_ui_is_list_of_declared_type(second(declaration));
}

object *fn_uitype (object *args, object *env) {
  (void) env;
  if (SilosUiTypeDeclared || !symbolp(first(args))) error2("invalid ui type");
  const int field_count = listlength(cdr(args));
  if (field_count < 1 || field_count > static_cast<int>(SilosUiFieldCapacity) ||
      !silos_copy_symbol_name(first(args), SilosUiTypeName,
                              sizeof(SilosUiTypeName))) {
    error2("invalid ui type");
  }
  object *fields = cdr(args);
  for (int index = 0; index < field_count; ++index) {
    object *field = first(fields);
    if (!consp(field) || listlength(field) != 3 || !symbolp(first(field)) ||
        !silos_symbol_name_is(second(field), "string") || !integerp(third(field)) ||
        !silos_copy_symbol_name(first(field), SilosUiTypeFields[index].name,
                                InMemoryFieldNameCapacity)) {
      error2("invalid ui type field");
    }
    const int slot = checkinteger(third(field));
    if (slot < 0 || slot >= field_count) error2("invalid ui type slot");
    for (int previous = 0; previous < index; ++previous) {
      if (SilosUiTypeFields[previous].slot == slot) error2("duplicate ui type slot");
    }
    SilosUiTypeFields[index].slot = slot;
    fields = rest(fields);
  }
  for (int slot = 0; slot < field_count; ++slot) {
    bool found = false;
    for (int index = 0; index < field_count; ++index) {
      if (SilosUiTypeFields[index].slot == slot) {
        found = true;
        break;
      }
    }
    if (!found) error2("non-contiguous ui type slots");
  }
  SilosUiTypeFieldCount = static_cast<std::size_t>(field_count);
  SilosUiTypeDeclared = true;
  return number(1);
}

object *fn_uibind (object *args, object *env) {
  if (SilosUiRefDeclared || !symbolp(first(args)) ||
      !silos_ui_is_store_ref_list_of_declared_type(second(args))) {
    error2("invalid ui source");
  }
  object *source = eval(third(args), env);
  if (source != SilosBoundStoreRef) error2("invalid ui source");
  SilosUiSourceRef = source;
  SilosUiRefDeclared = true;
  SilosUiRefHandle = 1;
  return number(SilosUiRefHandle);
}

object *fn_uitemplate (object *args, object *env) {
  (void) env;
  if (!SilosUiTypeDeclared || SilosUiItemTemplateDeclared || !symbolp(first(args)) ||
      !consp(second(args)) || listlength(second(args)) != 2 ||
      !symbolp(first(second(args))) || !symbolp(second(second(args))) ||
      !silos_symbol_name_is(second(second(args)), SilosUiTypeName)) {
    error2("invalid ui item template");
  }
  char item_parameter[InMemoryFieldNameCapacity]{};
  if (!silos_copy_symbol_name(first(second(args)), item_parameter,
                              sizeof(item_parameter))) error2("invalid ui item parameter");
  args = cddr(args);
  while (args != nil) {
    object *spec = first(args);
    if (!consp(spec) || listlength(spec) != 6 ||
        SilosUiFieldCount == SilosUiFieldCapacity ||
        !silos_symbol_name_is(first(spec), "ui-text") || !consp(second(spec)) ||
        listlength(second(spec)) != 3 ||
        !silos_symbol_name_is(first(second(spec)), "ui-field") ||
        !silos_symbol_name_is(second(second(spec)), item_parameter) ||
        !silos_ui_is_declared_field(third(second(spec))) ||
        !silos_symbol_name_is(third(spec), ":width") ||
        !integerp(second(cddr(spec))) ||
        !silos_symbol_name_is(first(cddr(cddr(spec))), ":overflow") ||
        !silos_symbol_name_is(second(cddr(cddr(spec))), "chop")) {
      error2("invalid ui field");
    }
    if (!silos_copy_symbol_name(third(second(spec)),
                                SilosUiFields[SilosUiFieldCount].name,
                                InMemoryFieldNameCapacity)) error2("ui field too long");
    SilosUiFields[SilosUiFieldCount].width = checkinteger(second(cddr(spec)));
    if (SilosUiFields[SilosUiFieldCount].width < 1 || SilosUiFields[SilosUiFieldCount].width > 32) error2("invalid ui field width");
    ++SilosUiFieldCount;
    args = rest(args);
  }
  if (SilosUiFieldCount == 0) error2("empty ui item template");
  SilosUiItemTemplateDeclared = true;
  SilosUiTemplateHandle = 2;
  return number(SilosUiTemplateHandle);
}

object *fn_uitemplatelist (object *args, object *env) {
  // This special form receives the public declarative keyword spelling intact.
  // In uLisp's builtin arity encoding, a maximum of seven means an unbounded
  // tail, so the compact implementation does not require a public alternate.
  object *config = cdr(args);
  if (listlength(config) != 14) error2("invalid ui list");
  object *source_keyword = first(config);
  object *source_form = second(config);
  config = cddr(config);
  object *template_keyword = first(config);
  object *template_form = second(config);
  config = cddr(config);
  object *offset_keyword = first(config);
  object *offset = second(config);
  config = cddr(config);
  object *limit_keyword = first(config);
  object *limit = second(config);
  config = cddr(config);
  object *pending_keyword = first(config);
  object *pending = second(config);
  config = cddr(config);
  object *empty_keyword = first(config);
  object *empty = second(config);
  config = cddr(config);
  object *error_keyword = first(config);
  object *error_text = second(config);
  if (SilosUiListDeclared || !SilosUiRefDeclared || !SilosUiItemTemplateDeclared ||
      !symbolp(first(args)) || !silos_symbol_is(source_keyword, ":source") ||
      checkinteger(eval(source_form, env)) != SilosUiRefHandle ||
      !silos_symbol_is(template_keyword, ":item-template") ||
      checkinteger(eval(template_form, env)) != SilosUiTemplateHandle ||
      !silos_symbol_is(offset_keyword, ":offset") || checkinteger(offset) != 0 ||
      !silos_symbol_is(limit_keyword, ":limit") || checkinteger(limit) != 5 ||
      !silos_symbol_is(pending_keyword, ":pending") ||
      !silos_symbol_is(empty_keyword, ":empty") ||
      !silos_symbol_is(error_keyword, ":error")) error2("invalid ui list");
  cstring(checkstring(pending), SilosUiPendingText, sizeof(SilosUiPendingText));
  cstring(checkstring(empty), SilosUiEmptyText, sizeof(SilosUiEmptyText));
  cstring(checkstring(error_text), SilosUiErrorText, sizeof(SilosUiErrorText));
  SilosUiListDeclared = true;
  SilosUiListHandle = 3;
  return number(SilosUiListHandle);
}

object *fn_uimount (object *args, object *env) {
  (void) env;
  if (!SilosUiListDeclared || SilosUiMounted || checkinteger(first(args)) != SilosUiListHandle ||
      !silos_symbol_is(second(args), ":region") || !symbolp(third(args))) error2("invalid ui mount");
  SilosUiMounted = true;
  silos_render_ui();
  return number(4);
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
  if (SilosActiveHandler != NULL) error2("app already started");
  SilosActiveHandler = first(args);
  silos_record_app_start();
  ShellEvent initialise{};
  initialise.kind = ShellEventKind::AppInitialise;
  initialise.app_generation = ActiveAppGeneration;
  if (xQueueSend(ShellEventQueue, &initialise, 0) != pdPASS) error2("shell event queue is full");
  return tee;
}

object *fn_apppoke (object *args, object *env) {
  (void) env;
  if (SilosActiveHandler == NULL) error2("app is not started");
  if (SilosPokePending) error2("poke-pending");
  ShellRequest request{};
  request.app_generation = ActiveAppGeneration;
  if (!silos_serialise_value(args, request.payload, 0, request.payload.root)) error2("unsupported poke payload");
  if (xQueueSend(ShellRequestQueue, &request, 0) != pdPASS) error2("shell request queue is full");
  SilosPokePending = true;
  return tee;
}

// The Shell only knows event tags and copied payloads. App conventions such as
// `init` and its stage numbers are interpreted exclusively by the Lisp handler.
void silos_dispatch_shell_event(const ShellEvent &event) {
  if (event.app_generation != ActiveAppGeneration || SilosActiveHandler == NULL) return;
  object *event_value = nil;
  if (event.kind == ShellEventKind::AppInitialise) {
    AppInitialiseDelivered = true;
    AppObservedNoBindBeforeInit = SilosBoundStoreRef == nil;
    event_value = silos_symbol("app-initialise");
  } else {
    SilosPokePending = false;
    ++AppPokeCount;
    object *payload = silos_deserialise_value(event.payload, event.payload.root);
    protect(payload);
    object *tag = silos_symbol("poke");
    event_value = cons(tag, payload);
    unprotect();
  }
  protect(event_value);
  object *arguments = cons(event_value, nil);
  protect(arguments);
  (void)apply(SilosActiveHandler, arguments, nullptr);
  ++AppEventCount;
  unprotect();
  unprotect();
}

// Cleanup is intentionally explicit even though this one-app proof does not
// expose a Reload button yet. It prevents old roots or a queued poke from
// leaking into the next app generation when bootstrap/reload is added.
void silos_cleanup_active_app() {
  SilosActiveHandler = nil;
  SilosBoundStoreRef = nil;
  SilosBoundStoreWatch = nil;
  SilosUiSourceRef = nil;
  SilosPokePending = false;
  SilosUiMounted = false;
  xQueueReset(ShellRequestQueue);
  xQueueReset(ShellEventQueue);
}

// This is a test-only observation sink, not a proposed Store or Shell API.
// The Lisp watch has already read LIVE and OLD-VALUE; this hook just records
// that bounded proof data for the native CTest client without invoking the
// app-level handler.
object *fn_silostestwatchobservation (object *args, object *env) {
  (void) env;
  object *old_status = first(cdr(cdr(cdr(args))));
  if (!silos_symbol_is(first(args), "ready") || checkinteger(second(args)) != 2 ||
      !silos_symbol_is(old_status, "pending")) {
    error2("unexpected StoreRef watch observation");
  }
  cstring(checkstring(third(args)), StoreRefWatchObservedDescription,
          sizeof(StoreRefWatchObservedDescription));
  ++StoreRefWatchObservationCount;
  StoreRefWatchObservedReady = true;
  StoreRefWatchObservedCount = true;
  StoreRefWatchObservedOldPending = true;
  return tee;
}

// Like the watch observation hook, this is only a compact CTest witness. The
// actual staging decision stays in the app's ordinary event handler.
object *fn_silostestappstage (object *args, object *env) {
  (void) env;
  const int stage = checkinteger(first(args));
  if (stage == 1 && !AppObservedStageOne) {
    AppObservedStageOne = true;
  } else if (stage == 2 && AppObservedStageOne && !AppObservedStageTwo) {
    AppObservedStageTwo = true;
    AppObservedPokeFifo = AppPokeCount == 2;
  } else {
    error2("unexpected app stage");
  }
  return tee;
}

void silos_mark_roots () {
  markobject(SilosActiveHandler);
  markobject(SilosBoundStoreRef);
  markobject(SilosBoundStoreWatch);
  markobject(SilosUiSourceRef);
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
  "  { \":output\", (fn_ptr_type)OUTPUT, PINMODE, NULL },\n  { \"app-declare\", fn_appdeclare, 0207, doc_appdeclare },\n  { \"app-start\", fn_appstart, 0211, doc_appstart },\n  { \"app-request-poke\", fn_apppoke, 0207, doc_apppoke },\n  { \"store-bind\", fn_storebind, 0244, doc_storebind },\n  { \"store-status\", fn_storestatus, 0211, doc_storestatus },\n  { \"store-error\", fn_storeerror, 0211, doc_storeerror },\n  { \"store-value\", fn_storevalue, 0211, doc_storevalue },\n  { \"store-row-count\", fn_storerowcount, 0211, doc_storerowcount },\n  { \"store-row-at\", fn_storerowat, 0222, doc_storerowat },\n  { \"store-row-id\", fn_storerowid, 0211, doc_storerowid },\n  { \"store-row-revision\", fn_storerowrevision, 0211, doc_storerowrevision },\n  { \"store-row-status\", fn_storerowstatus, 0211, doc_storerowstatus },\n  { \"store-row-error\", fn_storerowerror, 0211, doc_storerowerror },\n  { \"store-row-field\", fn_storerowfield, 0222, doc_storerowfield },\n  { \"store-ref-watch\", fn_storerefwatch, 0222, doc_storerefwatch },\n  { \"ui-type\", fn_uitype, 0327, doc_uitype },\n  { \"ui-bind\", fn_uibind, 0333, doc_uibind },\n  { \"ui-template\", fn_uitemplate, 0327, doc_uitemplate },\n  { \"ui-template-list\", fn_uitemplatelist, 0327, doc_uitemplatelist },\n  { \"ui-mount\", fn_uimount, 0233, doc_uimount },\n  { \"silos-test-watch-observation\", fn_silostestwatchobservation, 0244, NULL },\n  { \"silos-test-app-stage\", fn_silostestappstage, 0211, NULL },\n};"
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
