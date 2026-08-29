#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <vector>

// A field is application data, not backend metadata.  The backend does not
// reserve names such as "text", "desc", or "status"; those names belong to
// the store rows supplied by the startup importer (and, later, by writes).
struct InMemoryStoreFieldInput {
  std::string_view name;
  std::string_view value;
};

struct InMemoryStoreField {
  std::string name;
  std::string value;
};

// Every row has stable system metadata plus a memory-sized list of named string
// fields.  Source rows simply use a "text" field; to-do rows use "desc" and
// "status".  No union or store-specific row layout is needed to hold either.
struct InMemoryStoreRow {
  std::uint32_t id = 0;
  std::uint32_t revision = 0;
  std::vector<InMemoryStoreField> fields;
};

class InMemoryStore {
public:
  using const_iterator = std::list<InMemoryStoreRow>::const_iterator;

  const char *name() const { return name_.c_str(); }
  std::size_t row_count() const { return rows_.size(); }
  const_iterator begin() const { return rows_.begin(); }
  const_iterator end() const { return rows_.end(); }

private:
  friend class InMemoryStoreBackend;

  std::string name_;
  std::list<InMemoryStoreRow> rows_;
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
    for (const InMemoryStore &store : stores_) visitor(store);
  }

private:
  InMemoryStore *find_mutable(const char *name);

  std::list<InMemoryStore> stores_;
};
