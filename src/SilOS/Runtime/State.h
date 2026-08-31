#pragma once

#include "SilOS/Shell/Events.h"
#include "SilOS/Store/InMemoryStorageEngine.h"
#include "SilOS/Store/Messages.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

struct sobject;

constexpr std::size_t SilosInvalidAppIndex =
    std::numeric_limits<std::size_t>::max();

struct AppDeclaration {
  std::string name;
  sobject *display_name = nullptr;
  int ideal_width = 0;
  int ideal_height = 0;
  std::string entry;
  bool present = false;
};

extern InMemoryStorageEngine SourceStores;
extern std::vector<AppDeclaration> AppDeclarations;
extern std::vector<bool> AppStarted;
extern std::vector<std::uint32_t> AppGenerations;
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
extern std::string StoreRefWatchObservedDescription;
