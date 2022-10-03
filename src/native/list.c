#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "list.h"
#include "../vm.h"
#include "../memory.h"

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

bool rangeNative(int argCount, Value* args)
{
    if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2]))
    {
        char* msg = "Only numbers can be used to create a range";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
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

static void concatValue(char* str, Value value) 
{
    switch (value.type) 
    {
        case VAL_BOOL:
            sprintf(str, AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NUMBER: 
            sprintf(str, "%g", AS_NUMBER(value)); 
            break;
         case VAL_OBJ: 
            switch (OBJ_TYPE(value)) 
            {
                case OBJ_STRING:
                    sprintf(str, "%s", AS_CSTRING(value));
                    break;
                case OBJ_FUNCTION:
                    sprintf(str, "%s", "<function>");
                    break;
                case OBJ_NATIVE:
                    sprintf(str, "%s", "<native fn>");
                    break;
                case OBJ_CLOSURE:
                    sprintf(str, "%s", "<closure>");
                    break;
                case OBJ_UPVALUE:
                    sprintf(str, "%s", "<upvalue>");
                    break;
                case OBJ_LIST:
                    sprintf(str, "%s", "<list>");
                    break;
            }
            break;
        case VAL_NIL:
            sprintf(str, "%s", "NIL");
            break;
    }
}

bool joinNative(int argCount, Value* args)
{
    if (!IS_LIST(args[0]))
    {
        char* msg = "Only a list can be joined";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }

    ObjList* list = AS_LIST(args[0]);
    push(OBJ_VAL(list));

    char* result = ALLOCATE(char,1000); //TODO:allocate!!!
    //char strval[1000];
    result[0] = '\0';
    for(int i=0; i < list->elements.count; i++)
    {
        Value val = list->elements.values[i];
        concatValue(result + strlen(result), val);
        //strcat(result, strval);
    }   

    args[-1] = OBJ_VAL(copyStringRaw(result, (int)strlen(result)));
    pop();

    return true;
}
