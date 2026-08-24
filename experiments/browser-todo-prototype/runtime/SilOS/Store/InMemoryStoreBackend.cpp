#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstdio>
#include <cstring>

namespace {
bool copy_bounded_string(char destination[], std::size_t destination_capacity,
                         const char *source, bool require_nonempty) {
  if (source == nullptr || (require_nonempty && source[0] == '\0') ||
      std::strlen(source) >= destination_capacity) {
    return false;
  }
  std::snprintf(destination, destination_capacity, "%s", source);
  return true;
}
}

bool InMemoryStoreBackend::create_store(const char *name) {
  if (count_ == InMemoryStoreCapacity || get(name) != nullptr) return false;

  InMemoryStore &store = stores_[count_];
  if (!copy_bounded_string(store.name, sizeof(store.name), name, true)) return false;
  ++count_;
  return true;
}

bool InMemoryStoreBackend::append_row(const char *store_name, std::uint32_t id,
                                      std::uint32_t revision,
                                      const InMemoryStoreFieldInput *fields,
                                      std::size_t field_count) {
  InMemoryStore *store = find_mutable(store_name);
  if (store == nullptr || id == 0 || revision == 0 || fields == nullptr ||
      field_count == 0 || field_count > InMemoryFieldsPerRowCapacity ||
      store->row_count == InMemoryRowsPerStoreCapacity) {
    return false;
  }

  InMemoryStoreRow candidate{};
  candidate.id = id;
  candidate.revision = revision;
  candidate.field_count = field_count;
  for (std::size_t index = 0; index < field_count; ++index) {
    if (fields[index].value == nullptr ||
        !copy_bounded_string(candidate.fields[index].name,
                             sizeof(candidate.fields[index].name),
                             fields[index].name, true) ||
        !copy_bounded_string(candidate.fields[index].value,
                             sizeof(candidate.fields[index].value),
                             fields[index].value, false)) {
      return false;
    }
    for (std::size_t earlier = 0; earlier < index; ++earlier) {
      if (std::strcmp(candidate.fields[earlier].name,
                      candidate.fields[index].name) == 0) {
        return false;
      }
    }
  }

  // Only publish a completely validated row, so a rejected seed cannot leave
  // a half-shaped record in the catalogue.
  store->rows[store->row_count++] = candidate;
  return true;
}

const InMemoryStore *InMemoryStoreBackend::get(const char *name) const {
  if (name == nullptr) return nullptr;
  for (std::size_t index = 0; index < count_; ++index) {
    if (std::strcmp(stores_[index].name, name) == 0) return &stores_[index];
  }
  return nullptr;
}

const InMemoryStoreField *InMemoryStoreBackend::find_field(
    const InMemoryStoreRow &row, const char *name) const {
  if (name == nullptr) return nullptr;
  for (std::size_t index = 0; index < row.field_count; ++index) {
    if (std::strcmp(row.fields[index].name, name) == 0) return &row.fields[index];
  }
  return nullptr;
}

InMemoryStore *InMemoryStoreBackend::find_mutable(const char *name) {
  if (name == nullptr) return nullptr;
  for (std::size_t index = 0; index < count_; ++index) {
    if (std::strcmp(stores_[index].name, name) == 0) return &stores_[index];
  }
  return nullptr;
}
