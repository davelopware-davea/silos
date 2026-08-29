#pragma once

#include <cstddef>

#include "SilOS/UI/IPlatformRenderEngine.h"
#include "SilOS/UI/UIAppBinding.h"

struct UIRenderStats {
  std::size_t apps = 0;
  std::size_t mounts = 0;
  std::size_t rows = 0;
  std::size_t instructions = 0;
  bool pending = false;
  bool ready = false;
};

// Renders one application by traversing a UIAppBinding owned by
// UITemplateEngine and emitting semantic operations to IPlatformRenderEngine.
// UIRenderEngine owns one instance per app and holds the uLisp workspace lock
// for the entire render call. The binding and every uLisp object read from it
// are borrowed; this class neither owns nor copies application values.
class UIAppRenderer {
public:
  explicit UIAppRenderer(const UIAppBinding &binding) : binding_(&binding) {}
  void render(std::size_t app_index, sobject *display_name,
              IPlatformRenderEngine &platform, UIRenderStats &stats) const;

private:
  const UIAppBinding *binding_;
};
