#pragma once

#include <cstddef>
#include <cstdint>

enum class StorageRequestKind { BindStore, UpdateRow, AddRow, DeleteRow };
enum class StorageResult { Ready, Unsupported };

// Carries a bounded, pointer-free operation identity from StoreEngine to
// StoreService. It owns no application or platform data and is safe to copy
// through a FreeRTOS queue.
struct StorageRequest {
  StorageRequestKind kind = StorageRequestKind::BindStore;
  std::size_t app_index = 0;
  std::uint32_t app_generation = 0;
  std::size_t binding_handle = 0;
  std::size_t bound_store_handle = 0;
};

// Carries StoreService's bounded result identity back to StoreEngine. It owns
// no uLisp objects; StoreEngine validates the generation and projects the
// associated platform result while holding the uLisp workspace lock.
struct StorageCompletion {
  StorageRequestKind kind = StorageRequestKind::BindStore;
  StorageResult result = StorageResult::Ready;
  std::size_t app_index = 0;
  std::uint32_t app_generation = 0;
  std::size_t binding_handle = 0;
  std::size_t bound_store_handle = 0;
};
