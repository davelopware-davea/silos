#include "SilOS/UI/Renderer.h"

#include "SilOS/UI/PlatformSurface.h"

namespace {
const char *state_name(UiRenderState state) {
  switch (state) {
    case UiRenderState::Pending: return "pending";
    case UiRenderState::Ready: return "ready";
    case UiRenderState::Empty: return "empty";
    case UiRenderState::Error: return "error";
  }
  return "error";
}
}

void silos_ui_render_begin() { silos_ui_surface_begin(); }

void silos_ui_render_app_begin(std::size_t app_index, const char *name) {
  silos_ui_surface_begin_app(app_index, name);
}

void silos_ui_render_template_begin(std::size_t app_index,
                                    std::size_t mount_index,
                                    UiRenderState state, const char *message,
                                    bool is_list) {
  silos_ui_surface_begin_template(app_index, mount_index, state_name(state),
                                  message, is_list);
}

void silos_ui_render_text(std::size_t app_index, std::size_t mount_index,
                          int row_index, const char *field_name,
                          const char *value) {
  silos_ui_surface_add_text(app_index, mount_index, row_index, field_name,
                            value);
}
