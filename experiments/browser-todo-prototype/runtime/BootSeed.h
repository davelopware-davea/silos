#pragma once

class InMemoryStoreBackend;

// Installs the hard-coded application source and volatile to-do records by
// using the same generic store catalogue that the runtime later reads.
bool seed_boot_stores(InMemoryStoreBackend &stores);
