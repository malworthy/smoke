#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) 
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    object->isMarked = false;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method) 
{
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod,
                                        OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClosure* newClosure(ObjFunction* function) 
{
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);

    for (int i = 0; i < function->upvalueCount; i++) 
        upvalues[i] = NULL;
    
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;

    return closure;
}

ObjFunction* newFunction() 
{
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(ObjClass* klass) 
{
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjNative* newNative(NativeFn function, int arity) 
{
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    native->arity = arity;
    return native;
}

ObjList* newList()
{
    // Allocate this before the list object in case it triggers a GC which would
    // free the list.
    ValueArray elements;
    initValueArray(&elements);
    
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
    list->elements = elements;
    
    return list;
}

ObjClass* newClass(ObjString* name) 
{
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name; 
    initTable(&klass->methods);
    return klass;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) 
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    
    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL);
    pop();

    return string;
}

static uint32_t hashString(const char* key, int length) 
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) 
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

bool compareStrings(char* chars, int length, ObjString* compareString) 
{
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
    if (interned != NULL) 
        return interned == compareString;

    return false;
}

ObjString* takeString(char* chars, int length) 
{
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
    if (interned != NULL) 
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length) 
{
    char processedString[length];
    int charCount = 0;
    for(int i = 0; i < length; i++)
    {
        if(*(chars + i) == '\\' && i < length -1)
        {
            i++;
            switch (*(chars + i))
            {
            case 'n':
                processedString[charCount++] = '\n';
                break;
            case 'r':
                processedString[charCount++] = '\r';
                break;
            case 't':
                processedString[charCount++] = '\t';
                break;
            case '\\':
                processedString[charCount++] = '\\';
                break;
            case '"':
                processedString[charCount++] = '"';
                break;   
            case '%':
                processedString[charCount++] = '%';
                break;            
            default: // no valid escape sequence, so just treat as backslash
                i--;
                processedString[charCount++] = '\\';
                break;
            }
        }
        else
        {
            processedString[charCount++] = *(chars+i);
        }
    }
    
    uint32_t hash = hashString(processedString, charCount);
    ObjString* interned = tableFindString(&vm.strings, processedString, charCount,
                                          hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, charCount + 1);
    memcpy(heapChars, processedString, charCount);
    heapChars[charCount] = '\0';
    
    return allocateString(heapChars, charCount, hash);
}

ObjString* copyStringRaw(const char* chars, int length)
{
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length,
                                          hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    
    return allocateString(heapChars, length, hash);
} 

ObjUpvalue* newUpvalue(Value* slot) 
{
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;

    return upvalue;
}

static int stringifyFunction(ObjFunction* function, char* str)
{
    if (function->name == NULL)
    {
        return sprintf(str, "%s", "<script>");
    }
    return sprintf(str, "<fn %s>", function->name->chars);
}

static int stringifyList(ObjList* list, char* str)
{
    char* begin = str;
    sprintf(str, "%s", "[");
    str++;
    for(int i = 0; i < list->elements.count; i++)
    {
        if(i > 0)
        {
            sprintf(str, "%s", ", ");
            str +=2 ;
        }
        str += stringifyValue(list->elements.values[i], str);
    }
    sprintf(str, "%s", "]");
    str++;

    return (str - begin);
}

int stringifyObject(Value value, char* str)
{
    switch (OBJ_TYPE(value))
    {
        case OBJ_STRING:
            return sprintf(str, "%s", AS_CSTRING(value));
        case OBJ_FUNCTION:
            return stringifyFunction(AS_FUNCTION(value), str);
        case OBJ_NATIVE:
            return sprintf(str, "%s", "<native fn>");
        case OBJ_CLOSURE:
            return stringifyFunction(AS_CLOSURE(value)->function, str);
        case OBJ_UPVALUE:
            return sprintf(str, "%s", "upvalue");
        case OBJ_LIST:
            return stringifyList(AS_LIST(value), str);
        case OBJ_CLASS:
            return sprintf(str, "%s", AS_CLASS(value)->name->chars);
        case OBJ_INSTANCE:
            return sprintf(str, "%s instance", AS_INSTANCE(value)->klass->name->chars);
        case OBJ_BOUND_METHOD:
            return stringifyFunction(AS_BOUND_METHOD(value)->method->function, str);
    }
}

static int stringifyListLength(ObjList* list)
{
    int total = 2; // count '[' at start and ']' at end

    for(int i = 0; i < list->elements.count; i++)
    {
        if(i > 0)
        {
            total +=2; // ', '
        }
        total += stringifyValueLength(list->elements.values[i]);
    }

    return total;
}

int stringifyObjectLength(Value value)
{
    switch (OBJ_TYPE(value))
    {
        case OBJ_STRING:
            return AS_STRING(value)->length;
        case OBJ_FUNCTION:
            return AS_FUNCTION(value)->name == NULL ? 8 : AS_FUNCTION(value)->name->length + 5;
        case OBJ_NATIVE:
            return 11;
        case OBJ_CLOSURE:
            return AS_CLOSURE(value)->function->name == NULL ? 8 : AS_CLOSURE(value)->function->name->length + 5;
        case OBJ_UPVALUE:
            return 7;
        case OBJ_LIST:
            return stringifyListLength(AS_LIST(value));
        case OBJ_CLASS:
            return AS_CLASS(value)->name->length;
        case OBJ_INSTANCE:
            return AS_INSTANCE(value)->klass->name->length + 9;
        case OBJ_BOUND_METHOD:
            return AS_BOUND_METHOD(value)->method->function->name == NULL ? 8 : AS_BOUND_METHOD(value)->method->function->name->length + 5;
    }
}
