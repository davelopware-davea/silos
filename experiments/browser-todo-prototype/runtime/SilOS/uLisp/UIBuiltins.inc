const char doc_uitype[] = "(ui-type name (field type slot)...)";
const char doc_uibind[] = "(ui-bind name type value)";
const char doc_uitemplate[] = "(ui-template name (item type) (ui-text string | (ui-field item field) :width integer :overflow chop)...)";
const char doc_uitemplatelist[] = "(ui-template-list name :source ui-ref :item-template template :offset integer :limit integer :pending string :empty string :error string)";
const char doc_uimount[] = "(ui-mount list :region region)";
// This is intentionally a small real template registry, not a Todo-specific
// print path. A declaration owns an exact-size instruction array and each
// literal instruction owns an exact-size immutable string. Values remain in
// the uLisp StoreRef and are streamed during a Shell redraw.
constexpr std::size_t SilosUiTypeFieldCapacity = 3;
struct SilosUiTypeField {
  char name[InMemoryFieldNameCapacity]{};
  int slot = -1;
};
enum class SilosUiInstructionKind {
  LiteralText,
  FieldText,
};
struct SilosUiInstruction {
  SilosUiInstructionKind kind = SilosUiInstructionKind::LiteralText;
  char name[InMemoryFieldNameCapacity]{};
  char *literal = nullptr;
  int width = 0;
};
object *SilosUiSourceRef = NULL;
char SilosUiTypeName[InMemoryFieldNameCapacity]{};
SilosUiTypeField SilosUiTypeFields[SilosUiTypeFieldCapacity]{};
std::size_t SilosUiTypeFieldCount = 0;
SilosUiInstruction *SilosUiInstructions = nullptr;
std::size_t SilosUiInstructionCount = 0;
bool SilosUiTypeDeclared = false;
bool SilosUiRefDeclared = false;
bool SilosUiItemTemplateDeclared = false;
bool SilosUiListDeclared = false;
bool SilosUiMounted = false;
bool SilosUiLiteralDeclared = false;
bool SilosUiLiteralRendered = false;
int SilosUiRefHandle = 0;
int SilosUiTemplateHandle = 0;
int SilosUiListHandle = 0;
char SilosUiPendingText[PokeStringCapacity]{};
char SilosUiEmptyText[PokeStringCapacity]{};
char SilosUiErrorText[PokeStringCapacity]{};

void silos_release_ui_instructions(SilosUiInstruction *instructions,
                                   std::size_t count) {
  if (instructions == nullptr) return;
  for (std::size_t index = 0; index < count; ++index) {
    delete[] instructions[index].literal;
    instructions[index].literal = nullptr;
  }
  delete[] instructions;
}

SilosUiInstruction *silos_allocate_ui_instructions(std::size_t count) {
  if (count == 0 || count > SIZE_MAX / sizeof(SilosUiInstruction)) return nullptr;
  return new (std::nothrow) SilosUiInstruction[count];
}

void silos_copy_ui_literal_unchecked(object *source, char *destination) {
  // Preflight has already established that SOURCE is a valid Lisp string and
  // that DESTINATION has exactly stringlength(SOURCE) + 1 bytes. Copying the
  // packed character cells directly keeps this post-allocation phase free of
  // checkstring/error2 longjmps.
  source = cdr(source);
  std::size_t index = 0;
  while (source != nil) {
    const int characters = source->integer;
    for (int shift = (sizeof(int) - 1) * 8; shift >= 0; shift -= 8) {
      const char character = static_cast<char>((characters >> shift) & 0xFF);
      if (character != '\0') destination[index++] = character;
    }
    source = car(source);
  }
  destination[index] = '\0';
}

bool silos_ui_list_length(object *list, std::size_t &length) {
  length = 0;
  while (list != nil) {
    if (!consp(list) || length == SIZE_MAX) return false;
    ++length;
    list = rest(list);
  }
  return true;
}

