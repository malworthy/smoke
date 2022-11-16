#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "quicksort.h"
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "format.h"
#include "native/console.h"
#include "native/list.h"
#include "native/filesys.h"
#include "native/fileio.h"
#include "native/stringutil.h"
#include "native/date.h"
#include "native/native.h"
#include "native/mathmod.h"

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((DWORD)x)
#else
#include <unistd.h>
#define sleep(x) usleep(((int)x)*1000)
#endif

VM vm;

static bool typeNative(int argCount, Value* args)
{
    int t = args[0].type;
    if (t == VAL_OBJ)
    {
        t = t + AS_OBJ(args[0])->type;
    }
    args[-1] = NUMBER_VAL((double)t);
    return true;
}

static bool sleepNative(int argCount, Value* args)
{
    CHECK_NUM(0, "Sleep expects number as parameter.");
    sleep(AS_NUMBER(args[0]));

    return true;
}

static bool sortNative(int argCount, Value* args)
{
    CHECK_LIST(0, "Only lists can be sorted.");
    ObjList* list = AS_LIST(args[0]);
    sort(&list->elements);
    args[-1] = args[0];
    return true;
}

static bool numNative(int argCount, Value* args)
{
    CHECK_STRING(0, "num expects a string as parameter.");
    char *s = AS_CSTRING(args[0]);
    
    double val = atof(s);
    
    //stop making anything starting with 'nan' (eg nanny) NaN.  Should just be zero.
    if(s[0] == 'n' || s[0] == 'N') val = 0;


    args[-1] = NUMBER_VAL(val);

    return true;
}

