#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

bool InMemoryStoreBackend::create_store(const char *name) {
  if (name == nullptr || name[0] == '\0' || get(name) != nullptr) return false;
  try {
    InMemoryStore store;
    store.name_ = name;
    stores_.push_back(std::move(store));
    return true;
  } catch (const std::bad_alloc &) {
    return false;
  } catch (const std::length_error &) {
    return false;
  }
}

bool InMemoryStoreBackend::append_row(const char *store_name, std::uint32_t id,
                                      std::uint32_t revision,
                                      const InMemoryStoreFieldInput *fields,
                                      std::size_t field_count) {
  InMemoryStore *store = find_mutable(store_name);
  if (store == nullptr || id == 0 || revision == 0 || fields == nullptr ||
      field_count == 0) {
    return false;
  }

  try {
    InMemoryStoreRow candidate{};
    candidate.id = id;
    candidate.revision = revision;
    candidate.fields.reserve(field_count);
    for (std::size_t index = 0; index < field_count; ++index) {
      if (fields[index].name.empty()) return false;
      for (const InMemoryStoreField &earlier : candidate.fields) {
        if (earlier.name == fields[index].name) return false;
      }
      candidate.fields.push_back(
          InMemoryStoreField{std::string(fields[index].name),
                             std::string(fields[index].value)});
    }

    // Only publish a completely validated row, so a rejected seed cannot leave
    // a half-shaped record in the catalogue.
    store->rows_.push_back(std::move(candidate));
  } catch (const std::bad_alloc &) {
    return false;
  } catch (const std::length_error &) {
    return false;
  }
  return true;
}

const InMemoryStore *InMemoryStoreBackend::get(const char *name) const {
  if (name == nullptr) return nullptr;
  for (const InMemoryStore &store : stores_) {
    if (store.name_ == name) return &store;
  }
  return nullptr;
}

const InMemoryStoreField *InMemoryStoreBackend::find_field(
    const InMemoryStoreRow &row, const char *name) const {
  if (name == nullptr) return nullptr;
  for (const InMemoryStoreField &field : row.fields) {
    if (field.name == name) return &field;
  }
  return nullptr;
}

InMemoryStore *InMemoryStoreBackend::find_mutable(const char *name) {
  if (name == nullptr) return nullptr;
  for (InMemoryStore &store : stores_) {
    if (store.name_ == name) return &store;
  }
  return nullptr;
}
