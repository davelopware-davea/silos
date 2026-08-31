#include "SilOS/Store/StoreEngine.h"

#include <new>

namespace {
using ulisp::Object;

Object *at(Object *list, std::size_t index) {
  while (index-- != 0 && list != ulisp::nil) list = ulisp::tail(list);
  return list == ulisp::nil ? ulisp::nil : ulisp::head(list);
}

void move_root(Object *&root, Object *from, Object *to) {
  if (root == from) root = to;
}

Object *metadata(Object *ref) {
  Object *entry = ulisp::findField(ref, "meta");
  if (entry == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "operation") == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "status") == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "count") == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "error") == ulisp::nil ||
      ulisp::findField(ref, "value") == ulisp::nil) {
    ulisp::error("invalid StoreRef");
  }
  return ulisp::tail(entry);
}

Object *row_metadata(Object *row) {
  Object *entry = ulisp::findField(row, "meta");
  if (entry == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "id") == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "revision") == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "status") == ulisp::nil ||
      ulisp::findField(ulisp::tail(entry), "error") == ulisp::nil ||
      ulisp::findField(row, "value") == ulisp::nil) {
    ulisp::error("invalid StoreRowRef");
  }
  return ulisp::tail(entry);
}

Object *make_store_state() {
  return ulisp::prependField("blocked", ulisp::nil, ulisp::nil);
}

