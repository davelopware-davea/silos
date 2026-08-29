#pragma once

#include <cstddef>

#include "SilOS/uLisp/ULispAccess.h"

// Holds one application's UI declaration state as roots into uLisp memory.
// UITemplateEngine owns these objects and mutates their type, UiRef, template,
// and mount registries; UIAppRenderer borrows one while traversing an app.
// Values remain in uLisp rather than being copied into C++ storage. The root
// hooks keep the retained list heads visible to GC and repair them after moves.
class UIAppBinding {
public:
  enum class Registry { Types, Refs, Templates, Mounts };
  using RootVisitor = void (*)(ulisp::Object *root);

  ulisp::Object *entry(Registry registry, int handle) const;
  void add(Registry registry, ulisp::Object *entry);
  std::size_t count(Registry registry) const;

  std::size_t typeCount() const { return type_count_; }
  std::size_t refCount() const { return ref_count_; }
  std::size_t templateCount() const { return template_count_; }
  std::size_t mountCount() const { return mount_count_; }

  ulisp::Object *root(Registry registry) const;
  void visitRoots(RootVisitor visitor) const;
  void moveRoot(ulisp::Object *from, ulisp::Object *to);
  void clear();

private:
  ulisp::Object *types_ = ulisp::nil;
  ulisp::Object *refs_ = ulisp::nil;
  ulisp::Object *templates_ = ulisp::nil;
  ulisp::Object *mounts_ = ulisp::nil;
  std::size_t type_count_ = 0;
  std::size_t ref_count_ = 0;
  std::size_t template_count_ = 0;
  std::size_t mount_count_ = 0;
};
