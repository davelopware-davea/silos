const char doc_appdeclare[] = "(app-declare :name string :ideal-width integer :ideal-height integer :entry string)\nRegisters the current app declaration during bootstrap.";
const char doc_appstart[] = "(app-start handler)\nRegisters the current app event handler.";
// The initial bootstrap deliberately trusts its one manifest.  It reads the
// documented positional keyword/value shape and leaves manifest validation for
// the later, bounded describe-mode experiment.
object *fn_appdeclare (object *args, object *env) {
  (void) env;
  char name[32];
  char entry[48];
  args = cdr(args);                         // Skip :name.
  cstring(checkstring(first(args)), name, sizeof(name));
  args = cdr(cdr(args));                    // Skip name and :ideal-width.
  int ideal_width = checkinteger(first(args));
  args = cdr(cdr(args));                    // Skip width and :ideal-height.
  int ideal_height = checkinteger(first(args));
  args = cdr(cdr(args));                    // Skip height and :entry.
  cstring(checkstring(first(args)), entry, sizeof(entry));
  silos_capture_app_declaration(name, ideal_width, ideal_height, entry);
  return tee;
}

namespace {
const InMemoryStore *SilosCurrentSource = nullptr;
std::size_t SilosCurrentRow = 0;
std::size_t SilosCurrentCharacter = 0;
bool SilosSourceAtEnd = false;

int silos_read_source_character() {
  while (SilosCurrentSource != nullptr &&
         SilosCurrentRow < SilosCurrentSource->row_count) {
    const InMemoryStoreField *text = SourceStores.find_field(
        SilosCurrentSource->rows[SilosCurrentRow], "text");
    configASSERT(text != nullptr);
    const char character = text->value[SilosCurrentCharacter++];
    if (character != '\0') return static_cast<unsigned char>(character);
    ++SilosCurrentRow;
    SilosCurrentCharacter = 0;
    return '\n';
  }
  SilosSourceAtEnd = true;
  return -1;
}
}

bool silos_ulisp_evaluate(const InMemoryStore &store) {
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
