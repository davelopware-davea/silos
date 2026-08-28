#include "SilOS/Runtime/State.h"

InMemoryStoreBackend SourceStores;
AppDeclaration AppDeclarations[SilosAppCapacity]{};
bool AppStarted[SilosAppCapacity]{};
std::uint32_t AppGenerations[SilosAppCapacity]{};
std::size_t AppCount = 0;
std::size_t CurrentAppIndex = SilosInvalidAppIndex;
std::uint32_t NextAppGeneration = 0;
bool AppInitialiseDelivered = false;
int AppEventCount = 0;
int AppPokeCount = 0;
bool AppObservedNoBindBeforeInit = false;
bool AppObservedStageOne = false;
bool AppObservedStageTwo = false;
bool AppObservedPokeFifo = false;
bool UiPendingRendered = false;
bool UiReadyRendered = false;
bool StoreBindStartedPending = false;
bool StoreBindCompletedReady = false;
bool StoreRefWatchRegistered = false;
bool StoreRefWatchOldSnapshotCreated = false;
int StoreRefWatchInvocationCount = 0;
int StoreRefWatchObservationCount = 0;
bool StoreRefWatchObservedReady = false;
bool StoreRefWatchObservedCount = false;
bool StoreRefWatchObservedOldPending = false;
bool StoreRefWatchObservedOldValueNil = false;
char StoreRefWatchObservedDescription[StoreRefWatchObservedDescriptionCapacity]{};
