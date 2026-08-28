#include "SilOS/UI/PlatformSurface.h"

#include <emscripten.h>

void silos_ui_surface_begin() {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const root = document.getElementById('silos-apps');
    if (root != null) root.replaceChildren();
  });
}

void silos_ui_surface_begin_app(std::size_t app_index, const char *name) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const root = document.getElementById('silos-apps');
    if (root == null) return;
    const app = document.createElement('section');
    app.className = 'silos-app';
    app.dataset.appIndex = String($0);
    const title = document.createElement('h1');
    title.textContent = UTF8ToString($1);
    app.append(title);
    root.append(app);
  }, app_index, name);
}

void silos_ui_surface_begin_template(std::size_t app_index,
                                     std::size_t mount_index,
                                     const char *state, const char *message,
                                     bool is_list) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const root = document.querySelector(
        '.silos-app[data-app-index="' + String($0) + '"]');
    if (root == null) return;
    const view = document.createElement('section');
    view.className = 'silos-template';
    view.dataset.mountIndex = String($1);
    view.dataset.state = UTF8ToString($2);
    const message = UTF8ToString($3);
    if (message.length !== 0) {
      const summary = document.createElement('p');
      summary.className = 'silos-ui-state';
      summary.textContent = message;
      view.append(summary);
    }
    const content = document.createElement($4 ? 'ul' : 'div');
    content.className = $4 ? 'silos-template-list' : 'silos-template-content';
    view.append(content);
    root.append(view);
  }, app_index, mount_index, state, message, is_list);
}

void silos_ui_surface_add_text(std::size_t app_index,
                               std::size_t mount_index, int row_index,
                               const char *field_name, const char *value) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const view = document.querySelector(
        '.silos-app[data-app-index="' + String($0) + '"] ' +
        '.silos-template[data-mount-index="' + String($1) + '"]');
    if (view == null) return;
    const content = view.querySelector('.silos-template-list, .silos-template-content');
    if (content == null) return;
    let row = content;
    if ($2 >= 0) {
      const index = String($2);
      row = content.querySelector('[data-row-index="' + index + '"]');
      if (row == null) {
        row = document.createElement('li');
        row.dataset.rowIndex = index;
        content.append(row);
      }
    }
    const text = document.createElement('span');
    if ($3 === 0) {
      text.className = 'silos-template-literal';
      text.dataset.templateKind = 'literal';
    } else {
      text.className = 'silos-template-field';
      text.dataset.field = UTF8ToString($3);
    }
    text.textContent = UTF8ToString($4);
    row.append(text);
  }, app_index, mount_index, row_index, field_name, value);
}
