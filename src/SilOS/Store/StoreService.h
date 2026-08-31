#pragma once

#include "SilOS/Store/IPlatformStorageEngine.h"
#include "SilOS/Store/Messages.h"

// Owns Store-task-side request dispatch. It borrows the platform storage
// engine, performs no uLisp access, and returns pointer-free keyed completions
// to StoreEngine. Bind is currently an acknowledgement; mutation cases are
// explicit placeholders until their persistence semantics are implemented.
class StoreService {
public:
  explicit StoreService(const IPlatformStorageEngine &storage)
      : storage_(&storage) {}
  StorageCompletion process(const StorageRequest &request) const;

private:
  const IPlatformStorageEngine *storage_;
};
