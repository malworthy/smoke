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

static void printFunction(ObjFunction* function) 
{
    if (function->name == NULL) 
    {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

static void printList(ObjList* list)
{
    printf("[");
    for(int i = 0; i < list->elements.count; i++)
    {
        if(i > 0) printf(", ");
        printValue(list->elements.values[i]);
    }
    printf("]");
}

void printObject(Value value) 
{
    switch (OBJ_TYPE(value)) 
    {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        case OBJ_LIST:
            printList(AS_LIST(value));
            break;
    }
}