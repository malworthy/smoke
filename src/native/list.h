#ifndef sm_list_h
#define sm_list_h

#include "../common.h"
#include "../value.h"
#include "../object.h"

bool addNative(int argCount, Value* args);
bool lenNative(int argCount, Value* args);
bool rangeNative(int argCount, Value* args);
bool joinNative(int argCount, Value* args);
Value join(ObjList* list);

#endif