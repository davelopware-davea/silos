#include "SilOS/UI/Renderer.h"

#include "SilOS/FreeRTOS/QueueRuntime.h"
#include "SilOS/Runtime/State.h"
#include "SilOS/UI/IPlatformRenderEngine.h"
#include "SilOS/UI/UIRenderEngine.h"
#include "SilOS/UI/UITemplateEngine.h"

namespace {
UITemplateEngine TemplateEngine;
UIRenderEngine RenderEngine;

std::size_t current_app_count() { return AppCount; }

sobject *app_display_name(std::size_t app_index) {
  return AppDeclarations[app_index].display_name;
}
}

std::size_t SilosRenderedAppCount = 0;
std::size_t SilosRenderedMountCount = 0;
std::size_t SilosRenderedListRowCount = 0;
std::size_t SilosRenderedInstructionCount = 0;

void silos_render_ui() {
  const UIRenderStats stats = RenderEngine.renderFrame(
      silos_platform_render_engine(), silos_lock_ulisp_workspace,
      silos_unlock_ulisp_workspace, current_app_count, app_display_name);
  SilosRenderedAppCount = stats.apps;
  SilosRenderedMountCount = stats.mounts;
  SilosRenderedListRowCount = stats.rows;
  SilosRenderedInstructionCount = stats.instructions;
  UiPendingRendered = stats.pending;
  UiReadyRendered = stats.ready;
}

bool silos_prepare_ui_renderers(std::size_t count) {
  // Renderers borrow bindings, so discard them before replacing binding
  // storage and rebuild them only after the new storage is stable.
  RenderEngine.clear();
  if (!TemplateEngine.prepare(count)) return false;
  if (RenderEngine.prepare(count, TemplateEngine)) return true;
  (void)TemplateEngine.prepare(0);
  return false;
}

void silos_cleanup_app_ui(std::size_t app_index) {
  TemplateEngine.clearApp(app_index);
}

UIAppBinding &silos_ui_binding(std::size_t app_index) {
  return TemplateEngine.binding(app_index);
}

UITemplateEngine &silos_ui_template_engine() { return TemplateEngine; }

void silos_ui_visit_roots(void (*visitor)(sobject *)) {
  TemplateEngine.visitRoots(visitor);
}

void silos_ui_move_roots(sobject *from, sobject *to) {
  TemplateEngine.moveRoots(from, to);
}
