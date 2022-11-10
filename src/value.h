#ifndef min_value_h
#define min_value_h

#include "common.h"
#include <time.h>

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_DATETIME,
    VAL_OBJ
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        time_t datetime;
        Obj* obj;
    } as; 
} Value;

#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_DATETIME(value)  ((value).type == VAL_DATETIME)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)
#define IS_NIL(value)       ((value).type == VAL_NIL)

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_DATETIME(value)  ((value).as.datetime)
#define AS_OBJ(value)       ((value).as.obj)

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define DATETIME_VAL(value) ((Value){VAL_DATETIME, {.datetime = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})           

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);
bool valuesEqual(Value a, Value b);
int stringifyValue(Value value, char* str);
int stringifyValueLength(Value value);

#endif
