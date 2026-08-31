#include "SilOS/Store/InMemoryStorageEngine.h"
#include "SilOS/Store/StoreService.h"

#include <cstdint>
#include <string>
#include <vector>

int main() {
  InMemoryStorageEngine stores;
  const std::string store_name(180, 's');
  if (!stores.createStore(store_name.c_str())) return 1;
  constexpr std::uint32_t RowCount = 100;
  for (std::uint32_t id = 1; id <= RowCount; ++id) {
    const std::string text(300 + id, static_cast<char>('a' + id % 26));
    std::vector<std::string> names;
    std::vector<StoreFieldInput> fields;
    for (std::size_t field = 0; field < 12; ++field)
      names.push_back(std::string(40, static_cast<char>('A' + field)));
    for (const std::string &name : names) fields.push_back({name, text});
    if (!stores.appendRow(store_name.c_str(), id, 1, fields.data(), fields.size())) return 2;
  }
  const IPlatformStore *store = stores.get(store_name.c_str());
  if (store == nullptr || store->rowCount() != RowCount) return 3;
  for (std::size_t index = 0; index < RowCount; ++index) {
    const StoreRow *row = store->rowAt(index);
    const std::string field_name(40, 'A');
    const StoreField *text = row == nullptr ? nullptr : store->findField(*row, field_name.c_str());
    if (row == nullptr || row->id != index + 1 || text == nullptr ||
        text->value.size() != 301 + index) return 4;
  }
  std::size_t visited = 0;
  stores.visit([](const IPlatformStore &, void *context) {
    ++*static_cast<std::size_t *>(context);
  }, &visited);
  if (visited != 1) return 5;

  StoreService service(stores);
  StorageRequest bind{StorageRequestKind::BindStore, 2, 7, 3, 1};
  StorageCompletion ready = service.process(bind);
  if (ready.result != StorageResult::Ready || ready.binding_handle != 3 ||
      ready.bound_store_handle != 1 || ready.app_generation != 7) return 6;
  bind.kind = StorageRequestKind::UpdateRow;
  return service.process(bind).result == StorageResult::Unsupported ? 0 : 7;
}
