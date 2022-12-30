#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "object.h"
#include "memory.h"
#include "value.h"

void initValueArray(ValueArray* array) 
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) 
{
    if (array->capacity < array->count + 1) 
    {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
                                   oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) 
{
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) 
{
    //char* buffer = ALLOCATE(char, stringifyValueLength(value) + 1);
    char* buffer = (char*)malloc(stringifyValueLength(value, false) + 1);
    stringifyValue(value, buffer, false);
    printf("%s", buffer);
    free(buffer);
    //FREE(char, buffer);
}

bool valuesEqual(Value a, Value b)
{
    if (a.type != b.type) return false;

    switch (a.type) 
    {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:       return AS_OBJ(a) == AS_OBJ(b);
        case VAL_DATETIME:  return AS_DATETIME(a) == AS_DATETIME(b);
        case VAL_NIL:       return true;
        default:            return false; // Unreachable.
    }
}

int stringifyValue(Value value, char* str, bool escape)
{
    switch (value.type)
    {
        case VAL_BOOL:
            return sprintf(str, "%s", AS_BOOL(value) ? "true" : "false");
        case VAL_NUMBER:
            return sprintf(str, "%g", AS_NUMBER(value));
        case VAL_OBJ:
            return stringifyObject(value, str, escape);
        case VAL_NIL:
            return sprintf(str, "%s", "null");
        case VAL_DATETIME: {
            time_t t = AS_DATETIME(value);
            struct tm *tm = localtime(&t);
            char s[64];
            strftime(s, sizeof(s), DATE_FMT, tm);
            return sprintf(str, "%s", s);
        }
    }
}

int stringifyValueLength(Value value, bool escape)
{
    switch (value.type)
    {
        case VAL_BOOL:
            return AS_BOOL(value) ? 4 : 5;
        case VAL_NUMBER:  {
            char str[100];
            return sprintf(str, "%g", AS_NUMBER(value));
        }
        case VAL_OBJ:
            return stringifyObjectLength(value, escape);
        case VAL_NIL:
            return 4;
        case VAL_DATETIME: {
            time_t t = AS_DATETIME(value);
            struct tm *tm = localtime(&t);
            char str[64];
            return strftime(str, sizeof(str), DATE_FMT, tm);
        }
    }
}
