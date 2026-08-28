#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

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
  if (!copy_bounded_string(store.name_, sizeof(store.name_), name, true)) return false;
  ++count_;
  return true;
}

bool InMemoryStoreBackend::append_row(const char *store_name, std::uint32_t id,
                                      std::uint32_t revision,
                                      const InMemoryStoreFieldInput *fields,
                                      std::size_t field_count) {
  InMemoryStore *store = find_mutable(store_name);
  if (store == nullptr || id == 0 || revision == 0 || fields == nullptr ||
      field_count == 0 || field_count > InMemoryFieldsPerRowCapacity) {
    return false;
  }

  try {
    InMemoryStoreRow candidate{};
    candidate.id = id;
    candidate.revision = revision;
    candidate.field_count = field_count;
    for (std::size_t index = 0; index < field_count; ++index) {
      if (fields[index].name.empty() ||
          fields[index].name.size() >= sizeof(candidate.fields[index].name)) {
        return false;
      }
      std::memcpy(candidate.fields[index].name, fields[index].name.data(),
                  fields[index].name.size());
      candidate.fields[index].name[fields[index].name.size()] = '\0';
      candidate.fields[index].value.assign(fields[index].value);
      for (std::size_t earlier = 0; earlier < index; ++earlier) {
        if (std::strcmp(candidate.fields[earlier].name,
                        candidate.fields[index].name) == 0) {
          return false;
        }
      }
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
  for (std::size_t index = 0; index < count_; ++index) {
    if (std::strcmp(stores_[index].name_, name) == 0) return &stores_[index];
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
    if (std::strcmp(stores_[index].name_, name) == 0) return &stores_[index];
  }
  return nullptr;
}
