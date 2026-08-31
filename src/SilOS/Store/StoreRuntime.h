#pragma once

#include "SilOS/Store/Messages.h"

#include <cstddef>

class StoreEngine;
class StoreService;
struct sobject;

StoreEngine &silos_store_engine();
StoreService &silos_store_service();
bool silos_prepare_store(std::size_t app_count);
void silos_clear_store();
void silos_clear_store_app(std::size_t app_index);
bool silos_store_owns_ref(std::size_t app_index, sobject *ref);
void silos_store_visit_roots(void (*visitor)(sobject *));
void silos_store_move_roots(sobject *from, sobject *to);