void silos_render_ui() {
  if (!SilosUiMounted || SilosUiSourceRef == nil) return;
  object *metadata = silos_find_field(SilosUiSourceRef, "meta");
  object *status = metadata == nil ? nil : silos_find_field(cdr(metadata), "status");
  if (status == nil || silos_symbol_is(cdr(status), "pending")) {
    silos_ui_render_begin(UiRenderState::Pending, SilosUiPendingText);
    std::printf("ui-list region=main state=pending text=%s\\n", SilosUiPendingText);
    UiPendingRendered = true;
    return;
  }
  if (!silos_symbol_is(cdr(status), "ready")) {
    silos_ui_render_begin(UiRenderState::Error, SilosUiErrorText);
    std::printf("ui-list region=main state=error text=%s\\n", SilosUiErrorText);
    return;
  }
  object *value = silos_find_field(SilosUiSourceRef, "value");
  object *rows = value == nil ? nil : cdr(value);
  if (rows == nil) {
    silos_ui_render_begin(UiRenderState::Empty, SilosUiEmptyText);
    std::printf("ui-list region=main state=empty text=%s\\n", SilosUiEmptyText);
    return;
  }
  silos_ui_render_begin(UiRenderState::Ready, "Bound to-do items");
  std::printf("ui-list region=main state=ready\\n");
  std::size_t row_index = 0;
  while (rows != nil && row_index < 5) {
    if (!consp(rows)) { std::printf("ui-list render-error=improper-rows\\n"); return; }
    object *row_value = silos_find_field(first(rows), "value");
    std::printf("ui-row index=%zu", row_index);
    for (std::size_t instruction_index = 0;
         instruction_index < SilosUiInstructionCount; ++instruction_index) {
      const SilosUiInstruction &instruction = SilosUiInstructions[instruction_index];
      if (instruction.kind == SilosUiInstructionKind::LiteralText) {
        std::printf(" text=%s", instruction.literal);
        silos_ui_render_text(static_cast<int>(row_index), nullptr,
                             instruction.literal);
        SilosUiLiteralRendered = true;
        continue;
      }
      object *field = row_value == nil ? nil :
          silos_find_field(cdr(row_value), instruction.name);
      if (field == nil || !stringp(cdr(field))) {
        std::printf(" %s=<invalid>", instruction.name);
      } else {
        char rendered[InMemoryFieldValueCapacity]{};
        cstring(cdr(field), rendered, sizeof(rendered));
        rendered[instruction.width < static_cast<int>(sizeof(rendered)) ?
                 instruction.width : static_cast<int>(sizeof(rendered)) - 1] = '\0';
        std::printf(" %s=%s", instruction.name, rendered);
        silos_ui_render_text(static_cast<int>(row_index),
                             instruction.name, rendered);
      }
    }
    std::printf("\\n");
    ++row_index;
    rows = rest(rows);
  }
  UiReadyRendered = row_index == 2;
}

bool silos_ui_is_declared_field(object *field) {
  if (!symbolp(field)) return false;
  for (std::size_t index = 0; index < SilosUiTypeFieldCount; ++index) {
    if (silos_symbol_name_is(field, SilosUiTypeFields[index].name)) return true;
  }
  return false;
}

bool silos_ui_copy_declared_field_name_unchecked(object *field, char *name) {
  // Validation has already proved FIELD belongs to the declared type. Match
  // against that type's stored C names and copy one of those names, avoiding
  // printsymbol (and its error path) after native allocation begins.
  for (std::size_t index = 0; index < SilosUiTypeFieldCount; ++index) {
    if (silos_symbol_is(field, SilosUiTypeFields[index].name)) {
      std::snprintf(name, InMemoryFieldNameCapacity, "%s",
                    SilosUiTypeFields[index].name);
      return true;
    }
  }
  return false;
}

bool silos_ui_template_preflight(object *specs, const char *item_parameter,
                                 std::size_t instruction_count) {
  std::size_t visited = 0;
  while (specs != nil) {
    object *spec = first(specs);
    std::size_t spec_length = 0;
    if (!consp(spec) || !silos_ui_list_length(spec, spec_length) ||
        !silos_symbol_name_is(first(spec), "ui-text")) return false;
    if (spec_length == 2 && stringp(second(spec))) {
      const int literal_length = stringlength(second(spec));
      // The allocation expression adds one signed byte for the terminator.
      // Reject INT_MAX before that addition can overflow.
      if (literal_length < 0 || literal_length == INT_MAX) return false;
    } else {
      std::size_t field_length = 0;
      if (spec_length != 6 || !consp(second(spec)) ||
          !silos_ui_list_length(second(spec), field_length) || field_length != 3 ||
          !silos_symbol_name_is(first(second(spec)), "ui-field") ||
          !silos_symbol_name_is(second(second(spec)), item_parameter) ||
          !silos_ui_is_declared_field(third(second(spec))) ||
          !silos_symbol_name_is(third(spec), ":width") ||
          !integerp(second(cddr(spec))) ||
          !silos_symbol_name_is(first(cddr(cddr(spec))), ":overflow") ||
          !silos_symbol_name_is(second(cddr(cddr(spec))), "chop")) return false;
      const int width = checkinteger(second(cddr(spec)));
      if (width < 1 || width > 32) return false;
    }
    ++visited;
    specs = rest(specs);
  }
  return visited == instruction_count;
}

