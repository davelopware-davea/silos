#pragma once

// The portable UI renderer depends only on resolved text projection. Each
// platform supplies these operations without receiving StoreRefs or mutation
// capabilities.
void silos_ui_surface_begin(const char *state, const char *message);
void silos_ui_surface_add_text(int row_index, const char *field_name,
                               const char *value);
