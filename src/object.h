#ifndef sm_object_h
#define sm_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJ(value))->chars)
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value)        ((ObjNative*)AS_OBJ(value))

#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))

#define IS_LIST(value)          isObjType(value, OBJ_LIST)
#define AS_LIST(value)          ((ObjList*)AS_OBJ(value))

#define IS_TABLE(value)          isObjType(value, OBJ_TABLE)
#define AS_TABLE(value)          ((ObjTable*)AS_OBJ(value))

#define IS_CLASS(value)         isObjType(value, OBJ_CLASS)
#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))

#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

#define IS_ENUM(value)         isObjType(value, OBJ_ENUM)
#define AS_ENUM(value)         ((ObjEnum*)AS_OBJ(value))

typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_LIST,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_ENUM,
    OBJ_TABLE
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
    bool isMarked;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct
{
  Obj obj;

  // The elements in the list.
  ValueArray elements;
} ObjList;

typedef struct
{
  Obj obj;
  Table elements;
  ValueArray keys;
} ObjTable;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    int arity;
    int optionals;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef bool (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
    int arity;
} ObjNative;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
    bool module;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields; 
} ObjInstance;

typedef struct
{
    Obj obj;
    ObjString* name;
    Table fields;
    int counter;
} ObjEnum;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjInstance* newInstance(ObjClass* klass);
ObjNative* newNative(NativeFn function, int arity);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjString* copyStringRaw(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
ObjList* newList();
ObjTable* newTable();
ObjEnum* newEnum(ObjString* name);
ObjClass* newMod(ObjString* name);

bool compareStrings(char* chars, int length, ObjString* compareString);
//void printObject(Value value);
int stringifyObject(Value value, char* str, bool escape);
int stringifyObjectLength(Value value, bool escape);

static inline bool isObjType(Value value, ObjType type) 
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
