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

void silos_ui_render_begin(UiRenderState state, const char *message) {
  silos_ui_surface_begin(state_name(state), message);
}

void silos_ui_render_text(int row_index, const char *field_name,
                          const char *value) {
  silos_ui_surface_add_text(row_index, field_name, value);
}
