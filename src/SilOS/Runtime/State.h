#pragma once

#include "SilOS/Shell/Events.h"
#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstddef>
#include <cstdint>

constexpr std::size_t StoreNameCapacity = InMemoryStoreNameCapacity;
constexpr std::size_t StoreRefWatchObservedDescriptionCapacity = 256;
constexpr std::size_t SilosAppCapacity = 4;
constexpr std::size_t SilosInvalidAppIndex = SilosAppCapacity;

struct AppDeclaration {
  char name[32]{};
  int ideal_width = 0;
  int ideal_height = 0;
  char entry[StoreNameCapacity]{};
  bool present = false;
};

enum class StorageRequestKind { BindStore };
struct StorageRequest {
  StorageRequestKind kind = StorageRequestKind::BindStore;
  std::size_t app_index = 0;
  std::uint32_t app_generation = 0;
  char store_name[StoreNameCapacity]{};
};
struct StorageCompletion {
  StorageRequestKind kind = StorageRequestKind::BindStore;
  std::size_t app_index = 0;
  std::uint32_t app_generation = 0;
  char store_name[StoreNameCapacity]{};
};

extern InMemoryStoreBackend SourceStores;
extern AppDeclaration AppDeclarations[SilosAppCapacity];
extern bool AppStarted[SilosAppCapacity];
extern std::uint32_t AppGenerations[SilosAppCapacity];
extern std::size_t AppCount;
extern std::size_t CurrentAppIndex;
extern std::uint32_t NextAppGeneration;
extern bool AppInitialiseDelivered;
extern int AppEventCount;
extern int AppPokeCount;
extern bool AppObservedNoBindBeforeInit;
extern bool AppObservedStageOne;
extern bool AppObservedStageTwo;
extern bool AppObservedPokeFifo;
extern bool UiPendingRendered;
extern bool UiReadyRendered;
extern bool StoreBindStartedPending;
extern bool StoreBindCompletedReady;
extern bool StoreRefWatchRegistered;
extern bool StoreRefWatchOldSnapshotCreated;
extern int StoreRefWatchInvocationCount;
extern int StoreRefWatchObservationCount;
extern bool StoreRefWatchObservedReady;
extern bool StoreRefWatchObservedCount;
extern bool StoreRefWatchObservedOldPending;
extern bool StoreRefWatchObservedOldValueNil;
extern char StoreRefWatchObservedDescription[StoreRefWatchObservedDescriptionCapacity];
