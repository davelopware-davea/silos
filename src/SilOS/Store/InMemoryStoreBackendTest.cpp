#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstdint>
#include <string>

int main() {
  InMemoryStoreBackend stores;
  if (!stores.create_store("source.lisp")) return 1;

  constexpr std::uint32_t RowCount = 100;
  for (std::uint32_t id = 1; id <= RowCount; ++id) {
    const std::string text(300 + id, static_cast<char>('a' + id % 26));
    const InMemoryStoreFieldInput field{"text", text};
    if (!stores.append_row("source.lisp", id, 1, &field, 1)) return 2;
  }

  const InMemoryStore *store = stores.get("source.lisp");
  if (store == nullptr || store->row_count() != RowCount) return 3;

  std::uint32_t expected_id = 1;
  for (const InMemoryStoreRow &row : *store) {
    const InMemoryStoreField *text = stores.find_field(row, "text");
    if (row.id != expected_id || text == nullptr ||
        text->value.size() != 300 + expected_id) {
      return 4;
    }
    ++expected_id;
  }
  return expected_id == RowCount + 1 ? 0 : 5;
}
