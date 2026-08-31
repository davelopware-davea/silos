const char doc_shellappregister[] = "(shell-app-register :name string :ideal-width integer :ideal-height integer :entry string)\nRegisters the current app declaration during bootstrap.";
const char doc_shellapponevent[] = "(shell-app-on-event handler)\nRegisters the current app event handler.";
// Bootstrap deliberately trusts each bounded manifest. It reads the
// documented positional keyword/value shape and leaves manifest validation for
// the later, bounded describe-mode experiment.
object *fn_shellappregister (object *args, object *env) {
  (void) env;
  args = cdr(args);                         // Skip :name.
  object *display_name = checkstring(first(args));
  std::string name = silos_ulisp_string(display_name);
  args = cdr(cdr(args));                    // Skip name and :ideal-width.
  int ideal_width = checkinteger(first(args));
  args = cdr(cdr(args));                    // Skip width and :ideal-height.
  int ideal_height = checkinteger(first(args));
  args = cdr(cdr(args));                    // Skip height and :entry.
  std::string entry = silos_ulisp_string(checkstring(first(args)));
  silos_capture_app_declaration(display_name, std::move(name), ideal_width, ideal_height,
                                std::move(entry));
  return tee;
}

namespace {
const IPlatformStore *SilosCurrentSource = nullptr;
std::size_t SilosCurrentRow = 0;
std::size_t SilosCurrentCharacter = 0;
bool SilosSourceAtEnd = false;

int silos_read_source_character() {
  while (SilosCurrentSource != nullptr &&
         SilosCurrentRow < SilosCurrentSource->rowCount()) {
    const StoreRow *row = SilosCurrentSource->rowAt(SilosCurrentRow);
    configASSERT(row != nullptr);
    const StoreField *text = SilosCurrentSource->findField(*row, "text");
    configASSERT(text != nullptr);
    if (SilosCurrentCharacter < text->value.size()) {
      return static_cast<unsigned char>(text->value[SilosCurrentCharacter++]);
    }
    ++SilosCurrentRow;
    SilosCurrentCharacter = 0;
    return '\n';
  }
  SilosSourceAtEnd = true;
  return -1;
}
}

bool silos_ulisp_evaluate(const IPlatformStore &store) {
  SilosCurrentSource = &store;
  SilosCurrentRow = 0;
  SilosCurrentCharacter = 0;
  SilosSourceAtEnd = false;
  if (setjmp(toplevel_handler)) {
    ulisperror();
    return false;
  }
  for (;;) {
    object *form = readmain(silos_read_source_character);
    if (SilosSourceAtEnd && form == nil) return true;
    protect(form);
    (void)eval(form, nullptr);
    unprotect();
  }
}
