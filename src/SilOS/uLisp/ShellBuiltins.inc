const char doc_shelleventtype[] = "(shell-event-type event)";
const char doc_shellrequestpoke[] = "(shell-request-poke type arg...)";
std::vector<object *> SilosActiveHandlers;
std::vector<bool> SilosPokePending;
// Copy one admissible Lisp value into a bounded flat native arena. The queue
// therefore owns data, not a uLisp pointer which could be reclaimed or mutate
// before the Shell's later delivery turn.
bool silos_serialise_value(object *value, SerializedPayload &payload,
                            std::size_t depth, std::uint8_t &index) {
  if (depth > PokeDepthCapacity || payload.value_count == PokeValueCapacity) return false;
  index = payload.value_count++;
  SerializedValue &out = payload.values[index];
  out = SerializedValue{};
  if (value == nil) { out.kind = SerializedValueKind::Nil; return true; }
  if (value == tee) { out.kind = SerializedValueKind::Boolean; return true; }
  if (integerp(value)) {
    out.kind = SerializedValueKind::Integer;
    out.integer = value->integer;
    return true;
  }
  if (symbolp(value)) {
    out.kind = SerializedValueKind::Symbol;
    return silos_copy_symbol_name(value, out.text, PokeSymbolCapacity);
  }
  if (stringp(value)) {
    out.kind = SerializedValueKind::String;
    cstring(value, out.text, sizeof(out.text));
    return true;
  }
  if (!consp(value)) return false;
  out.kind = SerializedValueKind::List;
  out.child_count = 0;
  while (value != nil) {
    std::uint8_t child = InvalidSerializedValueIndex;
    if (!consp(value) || out.child_count == PokeListCapacity ||
        !silos_serialise_value(first(value), payload, depth + 1, child)) return false;
    out.children[out.child_count] = child;
    ++out.child_count;
    value = rest(value);
  }
  return true;
}

object *silos_deserialise_value(const SerializedPayload &payload, std::uint8_t index) {
  if (index >= payload.value_count) error2("invalid serial event");
  const SerializedValue &value = payload.values[index];
  switch (value.kind) {
    case SerializedValueKind::Nil: return nil;
    case SerializedValueKind::Boolean: return tee;
    case SerializedValueKind::Integer: return number(value.integer);
    case SerializedValueKind::Symbol: return silos_symbol(value.text);
    case SerializedValueKind::String: return lispstring(const_cast<char *>(value.text));
    case SerializedValueKind::List: {
      object *result = nil;
      for (std::size_t index = value.child_count; index > 0; --index) {
        protect(result);
        object *item = silos_deserialise_value(payload, value.children[index - 1]);
        protect(item);
        result = cons(item, result);
        unprotect();
        unprotect();
      }
      return result;
    }
  }
  error2("invalid serial event");
}

object *fn_shellapponevent (object *args, object *env) {
  (void) env;
  if (CurrentAppIndex >= AppCount) error2("no current app");
  if (SilosActiveHandlers[CurrentAppIndex] != nil) error2("app already started");
  SilosActiveHandlers[CurrentAppIndex] = first(args);
  silos_record_app_on_event();
  ShellEvent initialise{};
  initialise.kind = ShellEventKind::AppInitialise;
  initialise.app_index = CurrentAppIndex;
  initialise.app_generation = AppGenerations[CurrentAppIndex];
  if (xQueueSend(ShellEventQueue, &initialise, 0) != pdPASS) error2("shell event queue is full");
  return tee;
}

object *fn_shelleventtype (object *args, object *env) {
  (void) env;
  object *event = first(args);
  if (!consp(event) || !symbolp(first(event))) error2("invalid shell event");
  object *item = event;
  while (item != nil) {
    if (!consp(item)) error2("invalid shell event");
    item = rest(item);
  }
  return first(event);
}

object *fn_shellrequestpoke (object *args, object *env) {
  (void) env;
  if (CurrentAppIndex >= AppCount || SilosActiveHandlers[CurrentAppIndex] == nil) {
    error2("app is not started");
  }
  if (!symbolp(first(args))) error2("event type is not a symbol");
  if (SilosPokePending[CurrentAppIndex]) error2("poke-pending");
  ShellRequest request{};
  request.app_index = CurrentAppIndex;
  request.app_generation = AppGenerations[CurrentAppIndex];
  if (!silos_serialise_value(args, request.payload, 0, request.payload.root)) error2("unsupported poke payload");
  if (xQueueSend(ShellRequestQueue, &request, 0) != pdPASS) error2("shell request queue is full");
  SilosPokePending[CurrentAppIndex] = true;
  return tee;
}

// The native event kind controls delivery only. Every app-facing event is a
// proper list whose first item is its type; app-owned types such as `init` and
// their parameters are interpreted exclusively by the Lisp handler.
void silos_ulisp_dispatch_shell_event(const ShellEvent &event) {
  if (event.app_index >= AppCount ||
      event.app_generation != AppGenerations[event.app_index] ||
      SilosActiveHandlers[event.app_index] == nil) return;
  const std::size_t previous_app = CurrentAppIndex;
  CurrentAppIndex = event.app_index;
  object *event_value = nil;
  if (event.kind == ShellEventKind::AppInitialise) {
    AppInitialiseDelivered = true;
    AppObservedNoBindBeforeInit =
        silos_store_engine().bindingCount(CurrentAppIndex) == 0;
    object *type = silos_symbol("shell-app-initialise");
    protect(type);
    event_value = cons(type, nil);
    unprotect();
  } else {
    SilosPokePending[CurrentAppIndex] = false;
    ++AppPokeCount;
    event_value = silos_deserialise_value(event.payload, event.payload.root);
  }
  protect(event_value);
  object *arguments = cons(event_value, nil);
  protect(arguments);
  (void)apply(SilosActiveHandlers[CurrentAppIndex], arguments, nullptr);
  ++AppEventCount;
  unprotect();
  unprotect();
  CurrentAppIndex = previous_app;
}

// Cleanup is explicit so roots and queued events cannot leak across a complete
// catalogue bootstrap. Per-app reload remains a later lifecycle operation.
void silos_cleanup_apps() {
  for (std::size_t index = 0; index < AppCount; ++index) {
    silos_cleanup_app_ui(index);
    silos_clear_store_app(index);
    SilosActiveHandlers[index] = nil;
    SilosPokePending[index] = false;
    AppDeclarations[index] = AppDeclaration{};
    AppStarted[index] = false;
    AppGenerations[index] = 0;
  }
  AppCount = 0;
  CurrentAppIndex = SilosInvalidAppIndex;
  (void)silos_prepare_ui_renderers(0);
  silos_clear_store();
  SilosActiveHandlers.clear();
  SilosPokePending.clear();
  AppDeclarations.clear();
  AppStarted.clear();
  AppGenerations.clear();
  xQueueReset(ShellRequestQueue);
  xQueueReset(ShellEventQueue);
}

bool silos_ulisp_prepare_apps(std::size_t count) {
  try {
    if (!silos_prepare_ui_renderers(count)) return false;
    if (!silos_prepare_store(count)) {
      (void)silos_prepare_ui_renderers(0);
      return false;
    }
    SilosActiveHandlers.resize(count, nil);
    SilosPokePending.resize(count, false);
    return true;
  } catch (const std::bad_alloc &) {
    (void)silos_prepare_ui_renderers(0);
    (void)silos_prepare_store(0);
    SilosActiveHandlers.clear();
    SilosPokePending.clear();
    return false;
  }
}
