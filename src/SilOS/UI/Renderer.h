#pragma once

#include <cstddef>

enum class UiRenderState { Pending, Ready, Empty, Error };

void silos_ui_render_begin();
void silos_ui_render_app_begin(std::size_t app_index, const char *name);
void silos_ui_render_template_begin(std::size_t app_index,
                                    std::size_t mount_index,
                                    UiRenderState state, const char *message,
                                    bool is_list);
void silos_ui_render_text(std::size_t app_index, std::size_t mount_index,
                          int row_index, const char *field_name,
                          const char *value);
