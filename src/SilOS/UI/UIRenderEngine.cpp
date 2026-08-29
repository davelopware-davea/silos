#include "SilOS/UI/UIRenderEngine.h"

#include "SilOS/UI/UITemplateEngine.h"

#include <new>

bool UIRenderEngine::prepare(std::size_t app_count,
                             const UITemplateEngine &template_engine) {
  if (app_count != template_engine.appCount()) {
    renderers_.clear();
    return false;
  }
  try {
    renderers_.clear();
    renderers_.reserve(app_count);
    for (std::size_t index = 0; index < app_count; ++index) {
      renderers_.emplace_back(template_engine.binding(index));
    }
    return true;
  } catch (const std::bad_alloc &) {
    renderers_.clear();
    return false;
  }
}

void UIRenderEngine::clear() { renderers_.clear(); }

UIRenderStats UIRenderEngine::renderFrame(IPlatformRenderEngine &platform,
                                           LockOperation lock,
                                           LockOperation unlock,
                                           AppCountReader app_count,
                                           AppNameReader app_name) {
  UIRenderStats stats;
  platform.startFrame();
  lock();
  const std::size_t count = app_count() < renderers_.size()
      ? app_count() : renderers_.size();
  unlock();
  for (std::size_t index = 0; index < count; ++index) {
    lock();
    if (index >= app_count() || index >= renderers_.size()) {
      unlock();
      break;
    }
    renderers_[index].render(index, app_name(index), platform, stats);
    unlock();
  }
  platform.endFrame();
  return stats;
}
