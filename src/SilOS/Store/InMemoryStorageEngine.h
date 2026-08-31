#pragma once

#include "SilOS/Store/IPlatformStorageEngine.h"

#include <list>

// Stores one named collection of rows in dynamically allocated host memory.
// It owns its name and linked rows, implements IPlatformStore for borrowed
// reads, and is created and mutated only by InMemoryStorageEngine.
class InMemoryStore final : public IPlatformStore {
public:
  const char *name() const override { return name_.c_str(); }
  std::size_t rowCount() const override { return rows_.size(); }
  const StoreRow *rowAt(std::size_t index) const override;
  const StoreField *findField(const StoreRow &row,
                              const char *name) const override;

private:
  friend class InMemoryStorageEngine;
  std::string name_;
  std::list<StoreRow> rows_;
};

// Implements the platform storage interfaces with an in-memory catalogue. It
// owns every InMemoryStore and all row strings; StoreEngine, StoreService, and
// bootstrap code borrow stable store identities while the catalogue is live.
class InMemoryStorageEngine final : public IPlatformStorageEngine {
public:
  bool createStore(const char *name);
  bool appendRow(const char *store_name, std::uint32_t id,
                 std::uint32_t revision, const StoreFieldInput *fields,
                 std::size_t field_count);

  const IPlatformStore *get(const char *name) const override;
  void visit(Visitor visitor, void *context) const override;

private:
  InMemoryStore *findMutable(const char *name);
  std::list<InMemoryStore> stores_;
};
