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
object *SilosBoundStoreRef = NULL;
object *SilosBoundStoreWatch = NULL;
char SilosBoundStoreName[StoreNameCapacity]{};
char SilosBoundFieldNames[InMemoryFieldsPerRowCapacity][InMemoryFieldNameCapacity]{};
std::size_t SilosBoundFieldCount = 0;
std::size_t SilosBoundStart = 0;
std::size_t SilosBoundCount = 0;
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

object *fn_storewatch (object *args, object *env) {
  (void) env;
  if (first(args) != SilosBoundStoreRef) error2("unknown StoreRef");
  if (SilosBoundStoreWatch != NULL) error2("StoreRef already has a watch");
  // The callback remains rooted until the future app-stop/reload lifecycle
  // releases it. This one-watch lifetime is deliberately the current limit.
  SilosBoundStoreWatch = second(args);
  StoreRefWatchRegistered = true;
  return tee;
}

void silos_ulisp_complete_store_bind(const StorageCompletion &completion) {
  const InMemoryStore *store = SourceStores.get(completion.store_name);
  configASSERT(store != nullptr);
  configASSERT(SilosBoundStoreRef != nil);
  configASSERT(std::strcmp(completion.store_name, SilosBoundStoreName) == 0);
  object *value = silos_find_field(SilosBoundStoreRef, "value");
  object *metadata = silos_find_field(SilosBoundStoreRef, "meta");
  configASSERT(value != nil);
  configASSERT(metadata != nil);
  object *status = silos_find_field(cdr(metadata), "status");
  configASSERT(status != nil);
  object *old_snapshot = silos_snapshot_store_ref(SilosBoundStoreRef);
  protect(old_snapshot);
  StoreRefWatchOldSnapshotCreated = true;
  object *old_value = silos_find_field(old_snapshot, "value");
  configASSERT(old_value != nil);
  StoreRefWatchObservedOldValueNil = cdr(old_value) == nil;
  cdr(value) = silos_make_store_row_refs(*store, SilosBoundFieldNames,
                                        SilosBoundFieldCount, SilosBoundStart,
                                        SilosBoundCount);
  cdr(status) = silos_symbol("ready");
  configASSERT(SilosBoundStoreWatch != NULL);
  object *watch_arguments = cons(SilosBoundStoreRef, cons(old_snapshot, nil));
  (void)apply(SilosBoundStoreWatch, watch_arguments, nullptr);
  ++StoreRefWatchInvocationCount;
  unprotect();
  StoreBindCompletedReady = true;
  silos_render_ui();
  std::printf("store-bind=%s status=ready watch=%d\n", completion.store_name,
              StoreRefWatchInvocationCount);
  const bool ready = true;
  configASSERT(xQueueSend(BindReadyQueue, &ready, portMAX_DELAY) == pdPASS);
}
