#pragma once

#include <cstddef>

// The platform receives only renderer-resolved identities and text. It never
// receives StoreRefs, Lisp objects, or mutation capabilities.
void silos_ui_surface_begin();
void silos_ui_surface_begin_app(std::size_t app_index, const char *name);
void silos_ui_surface_begin_template(std::size_t app_index,
                                     std::size_t mount_index,
                                     const char *state, const char *message,
                                     bool is_list);
void silos_ui_surface_add_text(std::size_t app_index,
                               std::size_t mount_index, int row_index,
                               const char *field_name, const char *value);