bool silos_ui_is_list_of_declared_type(object *declaration) {
  return consp(declaration) && listlength(declaration) == 2 &&
      silos_symbol_name_is(first(declaration), "list-of") &&
      symbolp(second(declaration)) &&
      silos_symbol_name_is(second(declaration), SilosUiTypeName);
}

bool silos_ui_is_store_ref_list_of_declared_type(object *declaration) {
  return consp(declaration) && listlength(declaration) == 2 &&
      silos_symbol_name_is(first(declaration), "store-ref") &&
      silos_ui_is_list_of_declared_type(second(declaration));
}

object *fn_uitype (object *args, object *env) {
  (void) env;
  if (SilosUiTypeDeclared || !symbolp(first(args))) error2("invalid ui type");
  const int field_count = listlength(cdr(args));
  if (field_count < 1 || field_count > static_cast<int>(SilosUiTypeFieldCapacity) ||
      !silos_copy_symbol_name(first(args), SilosUiTypeName,
                              sizeof(SilosUiTypeName))) {
    error2("invalid ui type");
  }
  object *fields = cdr(args);
  for (int index = 0; index < field_count; ++index) {
    object *field = first(fields);
    if (!consp(field) || listlength(field) != 3 || !symbolp(first(field)) ||
        !silos_symbol_name_is(second(field), "string") || !integerp(third(field)) ||
        !silos_copy_symbol_name(first(field), SilosUiTypeFields[index].name,
                                InMemoryFieldNameCapacity)) {
      error2("invalid ui type field");
    }
    const int slot = checkinteger(third(field));
    if (slot < 0 || slot >= field_count) error2("invalid ui type slot");
    for (int previous = 0; previous < index; ++previous) {
      if (SilosUiTypeFields[previous].slot == slot) error2("duplicate ui type slot");
    }
    SilosUiTypeFields[index].slot = slot;
    fields = rest(fields);
  }
  for (int slot = 0; slot < field_count; ++slot) {
    bool found = false;
    for (int index = 0; index < field_count; ++index) {
      if (SilosUiTypeFields[index].slot == slot) {
        found = true;
        break;
      }
    }
    if (!found) error2("non-contiguous ui type slots");
  }
  SilosUiTypeFieldCount = static_cast<std::size_t>(field_count);
  SilosUiTypeDeclared = true;
  return number(1);
}

object *fn_uibind (object *args, object *env) {
  if (SilosUiRefDeclared || !symbolp(first(args)) ||
      !silos_ui_is_store_ref_list_of_declared_type(second(args))) {
    error2("invalid ui source");
  }
  object *source = eval(third(args), env);
  if (source != SilosBoundStoreRef) error2("invalid ui source");
  SilosUiSourceRef = source;
  SilosUiRefDeclared = true;
  SilosUiRefHandle = 1;
  return number(SilosUiRefHandle);
}

