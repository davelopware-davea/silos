const char doc_uitype[] = "(ui-type name (field type slot)...)";
const char doc_uibind[] = "(ui-bind name type)";
const char doc_uitemplate[] = "(ui-template name [(item type)] (ui-text string | (ui-field item-or-ui-ref field) :width integer :overflow ui-chop)...)";
const char doc_uitemplatelist[] = "(ui-template-list name :source ui-ref :item-template template :offset integer :limit integer :pending string :empty string :error string)";
const char doc_uimount[] = "(ui-mount template)";

constexpr std::size_t SilosUiRefCapacity = 16;
constexpr std::size_t SilosUiTypeCapacity = 8;
constexpr std::size_t SilosUiTemplateCapacity = 12;
constexpr std::size_t SilosUiMountCapacity = 8;
constexpr std::size_t SilosUiTypeFieldCapacity = InMemoryFieldsPerRowCapacity;
constexpr std::size_t SilosUiRenderedFieldCapacity = 256;
constexpr std::size_t SilosUiVisibleRowCapacity = 5;

struct SilosUiTypeField {
  char name[InMemoryFieldNameCapacity]{};
  int slot = -1;
};

struct SilosUiType {
  char name[InMemoryFieldNameCapacity]{};
  SilosUiTypeField fields[SilosUiTypeFieldCapacity]{};
  std::size_t field_count = 0;
};

enum class SilosUiInstructionKind { LiteralText, FieldText, UiRefFieldText };

struct SilosUiInstruction {
  SilosUiInstructionKind kind = SilosUiInstructionKind::LiteralText;
  char name[InMemoryFieldNameCapacity]{};
  char *literal = nullptr;
  int width = 0;
  std::size_t source_ref_index = 0;
};

struct SilosUiRef {
  object *binding = nil;
  std::size_t type_index = 0;
};

enum class SilosUiTemplateKind { Flow, Item, List };

struct SilosUiTemplate {
  SilosUiTemplateKind kind = SilosUiTemplateKind::Item;
  char name[InMemoryFieldNameCapacity]{};
  std::size_t item_type_index = 0;
  SilosUiInstruction *instructions = nullptr;
  std::size_t instruction_count = 0;
  std::size_t source_ref_index = 0;
  std::size_t item_template_index = 0;
  std::size_t offset = 0;
  std::size_t limit = 0;
  char pending[PokeStringCapacity]{};
  char empty[PokeStringCapacity]{};
  char error[PokeStringCapacity]{};
};

struct SilosAppUi {
  SilosUiType types[SilosUiTypeCapacity]{};
  std::size_t type_count = 0;
  SilosUiRef refs[SilosUiRefCapacity]{};
  std::size_t ref_count = 0;
  SilosUiTemplate templates[SilosUiTemplateCapacity]{};
  std::size_t template_count = 0;
  std::size_t mounts[SilosUiMountCapacity]{};
  std::size_t mount_count = 0;
};

SilosAppUi SilosAppUis[SilosAppCapacity]{};
std::size_t SilosRenderedAppCount = 0;
std::size_t SilosRenderedMountCount = 0;
std::size_t SilosRenderedListRowCount = 0;
std::size_t SilosRenderedInstructionCount = 0;

void silos_release_ui_instructions(SilosUiInstruction *instructions,
                                   std::size_t count) {
  if (instructions == nullptr) return;
  for (std::size_t index = 0; index < count; ++index) {
    delete[] instructions[index].literal;
  }
  delete[] instructions;
}

SilosUiInstruction *silos_allocate_ui_instructions(std::size_t count) {
  if (count == 0 || count > SIZE_MAX / sizeof(SilosUiInstruction)) return nullptr;
  return new (std::nothrow) SilosUiInstruction[count];
}

