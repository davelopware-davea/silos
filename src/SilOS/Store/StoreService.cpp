#include "SilOS/Store/StoreService.h"

StorageCompletion StoreService::process(const StorageRequest &request) const {
  (void)storage_;
  StorageCompletion completion{};
  completion.kind = request.kind;
  completion.result = request.kind == StorageRequestKind::BindStore
      ? StorageResult::Ready : StorageResult::Unsupported;
  completion.app_index = request.app_index;
  completion.app_generation = request.app_generation;
  completion.binding_handle = request.binding_handle;
  completion.bound_store_handle = request.bound_store_handle;
  return completion;
}
