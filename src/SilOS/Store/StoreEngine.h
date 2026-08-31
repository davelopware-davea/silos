#pragma once

#include "SilOS/Store/IPlatformStorageEngine.h"
#include "SilOS/Store/Messages.h"
#include "SilOS/uLisp/ULispAccess.h"

#include <cstddef>
#include <vector>

// Represents one named store shared by every application binding. It borrows a
// stable IPlatformStore, owns the rooted uLisp writability state exposed by all
// of its StoreRefs, and records the store-wide mutation gate. StoreEngine alone
// mutates its uLisp state; StoreService addresses it by stable numeric handle.
class BoundStore {
public:
  const IPlatformStore *platformStore() const { return platform_store_; }
  ulisp::Object *state() const { return state_; }
  bool blocked() const { return blocked_; }
  void visitRoot(void (*visitor)(ulisp::Object *)) const;
  void moveRoot(ulisp::Object *from, ulisp::Object *to);

private:
  friend class StoreEngine;
  const IPlatformStore *platform_store_ = nullptr;
  ulisp::Object *state_ = nullptr;
  bool blocked_ = false;
};

// Represents one store-bind invocation. It owns the uLisp roots for the live
// StoreRef, watch, requested name, and field list, plus its window and shared
// BoundStore handle. StoreAppBinding owns and moves instances; StoreEngine is
// the only class that interprets or changes their rooted values.
class StoreBinding {
public:
  void visitRoots(void (*visitor)(ulisp::Object *)) const;
  void moveRoot(ulisp::Object *from, ulisp::Object *to);

private:
  friend class StoreEngine;
  ulisp::Object *ref_ = nullptr;
  ulisp::Object *watch_ = nullptr;
  ulisp::Object *name_ = nullptr;
  ulisp::Object *field_names_ = nullptr;
  std::size_t start_ = 0;
  std::size_t count_ = 0;
  std::size_t bound_store_handle_ = 0;
};

// Owns all StoreBinding instances belonging to one application. Its dynamic
// storage is prepared by StoreEngine from the app catalogue and may grow with
// available memory. It exposes no Store policy; StoreEngine uses it for
// identity lookup, cleanup, and GC traversal.
class StoreAppBinding {
public:
  void visitRoots(void (*visitor)(ulisp::Object *)) const;
  void moveRoots(ulisp::Object *from, ulisp::Object *to);

private:
  friend class StoreEngine;
  std::vector<StoreBinding> bindings_;
};

// Owns portable Store binding policy and all per-app and per-store live state.
// It borrows IPlatformStorageEngine stores, roots canonical uLisp StoreRefs,
// sends pointer-free requests through an injected adapter, and consumes keyed
// StoreService completions. UI asks it only whether an app owns a StoreRef.
class StoreEngine {
public:
  using RequestSender = bool (*)(const StorageRequest &request);
  using WatchInvoker = void (*)(std::size_t app_index, ulisp::Object *handler,
                                ulisp::Object *live,
                                ulisp::Object *old_value);
  using RootVisitor = void (*)(ulisp::Object *root);

  bool prepare(std::size_t app_count, const IPlatformStorageEngine &storage);
  void clear();
  void clearApp(std::size_t app_index);

  ulisp::Object *bind(std::size_t app_index, std::uint32_t app_generation,
                      ulisp::Object *args, RequestSender sender);
  ulisp::Object *watch(std::size_t app_index, ulisp::Object *ref,
                       ulisp::Object *handler);
  void complete(const StorageCompletion &completion, WatchInvoker invoke);

  bool ownsRef(std::size_t app_index, ulisp::Object *ref) const;
  std::size_t bindingCount(std::size_t app_index) const;
  bool blocked(std::size_t app_index, ulisp::Object *ref) const;
  bool waitUntilWritable(std::size_t app_index, ulisp::Object *ref,
                         int timeout_ms) const;

  ulisp::Object *status(ulisp::Object *ref) const;
  ulisp::Object *error(ulisp::Object *ref) const;
  ulisp::Object *value(ulisp::Object *ref) const;
  ulisp::Object *rowCount(ulisp::Object *ref) const;
  ulisp::Object *rowAt(ulisp::Object *ref, int index) const;
  ulisp::Object *rowId(ulisp::Object *row) const;
  ulisp::Object *rowRevision(ulisp::Object *row) const;
  ulisp::Object *rowStatus(ulisp::Object *row) const;
  ulisp::Object *rowError(ulisp::Object *row) const;
  ulisp::Object *rowField(ulisp::Object *row, ulisp::Object *field) const;

  void visitRoots(RootVisitor visitor) const;
  void moveRoots(ulisp::Object *from, ulisp::Object *to);

private:
  StoreBinding *findBinding(std::size_t app_index, ulisp::Object *ref);
  const StoreBinding *findBinding(std::size_t app_index,
                                  ulisp::Object *ref) const;
  std::vector<StoreAppBinding> apps_;
  std::vector<BoundStore> stores_;
};
