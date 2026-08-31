#include "SilOS/Store/StoreRuntime.h"

#include "SilOS/Runtime/State.h"
#include "SilOS/Store/StoreEngine.h"
#include "SilOS/Store/StoreService.h"

namespace {
StoreEngine Engine;
StoreService Service(SourceStores);
}

StoreEngine &silos_store_engine() { return Engine; }
StoreService &silos_store_service() { return Service; }
bool silos_prepare_store(std::size_t app_count) {
  return Engine.prepare(app_count, SourceStores);
}
void silos_clear_store() { Engine.clear(); }
void silos_clear_store_app(std::size_t app_index) { Engine.clearApp(app_index); }
bool silos_store_owns_ref(std::size_t app_index, sobject *ref) {
  return Engine.ownsRef(app_index, ref);
}
void silos_store_visit_roots(void (*visitor)(sobject *)) { Engine.visitRoots(visitor); }
void silos_store_move_roots(sobject *from, sobject *to) { Engine.moveRoots(from, to); }
