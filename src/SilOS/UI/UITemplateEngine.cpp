#include "SilOS/UI/UITemplateEngine.h"

#include <new>

namespace {
using namespace ulisp;
using Registry = UIAppBinding::Registry;

Object *at(Object *list, std::size_t index) {
  while (index-- != 0 && list != nil) list = tail(list);
  return list == nil ? nil : head(list);
}

Object *type_named(const UIAppBinding &app, Object *name) {
  Object *cursor = app.root(Registry::Types);
  while (cursor != nil) {
    Object *declaration = head(cursor);
    if (head(declaration) == name) return declaration;
    cursor = tail(cursor);
  }
  return nil;
}

bool declared_field(Object *type, Object *field) {
  Object *fields = tail(type);
  while (fields != nil) {
    Object *declaration = head(fields);
    if (isCons(declaration) && head(declaration) == field) return true;
    fields = tail(fields);
  }
  return false;
}

Object *ref_type(Object *entry) {
  Object *declaration = secondValue(tail(entry));
  if (isCons(declaration) && symbolIs(head(declaration), "store-ref")) {
    declaration = secondValue(declaration);
  }
  if (isCons(declaration) && symbolIs(head(declaration), "ui-list-of")) {
    return secondValue(declaration);
  }
  return nil;
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

Object *template_args(Object *entry) { return tail(entry); }

bool item_template(Object *args) {
  return isCons(secondValue(args)) && listLength(secondValue(args)) == 2 &&
      isSymbol(head(secondValue(args))) && isSymbol(secondValue(secondValue(args)));
}

enum class TemplateKind { Flow, Item, List };
TemplateKind template_kind(Object *args) {
  if (listLength(tail(args)) == 14 && symbolIs(secondValue(args), ":source")) {
    return TemplateKind::List;
  }
  return item_template(args) ? TemplateKind::Item : TemplateKind::Flow;
}

bool validate_specs(Object *specs, Object *parameter, Object *type,
                    const UIAppBinding &app, Object *environment) {
  if (specs == nil) return false;
  while (specs != nil) {
    Object *spec = head(specs);
    const int length = isCons(spec) ? listLength(spec) : -1;
    if (length < 0 || !symbolIs(head(spec), "ui-text")) return false;
    if (length == 2 && isString(secondValue(spec))) {
      specs = tail(specs);
      continue;
    }
    Object *field = length == 6 ? secondValue(spec) : nil;
    if (!isCons(field) || listLength(field) != 3 ||
        !symbolIs(head(field), "ui-field") ||
        !symbolIs(thirdValue(spec), ":width") || !isInteger(at(spec, 3)) ||
        integerValue(at(spec, 3)) < 1 ||
        !symbolIs(at(spec, 4), ":overflow") ||
        !symbolIs(at(spec, 5), "ui-chop")) return false;
    if (parameter != nil) {
      if (secondValue(field) != parameter || !declared_field(type, thirdValue(field))) {
        return false;
      }
    } else if (ref_handle(app, secondValue(field), environment) == 0 ||
               !symbolIs(thirdValue(field), "count")) {
      return false;
    }
    specs = tail(specs);
  }
  return true;
}
}

bool UITemplateEngine::prepare(std::size_t app_count) {
  try {
    bindings_.assign(app_count, UIAppBinding{});
    return true;
  } catch (const std::bad_alloc &) {
    bindings_.clear();
    return false;
  }
}

UIAppBinding &UITemplateEngine::binding(std::size_t app_index) {
  return bindings_.at(app_index);
}

const UIAppBinding &UITemplateEngine::binding(std::size_t app_index) const {
  return bindings_.at(app_index);
}

void UITemplateEngine::clearApp(std::size_t app_index) {
  if (app_index < bindings_.size()) bindings_[app_index].clear();
}

void UITemplateEngine::visitRoots(UIAppBinding::RootVisitor visitor) const {
  for (const UIAppBinding &binding : bindings_) binding.visitRoots(visitor);
}

void UITemplateEngine::moveRoots(sobject *from, sobject *to) {
  for (UIAppBinding &binding : bindings_) binding.moveRoot(from, to);
}

ulisp::Object *UITemplateEngine::defineType(std::size_t app_index,
                                             ulisp::Object *args) {
  using namespace ulisp;
  UIAppBinding &app = binding(app_index);
  if (!isSymbol(head(args)) || type_named(app, head(args)) != nil) {
    error("invalid or duplicate ui type");
  }
  const int field_count = listLength(tail(args));
  if (field_count < 1) error("invalid ui type");
  Object *fields = tail(args);
  for (int index = 0; index < field_count; ++index) {
    Object *field = head(fields);
    if (!isCons(field) || listLength(field) != 3 || !isSymbol(head(field)) ||
        !symbolNameIs(secondValue(field), "string") || !isInteger(thirdValue(field)) ||
        integerValue(thirdValue(field)) < 0 ||
        integerValue(thirdValue(field)) >= field_count) {
      error("invalid ui type field");
    }
    Object *earlier = tail(args);
    for (int previous = 0; previous < index; ++previous) {
      if (head(head(earlier)) == head(field) ||
          integerValue(thirdValue(head(earlier))) == integerValue(thirdValue(field))) {
        error("duplicate ui type field or slot");
      }
      earlier = tail(earlier);
    }
    fields = tail(fields);
  }
  app.add(Registry::Types, args);
  return makeNumber(static_cast<int>(app.typeCount()));
}

ulisp::Object *UITemplateEngine::bind(std::size_t app_index,
                                      ulisp::Object *args,
                                      ulisp::Object *environment,
                                      ulisp::Object *expected_store_ref) {
  using namespace ulisp;
  UIAppBinding &app = binding(app_index);
  if (!isSymbol(head(args))) error("invalid ui source");
  Object *declaration = secondValue(args);
  if (!isCons(declaration) || !symbolIs(head(declaration), "store-ref") ||
      !isCons(secondValue(declaration)) ||
      !symbolIs(head(secondValue(declaration)), "ui-list-of") ||
      type_named(app, secondValue(secondValue(declaration))) == nil) {
    error("invalid ui source type");
  }
  Object *binding = findPair(head(args), environment);
  if (binding == nil || tail(binding) != expected_store_ref) {
    error("invalid ui source");
  }
  pushRoot(args);
  Object *entry = makeCons(binding, args);
  app.add(Registry::Refs, entry);
  popRoot();
  return makeNumber(static_cast<int>(app.refCount()));
}

ulisp::Object *UITemplateEngine::defineTemplate(std::size_t app_index,
                                                 ulisp::Object *args,
                                                 ulisp::Object *environment) {
  using namespace ulisp;
  UIAppBinding &app = binding(app_index);
  if (!isSymbol(head(args))) error("invalid ui template");
  Object *existing = app.root(Registry::Templates);
  while (existing != nil) {
    if (head(template_args(head(existing))) == head(args)) {
      error("duplicate ui template");
    }
    existing = tail(existing);
  }
  const bool item = item_template(args);
  Object *parameter = item ? head(secondValue(args)) : nil;
  Object *type = item ? type_named(app, secondValue(secondValue(args))) : nil;
  if (item && type == nil) error("invalid ui item type");
  Object *specs = item ? tail2(args) : tail(args);
  if (!validate_specs(specs, parameter, type, app, environment)) {
    error("invalid ui instruction");
  }
  pushRoot(args);
  Object *entry = makeCons(environment, args);
  app.add(Registry::Templates, entry);
  popRoot();
  return makeNumber(static_cast<int>(app.templateCount()));
}

ulisp::Object *UITemplateEngine::defineList(std::size_t app_index,
                                             ulisp::Object *args,
                                             ulisp::Object *environment) {
  using namespace ulisp;
  UIAppBinding &app = binding(app_index);
  if (!isSymbol(head(args)) || listLength(tail(args)) != 14 ||
      !symbolIs(at(args, 1), ":source") ||
      !symbolIs(at(args, 3), ":item-template") ||
      !symbolIs(at(args, 5), ":offset") || !isInteger(at(args, 6)) ||
      integerValue(at(args, 6)) < 0 || !symbolIs(at(args, 7), ":limit") ||
      !isInteger(at(args, 8)) || integerValue(at(args, 8)) < 0 ||
      !symbolIs(at(args, 9), ":pending") || !isString(at(args, 10)) ||
      !symbolIs(at(args, 11), ":empty") || !isString(at(args, 12)) ||
      !symbolIs(at(args, 13), ":error") || !isString(at(args, 14))) {
    error("invalid ui list");
  }
  const int source_handle = resolve_handle(at(args, 2), environment);
  const int item_handle = resolve_handle(at(args, 4), environment);
  Object *ref = app.entry(Registry::Refs, source_handle);
  Object *item = app.entry(Registry::Templates, item_handle);
  if (ref == nil || item == nil ||
      template_kind(template_args(item)) != TemplateKind::Item ||
      ref_type(ref) != secondValue(secondValue(template_args(item)))) {
    error("invalid ui list source or item template");
  }
  pushRoot(args);
  Object *entry = makeCons(environment, args);
  app.add(Registry::Templates, entry);
  popRoot();
  return makeNumber(static_cast<int>(app.templateCount()));
}

ulisp::Object *UITemplateEngine::mount(std::size_t app_index,
                                       ulisp::Object *args) {
  using namespace ulisp;
  UIAppBinding &app = binding(app_index);
  const int handle = checkInteger(head(args));
  if (app.entry(Registry::Templates, handle) == nil) {
    error("invalid ui mount");
  }
  app.add(Registry::Mounts, args);
  return makeNumber(static_cast<int>(app.mountCount()));
}
