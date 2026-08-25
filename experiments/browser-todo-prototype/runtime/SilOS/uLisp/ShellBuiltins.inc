const char doc_shelleventtype[] = "(shell-event-type event)";
const char doc_shellrequestpoke[] = "(shell-request-poke type arg...)";
object *SilosActiveHandler = NULL;
bool SilosPokePending = false;
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
  if (SilosActiveHandler != NULL) error2("app already started");
  SilosActiveHandler = first(args);
  silos_record_app_on_event();
  ShellEvent initialise{};
  initialise.kind = ShellEventKind::AppInitialise;
  initialise.app_generation = ActiveAppGeneration;
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
  if (SilosActiveHandler == NULL) error2("app is not started");
  if (!symbolp(first(args))) error2("event type is not a symbol");
  if (SilosPokePending) error2("poke-pending");
  ShellRequest request{};
  request.app_generation = ActiveAppGeneration;
  if (!silos_serialise_value(args, request.payload, 0, request.payload.root)) error2("unsupported poke payload");
  if (xQueueSend(ShellRequestQueue, &request, 0) != pdPASS) error2("shell request queue is full");
  SilosPokePending = true;
  return tee;
}

// The native event kind controls delivery only. Every app-facing event is a
// proper list whose first item is its type; app-owned types such as `init` and
// their parameters are interpreted exclusively by the Lisp handler.
void silos_ulisp_dispatch_shell_event(const ShellEvent &event) {
  if (event.app_generation != ActiveAppGeneration || SilosActiveHandler == NULL) return;
  object *event_value = nil;
  if (event.kind == ShellEventKind::AppInitialise) {
    AppInitialiseDelivered = true;
    AppObservedNoBindBeforeInit = SilosBoundStoreRef == nil;
    object *type = silos_symbol("shell-app-initialise");
    protect(type);
    event_value = cons(type, nil);
    unprotect();
  } else {
    SilosPokePending = false;
    ++AppPokeCount;
    event_value = silos_deserialise_value(event.payload, event.payload.root);
  }
  protect(event_value);
  object *arguments = cons(event_value, nil);
  protect(arguments);
  (void)apply(SilosActiveHandler, arguments, nullptr);
  ++AppEventCount;
  unprotect();
  unprotect();
}

// Cleanup is intentionally explicit even though this one-app proof does not
// expose a Reload button yet. It prevents old roots or a queued poke from
// leaking into the next app generation when bootstrap/reload is added.
void silos_cleanup_active_app() {
  silos_release_ui_instructions(SilosUiInstructions, SilosUiInstructionCount);
  SilosUiInstructions = nullptr;
  SilosUiInstructionCount = 0;
  SilosActiveHandler = nil;
  SilosBoundStoreRef = nil;
  SilosBoundStoreWatch = nil;
  SilosUiSourceBinding = nil;
  SilosPokePending = false;
  SilosUiMounted = false;
  SilosUiTypeFieldCount = 0;
  SilosUiTypeDeclared = false;
  SilosUiRefDeclared = false;
  SilosUiItemTemplateDeclared = false;
  SilosUiListDeclared = false;
  SilosUiLiteralDeclared = false;
  SilosUiLiteralRendered = false;
  SilosUiRefHandle = 0;
  SilosUiTemplateHandle = 0;
  SilosUiListHandle = 0;
  xQueueReset(ShellRequestQueue);
  xQueueReset(ShellEventQueue);
}
