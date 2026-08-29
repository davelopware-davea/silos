#pragma once

#include <cstddef>

struct sobject;

enum class UiRenderState { Pending, Ready, Empty, Error };

struct UiRenderFormat {
  std::size_t width = 0;
};

// Platform rendering seam used by UIRenderEngine and UIAppRenderer. Portable
// UI code emits frame/app/template/field structure; each target owns the actual
// drawing strategy, including direct output, buffering, caching, or diffing.
// All sobject pointers are borrowed from uLisp and valid only for the callback
// while the current app's workspace lock is held; implementations must not
// retain them, though they may retain their own platform-specific projection.
class IPlatformRenderEngine {
public:
  virtual ~IPlatformRenderEngine() = default;
  virtual std::size_t framePeriodMilliseconds() const = 0;
  virtual void startFrame() = 0;
  virtual void startApp(std::size_t app_index, const sobject *name) = 0;
  virtual void startTemplate(std::size_t app_index, std::size_t mount_index,
                             UiRenderState state, const sobject *state_text,
                             bool is_list) = 0;
  virtual void writeStaticField(std::size_t app_index,
                                std::size_t mount_index, int row_index,
                                const sobject *text) = 0;
  virtual void writeDynamicField(std::size_t app_index,
                                 std::size_t mount_index, int row_index,
                                 const sobject *field_name,
                                 const sobject *value,
                                 const UiRenderFormat &format) = 0;
  virtual void endTemplate() = 0;
  virtual void endApp() = 0;
  virtual void endFrame() = 0;
};

IPlatformRenderEngine &silos_platform_render_engine();

using SilosUlispCharacterVisitor = void (*)(char character, void *context);
bool silos_ulisp_visit_string(const sobject *value, std::size_t limit,
                              SilosUlispCharacterVisitor visitor,
                              void *context);
bool silos_ulisp_read_integer(const sobject *value, int &result);
bool silos_ulisp_visit_symbol(const sobject *value,
                              SilosUlispCharacterVisitor visitor,
                              void *context);
