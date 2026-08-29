#include "SilOS/UI/UIAppRenderer.h"

namespace {
using namespace ulisp;
using Registry = UIAppBinding::Registry;

Object *at(Object *list, std::size_t index) {
  while (index-- != 0 && list != nil) list = tail(list);
  return list == nil ? nil : head(list);
}

Object *template_args(Object *entry) { return tail(entry); }
Object *template_environment(Object *entry) { return head(entry); }

bool item_template(Object *args) {
  return isCons(secondValue(args)) && listLength(secondValue(args)) == 2 &&
      isSymbol(head(secondValue(args))) && isSymbol(secondValue(secondValue(args)));
}

bool list_template(Object *args) {
  return listLength(tail(args)) == 14 && symbolIs(secondValue(args), ":source");
}

int ref_handle(const UIAppBinding &app, Object *name, Object *environment) {
  Object *binding = findPair(name, environment);
  if (binding == nil || !isInteger(tail(binding))) return 0;
  const int handle = integerValue(tail(binding));
  return app.entry(Registry::Refs, handle) == nil ? 0 : handle;
}

int resolve_handle(Object *form, Object *environment) {
  if (isInteger(form)) return integerValue(form);
  if (!isSymbol(form)) return 0;
  Object *binding = findPair(form, environment);
  return binding != nil && isInteger(tail(binding))
      ? integerValue(tail(binding)) : 0;
}

void render_instructions(IPlatformRenderEngine &platform,
                         std::size_t app_index, std::size_t mount_index,
                         const UIAppBinding &app, Object *template_entry,
                         Object *row_value, int row_index,
                         UIRenderStats &stats) {
  Object *args = template_args(template_entry);
  const bool item = item_template(args);
  Object *specs = item ? tail2(args) : tail(args);
  while (specs != nil) {
    ++stats.instructions;
    Object *spec = head(specs);
    if (isString(secondValue(spec))) {
      platform.writeStaticField(app_index, mount_index, row_index, secondValue(spec));
    } else {
      Object *field = secondValue(spec);
      Object *name = thirdValue(field);
      Object *value = nil;
      if (item) {
        Object *entry = row_value == nil ? nil :
            findSymbolField(tail(row_value), name);
        if (entry != nil) value = tail(entry);
      } else {
        const int handle = ref_handle(app, secondValue(field),
                                      template_environment(template_entry));
        Object *ref = app.entry(Registry::Refs, handle);
        Object *source = ref == nil || head(ref) == nil
            ? nil : tail(head(ref));
        Object *metadata = source == nil ? nil : findField(source, "meta");
        Object *entry = metadata == nil ? nil :
            findSymbolField(tail(metadata), name);
        if (entry != nil) value = tail(entry);
      }
      if (value != nil) {
        platform.writeDynamicField(
            app_index, mount_index, row_index, name, value,
            UiRenderFormat{static_cast<std::size_t>(integerValue(at(spec, 3)))});
      }
    }
    specs = tail(specs);
  }
}

void render_template(IPlatformRenderEngine &platform, std::size_t app_index,
                     std::size_t mount_index, const UIAppBinding &app,
                     Object *entry, UIRenderStats &stats) {
  ++stats.mounts;
  Object *args = template_args(entry);
  if (!list_template(args)) {
    platform.startTemplate(app_index, mount_index, UiRenderState::Ready, nullptr,
                           false);
    render_instructions(platform, app_index, mount_index, app, entry, nil, -1,
                        stats);
    platform.endTemplate();
    return;
  }

  Object *ref = app.entry(
      Registry::Refs,
      resolve_handle(at(args, 2), template_environment(entry)));
  Object *source = ref == nil || head(ref) == nil ? nil : tail(head(ref));
  Object *metadata = source == nil ? nil : findField(source, "meta");
  Object *status = metadata == nil ? nil : findField(tail(metadata), "status");
  if (status == nil || symbolIs(tail(status), "silos-pending")) {
    stats.pending = true;
    platform.startTemplate(app_index, mount_index, UiRenderState::Pending,
                           at(args, 10), true);
    platform.endTemplate();
    return;
  }
  if (!symbolIs(tail(status), "silos-ready")) {
    platform.startTemplate(app_index, mount_index, UiRenderState::Error,
                           at(args, 14), true);
    platform.endTemplate();
    return;
  }
  Object *value = findField(source, "value");
  Object *rows = value == nil ? nil : tail(value);
  int offset = integerValue(at(args, 6));
  while (rows != nil && offset-- > 0) rows = tail(rows);
  if (rows == nil) {
    platform.startTemplate(app_index, mount_index, UiRenderState::Empty,
                           at(args, 12), true);
    platform.endTemplate();
    return;
  }
  stats.ready = true;
  platform.startTemplate(app_index, mount_index, UiRenderState::Ready, nullptr,
                         true);
  Object *item = app.entry(
      Registry::Templates,
      resolve_handle(at(args, 4), template_environment(entry)));
  const int limit = integerValue(at(args, 8));
  int row_index = 0;
  while (rows != nil && row_index < limit) {
    Object *row = head(rows);
    Object *row_value = findField(row, "value");
    render_instructions(platform, app_index, mount_index, app, item, row_value,
                        row_index++, stats);
    ++stats.rows;
    rows = tail(rows);
  }
  platform.endTemplate();
}
}

void UIAppRenderer::render(std::size_t app_index, sobject *display_name,
                           IPlatformRenderEngine &platform,
                           UIRenderStats &stats) const {
  const UIAppBinding &binding = *binding_;
  ++stats.apps;
  platform.startApp(app_index, display_name);
  for (std::size_t handle = 1; handle <= binding.mountCount(); ++handle) {
    ulisp::Object *mount = binding.entry(
        UIAppBinding::Registry::Mounts, static_cast<int>(handle));
    ulisp::Object *entry = binding.entry(
        UIAppBinding::Registry::Templates,
        ulisp::checkInteger(ulisp::head(mount)));
    render_template(platform, app_index, handle - 1, binding, entry, stats);
  }
  platform.endApp();
}
