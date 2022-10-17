#ifndef mal_console_h
#define mal_console_h

#include "../common.h"
#include "../value.h"
#include "../object.h"

bool writeNative(int argCount, Value* args);
bool locateNative(int argCount, Value* args);
bool clearNative(int argCount, Value* args);
bool textColorNative(int argCount, Value* args);
bool backColorNative(int argCount, Value* args);
bool inputNative(int argCount, Value* args);
bool kbhitNative(int argCount, Value* args);
bool getchNative(int argCount, Value* args);

#endif