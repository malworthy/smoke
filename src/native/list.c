#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "list.h"


bool addNative(int argCount, Value* args)
{
    if (!IS_LIST(args[0]))
    {
        char* msg = "Parameter 1 of add must be a list";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
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
        char* s = AS_CSTRING(args[0]);
        args[-1] = NUMBER_VAL(strlen(s));
        
        return true;
    }
    
    char* msg = "len only available for strings and lists";
    args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

    return false;
}

/*
bool getNative(int argCount, Value* args)
{
    if (!IS_LIST(args[0]))
    {
        char* msg = "Parameter 1 of get must be a list";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
    ObjList* list = AS_LIST(args[0]);

    int index = (int)AS_NUMBER(args[1]);
    if (index >= list->elements.count)
    {
        char* msg = "Index outside the bounds of the list";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }

    Value value = list->elements.values[index];
    args[-1] = value;

    return true;
}

bool sliceNative(int argCount, Value* args)
{
    if (!IS_LIST(args[0]))
    {
        char* msg = "Parameter 1 of get must be a list";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
    ObjList* list = AS_LIST(args[0]);

    int start = (int)AS_NUMBER(args[1]);
    int end = (int)AS_NUMBER(args[2]);
    if (start >= list->elements.count || end >= list->elements.count)
    {
        char* msg = "Index outside the bounds of the list";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
    ObjList* sliced = newList();

    for(int i = start; i < end; i++)
    {
        writeValueArray(&sliced->elements, list->elements.values[i]);
        //printf("sliced: %d\n", sliced->elements.count);
    }    

    args[-1] = OBJ_VAL(sliced);

    return true;
}
*/