#include "SilOS/UI/PlatformSurface.h"

#include <emscripten.h>

void silos_ui_surface_begin(const char *state, const char *message) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const root = document.getElementById('silos-app');
    if (root == null) return;
    root.replaceChildren();
    const title = document.createElement('h1');
    title.textContent = 'SilOS to-dos';
    const summary = document.createElement('p');
    summary.className = 'silos-ui-state';
    summary.dataset.state = UTF8ToString($0);
    summary.textContent = UTF8ToString($1);
    root.append(title, summary);
    if (summary.dataset.state === 'ready') {
      const list = document.createElement('ul');
      list.id = 'silos-todo-list';
      list.setAttribute('aria-label', 'Bound to-do items');
      root.append(list);
    }
  }, state, message);
}

void silos_ui_surface_add_text(int row_index, const char *field_name,
                               const char *value) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const list = document.getElementById('silos-todo-list');
    if (list == null) return;
    const index = String($0);
    let row = list.querySelector('li[data-row-index="' + index + '"]');
    if (row == null) {
      row = document.createElement('li');
      row.dataset.rowIndex = index;
      list.append(row);
    }
    const text = document.createElement('span');
    if ($1 === 0) {
      text.className = 'silos-template-literal';
      text.dataset.templateKind = 'literal';
    } else {
      text.className = 'silos-template-field';
      text.dataset.field = UTF8ToString($1);
    }
    text.textContent = UTF8ToString($2);
    row.append(text);
  }, row_index, field_name, value);
}
