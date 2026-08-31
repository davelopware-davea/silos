#pragma once

#include <cstddef>

class InMemoryStorageEngine;

struct StoreInitLoadResult {
  std::size_t store_count = 0;
  std::size_t source_row_count = 0;
  std::size_t csv_row_count = 0;
};

// Imports the preloaded directory tree into the volatile catalogue. The exact
// store name is each file's normalized path relative to root, including its
// extension (for example, apps/todo/app.lisp or todo/items.csv).
bool load_store_init(const char *root, InMemoryStorageEngine &stores,
                     StoreInitLoadResult &result);
