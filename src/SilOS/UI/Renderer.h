#pragma once

enum class UiRenderState { Pending, Ready, Empty, Error };

void silos_ui_render_begin(UiRenderState state, const char *message);
void silos_ui_render_text(int row_index, const char *field_name,
                          const char *value);
