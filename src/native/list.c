#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

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
/*
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
                case OBJ_BOUND_METHOD:
                    sprintf(str, "%s", "<method>");
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
                case OBJ_CLASS:
                    sprintf(str, "%s", AS_CLASS(value)->name->chars);
                    break;
                case OBJ_INSTANCE:
                    sprintf(str, "%s instance", AS_INSTANCE(value)->klass->name->chars);
                    break;
            }
            break;
        case VAL_DATETIME: {
            time_t t = AS_DATETIME(value);
            struct tm *tm = localtime(&t);
            char s[64];
            strftime(s, sizeof(s), "%c", tm);
            sprintf(str, "%s", s);
            break;
        }
        case VAL_NIL:
            sprintf(str, "%s", "NIL");
            break;
    }
}

static int getValueLength(Value value) 
{
    switch (value.type) 
    {
        case VAL_BOOL:
            return AS_BOOL(value) ? 5 : 6;
            break;
        case VAL_NUMBER: 
        { 
            char str[100];
            sprintf(str, "%g", AS_NUMBER(value)); 
            return strlen(str) + 1;
        }
         case VAL_OBJ: 
            switch (OBJ_TYPE(value)) 
            {
                case OBJ_STRING: 
                {
                    ObjString* s = AS_STRING(value);
                    return s->length + 1;
                }
                case OBJ_FUNCTION:
                    return 11;
                case OBJ_BOUND_METHOD:
                    return 9;
                case OBJ_NATIVE:
                    return 12;
                case OBJ_CLOSURE:
                    return 10;
                case OBJ_UPVALUE:
                    return 10;
                case OBJ_LIST:
                    return 7;
                case OBJ_CLASS:
                    return strlen(AS_CLASS(value)->name->chars) + 1;
                case OBJ_INSTANCE:
                    return strlen(AS_INSTANCE(value)->klass->name->chars) + 10;
            }
            break;
        case VAL_DATETIME:
            return 64;
        case VAL_NIL:
            return 4;
    }
}
*/

Value join(ObjList* list)
{
    // work out how much memory we need for the joined string
    int resultLength = 0;
    for(int i=0; i < list->elements.count; i++)
        resultLength += stringifyValueLength(list->elements.values[i]);

    char* result = ALLOCATE(char, resultLength); 
    result[0] = '\0';

    for(int i=0; i < list->elements.count; i++)
    {
        Value val = list->elements.values[i];
        stringifyValue(val, result + strlen(result));
    }   

    Value joined = OBJ_VAL(takeString(result, (int)strlen(result)));

    return joined;
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
    args[-1] = join(list);
    pop();
    
    return true;
}
