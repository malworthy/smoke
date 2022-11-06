#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "../memory.h"
#include "native.h"

#define ITEM_BUFFER_SIZE 100

bool splitlinesNative(int argCount, Value* args)
{
    CHECK_STRING(0, "Parameters for splitlines must be of type string");
    
    ObjString* string = AS_STRING(args[0]);
    const char* str = string->chars;
	int itemSize = ITEM_BUFFER_SIZE;
    char* item = malloc(ITEM_BUFFER_SIZE);
    int start = 0;
    int end = string->length; 

    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected

	for(int i=0; i<end; i++)
	{
		if(str[i] == '\n' || str[i] == '\r')
		{
            if(start < i)
            {
                if(itemSize < i - start)
                {
                    item = (char*)realloc(item, (i - start)+1);
                    itemSize = i - start;
                }
                                
                memcpy(item, str + start, i - start);
                item[i-start] = '\0';
                Value val =OBJ_VAL(copyStringRaw(item, i-start));
                push(val);
                writeValueArray(&list->elements, val);
                pop();
            }
			start = i + 1;
		}
	}
    if (start < end)
    {
        if(itemSize < end - start)
        {
            size_t newSize = (size_t) end-start;
            item = (char*)realloc(item, newSize + 1);
        }
        memcpy(item, str + start,  end-start);
        item[end-start] = '\0';

        Value val =OBJ_VAL(copyStringRaw(item, end-start));
        push(val);
        writeValueArray(&list->elements, val);
        pop();
    }
    args[-1] = OBJ_VAL(list);
    pop();
    free(item);
    return true;
}

bool splitNative(int argCount, Value* args)
{
    if (!IS_STRING(args[0]) || !IS_STRING(args[1]))
    {
        NATIVE_ERROR("Parameters for split must be of type string");
    }

    const ObjString* strObj = AS_STRING(args[0]);
    const ObjString* delimObj = AS_STRING(args[1]);

    const char* str = strObj->chars; 
    const char* delim = delimObj->chars; 

    int itemSize = ITEM_BUFFER_SIZE;
    char* item = malloc(ITEM_BUFFER_SIZE);
    int start = 0;
    int len = delimObj->length; 
    int end = strObj->length;
    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected

	for(int i=0; i < end - len; i++)
	{
		if(memcmp(str+i,delim,len) == 0)
		{
            if(start < i)
            {
                if(itemSize < i - start)
                {
                    item = (char*)realloc(item, (i - start)+1);
                    itemSize = i - start;
                }
                memcpy(item, str + start, i - start);
                item[i-start] = '\0';
                Value val = OBJ_VAL(copyStringRaw(item, i-start ));
                push(val);
                writeValueArray(&list->elements, val);
                pop();
            }
            i += (len-1);
            start = i+1;
		}
	}

    if (memcmp(str+end-len,delim,len) == 0)
        end -= len;

    if (start < end)
    {
        if(itemSize < end - start)
        {
            size_t newSize = (size_t) end-start;
            item = (char*)realloc(item, newSize + 1);
        }
        memcpy(item, str + start, end - start);
        item[end-start] = '\0';
        Value val =OBJ_VAL(copyStringRaw(item, end-start));
        push(val);
        writeValueArray(&list->elements, val);
        pop();
    }
    args[-1] = OBJ_VAL(list);
    pop();
    free(item);

    return true;
}

bool asciiNative(int argCount, Value* args)
{
    CHECK_STRING(0, "ascii expects a string as parameter.");
    
    char* string = AS_CSTRING(args[0]);
    if (string[0] == '\0')
    {
        NATIVE_ERROR("Cannet get ascii value for an empty string");
    }
    double val = string[0];

    args[-1] = NUMBER_VAL(val);
    
    return true;
}

bool charNative(int argCount, Value* args)
{
    CHECK_NUM(0, "char expects a number as parameter.");

    int val = (int)AS_NUMBER(args[0]);
    if (val < 0 || val > 255)
    {
        NATIVE_ERROR("char expect a number between 0 and 255");
    }
    char c[1];
    c[0] = val;
    //c[1] = '\0';
    args[-1] = OBJ_VAL(copyStringRaw(c, 1));

    return true;
}

bool upperNative(int argCount, Value* args)
{
    CHECK_STRING(0, "ascii expects a string as parameter.");
    
    ObjString* string = AS_STRING(args[0]);
    char *result = ALLOCATE(char, string->length+1);

    for (int i=0; i<string->length; i++)
        result[i] = toupper(string->chars[i]);
    result[string->length] = '\0';

    args[-1] = OBJ_VAL(takeString(result, string->length));
    
    return true;
}