Object *make_pending_ref(Object *store_state) {
  Object *meta = ulisp::prependField("error", ulisp::nil, ulisp::nil);
  ulisp::pushRoot(meta);
  meta = ulisp::prependField("count", ulisp::makeNumber(0), meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  meta = ulisp::prependField("status", ulisp::makeSymbol("silos-pending"), meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  meta = ulisp::prependField("operation", ulisp::makeSymbol("bind"), meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  meta = ulisp::prependField("store", store_state, meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  Object *ref = ulisp::prependField("value", ulisp::nil, ulisp::nil);
  ulisp::pushRoot(ref);
  ref = ulisp::prependField("meta", meta, ref);
  ulisp::popRoot(); ulisp::popRoot();
  return ref;
}

Object *snapshot(Object *live) {
  Object *live_meta = metadata(live);
  Object *copy = ulisp::nil;
  const char *fields[] = {"error", "count", "status", "operation", "store"};
  for (const char *name : fields) {
    Object *entry = ulisp::findField(live_meta, name);
    if (entry == ulisp::nil) ulisp::error("invalid StoreRef metadata");
    ulisp::pushRoot(copy);
    copy = ulisp::prependField(name, ulisp::tail(entry), copy);
    ulisp::popRoot();
  }
  ulisp::pushRoot(copy);
  Object *value_entry = ulisp::findField(live, "value");
  Object *result = ulisp::prependField("value", ulisp::tail(value_entry), ulisp::nil);
  ulisp::pushRoot(result);
  result = ulisp::prependField("meta", copy, result);
  ulisp::popRoot(); ulisp::popRoot();
  return result;
}

Object *make_row(const StoreRow &row, Object *field_names) {
  Object *value = ulisp::nil;
  while (field_names != ulisp::nil) {
    if (!ulisp::isCons(field_names) || !ulisp::isSymbol(ulisp::head(field_names)))
      ulisp::error("invalid retained store fields");
    Object *field_name = ulisp::head(field_names);
    const StoreField *selected = nullptr;
    for (const StoreField &field : row.fields) {
      if (ulisp::symbolNameIs(field_name, field.name.c_str())) {
        selected = &field;
        break;
      }
    }
    if (selected != nullptr) {
      ulisp::pushRoot(value);
      Object *field_value = ulisp::makeString(selected->value.c_str());
      ulisp::pushRoot(field_value);
      Object *entry = ulisp::makeCons(field_name, field_value);
      ulisp::pushRoot(entry);
      value = ulisp::makeCons(entry, value);
      ulisp::popRoot(); ulisp::popRoot(); ulisp::popRoot();
    }
    field_names = ulisp::tail(field_names);
  }
  ulisp::pushRoot(value);
  Object *meta = ulisp::prependField("error", ulisp::nil, ulisp::nil);
  ulisp::pushRoot(meta);
  meta = ulisp::prependField("status", ulisp::makeSymbol("silos-ready"), meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  meta = ulisp::prependField("revision", ulisp::makeNumber(static_cast<int>(row.revision)), meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  meta = ulisp::prependField("id", ulisp::makeNumber(static_cast<int>(row.id)), meta);
  ulisp::popRoot(); ulisp::pushRoot(meta);
  Object *ref = ulisp::prependField("value", value, ulisp::nil);
  ulisp::pushRoot(ref);
  ref = ulisp::prependField("meta", meta, ref);
  ulisp::popRoot(); ulisp::popRoot(); ulisp::popRoot();
  return ref;
}

Object *make_rows(const IPlatformStore &store, Object *fields,
                  std::size_t start, std::size_t count) {
  Object *rows = ulisp::nil;
  const std::size_t first = start < store.rowCount() ? start : store.rowCount();
  const std::size_t selected = count < store.rowCount() - first
      ? count : store.rowCount() - first;
  for (std::size_t offset = selected; offset != 0; --offset) {
    const StoreRow *source = store.rowAt(first + offset - 1);
    if (source == nullptr) ulisp::error("platform store row disappeared");
    ulisp::pushRoot(rows);
    Object *row = make_row(*source, fields);
    ulisp::pushRoot(row);
    rows = ulisp::makeCons(row, rows);
    ulisp::popRoot(); ulisp::popRoot();
  }
  return rows;
}
}

void BoundStore::visitRoot(void (*visitor)(ulisp::Object *)) const { visitor(state_); }
void BoundStore::moveRoot(ulisp::Object *from, ulisp::Object *to) { move_root(state_, from, to); }

void StoreBinding::visitRoots(void (*visitor)(ulisp::Object *)) const {
  visitor(ref_); visitor(watch_); visitor(name_); visitor(field_names_);
}
void StoreBinding::moveRoot(ulisp::Object *from, ulisp::Object *to) {
  move_root(ref_, from, to); move_root(watch_, from, to);
  move_root(name_, from, to); move_root(field_names_, from, to);
}
void StoreAppBinding::visitRoots(void (*visitor)(ulisp::Object *)) const {
  for (const StoreBinding &binding : bindings_) binding.visitRoots(visitor);
}
void StoreAppBinding::moveRoots(ulisp::Object *from, ulisp::Object *to) {
  for (StoreBinding &binding : bindings_) binding.moveRoot(from, to);
}

bool StoreEngine::prepare(std::size_t app_count,
                          const IPlatformStorageEngine &storage) {
  try {
    apps_.assign(app_count, StoreAppBinding{});
    stores_.clear();
    storage.visit([](const IPlatformStore &store, void *context) {
      auto *stores = static_cast<std::vector<BoundStore> *>(context);
      BoundStore bound;
      bound.platform_store_ = &store;
      stores->push_back(bound);
    }, &stores_);
    return true;
  } catch (const std::bad_alloc &) {
    clear();
    return false;
  }
}

void StoreEngine::clear() { apps_.clear(); stores_.clear(); }
void StoreEngine::clearApp(std::size_t app_index) {
  if (app_index < apps_.size()) apps_[app_index].bindings_.clear();
}

ulisp::Object *StoreEngine::bind(std::size_t app_index,
                                 std::uint32_t generation,
                                 ulisp::Object *args, RequestSender sender) {
  if (app_index >= apps_.size()) ulisp::error("no current app");
  Object *name = ulisp::head(args);
  Object *fields = ulisp::secondValue(args);
  if (!ulisp::isString(name)) ulisp::error("unsupported store-bind request");
  const int start = ulisp::checkInteger(ulisp::thirdValue(args));
  const int count = ulisp::checkInteger(at(args, 3));
  if (start < 0 || count <= 0) ulisp::error("unsupported store-bind request");
  for (Object *cursor = fields; cursor != ulisp::nil; cursor = ulisp::tail(cursor)) {
    if (!ulisp::isCons(cursor) || !ulisp::isSymbol(ulisp::head(cursor)))
      ulisp::error("unsupported store-bind fields");
  }
  std::size_t store_handle = 0;
  for (std::size_t index = 0; index < stores_.size(); ++index) {
    if (ulisp::stringEquals(name, stores_[index].platform_store_->name())) {
      store_handle = index + 1;
      break;
    }
  }
  if (store_handle == 0) ulisp::error("unsupported store-bind request");
  BoundStore &store = stores_[store_handle - 1];
  if (store.state_ == ulisp::nil) store.state_ = make_store_state();
  StoreBinding binding;
  binding.ref_ = make_pending_ref(store.state_);
  binding.name_ = name;
  binding.field_names_ = fields;
  binding.start_ = static_cast<std::size_t>(start);
  binding.count_ = static_cast<std::size_t>(count);
  binding.bound_store_handle_ = store_handle;
  ulisp::pushRoot(binding.ref_);
  try {
    apps_[app_index].bindings_.push_back(binding);
  } catch (const std::bad_alloc &) {
    ulisp::popRoot();
    ulisp::error("store binding allocation failed");
  }
  ulisp::popRoot();
  const std::size_t binding_handle = apps_[app_index].bindings_.size();
  StorageRequest request{StorageRequestKind::BindStore, app_index, generation,
                         binding_handle, store_handle};
  if (sender == nullptr || !sender(request)) {
    apps_[app_index].bindings_.pop_back();
    ulisp::error("storage request queue is full");
  }
  return apps_[app_index].bindings_.back().ref_;
}

ulisp::Object *StoreEngine::watch(std::size_t app_index, ulisp::Object *ref,
                                  ulisp::Object *handler) {
  StoreBinding *binding = findBinding(app_index, ref);
  if (binding == nullptr) ulisp::error("unknown StoreRef");
  if (binding->watch_ != ulisp::nil) ulisp::error("StoreRef already has a watch");
  binding->watch_ = handler;
  return ulisp::trueValue();
}

void StoreEngine::complete(const StorageCompletion &completion,
                           WatchInvoker invoke) {
  if (completion.result != StorageResult::Ready ||
      completion.app_index >= apps_.size() || completion.binding_handle == 0 ||
      completion.binding_handle > apps_[completion.app_index].bindings_.size()) return;
  StoreBinding &binding =
      apps_[completion.app_index].bindings_[completion.binding_handle - 1];
  if (binding.bound_store_handle_ != completion.bound_store_handle ||
      completion.bound_store_handle == 0 ||
      completion.bound_store_handle > stores_.size()) return;
  const IPlatformStore *store = stores_[completion.bound_store_handle - 1].platform_store_;
  Object *old = snapshot(binding.ref_);
  ulisp::pushRoot(old);
  Object *value_entry = ulisp::findField(binding.ref_, "value");
  Object *meta = metadata(binding.ref_);
  Object *status_entry = ulisp::findField(meta, "status");
  Object *count_entry = ulisp::findField(meta, "count");
  Object *rows = make_rows(*store, binding.field_names_, binding.start_, binding.count_);
  ulisp::setTail(value_entry, rows);
  ulisp::setTail(count_entry, ulisp::makeNumber(ulisp::listLength(rows)));
  ulisp::setTail(status_entry, ulisp::makeSymbol("silos-ready"));
  if (binding.watch_ != ulisp::nil && invoke != nullptr)
    invoke(completion.app_index, binding.watch_, binding.ref_, old);
  ulisp::popRoot();
}

StoreBinding *StoreEngine::findBinding(std::size_t app_index, Object *ref) {
  if (app_index >= apps_.size()) return nullptr;
  for (StoreBinding &binding : apps_[app_index].bindings_)
    if (binding.ref_ == ref) return &binding;
  return nullptr;
}
const StoreBinding *StoreEngine::findBinding(std::size_t app_index,
                                             Object *ref) const {
  if (app_index >= apps_.size()) return nullptr;
  for (const StoreBinding &binding : apps_[app_index].bindings_)
    if (binding.ref_ == ref) return &binding;
  return nullptr;
}
bool StoreEngine::ownsRef(std::size_t app_index, Object *ref) const {
  return findBinding(app_index, ref) != nullptr;
}
std::size_t StoreEngine::bindingCount(std::size_t app_index) const {
  return app_index < apps_.size() ? apps_[app_index].bindings_.size() : 0;
}
bool StoreEngine::blocked(std::size_t app_index, Object *ref) const {
  const StoreBinding *binding = findBinding(app_index, ref);
  if (binding == nullptr) ulisp::error("unknown StoreRef");
  return stores_[binding->bound_store_handle_ - 1].blocked_;
}
bool StoreEngine::waitUntilWritable(std::size_t app_index, Object *ref,
                                    int timeout_ms) const {
  if (timeout_ms < 0 || timeout_ms > 1000) ulisp::error("invalid store wait timeout");
  return !blocked(app_index, ref);
}

Object *StoreEngine::status(Object *ref) const {
  return ulisp::tail(ulisp::findField(metadata(ref), "status"));
}
Object *StoreEngine::error(Object *ref) const {
  return ulisp::tail(ulisp::findField(metadata(ref), "error"));
}
Object *StoreEngine::value(Object *ref) const {
  if (!ulisp::symbolIs(status(ref), "silos-ready")) ulisp::error("StoreRef is not ready");
  return ulisp::tail(ulisp::findField(ref, "value"));
}
Object *StoreEngine::rowCount(Object *ref) const {
  Object *meta = metadata(ref);
  if (!ulisp::symbolIs(ulisp::tail(ulisp::findField(meta, "operation")), "bind"))
    ulisp::error("StoreRef does not contain bound rows");
  (void)value(ref);
  Object *count = ulisp::tail(ulisp::findField(meta, "count"));
  if (!ulisp::isInteger(count)) ulisp::error("invalid StoreRef count");
  return count;
}
Object *StoreEngine::rowAt(Object *ref, int index) const {
  if (index < 0) ulisp::error("StoreRowRef index is negative");
  Object *rows = value(ref);
  while (index-- > 0 && rows != ulisp::nil) rows = ulisp::tail(rows);
  if (rows == ulisp::nil) ulisp::error("StoreRowRef index out of bounds");
  Object *row = ulisp::head(rows); (void)row_metadata(row); return row;
}
Object *StoreEngine::rowId(Object *row) const { return ulisp::tail(ulisp::findField(row_metadata(row), "id")); }
Object *StoreEngine::rowRevision(Object *row) const { return ulisp::tail(ulisp::findField(row_metadata(row), "revision")); }
Object *StoreEngine::rowStatus(Object *row) const { return ulisp::tail(ulisp::findField(row_metadata(row), "status")); }
Object *StoreEngine::rowError(Object *row) const { return ulisp::tail(ulisp::findField(row_metadata(row), "error")); }
Object *StoreEngine::rowField(Object *row, Object *field) const {
  Object *state = rowStatus(row);
  if (!ulisp::symbolIs(state, "silos-ready") && !ulisp::symbolIs(state, "silos-saving"))
    ulisp::error("StoreRowRef fields are not readable");
  if (!ulisp::isSymbol(field)) ulisp::error("StoreRowRef field name is not a supported symbol");
  Object *entry = ulisp::findSymbolField(ulisp::tail(ulisp::findField(row, "value")), field);
  if (entry == ulisp::nil) ulisp::error("unknown StoreRowRef field");
  return ulisp::tail(entry);
}
void StoreEngine::visitRoots(RootVisitor visitor) const {
  for (const BoundStore &store : stores_) store.visitRoot(visitor);
  for (const StoreAppBinding &app : apps_) app.visitRoots(visitor);
}
void StoreEngine::moveRoots(Object *from, Object *to) {
  for (BoundStore &store : stores_) store.moveRoot(from, to);
  for (StoreAppBinding &app : apps_) app.moveRoots(from, to);
}
