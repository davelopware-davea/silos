#pragma once

#include <cstddef>
#include <vector>

#include "SilOS/UI/UIAppRenderer.h"

class UITemplateEngine;

// Coordinates frame traversal and owns one UIAppRenderer per bootstrapped app.
// It borrows stable app bindings from UITemplateEngine when prepared, then
// locks the uLisp workspace separately around each application's render so a
// single app is coherent without locking the complete frame. Actual drawing,
// buffering, diffing, and output remain the platform renderer's responsibility.
class UIRenderEngine {
public:
  using LockOperation = void (*)();
  using AppCountReader = std::size_t (*)();
  using AppNameReader = sobject *(*)(std::size_t app_index);

  bool prepare(std::size_t app_count, const UITemplateEngine &template_engine);
  void clear();

  UIRenderStats renderFrame(IPlatformRenderEngine &platform,
                            LockOperation lock, LockOperation unlock,
                            AppCountReader app_count,
                            AppNameReader app_name);

private:
  std::vector<UIAppRenderer> renderers_;
};
