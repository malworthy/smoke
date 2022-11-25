#ifndef sm_compiler_h
#define sm_compiler_h

#include "vm.h"
#include "object.h"

ObjFunction* compile(const char* source, char* filename);

#endif
