void silos_render_ui();

ulisp::Object *ulisp::head(Object *value) { return car(value); }
ulisp::Object *ulisp::tail(Object *value) { return cdr(value); }
ulisp::Object *ulisp::secondValue(Object *value) { return second(value); }
ulisp::Object *ulisp::thirdValue(Object *value) { return third(value); }
ulisp::Object *ulisp::tail2(Object *value) { return cddr(value); }
bool ulisp::isCons(Object *value) { return consp(value); }
bool ulisp::isInteger(Object *value) { return integerp(value); }
bool ulisp::isString(Object *value) { return stringp(value); }
bool ulisp::isSymbol(Object *value) { return symbolp(value); }
int ulisp::integerValue(Object *value) { return value->integer; }
int ulisp::checkInteger(Object *value) { return checkinteger(value); }
int ulisp::listLength(Object *value) { return listlength(value); }
ulisp::Object *ulisp::findPair(Object *name, Object *environment) {
  return findpair(name, environment);
}
ulisp::Object *ulisp::makeNumber(int value) { return number(value); }
ulisp::Object *ulisp::makeCons(Object *head, Object *tail) { return cons(head, tail); }
void ulisp::pushRoot(Object *value) { protect(value); }
void ulisp::popRoot() { unprotect(); }
void ulisp::error(const char *message) { error2(message); }

std::string silos_ulisp_string(object *value) {
  std::string result;
  const int length = stringlength(value);
  if (length < 0) error2("invalid string length");
  try {
    result.reserve(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
      result.push_back(static_cast<char>(nthchar(value, index)));
    }
  } catch (const std::bad_alloc &) {
    error2("native allocation failed");
  } catch (const std::length_error &) {
    error2("native string is too long");
  }
  return result;
}

bool silos_ulisp_string_is(object *value, const char *text) {
  if (!stringp(value) || text == nullptr) return false;
  int index = 0;
  for (;; ++index) {
    const char actual = static_cast<char>(nthchar(value, index));
    if (actual != text[index]) return false;
    if (actual == '\0') return true;
  }
}

object *silos_find_symbol_field(object *record, object *name) {
  while (record != nil) {
    if (!consp(record) || !consp(first(record))) return nil;
    object *entry = first(record);
    if (first(entry) == name) return entry;
    record = rest(record);
  }
  return nil;
}

// Refs are ordinary, compact association lists in this experiment. The typed
// StoreRef and StoreRowRef accessors below are the only application-facing
// view, so a later packed representation need not change Lisp source.
bool silos_symbol_is (object *value, const char *name) {
  if (!symbolp(value)) return false;
  // uLisp stores ordinary six-character-or-shorter symbols in its packed
  // radix-40 form, while longer names are linked character cells.  Never use
  // eqsymbols on a packed symbol: it expects that linked representation.
  if (!longsymbolp(value) && std::strlen(name) <= 6 &&
      valid40(const_cast<char *>(name))) {
    return value->name == twist(pack40(const_cast<char *>(name)));
  }
  return longsymbolp(value) && eqsymbols(value, const_cast<char *>(name));
}

object *silos_symbol (const char *name) {
  if (std::strlen(name) <= 6 && valid40(const_cast<char *>(name))) {
    return intern(twist(pack40(const_cast<char *>(name))));
  }
  return internlong(const_cast<char *>(name));
}

char *SilosSymbolDestination = nullptr;
std::size_t SilosSymbolCapacity = 0;
std::size_t SilosSymbolLength = 0;
bool SilosSymbolOverflow = false;

void silos_collect_symbol_character (char character) {
  if (SilosSymbolLength + 1 >= SilosSymbolCapacity) {
    SilosSymbolOverflow = true;
    return;
  }
  SilosSymbolDestination[SilosSymbolLength++] = character;
}

bool silos_copy_symbol_name (object *symbol, char name[], std::size_t capacity) {
  if (!symbolp(symbol) || capacity == 0) return false;
  SilosSymbolDestination = name;
  SilosSymbolCapacity = capacity;
  SilosSymbolLength = 0;
  SilosSymbolOverflow = false;
  printsymbol(symbol, silos_collect_symbol_character);
  if (SilosSymbolOverflow) return false;
  name[SilosSymbolLength] = '\0';
  return true;
}

const char *SilosComparedSymbol = nullptr;
bool SilosComparedSymbolMismatch = false;
void silos_compare_symbol_character(char character) {
  if (character != SilosComparedSymbol[SilosSymbolLength]) {
    SilosComparedSymbolMismatch = true;
  }
  ++SilosSymbolLength;
}

bool silos_symbol_name_is(object *value, const char *name) {
  if (!symbolp(value) || name == nullptr) return false;
  SilosComparedSymbol = name;
  SilosComparedSymbolMismatch = false;
  SilosSymbolLength = 0;
  printsymbol(value, silos_compare_symbol_character);
  return !SilosComparedSymbolMismatch && name[SilosSymbolLength] == '\0';
}

object *silos_prepend_field (const char *name, object *value, object *tail) {
  // myalloc may run GC. Protect every live intermediate before asking uLisp
  // for another object, because C++ locals are not automatically GC roots.
  protect(value);
  protect(tail);
  object *key = silos_symbol(name);
  protect(key);
  object *entry = cons(key, value);
  protect(entry);
  object *result = cons(entry, tail);
  unprotect();
  unprotect();
  unprotect();
  unprotect();
  return result;
}

object *silos_find_field (object *record, const char *name) {
  while (record != nil) {
    if (!consp(record)) return nil;
    object *entry = first(record);
    if (consp(entry) && silos_symbol_is(first(entry), name)) return entry;
    record = rest(record);
  }
  return nil;
}

bool ulisp::symbolIs(Object *value, const char *name) {
  return silos_symbol_is(value, name);
}
bool ulisp::symbolNameIs(Object *value, const char *name) {
  return silos_symbol_name_is(value, name);
}
ulisp::Object *ulisp::findField(Object *record, const char *name) {
  return silos_find_field(record, name);
}
ulisp::Object *ulisp::findSymbolField(Object *record, Object *name) {
  return silos_find_symbol_field(record, name);
}

namespace {
SilosUlispCharacterVisitor SymbolVisitor = nullptr;
void *SymbolVisitorContext = nullptr;
void visit_symbol_character(char character) {
  SymbolVisitor(character, SymbolVisitorContext);
}
}

bool silos_ulisp_visit_string(const sobject *raw, std::size_t limit,
                              SilosUlispCharacterVisitor visitor,
                              void *context) {
  object *value = const_cast<object *>(reinterpret_cast<const object *>(raw));
  if (!stringp(value) || visitor == nullptr) return false;
  for (std::size_t index = 0; limit == 0 || index < limit; ++index) {
    const char character = static_cast<char>(nthchar(value, static_cast<int>(index)));
    if (character == '\0') break;
    visitor(character, context);
  }
  return true;
}

bool silos_ulisp_read_integer(const sobject *raw, int &result) {
  object *value = const_cast<object *>(reinterpret_cast<const object *>(raw));
  if (!integerp(value)) return false;
  result = value->integer;
  return true;
}

bool silos_ulisp_visit_symbol(const sobject *raw,
                              SilosUlispCharacterVisitor visitor,
                              void *context) {
  object *value = const_cast<object *>(reinterpret_cast<const object *>(raw));
  if (!symbolp(value) || visitor == nullptr) return false;
  SymbolVisitor = visitor;
  SymbolVisitorContext = context;
  printsymbol(value, visit_symbol_character);
  SymbolVisitor = nullptr;
  SymbolVisitorContext = nullptr;
  return true;
}
