#pragma once

#include <cstddef>
#include <cstdint>

// This backend intentionally uses fixed-size records rather than heap-backed
// containers.  That keeps the Browser proof shaped like the constrained MCU
// target it is intended to inform.
constexpr std::size_t InMemoryStoreNameCapacity = 48;
constexpr std::size_t InMemoryStoreCapacity = 8;
// Source stores preserve one source line per row. 64 accommodates the
// documented lifecycle/UI proof while remaining a fixed Browser/MCU bound.
constexpr std::size_t InMemoryRowsPerStoreCapacity = 64;
constexpr std::size_t InMemoryFieldsPerRowCapacity = 4;
constexpr std::size_t InMemoryFieldNameCapacity = 16;
constexpr std::size_t InMemoryFieldValueCapacity = 256;

// A field is application data, not backend metadata.  The backend does not
// reserve names such as "text", "desc", or "status"; those names belong to
// the store rows supplied by the startup importer (and, later, by writes).
struct InMemoryStoreFieldInput {
  const char *name;
  const char *value;
};

struct InMemoryStoreField {
  char name[InMemoryFieldNameCapacity]{};
  char value[InMemoryFieldValueCapacity]{};
};

// Every row has stable system metadata plus a bounded list of named string
// fields.  Source rows simply use a "text" field; to-do rows use "desc" and
// "status".  No union or store-specific row layout is needed to hold either.
struct InMemoryStoreRow {
  std::uint32_t id = 0;
  std::uint32_t revision = 0;
  InMemoryStoreField fields[InMemoryFieldsPerRowCapacity]{};
  std::size_t field_count = 0;
};

struct InMemoryStore {
  char name[InMemoryStoreNameCapacity]{};
  InMemoryStoreRow rows[InMemoryRowsPerStoreCapacity]{};
  std::size_t row_count = 0;
};

class InMemoryStoreBackend {
public:
  bool create_store(const char *name);
  bool append_row(const char *store_name, std::uint32_t id,
                  std::uint32_t revision,
                  const InMemoryStoreFieldInput *fields,
                  std::size_t field_count);

  const InMemoryStore *get(const char *name) const;
  const InMemoryStoreField *find_field(const InMemoryStoreRow &row,
                                       const char *name) const;

  template <typename Visitor>
  void visit(Visitor visitor) const {
    for (std::size_t index = 0; index < count_; ++index) visitor(stores_[index]);
  }

private:
  InMemoryStore *find_mutable(const char *name);

  InMemoryStore stores_[InMemoryStoreCapacity]{};
  std::size_t count_ = 0;
};
