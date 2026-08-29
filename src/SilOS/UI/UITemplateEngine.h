#pragma once

#include <cstddef>
#include <vector>

#include "SilOS/UI/UIAppBinding.h"

// Owns the dynamically bootstrap-sized UIAppBinding collection and implements
// the declaration operations reached through the thin uLisp built-in adapter.
// It validates and roots the original uLisp forms, owns binding cleanup and GC
// integration, and exposes stable borrowed bindings to UIRenderEngine. Binding
// storage is replaced only while renderers are cleared during app bootstrap.
class UITemplateEngine {
public:
  bool prepare(std::size_t app_count);
  std::size_t appCount() const { return bindings_.size(); }
  UIAppBinding &binding(std::size_t app_index);
  const UIAppBinding &binding(std::size_t app_index) const;
  void clearApp(std::size_t app_index);
  void visitRoots(UIAppBinding::RootVisitor visitor) const;
  void moveRoots(sobject *from, sobject *to);

  ulisp::Object *defineType(std::size_t app_index, ulisp::Object *args);
  ulisp::Object *bind(std::size_t app_index, ulisp::Object *args,
                      ulisp::Object *environment,
                      ulisp::Object *expected_store_ref);
  ulisp::Object *defineTemplate(std::size_t app_index, ulisp::Object *args,
                                ulisp::Object *environment);
  ulisp::Object *defineList(std::size_t app_index, ulisp::Object *args,
                            ulisp::Object *environment);
  ulisp::Object *mount(std::size_t app_index, ulisp::Object *args);

private:
  std::vector<UIAppBinding> bindings_;
};
