#ifndef min_vm_h
#define min_vm_h

#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    Obj* objects;
    Table strings;
    Table globals;
    ObjUpvalue* openUpvalues;
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
    size_t bytesAllocated;
    size_t nextGC;
    ObjString* initString;
    ObjString* whereString;
    ObjString* selectString;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source, char* filename);
void push(Value value);
Value pop();
bool setTable(Value tableVal, Value item, Value index);

#endif
