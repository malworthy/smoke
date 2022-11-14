#ifndef mal_stringutil_h
#define mal_stringutil_h

bool splitlinesNative(int argCount, Value* args);
bool splitNative(int argCount, Value* args);
bool asciiNative(int argCount, Value* args);
bool upperNative(int argCount, Value* args);
bool charNative(int argCount, Value* args);
bool trimNative(int argCount, Value* args);

#endif