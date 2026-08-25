void silos_render_ui();

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

// FIELD receives a Lisp symbol, but the generic store keeps its field names
// as bounded C strings.  PRINTSYMBOL is the one reader-independent bridge:
// it works for uLisp's compact radix-40 symbols as well as long symbols.
char SilosPrintedSymbol[InMemoryFieldNameCapacity]{};
std::size_t SilosPrintedSymbolLength = 0;
bool SilosPrintedSymbolOverflow = false;

void silos_collect_symbol_character (char character) {
  if (SilosPrintedSymbolLength + 1 >= sizeof(SilosPrintedSymbol)) {
    SilosPrintedSymbolOverflow = true;
    return;
  }
  SilosPrintedSymbol[SilosPrintedSymbolLength++] = character;
}

bool silos_copy_symbol_name (object *symbol, char name[], std::size_t capacity) {
  if (!symbolp(symbol) || capacity == 0) return false;
  SilosPrintedSymbolLength = 0;
  SilosPrintedSymbolOverflow = false;
  SilosPrintedSymbol[0] = '\0';
  printsymbol(symbol, silos_collect_symbol_character);
  if (SilosPrintedSymbolOverflow || SilosPrintedSymbolLength + 1 > capacity) {
    return false;
  }
  SilosPrintedSymbol[SilosPrintedSymbolLength] = '\0';
  std::snprintf(name, capacity, "%s", SilosPrintedSymbol);
  return true;
}

bool silos_symbol_name_is(object *value, const char *name) {
  char value_name[InMemoryFieldNameCapacity]{};
  return silos_copy_symbol_name(value, value_name, sizeof(value_name)) &&
      std::strcmp(value_name, name) == 0;
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
