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
const char doc_storeblockedp[] = "(store-blocked-p store-ref)\nReturns t while the StoreRef's shared store cannot accept a mutation.";
const char doc_storewaituntilwritable[] = "(store-wait-until-writable store-ref timeout-ms)\nWaits at most 1000 ms for the shared store to become writable.";

namespace {
std::size_t current_store_app() {
  if (CurrentAppIndex >= AppCount) error2("no current app");
  return CurrentAppIndex;
}
bool send_store_request(const StorageRequest &request) {
  return xQueueSend(StorageRequestQueue, &request, 0) == pdPASS;
}
void invoke_store_watch(std::size_t app_index, object *handler,
                        object *live, object *old_value) {
  object *old_entry = silos_find_field(old_value, "value");
  StoreRefWatchOldSnapshotCreated = true;
  StoreRefWatchObservedOldValueNil = old_entry != nil && cdr(old_entry) == nil;
  protect(old_value);
  object *arguments = cons(live, cons(old_value, nil));
  const std::size_t previous_app = CurrentAppIndex;
  CurrentAppIndex = app_index;
  (void)apply(handler, arguments, nullptr);
  CurrentAppIndex = previous_app;
  ++StoreRefWatchInvocationCount;
  unprotect();
}
}

object *fn_storestatus(object *args, object *) { return silos_store_engine().status(first(args)); }
object *fn_storeerror(object *args, object *) { return silos_store_engine().error(first(args)); }
object *fn_storevalue(object *args, object *) { return silos_store_engine().value(first(args)); }
object *fn_storerowcount(object *args, object *) { return silos_store_engine().rowCount(first(args)); }
object *fn_storerowat(object *args, object *) { return silos_store_engine().rowAt(first(args), checkinteger(second(args))); }
object *fn_storerowid(object *args, object *) { return silos_store_engine().rowId(first(args)); }
object *fn_storerowrevision(object *args, object *) { return silos_store_engine().rowRevision(first(args)); }
object *fn_storerowstatus(object *args, object *) { return silos_store_engine().rowStatus(first(args)); }
object *fn_storerowerror(object *args, object *) { return silos_store_engine().rowError(first(args)); }
object *fn_storerowfield(object *args, object *) { return silos_store_engine().rowField(first(args), second(args)); }
object *fn_storebind(object *args, object *) {
  const std::size_t app = current_store_app();
  object *ref = silos_store_engine().bind(app, AppGenerations[app], args,
                                          send_store_request);
  StoreBindStartedPending = true;
  return ref;
}
object *fn_storewatch(object *args, object *) {
  object *result = silos_store_engine().watch(current_store_app(), first(args), second(args));
  StoreRefWatchRegistered = true;
  return result;
}
object *fn_storeblockedp(object *args, object *) {
  return silos_store_engine().blocked(current_store_app(), first(args)) ? tee : nil;
}
object *fn_storewaituntilwritable(object *args, object *) {
  return silos_store_engine().waitUntilWritable(current_store_app(), first(args),
                                                 checkinteger(second(args))) ? tee : nil;
}
void silos_ulisp_complete_store_bind(const StorageCompletion &completion) {
  if (completion.app_index >= AppCount ||
      completion.app_generation != AppGenerations[completion.app_index]) return;
  silos_store_engine().complete(completion, invoke_store_watch);
  StoreBindCompletedReady = true;
  const bool ready = true;
  configASSERT(xQueueSend(BindReadyQueue, &ready, portMAX_DELAY) == pdPASS);
}
