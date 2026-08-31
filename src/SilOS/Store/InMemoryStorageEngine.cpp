#include "SilOS/Store/InMemoryStorageEngine.h"

#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

bool InMemoryStorageEngine::createStore(const char *name) {
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

bool InMemoryStorageEngine::appendRow(const char *store_name, std::uint32_t id,
                                      std::uint32_t revision,
                                      const StoreFieldInput *fields,
                                      std::size_t field_count) {
  InMemoryStore *store = findMutable(store_name);
  if (store == nullptr || id == 0 || revision == 0 || fields == nullptr ||
      field_count == 0) return false;
  try {
    StoreRow candidate{};
    candidate.id = id;
    candidate.revision = revision;
    candidate.fields.reserve(field_count);
    for (std::size_t index = 0; index < field_count; ++index) {
      if (fields[index].name.empty()) return false;
      for (const StoreField &earlier : candidate.fields) {
        if (earlier.name == fields[index].name) return false;
      }
      candidate.fields.push_back(
          StoreField{std::string(fields[index].name),
                     std::string(fields[index].value)});
    }
    store->rows_.push_back(std::move(candidate));
  } catch (const std::bad_alloc &) {
    return false;
  } catch (const std::length_error &) {
    return false;
  }
  return true;
}

const IPlatformStore *InMemoryStorageEngine::get(const char *name) const {
  if (name == nullptr) return nullptr;
  for (const InMemoryStore &store : stores_) {
    if (std::strcmp(store.name(), name) == 0) return &store;
  }
  return nullptr;
}

void InMemoryStorageEngine::visit(Visitor visitor, void *context) const {
  if (visitor == nullptr) return;
  for (const InMemoryStore &store : stores_) visitor(store, context);
}

InMemoryStore *InMemoryStorageEngine::findMutable(const char *name) {
  if (name == nullptr) return nullptr;
  for (InMemoryStore &store : stores_) {
    if (store.name_ == name) return &store;
  }
  return nullptr;
}

const StoreRow *InMemoryStore::rowAt(std::size_t index) const {
  if (index >= rows_.size()) return nullptr;
  auto row = rows_.begin();
  std::advance(row, static_cast<std::ptrdiff_t>(index));
  return &*row;
}

const StoreField *InMemoryStore::findField(const StoreRow &row,
                                           const char *name) const {
  if (name == nullptr) return nullptr;
  for (const StoreField &field : row.fields) {
    if (field.name == name) return &field;
  }
  return nullptr;
}
