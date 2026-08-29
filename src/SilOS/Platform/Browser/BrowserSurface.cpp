#include "SilOS/UI/IPlatformRenderEngine.h"

#include <emscripten.h>

#include <string>

namespace {
void append_character(char character, void *context) {
  static_cast<std::string *>(context)->push_back(character);
}

std::string text(const sobject *value, std::size_t limit = 0) {
  std::string result;
  (void)silos_ulisp_visit_string(value, limit, append_character, &result);
  return result;
}

std::string symbol(const sobject *value) {
  std::string result;
  (void)silos_ulisp_visit_symbol(value, append_character, &result);
  return result;
}

class BrowserRenderEngine final : public IPlatformRenderEngine {
public:
  std::size_t framePeriodMilliseconds() const override { return 100; }

  void startFrame() override {
    EM_ASM({
      if (typeof document === 'undefined') return;
      const root = document.getElementById('silos-apps');
      if (root != null) root.replaceChildren();
    });
  }

  void startApp(std::size_t app_index, const sobject *name_value) override {
    const std::string name = text(name_value);
    EM_ASM({
      if (typeof document === 'undefined') return;
      const root = document.getElementById('silos-apps');
      if (root == null) return;
      const app = document.createElement('section');
      app.className = 'silos-app'; app.dataset.appIndex = String($0);
      const title = document.createElement('h1');
      title.textContent = UTF8ToString($1); app.append(title); root.append(app);
    }, app_index, name.c_str());
  }

  void startTemplate(std::size_t app_index, std::size_t mount_index,
                     UiRenderState state, const sobject *state_text,
                     bool is_list) override {
    const std::string message = state_text == nullptr ? std::string{} : text(state_text);
    const char *state_name = state == UiRenderState::Pending ? "pending" :
        state == UiRenderState::Ready ? "ready" :
        state == UiRenderState::Empty ? "empty" : "error";
    EM_ASM({
      if (typeof document === 'undefined') return;
      const root = document.querySelector('.silos-app[data-app-index="' + String($0) + '"]');
      if (root == null) return;
      const view = document.createElement('section');
      view.className = 'silos-template'; view.dataset.mountIndex = String($1);
      view.dataset.state = UTF8ToString($2);
      const message = UTF8ToString($3);
      if (message.length !== 0) { const summary = document.createElement('p'); summary.className = 'silos-ui-state'; summary.textContent = message; view.append(summary); }
      const content = document.createElement($4 ? 'ul' : 'div');
      content.className = $4 ? 'silos-template-list' : 'silos-template-content';
      view.append(content); root.append(view);
    }, app_index, mount_index, state_name, message.c_str(), is_list);
  }

  void writeStaticField(std::size_t app_index, std::size_t mount_index,
                        int row_index, const sobject *value) override {
    write(app_index, mount_index, row_index, nullptr, text(value));
  }

  void writeDynamicField(std::size_t app_index, std::size_t mount_index,
                         int row_index, const sobject *field_name,
                         const sobject *value,
                         const UiRenderFormat &format) override {
    std::string rendered;
    int integer = 0;
    if (!silos_ulisp_visit_string(value, format.width, append_character, &rendered) &&
        silos_ulisp_read_integer(value, integer)) {
      rendered = std::to_string(integer);
      if (format.width != 0 && rendered.size() > format.width) rendered.resize(format.width);
    }
    const std::string name = symbol(field_name);
    write(app_index, mount_index, row_index, name.c_str(), rendered);
  }

  void endTemplate() override {}
  void endApp() override {}
  void endFrame() override {}

private:
  static void write(std::size_t app_index, std::size_t mount_index,
                    int row_index, const char *field_name,
                    const std::string &value) {
    EM_ASM({
      if (typeof document === 'undefined') return;
      const view = document.querySelector('.silos-app[data-app-index="' + String($0) + '"] .silos-template[data-mount-index="' + String($1) + '"]');
      if (view == null) return;
      const content = view.querySelector('.silos-template-list, .silos-template-content');
      if (content == null) return;
      let row = content;
      if ($2 >= 0) { const index = String($2); row = content.querySelector('[data-row-index="' + index + '"]'); if (row == null) { row = document.createElement('li'); row.dataset.rowIndex = index; content.append(row); } }
      const span = document.createElement('span');
      if ($3 === 0) { span.className = 'silos-template-literal'; span.dataset.templateKind = 'literal'; }
      else { span.className = 'silos-template-field'; span.dataset.field = UTF8ToString($3); }
      span.textContent = UTF8ToString($4); row.append(span);
    }, app_index, mount_index, row_index, field_name, value.c_str());
  }
};

BrowserRenderEngine Engine;
}

IPlatformRenderEngine &silos_platform_render_engine() { return Engine; }
