#ifndef sm_fileio_h
#define sm_fileio_h

bool readlinesNative(int argCount, Value* args);
bool openNative(int argCount, Value* args);
bool closeNative(int argCount, Value* args);
bool writeFileNative(int argCount, Value* args);
bool readcharNative(int argCount, Value* args);

#endif