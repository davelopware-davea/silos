#pragma once

#include "SilOS/Store/IPlatformStore.h"

// Defines the platform storage catalogue required by portable Store modules.
// An adapter owns every IPlatformStore it exposes and must keep their identity
// stable while StoreEngine and StoreService borrow them after bootstrap.
// Lookup and visitation do not allocate or transfer ownership.
class IPlatformStorageEngine {
public:
  using Visitor = void (*)(const IPlatformStore &store, void *context);

  virtual ~IPlatformStorageEngine() = default;
  virtual const IPlatformStore *get(const char *name) const = 0;
  virtual void visit(Visitor visitor, void *context) const = 0;
};
