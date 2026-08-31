#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Borrows one field name and value while an adapter copies a row into its own
// storage. Callers own both character ranges for the duration of appendRow.
struct StoreFieldInput {
  std::string_view name;
  std::string_view value;
};

// Owns one materialised platform-row field. IPlatformStore lends this value to
// StoreEngine, which copies it once into the canonical uLisp StoreRowRef.
struct StoreField {
  std::string name;
  std::string value;
};

// Owns one platform row's stable identity, revision, and fields. A platform
// store owns StoreRow instances and lends immutable references during reads.
struct StoreRow {
  std::uint32_t id = 0;
  std::uint32_t revision = 0;
  std::vector<StoreField> fields;
};

// Represents one named platform store. It owns no portable StoreRef state;
// implementations own their row storage and return borrowed rows and fields
// whose addresses remain valid only until that implementation is mutated.
// StoreEngine materialises those borrowed values into canonical uLisp objects.
class IPlatformStore {
public:
  virtual ~IPlatformStore() = default;
  virtual const char *name() const = 0;
  virtual std::size_t rowCount() const = 0;
  virtual const StoreRow *rowAt(std::size_t index) const = 0;
  virtual const StoreField *findField(const StoreRow &row,
                                      const char *name) const = 0;
};
