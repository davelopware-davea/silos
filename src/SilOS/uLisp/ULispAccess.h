#pragma once

#include <cstddef>

struct sobject;

namespace ulisp {
using Object = sobject;
inline constexpr Object *nil = nullptr;

Object *head(Object *value);
Object *tail(Object *value);
Object *secondValue(Object *value);
Object *thirdValue(Object *value);
Object *tail2(Object *value);
bool isCons(Object *value);
bool isInteger(Object *value);
bool isString(Object *value);
bool isSymbol(Object *value);
int integerValue(Object *value);
int checkInteger(Object *value);
int listLength(Object *value);
Object *findPair(Object *name, Object *environment);
Object *makeNumber(int value);
Object *makeCons(Object *head, Object *tail);
Object *makeString(const char *value);
Object *makeSymbol(const char *name);
Object *trueValue();
void setTail(Object *entry, Object *value);
void pushRoot(Object *value);
void popRoot();
void error(const char *message);

bool symbolIs(Object *value, const char *name);
bool symbolNameIs(Object *value, const char *name);
Object *findField(Object *record, const char *name);
Object *findSymbolField(Object *record, Object *name);
Object *prependField(const char *name, Object *value, Object *tail);
bool stringEquals(Object *value, const char *text);
}
