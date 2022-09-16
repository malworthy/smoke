#include <stdio.h>
#include "console.h"

bool writeNative(int argCount, Value* args) 
{
    char* string = AS_CSTRING(args[0]);
    printf("%s", string);

    return true;
}