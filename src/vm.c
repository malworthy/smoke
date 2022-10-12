#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "native/console.h"
#include "native/list.h"
#include "native/filesys.h"
#include "native/fileio.h"
#include "native/stringutil.h"

VM vm; 

static bool clockNative(int argCount, Value* args) {
    args[-1] = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return true;
}

static bool nowNative(int argCount, Value* args)
{
    args[-1] = DATETIME_VAL(time(NULL));
    return true;
}

static double getRandomNumber(int max)
{
    return floor((double)rand() / ((double)RAND_MAX + 1) * max);
}

static bool randNative(int argCount, Value* args)
{
    if (IS_NUMBER(args[0]))
    {
        int max = (int)AS_NUMBER(args[0]);
        if (abs(max) > RAND_MAX)
        {
            char* msg = "Value too large for rand()";
            args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));
            return false;
        }
        double result = getRandomNumber(max);

        args[-1] = NUMBER_VAL((double) result);

        return true;
    }
    char* msg = "Parameter 1 of rand must be a number";
    args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));
    return false;
}

static bool argsNative(int argCount, Value* args) {
    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected

    // = 0 interpreter, 1 = source file, 2+ args passed to script
    if (_argc > 1)
    {
        for(int i = 1; i < _argc; i++)
        {
            Value val =OBJ_VAL(copyStringRaw(_args[i], (int)strlen(_args[i])));
            push(val);
            writeValueArray(&list->elements, val);
            pop();
        }
    }

    args[-1] = OBJ_VAL(list);

    pop();

    
    return true;
}