void silos_copy_ui_literal_unchecked(object *source, char *destination) {
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

SilosAppUi &silos_current_app_ui() {
  if (CurrentAppIndex >= AppCount) error2("no current app");
  return SilosAppUis[CurrentAppIndex];
}

std::size_t silos_ui_type_index(const SilosAppUi &app, object *name) {
  for (std::size_t index = 0; index < app.type_count; ++index) {
    if (silos_symbol_name_is(name, app.types[index].name)) return index;
  }
  return SilosUiTypeCapacity;
}

bool silos_ui_is_declared_field(const SilosUiType &type, object *field) {
  if (!symbolp(field)) return false;
  for (std::size_t index = 0; index < type.field_count; ++index) {
    if (silos_symbol_name_is(field, type.fields[index].name)) return true;
  }
  return false;
}

bool silos_ui_copy_declared_field_name_unchecked(const SilosUiType &type,
                                                  object *field, char *name) {
  for (std::size_t index = 0; index < type.field_count; ++index) {
    if (silos_symbol_is(field, type.fields[index].name)) {
      std::snprintf(name, InMemoryFieldNameCapacity, "%s", type.fields[index].name);
      return true;
    }
  }
  return false;
}

std::size_t silos_ui_ref_index(const SilosAppUi &app, object *name,
                               object *env) {
  if (!symbolp(name)) return SilosUiRefCapacity;
  object *binding = findpair(name, env);
  if (binding == nil || !integerp(cdr(binding))) return SilosUiRefCapacity;
  const int handle = cdr(binding)->integer;
  if (handle < 1 || static_cast<std::size_t>(handle) > app.ref_count) {
    return SilosUiRefCapacity;
  }
  return static_cast<std::size_t>(handle - 1);
}

bool silos_ui_template_preflight(object *specs, const char *item_parameter,
                                 const SilosUiType *type, const SilosAppUi &app,
                                 object *env, std::size_t instruction_count) {
  std::size_t visited = 0;
  while (specs != nil) {
    object *spec = first(specs);
    std::size_t spec_length = 0;
    if (!consp(spec) || !silos_ui_list_length(spec, spec_length) ||
        !silos_symbol_name_is(first(spec), "ui-text")) return false;
    if (spec_length == 2 && stringp(second(spec))) {
      const int literal_length = stringlength(second(spec));
      if (literal_length < 0 || literal_length == INT_MAX) return false;
    } else if (item_parameter != nullptr) {
      std::size_t field_length = 0;
      if (spec_length != 6 || !consp(second(spec)) ||
          !silos_ui_list_length(second(spec), field_length) || field_length != 3 ||
          !silos_symbol_name_is(first(second(spec)), "ui-field") ||
          !silos_symbol_name_is(second(second(spec)), item_parameter) ||
          type == nullptr ||
          !silos_ui_is_declared_field(*type, third(second(spec))) ||
          !silos_symbol_name_is(third(spec), ":width") ||
          !integerp(second(cddr(spec))) ||
          !silos_symbol_name_is(first(cddr(cddr(spec))), ":overflow") ||
          !silos_symbol_name_is(second(cddr(cddr(spec))), "ui-chop")) return false;
      const int width = checkinteger(second(cddr(spec)));
      if (width < 1 || width > 32) return false;
    } else {
      std::size_t field_length = 0;
      object *field = second(spec);
      if (spec_length != 6 || !consp(field) ||
          !silos_ui_list_length(field, field_length) || field_length != 3 ||
          !silos_symbol_name_is(first(field), "ui-field") ||
          silos_ui_ref_index(app, second(field), env) == SilosUiRefCapacity ||
          !silos_symbol_name_is(third(field), "count") ||
          !silos_symbol_name_is(third(spec), ":width") ||
          !integerp(second(cddr(spec))) ||
          !silos_symbol_name_is(first(cddr(cddr(spec))), ":overflow") ||
          !silos_symbol_name_is(second(cddr(cddr(spec))), "ui-chop")) {
        return false;
      }
      const int width = checkinteger(second(cddr(spec)));
      if (width < 1 || width > 32) return false;
    }
    ++visited;
    specs = rest(specs);
  }
  return visited == instruction_count;
}

bool silos_ui_is_list_of_type(object *declaration, const char *type_name) {
  return consp(declaration) && listlength(declaration) == 2 &&
      silos_symbol_name_is(first(declaration), "ui-list-of") &&
      silos_symbol_name_is(second(declaration), type_name);
}

bool silos_ui_is_store_ref_list_of_type(object *declaration,
                                        const char *type_name) {
  return consp(declaration) && listlength(declaration) == 2 &&
      silos_symbol_name_is(first(declaration), "store-ref") &&
      silos_ui_is_list_of_type(second(declaration), type_name);
}

void silos_render_item_template(std::size_t app_index,
                                std::size_t mount_index,
                                const SilosAppUi &app,
                                const SilosUiTemplate &item_template,
                                int row_index, object *row_value) {
  for (std::size_t index = 0; index < item_template.instruction_count; ++index) {
    ++SilosRenderedInstructionCount;
    const SilosUiInstruction &instruction = item_template.instructions[index];
    if (instruction.kind == SilosUiInstructionKind::LiteralText) {
      silos_ui_render_text(app_index, mount_index, row_index, nullptr,
                           instruction.literal);
      continue;
    }
    if (instruction.kind == SilosUiInstructionKind::UiRefFieldText) {
      const SilosUiRef &ref = app.refs[instruction.source_ref_index];
      object *source = ref.binding == nil ? nil : cdr(ref.binding);
      object *metadata = source == nil ? nil : silos_find_field(source, "meta");
      object *field = metadata == nil ? nil :
          silos_find_field(cdr(metadata), instruction.name);
      if (field == nil || !integerp(cdr(field))) continue;
      char rendered[24]{};
      std::snprintf(rendered, sizeof(rendered), "%d", cdr(field)->integer);
      rendered[instruction.width < static_cast<int>(sizeof(rendered))
          ? instruction.width : static_cast<int>(sizeof(rendered)) - 1] = '\0';
      silos_ui_render_text(app_index, mount_index, row_index, instruction.name,
                           rendered);
      continue;
    }
    object *field = row_value == nil ? nil :
        silos_find_field(cdr(row_value), instruction.name);
    if (field == nil || !stringp(cdr(field))) continue;
    char rendered[SilosUiRenderedFieldCapacity]{};
    cstring(cdr(field), rendered, sizeof(rendered));
    const int end = instruction.width < static_cast<int>(sizeof(rendered))
        ? instruction.width : static_cast<int>(sizeof(rendered)) - 1;
    rendered[end] = '\0';
    silos_ui_render_text(app_index, mount_index, row_index,
                         instruction.name, rendered);
  }
}

void silos_render_list_template(std::size_t app_index,
                                std::size_t mount_index,
                                const SilosAppUi &app,
                                const SilosUiTemplate &list_template) {
  const SilosUiRef &ref = app.refs[list_template.source_ref_index];
  object *source = ref.binding == nil ? nil : cdr(ref.binding);
  if (source == nil) return;
  object *metadata = silos_find_field(source, "meta");
  object *status = metadata == nil ? nil : silos_find_field(cdr(metadata), "status");
  if (status == nil || silos_symbol_is(cdr(status), "silos-pending")) {
    UiPendingRendered = true;
    silos_ui_render_template_begin(app_index, mount_index, UiRenderState::Pending,
                                   list_template.pending, true);
    return;
  }
  if (!silos_symbol_is(cdr(status), "silos-ready")) {
    silos_ui_render_template_begin(app_index, mount_index, UiRenderState::Error,
                                   list_template.error, true);
    return;
  }
  object *value = silos_find_field(source, "value");
  object *rows = value == nil ? nil : cdr(value);
  std::size_t skipped = 0;
  while (rows != nil && skipped < list_template.offset) {
    if (!consp(rows)) return;
    rows = rest(rows);
    ++skipped;
  }
  if (rows == nil) {
    silos_ui_render_template_begin(app_index, mount_index, UiRenderState::Empty,
                                   list_template.empty, true);
    return;
  }
  silos_ui_render_template_begin(app_index, mount_index, UiRenderState::Ready,
                                 "", true);
  UiReadyRendered = true;
  const SilosUiTemplate &item_template = app.templates[list_template.item_template_index];
  std::size_t row_index = 0;
  while (rows != nil && row_index < list_template.limit) {
    if (!consp(rows)) return;
    object *row_value = silos_find_field(first(rows), "value");
    silos_render_item_template(app_index, mount_index, app, item_template,
                               static_cast<int>(row_index), row_value);
    ++row_index;
    ++SilosRenderedListRowCount;
    rows = rest(rows);
  }
}

void silos_render_template(std::size_t app_index, std::size_t mount_index,
                           std::size_t template_index,
                           const SilosAppUi &app) {
  ++SilosRenderedMountCount;
  const SilosUiTemplate &rendered = app.templates[template_index];
  if (rendered.kind == SilosUiTemplateKind::List) {
    silos_render_list_template(app_index, mount_index, app, rendered);
    return;
  }
  silos_ui_render_template_begin(app_index, mount_index, UiRenderState::Ready,
                                 "", false);
  silos_render_item_template(app_index, mount_index, app, rendered, -1, nil);
}

void silos_render_app(std::size_t app_index) {
  ++SilosRenderedAppCount;
  const SilosAppUi &app = SilosAppUis[app_index];
  silos_ui_render_app_begin(app_index, AppDeclarations[app_index].name);
  for (std::size_t mount = 0; mount < app.mount_count; ++mount) {
    silos_render_template(app_index, mount, app.mounts[mount], app);
  }
}

void silos_render_ui() {
  SilosRenderedAppCount = 0;
  SilosRenderedMountCount = 0;
  SilosRenderedListRowCount = 0;
  SilosRenderedInstructionCount = 0;
  silos_ui_render_begin();
  for (std::size_t app_index = 0; app_index < AppCount; ++app_index) {
    silos_render_app(app_index);
  }
}

object *fn_uitype(object *args, object *env) {
  (void)env;
  SilosAppUi &app = silos_current_app_ui();
  if (app.type_count == SilosUiTypeCapacity || !symbolp(first(args))) {
    error2("ui type capacity or invalid name");
  }
  for (std::size_t index = 0; index < app.type_count; ++index) {
    if (silos_symbol_name_is(first(args), app.types[index].name)) error2("duplicate ui type");
  }
  const int field_count = listlength(cdr(args));
  if (field_count < 1 || field_count > static_cast<int>(SilosUiTypeFieldCapacity)) {
    error2("invalid ui type");
  }
  SilosUiType candidate{};
  if (!silos_copy_symbol_name(first(args), candidate.name, sizeof(candidate.name))) {
    error2("invalid ui type name");
  }
  object *fields = cdr(args);
  for (int index = 0; index < field_count; ++index) {
    object *field = first(fields);
    if (!consp(field) || listlength(field) != 3 || !symbolp(first(field)) ||
        !silos_symbol_name_is(second(field), "string") || !integerp(third(field)) ||
        !silos_copy_symbol_name(first(field), candidate.fields[index].name,
                                InMemoryFieldNameCapacity)) {
      error2("invalid ui type field");
    }
    const int slot = checkinteger(third(field));
    if (slot < 0 || slot >= field_count) error2("invalid ui type slot");
    for (int previous = 0; previous < index; ++previous) {
      if (candidate.fields[previous].slot == slot) error2("duplicate ui type slot");
    }
    candidate.fields[index].slot = slot;
    fields = rest(fields);
  }
  candidate.field_count = static_cast<std::size_t>(field_count);
  app.types[app.type_count] = candidate;
  ++app.type_count;
  return number(static_cast<int>(app.type_count));
}

object *fn_uibind(object *args, object *env) {
  SilosAppUi &app = silos_current_app_ui();
  if (app.ref_count == SilosUiRefCapacity || !symbolp(first(args))) {
    error2("ui ref capacity or invalid source");
  }
  std::size_t type_index = SilosUiTypeCapacity;
  for (std::size_t index = 0; index < app.type_count; ++index) {
    if (silos_ui_is_store_ref_list_of_type(second(args), app.types[index].name)) {
      type_index = index;
      break;
    }
  }
  object *binding = findpair(first(args), env);
  if (type_index == SilosUiTypeCapacity || binding == nil ||
      cdr(binding) != SilosAppStoreBindings[CurrentAppIndex].ref) {
    error2("invalid ui source");
  }
  SilosUiRef &ref = app.refs[app.ref_count];
  ref.binding = binding;
  ref.type_index = type_index;
  ++app.ref_count;
  return number(static_cast<int>(app.ref_count));
}

object *fn_uitemplate(object *args, object *env) {
  SilosAppUi &app = silos_current_app_ui();
  if (app.template_count == SilosUiTemplateCapacity || !symbolp(first(args))) {
    error2("invalid ui template");
  }
  const bool is_item = consp(second(args)) && listlength(second(args)) == 2 &&
      symbolp(first(second(args))) && symbolp(second(second(args))) &&
      silos_ui_type_index(app, second(second(args))) != SilosUiTypeCapacity;
  const std::size_t type_index = is_item
      ? silos_ui_type_index(app, second(second(args))) : 0;
  char item_parameter[InMemoryFieldNameCapacity]{};
  if (is_item &&
      !silos_copy_symbol_name(first(second(args)), item_parameter,
                              sizeof(item_parameter))) {
    error2("invalid ui item parameter");
  }
  object *specs = is_item ? cddr(args) : cdr(args);
  std::size_t instruction_count = 0;
  if (!silos_ui_list_length(specs, instruction_count) || instruction_count == 0 ||
      !silos_ui_template_preflight(specs,
                                   is_item ? item_parameter : nullptr,
                                   is_item ? &app.types[type_index] : nullptr,
                                   app, env, instruction_count)) {
    error2("invalid ui instruction");
  }
  SilosUiInstruction *candidate = silos_allocate_ui_instructions(instruction_count);
  if (candidate == nullptr) error2("ui template allocation failed");
  std::size_t candidate_count = 0;
  while (specs != nil) {
    object *spec = first(specs);
    SilosUiInstruction &instruction = candidate[candidate_count];
    if (stringp(second(spec))) {
      const std::size_t size = static_cast<std::size_t>(stringlength(second(spec))) + 1U;
      instruction.literal = new (std::nothrow) char[size];
      if (instruction.literal == nullptr) {
        silos_release_ui_instructions(candidate, instruction_count);
        error2("ui template allocation failed");
      }
      silos_copy_ui_literal_unchecked(second(spec), instruction.literal);
      instruction.kind = SilosUiInstructionKind::LiteralText;
    } else if (is_item) {
      if (!silos_ui_copy_declared_field_name_unchecked(
              app.types[type_index], third(second(spec)), instruction.name)) {
        silos_release_ui_instructions(candidate, instruction_count);
        std::abort();
      }
      instruction.kind = SilosUiInstructionKind::FieldText;
      instruction.width = second(cddr(spec))->integer;
    } else {
      instruction.kind = SilosUiInstructionKind::UiRefFieldText;
      instruction.source_ref_index = silos_ui_ref_index(
          app, second(second(spec)), env);
      std::snprintf(instruction.name, sizeof(instruction.name), "count");
      instruction.width = second(cddr(spec))->integer;
    }
    ++candidate_count;
    specs = rest(specs);
  }
  SilosUiTemplate &installed = app.templates[app.template_count];
  installed.kind = is_item ? SilosUiTemplateKind::Item
                           : SilosUiTemplateKind::Flow;
  if (is_item) installed.item_type_index = type_index;
  installed.instructions = candidate;
  installed.instruction_count = candidate_count;
  (void)silos_copy_symbol_name(first(args), installed.name, sizeof(installed.name));
  ++app.template_count;
  return number(static_cast<int>(app.template_count));
}

object *fn_uitemplatelist(object *args, object *env) {
  SilosAppUi &app = silos_current_app_ui();
  object *config = cdr(args);
  if (app.template_count == SilosUiTemplateCapacity || listlength(config) != 14) {
    error2("invalid ui list");
  }
  object *source_keyword = first(config); object *source_form = second(config); config = cddr(config);
  object *template_keyword = first(config); object *template_form = second(config); config = cddr(config);
  object *offset_keyword = first(config); object *offset_form = second(config); config = cddr(config);
  object *limit_keyword = first(config); object *limit_form = second(config); config = cddr(config);
  object *pending_keyword = first(config); object *pending = second(config); config = cddr(config);
  object *empty_keyword = first(config); object *empty = second(config); config = cddr(config);
  object *error_keyword = first(config); object *error_text = second(config);
  const int source_handle = checkinteger(eval(source_form, env));
  const int item_handle = checkinteger(eval(template_form, env));
  const int offset = checkinteger(offset_form);
  const int limit = checkinteger(limit_form);
  if (!symbolp(first(args)) || !silos_symbol_is(source_keyword, ":source") ||
      source_handle < 1 || static_cast<std::size_t>(source_handle) > app.ref_count ||
      !silos_symbol_is(template_keyword, ":item-template") || item_handle < 1 ||
      static_cast<std::size_t>(item_handle) > app.template_count ||
      app.templates[item_handle - 1].kind != SilosUiTemplateKind::Item ||
      app.refs[source_handle - 1].type_index != app.templates[item_handle - 1].item_type_index ||
      !silos_symbol_is(offset_keyword, ":offset") || offset < 0 ||
      !silos_symbol_is(limit_keyword, ":limit") || limit < 0 ||
      limit > static_cast<int>(SilosUiVisibleRowCapacity) ||
      !silos_symbol_is(pending_keyword, ":pending") ||
      !silos_symbol_is(empty_keyword, ":empty") ||
      !silos_symbol_is(error_keyword, ":error")) error2("invalid ui list");
  SilosUiTemplate &installed = app.templates[app.template_count];
  installed.kind = SilosUiTemplateKind::List;
  installed.source_ref_index = static_cast<std::size_t>(source_handle - 1);
  installed.item_template_index = static_cast<std::size_t>(item_handle - 1);
  installed.item_type_index = app.templates[item_handle - 1].item_type_index;
  installed.offset = static_cast<std::size_t>(offset);
  installed.limit = static_cast<std::size_t>(limit);
  cstring(checkstring(pending), installed.pending, sizeof(installed.pending));
  cstring(checkstring(empty), installed.empty, sizeof(installed.empty));
  cstring(checkstring(error_text), installed.error, sizeof(installed.error));
  (void)silos_copy_symbol_name(first(args), installed.name, sizeof(installed.name));
  ++app.template_count;
  return number(static_cast<int>(app.template_count));
}

object *fn_uimount(object *args, object *env) {
  (void)env;
  SilosAppUi &app = silos_current_app_ui();
  const int handle = checkinteger(first(args));
  if (app.mount_count == SilosUiMountCapacity || handle < 1 ||
      static_cast<std::size_t>(handle) > app.template_count) error2("invalid ui mount");
  app.mounts[app.mount_count] = static_cast<std::size_t>(handle - 1);
  ++app.mount_count;
  silos_render_ui();
  return number(static_cast<int>(app.mount_count));
}

void silos_cleanup_app_ui(std::size_t app_index) {
  SilosAppUi &app = SilosAppUis[app_index];
  for (std::size_t index = 0; index < app.template_count; ++index) {
    silos_release_ui_instructions(app.templates[index].instructions,
                                  app.templates[index].instruction_count);
  }
  app = SilosAppUi{};
}
