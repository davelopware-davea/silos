const char doc_shellappregister[] = "(shell-app-register :name string :ideal-width integer :ideal-height integer :entry string)\nRegisters the current app declaration during bootstrap.";
const char doc_shellapponevent[] = "(shell-app-on-event handler)\nRegisters the current app event handler.";
// The initial bootstrap deliberately trusts its one manifest.  It reads the
// documented positional keyword/value shape and leaves manifest validation for
// the later, bounded describe-mode experiment.
object *fn_shellappregister (object *args, object *env) {
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
InMemoryStore::const_iterator SilosCurrentRow;
std::size_t SilosCurrentCharacter = 0;
bool SilosSourceAtEnd = false;

int silos_read_source_character() {
  while (SilosCurrentSource != nullptr &&
         SilosCurrentRow != SilosCurrentSource->end()) {
    const InMemoryStoreField *text = SourceStores.find_field(
        *SilosCurrentRow, "text");
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

bool silos_ulisp_evaluate(const InMemoryStore &store) {
  SilosCurrentSource = &store;
  SilosCurrentRow = store.begin();
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
