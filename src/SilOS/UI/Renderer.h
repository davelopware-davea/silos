#pragma once

#include <cstddef>

class UIAppBinding;
class UITemplateEngine;
struct sobject;

void silos_render_ui();
bool silos_prepare_ui_renderers(std::size_t count);
void silos_cleanup_app_ui(std::size_t app_index);
UIAppBinding &silos_ui_binding(std::size_t app_index);
UITemplateEngine &silos_ui_template_engine();
void silos_ui_visit_roots(void (*visitor)(sobject *));
void silos_ui_move_roots(sobject *from, sobject *to);

extern std::size_t SilosRenderedAppCount;
extern std::size_t SilosRenderedMountCount;
extern std::size_t SilosRenderedListRowCount;
extern std::size_t SilosRenderedInstructionCount;
