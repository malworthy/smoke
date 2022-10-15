#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "list.h"
#include "native.h"
#include "../vm.h"
#include "../memory.h"

bool addNative(int argCount, Value* args)
{
    CHECK_LIST(0, "Parameter 1 of add must be a list");

    ObjList* list = AS_LIST(args[0]);
    writeValueArray(&list->elements, args[1]);
    return true;
}

bool lenNative(int argCount, Value* args)
{
    if (IS_LIST(args[0]))
    {
        ObjList* list = AS_LIST(args[0]);
        args[-1] = NUMBER_VAL(list->elements.count);
        
        return true;
    }

    if (IS_STRING(args[0]))
    {
        ObjString* string = AS_STRING(args[0]);
        args[-1] = NUMBER_VAL(string->length);
        
        return true;
    }

    NATIVE_ERROR("len only available for strings and lists");
    
}

bool rangeNative(int argCount, Value* args)
{
    if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2]))
    {
        NATIVE_ERROR("Only numbers can be used to create a range");
    }

    int start = (int)AS_NUMBER(args[1]);
    int end = (int)AS_NUMBER(args[2]);

    ObjList* list = AS_LIST(args[0]);
    push(OBJ_VAL(list));

    for(int i = start; (start > end) ? i >= end : i <= end; (start > end) ? i-- : i++)
    {
        writeValueArray(&list->elements, NUMBER_VAL((double)i));
    }    

    args[-1] = OBJ_VAL(list);
    pop();

    return true;
}

Value join(ObjList* list)
{
    // work out how much memory we need for the joined string
    int resultLength = 0;
    for(int i=0; i < list->elements.count; i++)
        resultLength += stringifyValueLength(list->elements.values[i]);

    char* result = ALLOCATE(char, resultLength + 1); 
    result[0] = '\0';
    int length = 0;
    for(int i=0; i < list->elements.count; i++)
    {
        Value val = list->elements.values[i];
        length += stringifyValue(val, result + length /* strlen(result)*/);
    }   

    Value joined = OBJ_VAL(takeString(result, resultLength /*(int)strlen(result)*/));

    return joined;
}

bool joinNative(int argCount, Value* args)
{ 
    CHECK_LIST(0, "Only a list can be joined");

    ObjList* list = AS_LIST(args[0]);

    push(OBJ_VAL(list));
    args[-1] = join(list);
    pop();
    
    return true;
}
