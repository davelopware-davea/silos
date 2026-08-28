const char doc_storebind[] = "(store-bind name fields start count)\nReturns a live StoreRef whose rows become ready asynchronously.";
const char doc_storewatch[] = "(store-watch store-ref handler)\nCalls handler with live and old StoreRef values after a top-level ref change.";
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
struct SilosAppStoreBinding {
  object *ref = nil;
  object *watch = nil;
  char name[StoreNameCapacity]{};
  char field_names[InMemoryFieldsPerRowCapacity][InMemoryFieldNameCapacity]{};
  std::size_t field_count = 0;
  std::size_t start = 0;
  std::size_t count = 0;
};
SilosAppStoreBinding SilosAppStoreBindings[SilosAppCapacity]{};

SilosAppStoreBinding &silos_current_store_binding() {
  if (CurrentAppIndex >= AppCount) error2("no current app");
  return SilosAppStoreBindings[CurrentAppIndex];
}
object *silos_make_pending_bind_ref () {
  object *metadata = nil;
  metadata = silos_prepend_field("error", nil, metadata);
  protect(metadata);
  metadata = silos_prepend_field("count", number(0), metadata);
  unprotect();
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
  object *count_entry = silos_find_field(cdr(metadata_entry), "count");
  if (operation_entry == nil || status_entry == nil || error_entry == nil ||
      count_entry == nil) {
    error2("invalid StoreRef metadata");
  }

  object *metadata = nil;
  metadata = silos_prepend_field("error", cdr(error_entry), metadata);
  protect(metadata);
  metadata = silos_prepend_field("count", cdr(count_entry), metadata);
  unprotect();
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
    object *field_value = lispstring(const_cast<char *>(field->value.c_str()));
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
  const std::size_t row_count = store.row_count();
  const std::size_t first = start < row_count ? start : row_count;
  const std::size_t available = row_count - first;
  const std::size_t selected = count < available ? count : available;
  InMemoryStore::const_iterator first_row = store.begin();
  std::advance(first_row, first);
  InMemoryStore::const_iterator after_last_row = first_row;
  std::advance(after_last_row, selected);
  while (after_last_row != first_row) {
    --after_last_row;
    protect(rows);
    object *row = silos_make_store_row_ref(*after_last_row, field_names,
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
      silos_find_field(cdr(metadata), "count") == nil ||
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

object *silos_store_ref_count(object *ref) {
  object *count = silos_find_field(silos_store_ref_metadata(ref), "count");
  if (!integerp(cdr(count))) error2("invalid StoreRef count");
  return cdr(count);
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
  (void)silos_store_bound_rows(first(args));
  return silos_store_ref_count(first(args));
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

object *fn_storebind (object *args, object *env) {
  (void) env;
  SilosAppStoreBinding &binding = silos_current_store_binding();
  char name[StoreNameCapacity];
  cstring(checkstring(first(args)), name, sizeof(name));
  object *fields = second(args);
  const int start = checkinteger(third(args));
  const int count = checkinteger(first(cdr(cddr(args))));
  if (SourceStores.get(name) == nullptr || start < 0 || count <= 0) {
    error2("unsupported store-bind request");
  }
  binding.field_count = 0;
  while (fields != nil) {
    if (!consp(fields) || binding.field_count == InMemoryFieldsPerRowCapacity ||
        !silos_copy_symbol_name(first(fields),
                                binding.field_names[binding.field_count],
                                InMemoryFieldNameCapacity)) {
      error2("unsupported store-bind fields");
    }
    ++binding.field_count;
    fields = rest(fields);
  }
  if (binding.ref != nil) error2("a store is already bound by this app");

  binding.ref = silos_make_pending_bind_ref();
  std::snprintf(binding.name, sizeof(binding.name), "%s", name);
  binding.start = static_cast<std::size_t>(start);
  binding.count = static_cast<std::size_t>(count);
  StoreBindStartedPending = true;
  StorageRequest request{};
  request.kind = StorageRequestKind::BindStore;
  request.app_index = CurrentAppIndex;
  request.app_generation = AppGenerations[CurrentAppIndex];
  std::snprintf(request.store_name, sizeof(request.store_name), "%s", name);
  if (xQueueSend(StorageRequestQueue, &request, 0) != pdPASS) {
    error2("storage request queue is full");
  }
  std::printf("store-bind=%s status=pending\n", name);
  return binding.ref;
}

object *fn_storewatch (object *args, object *env) {
  (void) env;
  SilosAppStoreBinding &binding = silos_current_store_binding();
  if (first(args) != binding.ref) error2("unknown StoreRef");
  if (binding.watch != nil) error2("StoreRef already has a watch");
  binding.watch = second(args);
  StoreRefWatchRegistered = true;
  return tee;
}

void silos_ulisp_complete_store_bind(const StorageCompletion &completion) {
  if (completion.app_index >= AppCount ||
      completion.app_generation != AppGenerations[completion.app_index]) return;
  SilosAppStoreBinding &binding = SilosAppStoreBindings[completion.app_index];
  const InMemoryStore *store = SourceStores.get(completion.store_name);
  configASSERT(store != nullptr);
  configASSERT(binding.ref != nil);
  configASSERT(std::strcmp(completion.store_name, binding.name) == 0);
  object *value = silos_find_field(binding.ref, "value");
  object *metadata = silos_find_field(binding.ref, "meta");
  configASSERT(value != nil);
  configASSERT(metadata != nil);
  object *status = silos_find_field(cdr(metadata), "status");
  object *count = silos_find_field(cdr(metadata), "count");
  configASSERT(status != nil);
  configASSERT(count != nil);
  object *old_snapshot = silos_snapshot_store_ref(binding.ref);
  protect(old_snapshot);
  StoreRefWatchOldSnapshotCreated = true;
  object *old_value = silos_find_field(old_snapshot, "value");
  configASSERT(old_value != nil);
  StoreRefWatchObservedOldValueNil = cdr(old_value) == nil;
  cdr(value) = silos_make_store_row_refs(*store, binding.field_names,
                                        binding.field_count, binding.start,
                                        binding.count);
  cdr(count) = number(listlength(cdr(value)));
  cdr(status) = silos_symbol("ready");
  configASSERT(binding.watch != nil);
  object *watch_arguments = cons(binding.ref, cons(old_snapshot, nil));
  const std::size_t previous_app = CurrentAppIndex;
  CurrentAppIndex = completion.app_index;
  (void)apply(binding.watch, watch_arguments, nullptr);
  CurrentAppIndex = previous_app;
  ++StoreRefWatchInvocationCount;
  unprotect();
  StoreBindCompletedReady = true;
  silos_render_ui();
  std::printf("store-bind=%s status=ready watch=%d\n", completion.store_name,
              StoreRefWatchInvocationCount);
  const bool ready = true;
  configASSERT(xQueueSend(BindReadyQueue, &ready, portMAX_DELAY) == pdPASS);
}
