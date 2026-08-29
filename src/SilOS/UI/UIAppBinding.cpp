#include "SilOS/UI/UIAppBinding.h"

namespace {
ulisp::Object *&selected_root(UIAppBinding::Registry registry,
                              ulisp::Object *&types, ulisp::Object *&refs,
                              ulisp::Object *&templates,
                              ulisp::Object *&mounts) {
  if (registry == UIAppBinding::Registry::Types) return types;
  if (registry == UIAppBinding::Registry::Refs) return refs;
  if (registry == UIAppBinding::Registry::Templates) return templates;
  return mounts;
}
}

ulisp::Object *UIAppBinding::entry(Registry registry, int handle) const {
  const std::size_t registry_count = count(registry);
  if (handle < 1 || static_cast<std::size_t>(handle) > registry_count) {
    return ulisp::nil;
  }
  ulisp::Object *cursor = root(registry);
  std::size_t skip = registry_count - static_cast<std::size_t>(handle);
  while (skip-- != 0) cursor = ulisp::tail(cursor);
  return ulisp::head(cursor);
}

void UIAppBinding::add(Registry registry, ulisp::Object *value) {
  ulisp::Object *&target = selected_root(registry, types_, refs_, templates_, mounts_);
  ulisp::pushRoot(value);
  target = ulisp::makeCons(value, target);
  ulisp::popRoot();
  if (registry == Registry::Types) ++type_count_;
  else if (registry == Registry::Refs) ++ref_count_;
  else if (registry == Registry::Templates) ++template_count_;
  else ++mount_count_;
}

std::size_t UIAppBinding::count(Registry registry) const {
  if (registry == Registry::Types) return type_count_;
  if (registry == Registry::Refs) return ref_count_;
  if (registry == Registry::Templates) return template_count_;
  return mount_count_;
}

ulisp::Object *UIAppBinding::root(Registry registry) const {
  if (registry == Registry::Types) return types_;
  if (registry == Registry::Refs) return refs_;
  if (registry == Registry::Templates) return templates_;
  return mounts_;
}

void UIAppBinding::visitRoots(RootVisitor visitor) const {
  visitor(types_); visitor(refs_); visitor(templates_); visitor(mounts_);
}

void UIAppBinding::moveRoot(ulisp::Object *from, ulisp::Object *to) {
  if (types_ == from) types_ = to;
  if (refs_ == from) refs_ = to;
  if (templates_ == from) templates_ = to;
  if (mounts_ == from) mounts_ = to;
}

void UIAppBinding::clear() { *this = UIAppBinding{}; }
