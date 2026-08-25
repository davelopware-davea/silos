#include "SilOS/Runtime/AppBootstrap.h"

#include "SilOS/Runtime/State.h"
#include "SilOS/Store/InMemoryStoreBackend.h"
#include "SilOS/uLisp/RuntimeAdapter.h"

#include <cstdio>
#include <cstring>

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
  stores.visit([&](const InMemoryStore &store) {
    if (!is_app_manifest_name(store.name)) return;
    found_manifest = true;
    silos_cleanup_active_app();
    ++ActiveAppGeneration;
    CurrentDeclaration = AppDeclaration{};
    AppStarted = false;
    loaded = silos_ulisp_evaluate(store) && CurrentDeclaration.present && loaded;
    const InMemoryStore *entry = stores.get(CurrentDeclaration.entry);
    loaded = entry != nullptr && silos_ulisp_evaluate(*entry) && AppStarted && loaded;
    std::printf("manifest=%s app=%s entry=%s started=%s\n", store.name,
                CurrentDeclaration.name, CurrentDeclaration.entry,
                AppStarted ? "yes" : "no");
  });
  return found_manifest && loaded;
}
