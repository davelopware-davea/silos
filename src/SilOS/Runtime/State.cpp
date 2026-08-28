#include "SilOS/Runtime/State.h"

InMemoryStoreBackend SourceStores;
AppDeclaration CurrentDeclaration;
bool AppStarted = false;
std::uint32_t ActiveAppGeneration = 0;
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
