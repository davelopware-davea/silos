#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstdint>
#include <string>
#include <vector>

int main() {
  InMemoryStoreBackend stores;
  const std::string store_name(180, 's');
  if (!stores.create_store(store_name.c_str())) return 1;

  constexpr std::uint32_t RowCount = 100;
  for (std::uint32_t id = 1; id <= RowCount; ++id) {
    const std::string text(300 + id, static_cast<char>('a' + id % 26));
    std::vector<std::string> names;
    std::vector<InMemoryStoreFieldInput> fields;
    for (std::size_t field = 0; field < 12; ++field) {
      names.push_back(std::string(40, static_cast<char>('A' + field)));
    }
    for (const std::string &name : names) fields.push_back({name, text});
    if (!stores.append_row(store_name.c_str(), id, 1, fields.data(),
                           fields.size())) return 2;
  }

  const InMemoryStore *store = stores.get(store_name.c_str());
  if (store == nullptr || store->row_count() != RowCount) return 3;

  std::uint32_t expected_id = 1;
  for (const InMemoryStoreRow &row : *store) {
    const std::string field_name(40, 'A');
    const InMemoryStoreField *text = stores.find_field(row, field_name.c_str());
    if (row.id != expected_id || text == nullptr ||
        text->value.size() != 300 + expected_id) {
      return 4;
    }
    ++expected_id;
  }
  return expected_id == RowCount + 1 ? 0 : 5;
}