static bool clockNative(int argCount, Value* args) {
    args[-1] = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
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

static void defineNativeMod(const char* name, const char* module,  NativeFn function, int arity) 
{
    Value value;
    //stack: 0 = module name, 1 = module, 2 = function name, 3 = navtive function 
    push(OBJ_VAL(copyStringRaw(module, (int)strlen(module)))); 
    if (!tableGet(&vm.globals, AS_STRING(vm.stack[0]), &value)) 
    {   
        push(OBJ_VAL(newMod(AS_STRING(vm.stack[0]))));
        tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    }
    else
    {
        push(value);
    }
    ObjClass* klass = AS_CLASS(vm.stack[1]);
    push(OBJ_VAL(copyStringRaw(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function, arity)));
    tableSet(&klass->methods, AS_STRING(vm.stack[2]), vm.stack[3]);
    pop();
    pop();
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
    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    vm.whereString = NULL;
    vm.whereString = copyString("filter", 6);

    vm.selectString = NULL;
    vm.selectString = copyString("map", 3);

    // Native Functions
    defineNative("sleep", sleepNative, 1);
    defineNative("clock", clockNative, 0);
    defineNative("args", argsNative, 0);
    defineNative("rand", randNative, 1);
    defineNative("num", numNative, 1);   
    defineNative("type", typeNative, 1);
    defineNative("sort", sortNative, 1);

    // STRING
    defineNativeMod("splitlines", "string", splitlinesNative, 1);
    defineNativeMod("split", "string", splitNative, 2);
    defineNativeMod("ascii", "string", asciiNative, 1);
    defineNativeMod("upper", "string", upperNative, 1);
    defineNativeMod("char", "string", charNative, 1);
    defineNativeMod("trim", "string", trimNative, 1);

    // Math
    defineNativeMod("bitand", "math", bitandNative, 2);
    defineNativeMod("bitor", "math", bitorNative, 2);
    defineNativeMod("atan", "math", atanNative, 1);

    defineNativeMod("cos", "math", cosNative, 1);
    defineNativeMod("sin", "math", sinNative, 1);
    defineNativeMod("tan", "math", tanNative, 1);
    defineNativeMod("exp", "math", expNative, 1);
    defineNativeMod("log", "math", logNative, 1);
    defineNativeMod("sqrt", "math", sqrtNative, 1);
    defineNativeMod("floor", "math", floorNative, 1);
    defineNativeMod("ceil", "math", ceilNative, 1);

    // Dates
    defineNativeMod("now", "date", nowNative, 0);
    defineNativeMod("date", "date", dateNative, 1);
    defineNativeMod("dateadd", "date", dateaddNative, 3);
    defineNativeMod("dateparts", "date", datepartsNative, 1);
    defineNativeMod("datediff", "date", datediffNative, 3);

    // CONSOLE
    defineNative("write", writeNative, 1);
    defineNative("locate", locateNative, 2);
    defineNative("clear", clearNative, 0);
    defineNative("textcolor", textColorNative, 1);
    defineNative("backcolor", backColorNative, 1);
    defineNative("input", inputNative, 0);
    defineNative("kbhit", kbhitNative, 0);
    defineNative("getch", getchNative, 0);
    defineNative("cursoff", cursoffNative, 1);

    // LISTS
    //defineNative("add", addNative, 2);
    defineNative("len", lenNative, 1);
    defineNative("~range", rangeNative, 3);
    defineNative("join", joinNative, 1);

    // FILESYS
    defineNativeMod("dir", "sys", dirNative, 1);
    defineNativeMod("run", "sys", runNative, 1);

    // FILEIO
    defineNativeMod("readlines", "file", readlinesNative, 1);
    defineNativeMod("open", "file", openNative, 2);
    defineNativeMod("close", "file", closeNative, 1);
    defineNativeMod("write", "file", writeFileNative, 2);
    defineNativeMod("readchar", "file", readcharNative, 1);
    
}

void freeVM() 
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    vm.whereString = NULL;
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
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                if (klass->module)
                {
                    runtimeError("Cannot create an instance of a module");
                    return false;
                }
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                Value initializer;

                if (tableGet(&klass->methods, vm.initString, &initializer)) 
                {
                    return call(AS_CLOSURE(initializer), argCount);
                }
                else if (argCount != 0)
                {
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }

                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                ObjNative* native = AS_NATIVE(callee);
                if (native->arity != argCount)
                {
                    runtimeError("Expected %d arguments but got %d. (N)", native->arity, argCount);
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

static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount) 
{
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    if (IS_CLOSURE(method))
        return call(AS_CLOSURE(method), argCount);
    else if (IS_NATIVE(method))
        return callValue(method, argCount);
    
    runtimeError("Not a valid function");
    return false;
}

static bool getEnumName(Value enumInstance)
{
    //printf("in get enumName\n");
    Value enumValue = pop();
    ObjEnum* _enum = AS_ENUM(enumInstance);
    for (int i=0; i < _enum->fields.capacity; i++)
    {
        //printf("looping i is %d\n",i);
        Value val = _enum->fields.entries[i].value;
        if (IS_NUMBER(val) && AS_NUMBER(val) == AS_NUMBER(enumValue))
        {
            ObjString* key = _enum->fields.entries[i].key;
            pop();
            push(OBJ_VAL(key));
            return true;
        }
    }
    runtimeError("No enum matches that value");
    return false;
}

static bool invoke(ObjString* name, int argCount) 
{
    Value receiver = peek(argCount);
    if (IS_ENUM(receiver))
    {
        if (compareStrings("name", 4, name))
        {
            return getEnumName(receiver);
        }
    }

    if (IS_CLASS(receiver))
    {
        ObjClass* klass = AS_CLASS(receiver);
        return invokeFromClass(klass, name, argCount);
    }
   
    if (!IS_INSTANCE(receiver)) 
    {
        runtimeError("Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (tableGet(&instance->fields, name, &value)) 
    {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }
    return invokeFromClass(instance->klass, name, argCount);
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

static void defineEnumField(ObjString* name) 
{
    //Value field = peek(0);
    ObjEnum* _enum = AS_ENUM(peek(0));
    Value val = NUMBER_VAL((double)_enum->counter);

    //printf("Defining enum field: %s\n", name->chars);
    
    tableSet(&_enum->fields, name, val);
    _enum->counter++;
}

static void setEnumField(ObjString* name) 
{
    //Value field = peek(0);
    ObjEnum* _enum = AS_ENUM(peek(1));
    Value val = pop();

    //printf("Defining enum field: %s\n", name->chars);
    
    tableSet(&_enum->fields, name, val);

    if(IS_NUMBER(val))
    {
        double numVal = AS_NUMBER(val);
        if (numVal >= _enum->counter)
            _enum->counter = ceil(numVal) + 1;
    }
}

static bool isFalsey(Value value) 
{
    // 0 is false, all other numbers true
    return IS_NIL(value) || (IS_NUMBER(value) && !AS_NUMBER(value)) || (IS_BOOL(value) && !AS_BOOL(value));
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

bool set(Value listVal, Value item, Value index)
{
    if (!IS_LIST(listVal))
    {
        runtimeError("Expect list");
        return false;
    }

    if (!IS_NUMBER(index))
    {
        runtimeError("Index must be a number");
        return false;
    }

    if (IS_LIST(item) && AS_LIST(item) == AS_LIST(listVal))
    {
        runtimeError("You can't do that");
        return false;
    }

    int i = (int)AS_NUMBER(index);

    ObjList* list = AS_LIST(listVal);    
    if (i < 0 ) i = list->elements.count + i;
    if (i >= list->elements.count  || i < 0)
    {
        runtimeError("Index outside the bounds of the list");
        return false;
    }
    list->elements.values[i] = item;

    return true;
}

Value get(Value item, Value index, bool* hasError)
{
    if (!(IS_LIST(item) || IS_STRING(item)))
    {
        runtimeError("Subscript invalid for type");
        *hasError = true;
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
            *hasError = true;
            return NIL_VAL;
        }
        return list->elements.values[i];
    }
    else
    {
        ObjString* string = AS_STRING(item);    
        if (i < 0 ) i = string->length + i;
        if (i >= string->length || i < 0)
        {
            runtimeError("Index outside the bounds of the string");
            *hasError = true;
            return NIL_VAL;
        }
        return OBJ_VAL(copyStringRaw(string->chars + i, 1));
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
        ObjString* string = AS_STRING(item);    
        if (start < 0) start = string->length  + start;
        if (end <= 0) end = string->length  + end;
        if (start >= string->length || end > string->length)
        {
            runtimeError("Index outside the bounds of the string");
            return NIL_VAL;
        }
        if (start > end) start = end;
        return OBJ_VAL(copyStringRaw(string->chars + start, end - start));
    }
}

static bool incDecProperty(ObjString* propName, double amount)
{
    if (!IS_INSTANCE(peek(0))) 
    {
        runtimeError("Only instances have fields.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(peek(0));
    Value val;

    tableGet(&instance->fields, propName, &val);
    if(!IS_NUMBER(val))
    {
        runtimeError("Property must be a number");
        return false;
    }

    Value value = val;
    AS_NUMBER(val) += amount;

    tableSet(&instance->fields, propName, val);
    pop();
    push(value);

    return true;
}

static InterpretResult run() 
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    #define READ_BYTE() (*frame->ip++)

    #define READ_SHORT() \
        (frame->ip += 2, \
        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

    #define READ_CONSTANT() \
        (frame->closure->function->chunk.constants.values[READ_SHORT()])
        
    #define COMPARE_OP(valueType, op) \
        do { \
            if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) { \
                double b = AS_NUMBER(pop()); \
                double a = AS_NUMBER(pop()); \
                push(valueType(a op b)); \
            } \
            else if (IS_DATETIME(peek(0)) && IS_DATETIME(peek(1))) \
            { \
                time_t b = AS_DATETIME(pop()); \
                time_t a = AS_DATETIME(pop()); \
                push(valueType(a op b)); \
            } \
            else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) \
            { \
                char* b = AS_CSTRING(pop()); \
                char* a = AS_CSTRING(pop()); \
                int result = strcmp(a, b); \
                push(valueType(result op 0)); \
            } \
            else \
            { \
                runtimeError("Operands must be numbers or dates."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
        } while (false)

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

    #define BINARY_OP_INT(valueType, op) \
        do { \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                runtimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            int b = (int)AS_NUMBER(pop()); \
            int a = (int)AS_NUMBER(pop()); \
            push(valueType(a op b)); \
        } while (false)

    #define INC_DEC_OP(value, number) \
        do { \
            uint8_t slot = READ_BYTE(); \
            Value val = value; \
            if (!IS_NUMBER(val)) \
            { \
                runtimeError("Variable must be a number"); \
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
            case OP_NIL:        push(NIL_VAL); break;
            case OP_TRUE:       push(BOOL_VAL(true)); break;
            case OP_FALSE:      push(BOOL_VAL(false)); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:    COMPARE_OP(BOOL_VAL, >); break;
            case OP_LESS:       COMPARE_OP(BOOL_VAL, <); break;
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
            case OP_MOD:        BINARY_OP_INT(NUMBER_VAL, %); break;
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
                Value value;
                if(tableGet(&vm.globals, name, &value))
                {
                    runtimeError("A global variable or function called '%s' already exists.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
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
                bool hasError = false;
                Value result = get(item, index, &hasError);
                if (hasError) return INTERPRET_RUNTIME_ERROR;
                push(result);

                break;
            }
            case OP_SUBSCRIPT_SET: {
                
                Value value = pop();
                Value index = pop();
                Value list = peek(0);
                if (!set(list, value, index))
                    return INTERPRET_RUNTIME_ERROR;
                //pop();

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
                Value val = peek(0); // pop();
                if (!IS_LIST(peek(1)))
                {
                    runtimeError("Expect list");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjList* list = AS_LIST(peek(1));

                writeValueArray(&list->elements, val);
                pop();

                break;
            }
            case OP_POP_LIST: {
                if (!IS_LIST(peek(0)))
                {
                    runtimeError("Expect list");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjList* list = AS_LIST(pop());
                if (list->elements.count == 0)
                {
                    runtimeError("List is empty. Nothing can be removed.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(list->elements.values[list->elements.count-1]);
                list->elements.count--;

                break;
            }
            case OP_JOIN: {
                ObjList* list = AS_LIST(peek(0));
                Value val = join(list);
                pop();
                push(val);
                break;
            }
            case OP_FORMAT: 
            {
                Value val = format(peek(0), peek(1));
                pop();
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
            case OP_NEW_LIST: {
                push(OBJ_VAL(newList()));
                break;
            }
            case OP_ENUM:
                push(OBJ_VAL(newEnum(READ_STRING())));
                break;
            case OP_ENUM_FIELD:
                defineEnumField(READ_STRING());
                break;
            case OP_ENUM_FIELD_SET:
                setEnumField(READ_STRING());
                break;
            case OP_CLASS:
                push(OBJ_VAL(newClass(READ_STRING())));
                break;
            case OP_MODULE:
                push(OBJ_VAL(newMod(READ_STRING())));
                break;
            case OP_GET_PROPERTY: {
                if (IS_ENUM(peek(0)))
                {
                    ObjEnum* _enum = AS_ENUM(peek(0));
                    ObjString* name = READ_STRING();

                    Value value;
                    //printf("enum count: %d\n", _enum->fields.count);
                    //printValue(_enum->fields.entries[0].value);
                    if (tableGet(&_enum->fields, name, &value)) 
                    {
                        pop(); // Enum.
                        push(value);
                        break;
                    }
                    else
                    {
                        runtimeError("Unknowm enum value '%s'.", name->chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
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
            case OP_INC_PROPERTY: 
            {
                ObjString* propName = READ_STRING();
                if (!incDecProperty(propName, 1)) return INTERPRET_RUNTIME_ERROR;
                break;
                
            }
            case OP_DEC_PROPERTY: 
            {
                ObjString* propName = READ_STRING();
                if (!incDecProperty(propName, -1)) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_METHOD:
                defineMethod(READ_STRING());
                break;
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_WHERE: {
                Value list = peek(1);
                Value fn = peek(0);

                if(!IS_LIST(list))
                {
                    runtimeError("Where only works on lists.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value filterFunction;
                if (!tableGet(&vm.globals, vm.whereString, &filterFunction))
                {
                    runtimeError("filter missing from core function.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop();
                pop();
                push(filterFunction);
                push(list);
                push(fn);

                break;
            }
            case OP_SELECT: {
                Value list = peek(1);
                Value fn = peek(0);

                if(!IS_LIST(list))
                {
                    runtimeError("select only works on lists.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value mapFunction;
                if (!tableGet(&vm.globals, vm.selectString, &mapFunction))
                {
                    runtimeError("map missing from core function.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop();
                pop();
                push(mapFunction);
                push(list);
                push(fn);

                break;
            }
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
    #undef COMPARE_OP
    #undef BINARY_OP_INT
}

InterpretResult interpret(const char* source, char* filename) 
{
    ObjFunction* function = compile(source, filename);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));

    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}


