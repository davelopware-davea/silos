#include "SilOS/Runtime/AppBootstrap.h"

#include "SilOS/Runtime/State.h"
#include "SilOS/Store/InMemoryStoreBackend.h"
#include "SilOS/uLisp/RuntimeAdapter.h"

#include <cstdio>
#include <cstring>
#include <new>

namespace {
bool is_app_manifest_name(const char *name) {
  constexpr char Prefix[] = "apps/";
  constexpr char Suffix[] = "/app.lisp";
  if (std::strncmp(name, Prefix, std::strlen(Prefix)) != 0) return false;
  const char *app_name = name + std::strlen(Prefix);
  const char *slash = std::strchr(app_name, '/');
  return slash != app_name && slash != nullptr && std::strcmp(slash, Suffix) == 0;
}
}

bool silos_bootstrap_apps(InMemoryStoreBackend &stores) {
  bool found_manifest = false;
  bool loaded = true;
  silos_cleanup_apps();
  std::size_t manifest_count = 0;
  stores.visit([&](const InMemoryStore &store) {
    if (is_app_manifest_name(store.name())) ++manifest_count;
  });
  try {
    AppDeclarations.resize(manifest_count);
    AppStarted.resize(manifest_count);
    AppGenerations.resize(manifest_count);
  } catch (const std::bad_alloc &) {
    AppDeclarations.clear();
    AppStarted.clear();
    AppGenerations.clear();
    return false;
  }
  if (!silos_ulisp_prepare_apps(manifest_count)) return false;
  stores.visit([&](const InMemoryStore &store) {
    if (!is_app_manifest_name(store.name())) return;
    found_manifest = true;
    CurrentAppIndex = AppCount++;
    AppDeclarations[CurrentAppIndex] = AppDeclaration{};
    AppStarted[CurrentAppIndex] = false;
    AppGenerations[CurrentAppIndex] = ++NextAppGeneration;
    loaded = silos_ulisp_evaluate(store) &&
             AppDeclarations[CurrentAppIndex].present && loaded;
    const InMemoryStore *entry =
        stores.get(AppDeclarations[CurrentAppIndex].entry.c_str());
    loaded = entry != nullptr && silos_ulisp_evaluate(*entry) &&
             AppStarted[CurrentAppIndex] && loaded;
    std::printf("manifest=%s app=%s entry=%s started=%s\n", store.name(),
                AppDeclarations[CurrentAppIndex].name.c_str(),
                AppDeclarations[CurrentAppIndex].entry.c_str(),
                AppStarted[CurrentAppIndex] ? "yes" : "no");
  });
  CurrentAppIndex = SilosInvalidAppIndex;
  return found_manifest && loaded;
}