void resetStack()
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) 
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) 
    {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if (function->name == NULL) 
        {
            fprintf(stderr, "script\n");
        } 
        else 
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char* name, NativeFn function, int arity) 
{
    push(OBJ_VAL(copyStringRaw(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function, arity)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() 
{
    resetStack();
    vm.objects = NULL;
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    initTable(&vm.strings);
    initTable(&vm.globals);

    // Native Functions
    defineNative("clock", clockNative, 0);
    defineNative("args", argsNative, 0);
    defineNative("now", nowNative, 0);
    defineNative("rand", randNative, 1);

    // CONSOLE
    defineNative("write", writeNative, 1);
    defineNative("locate", locateNative, 2);
    defineNative("clear", clearNative, 0);
    defineNative("textColor", textColorNative, 1);
    defineNative("backColor", backColorNative, 1);
    defineNative("input", inputNative, 0);

    // LISTS
    defineNative("add", addNative, 2);
    defineNative("len", lenNative, 1);
    defineNative("~range", rangeNative, 3);
    defineNative("join", joinNative, 1);

    // FILESYS
    defineNative("dir", dirNative, 1);
    defineNative("run", runNative, 1);

    // FILEIO
    defineNative("readlines", readlinesNative, 1);

    // STRING
    defineNative("splitlines", splitlinesNative, 1);
    defineNative("split", splitNative, 2);
}

void freeVM() 
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value) 
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() 
{
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) 
{
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) 
{
    if (argCount != closure->function->arity) 
    {
        runtimeError("Expected %d arguments but got %d.",
            closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) 
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];

    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;

    return true;
}

static bool callValue(Value callee, int argCount) 
{
    if (IS_OBJ(callee)) 
    {
        switch (OBJ_TYPE(callee)) 
        {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                ObjNative* native = AS_NATIVE(callee);
                if (native->arity != argCount)
                {
                    runtimeError("Expected %d arguments but got %d.", native->arity, argCount);
                    return false;
                }
                
                if (native->function(argCount, vm.stackTop - argCount)) 
                {
                    vm.stackTop -= argCount;
                    return true;
                } 
                else 
                {
                    runtimeError(AS_STRING(vm.stackTop[-argCount - 1])->chars);
                    return false;
                }
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");

    return false;
}

static bool bindMethod(ObjClass* klass, ObjString* name) 
{
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(0),
                                            AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* captureUpvalue(Value* local) 
{
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local) 
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) 
        return upvalue;
    
    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) 
        vm.openUpvalues = createdUpvalue;
    else 
        prevUpvalue->next = createdUpvalue;
    
    return createdUpvalue;
}

static void closeUpvalues(Value* last) 
{
    while (vm.openUpvalues != NULL &&
            vm.openUpvalues->location >= last) 
    {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void defineMethod(ObjString* name) 
{
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

static bool isFalsey(Value value) 
{
    // 0 is false, all other numbers true
    return (IS_NUMBER(value) && !AS_NUMBER(value)) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() 
{
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static void concatenateList() 
{

    ObjList* b = AS_LIST(peek(0));
    ObjList* a = AS_LIST(peek(1));
    ObjList* result = newList();

    push(OBJ_VAL(result));

    int length = a->elements.count + b->elements.count;
    Value* values = ALLOCATE(Value, length);
    result->elements.capacity = length;
    result->elements.count = length;

    memcpy(values, a->elements.values, a->elements.count * sizeof(Value));
    memcpy(values + a->elements.count, b->elements.values, b->elements.count * sizeof(Value));
    result->elements.values = values;

    pop();
    pop();
    pop();
    push(OBJ_VAL(result));
}

Value get(Value item, Value index)
{
    if (!(IS_LIST(item) || IS_STRING(item)))
    {
        runtimeError("Subscript invalid for type");
        return NIL_VAL;
    }
    int i = (int)AS_NUMBER(index);

    if (IS_LIST(item))
    {
        ObjList* list = AS_LIST(item);    
        if (i < 0 ) i = list->elements.count + i;
        if (i >= list->elements.count  || i < 0)
        {
            runtimeError("Index outside the bounds of the list");
            return NIL_VAL;
        }
        return list->elements.values[i];
    }
    else
    {
        char* string = AS_CSTRING(item);    
        if (i < 0 ) i = strlen(string) + i;
        if (i >= strlen(string) || i < 0)
        {
            runtimeError("Index outside the bounds of the string");
            return NIL_VAL;
        }
        return OBJ_VAL(copyStringRaw(string + i, 1));
    }
    
}

Value slice(Value item, Value startIndex, Value endIndex)
{
    if (!(IS_LIST(item) || IS_STRING(item)))
    {
        runtimeError("Slice invalid for type");
        return NIL_VAL;
    }

    int start = (int)AS_NUMBER(startIndex);
    int end = (int)AS_NUMBER(endIndex);

    if (IS_LIST(item))
    {
        ObjList* list = AS_LIST(item);
        if (start < 0) start = list->elements.count + start;
        if (end <= 0) end = list->elements.count + end;
        if (start >= list->elements.count || end > list->elements.count || start < 0 || end < 0)
        {
            runtimeError("Index outside the bounds of the list");
            return NIL_VAL;
        }
        ObjList* sliced = newList();
        push(OBJ_VAL(sliced));

        for(int i = start; i < end; i++)
        {
            writeValueArray(&sliced->elements, list->elements.values[i]);
        }   
        pop(); 

        return OBJ_VAL(sliced);
    }
    else
    {
        char* string = AS_CSTRING(item);    
        if (start < 0) start = strlen(string)  + start;
        if (end <= 0) end = strlen(string)  + end;
        if (start >= strlen(string) || end > strlen(string))
        {
            runtimeError("Index outside the bounds of the string");
            return NIL_VAL;
        }
        if (start > end) start = end;
        return OBJ_VAL(copyStringRaw(string + start, end - start));
    }
}

static InterpretResult run() 
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    #define READ_BYTE() (*frame->ip++)

    #define READ_SHORT() \
        (frame->ip += 2, \
        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

    #define READ_CONSTANT() \
        (frame->closure->function->chunk.constants.values[READ_BYTE()])
        
    #define BINARY_OP(valueType, op) \
        do { \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                runtimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(valueType(a op b)); \
        } while (false)

    #define INC_DEC_OP(value, number) \
        do { \
            uint8_t slot = READ_BYTE(); \
            Value val = value; \
            if (!IS_NUMBER(val)) \
            { \
                runtimeError("Can only increment a number"); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            push(val); \
            AS_NUMBER(val) += (double)number; \
            value = val; \
        } while (false)
        
        

    #define READ_STRING() AS_STRING(READ_CONSTANT())

    for (;;) 
    {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++) 
            {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk,
                (int)(frame->ip - frame->closure->function->chunk.code));
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) 
        {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_TRUE:       push(BOOL_VAL(true)); break;
            case OP_FALSE:      push(BOOL_VAL(false)); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD:        
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) 
                {
                    concatenate();
                } 
                else if (IS_LIST(peek(0)) && IS_LIST(peek(1))) 
                {
                    concatenateList();
                }              
                else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) 
                {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } 
                else 
                {
                    //concatenateAny();
                    runtimeError("Operands must be of the same type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:   
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) 
                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SUBSCRIPT: {
                
                Value index = pop();
                Value item = pop();
                Value result = get(item, index);
                if(IS_NIL(result))
                    return INTERPRET_RUNTIME_ERROR;
                push(result);

                break;
            }
            case OP_SLICE: {
                
                Value end = pop();
                Value start = pop();
                Value item = pop();
                Value result = slice(item, start, end);
                if(IS_NIL(result))
                    return INTERPRET_RUNTIME_ERROR;
                push(result);

                break;
            }
            case OP_LIST_ADD: {
                Value val = pop();
                ObjList* list = AS_LIST(peek(0));

                writeValueArray(&list->elements, val);

                break;
            }
            case OP_JOIN: {
                ObjList* list = AS_LIST(peek(0));
                Value val = join(list);
                pop();
                push(val);
                break;
            }
            case OP_RANGE: {
                int end = (int)AS_NUMBER(pop());
                int start = (int)AS_NUMBER(pop());
                ObjList* list = AS_LIST(peek(0));
                for(int i = start; (start > end) ? i >= end : i <= end; (start > end) ? i-- : i++)
                {
                    writeValueArray(&list->elements, NUMBER_VAL((double)i));
                }
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) 
                {
                    tableDelete(&vm.globals, name); 
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) 
                    return INTERPRET_RUNTIME_ERROR;

                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                
                push(OBJ_VAL(closure));
                
                for (int i = 0; i < closure->upvalueCount; i++) 
                {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) 
                    {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } 
                    else 
                    {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            case OP_INC_LOCAL: {
                INC_DEC_OP(frame->slots[slot],1);
                break;
            }
            case OP_INC_UPVALUE: {
                INC_DEC_OP(*frame->closure->upvalues[slot]->location,1);
                break;
            }
            case OP_DEC_LOCAL: {
                INC_DEC_OP(frame->slots[slot],-1);
                break;
            }
            case OP_DEC_UPVALUE: {
                INC_DEC_OP(*frame->closure->upvalues[slot]->location,-1);
                break;
            }
            case OP_NEW_OBJ: {
                uint8_t objType = READ_BYTE();
                //objType for future use - for now only create a list
                push(OBJ_VAL(newList()));
                break;
            }
            case OP_CLASS:
                push(OBJ_VAL(newClass(READ_STRING())));
                break;
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) 
                {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance* instance = AS_INSTANCE(peek(0));
                ObjString* name = READ_STRING();

                Value value;
                if (tableGet(&instance->fields, name, &value)) 
                {
                    pop(); // Instance.
                    push(value);
                    break;
                }

                if (!bindMethod(instance->klass, name)) 
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: 
            {
                if (!IS_INSTANCE(peek(1))) 
                {
                    runtimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(1));
                tableSet(&instance->fields, READ_STRING(), peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_METHOD:
                defineMethod(READ_STRING());
                break;
            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) 
                {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
    #undef READ_STRING
    #undef READ_SHORT
    #undef INC_DEC_OP
}

InterpretResult interpret(const char* source) 
{
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));

    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}


