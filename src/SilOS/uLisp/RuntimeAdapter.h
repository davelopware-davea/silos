#pragma once

#include <cstddef>

class IPlatformStore;
struct StorageCompletion;

bool silos_ulisp_evaluate(const IPlatformStore &store);
void silos_ulisp_complete_store_bind(const StorageCompletion &completion);
void silos_ulisp_dispatch_shell_event(const struct ShellEvent &event);
void silos_cleanup_apps();
bool silos_ulisp_prepare_apps(std::size_t count);
