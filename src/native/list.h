#ifndef mal_list_h
#define mal_list_h

#include "../common.h"
#include "../value.h"
#include "../object.h"

bool addNative(int argCount, Value* args);
bool getNative(int argCount, Value* args);

#endif