object *fn_uitemplate (object *args, object *env) {
  (void) env;
  if (!SilosUiTypeDeclared || SilosUiItemTemplateDeclared || !symbolp(first(args)) ||
      !consp(second(args)) || listlength(second(args)) != 2 ||
      !symbolp(first(second(args))) || !symbolp(second(second(args))) ||
      !silos_symbol_name_is(second(second(args)), SilosUiTypeName)) {
    error2("invalid ui item template");
  }
  char item_parameter[InMemoryFieldNameCapacity]{};
  if (!silos_copy_symbol_name(first(second(args)), item_parameter,
                              sizeof(item_parameter))) error2("invalid ui item parameter");
  args = cddr(args);
  std::size_t instruction_count = 0;
  if (!silos_ui_list_length(args, instruction_count) || instruction_count == 0) {
    error2("empty ui item template");
  }
  // All Lisp inspection, symbol printing, and value checks finish before the
  // first native allocation. uLisp errors use longjmp, so native ownership
  // must not exist while any of those operations can still signal.
  if (!silos_ui_template_preflight(args, item_parameter, instruction_count)) {
    error2("invalid ui instruction");
  }
  SilosUiInstruction *candidate = silos_allocate_ui_instructions(
      instruction_count);
  if (candidate == nullptr) error2("ui template allocation failed");
  std::size_t candidate_count = 0;
  while (args != nil) {
    object *spec = first(args);
    SilosUiInstruction &instruction = candidate[candidate_count];
    if (stringp(second(spec))) {
      const int literal_length = stringlength(second(spec));
      const std::size_t allocation_size =
          static_cast<std::size_t>(literal_length) + 1U;
      instruction.literal = new (std::nothrow) char[allocation_size];
      if (instruction.literal == nullptr) {
        silos_release_ui_instructions(candidate, instruction_count);
        error2("ui template allocation failed");
      }
      silos_copy_ui_literal_unchecked(second(spec), instruction.literal);
      instruction.kind = SilosUiInstructionKind::LiteralText;
    } else {
      const bool copied = silos_ui_copy_declared_field_name_unchecked(
          third(second(spec)), instruction.name);
      if (!copied) {
        // Preflight proved this match. A mismatch is an internal invariant
        // failure, not a Lisp validation error; release ownership explicitly
        // because uLisp's longjmp-based error path is forbidden here.
        silos_release_ui_instructions(candidate, instruction_count);
        std::abort();
      }
      instruction.kind = SilosUiInstructionKind::FieldText;
      instruction.width = second(cddr(spec))->integer;
    }
    ++candidate_count;
    args = rest(args);
  }
  // Publish only after every descriptor and literal allocation validated.
  // A failure above releases the candidate and leaves the registry unchanged.
  SilosUiInstructions = candidate;
  SilosUiInstructionCount = candidate_count;
  for (std::size_t index = 0; index < candidate_count; ++index) {
    if (candidate[index].kind == SilosUiInstructionKind::LiteralText) {
      SilosUiLiteralDeclared = true;
    }
  }
  SilosUiItemTemplateDeclared = true;
  SilosUiTemplateHandle = 2;
  return number(SilosUiTemplateHandle);
}

object *fn_uitemplatelist (object *args, object *env) {
  // This special form receives the public declarative keyword spelling intact.
  // In uLisp's builtin arity encoding, a maximum of seven means an unbounded
  // tail, so the compact implementation does not require a public alternate.
  object *config = cdr(args);
  if (listlength(config) != 14) error2("invalid ui list");
  object *source_keyword = first(config);
  object *source_form = second(config);
  config = cddr(config);
  object *template_keyword = first(config);
  object *template_form = second(config);
  config = cddr(config);
  object *offset_keyword = first(config);
  object *offset = second(config);
  config = cddr(config);
  object *limit_keyword = first(config);
  object *limit = second(config);
  config = cddr(config);
  object *pending_keyword = first(config);
  object *pending = second(config);
  config = cddr(config);
  object *empty_keyword = first(config);
  object *empty = second(config);
  config = cddr(config);
  object *error_keyword = first(config);
  object *error_text = second(config);
  if (SilosUiListDeclared || !SilosUiRefDeclared || !SilosUiItemTemplateDeclared ||
      !symbolp(first(args)) || !silos_symbol_is(source_keyword, ":source") ||
      checkinteger(eval(source_form, env)) != SilosUiRefHandle ||
      !silos_symbol_is(template_keyword, ":item-template") ||
      checkinteger(eval(template_form, env)) != SilosUiTemplateHandle ||
      !silos_symbol_is(offset_keyword, ":offset") || checkinteger(offset) != 0 ||
      !silos_symbol_is(limit_keyword, ":limit") || checkinteger(limit) != 5 ||
      !silos_symbol_is(pending_keyword, ":pending") ||
      !silos_symbol_is(empty_keyword, ":empty") ||
      !silos_symbol_is(error_keyword, ":error")) error2("invalid ui list");
  cstring(checkstring(pending), SilosUiPendingText, sizeof(SilosUiPendingText));
  cstring(checkstring(empty), SilosUiEmptyText, sizeof(SilosUiEmptyText));
  cstring(checkstring(error_text), SilosUiErrorText, sizeof(SilosUiErrorText));
  SilosUiListDeclared = true;
  SilosUiListHandle = 3;
  return number(SilosUiListHandle);
}

object *fn_uimount (object *args, object *env) {
  (void) env;
  if (!SilosUiListDeclared || SilosUiMounted || checkinteger(first(args)) != SilosUiListHandle ||
      !silos_symbol_is(second(args), ":region") || !symbolp(third(args))) error2("invalid ui mount");
  SilosUiMounted = true;
  silos_render_ui();
  return number(4);
}
