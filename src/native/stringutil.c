#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#define ITEM_BUFFER_SIZE 100

bool splitlinesNative(int argCount, Value* args)
{
    if (!IS_STRING(args[0]))
    {
        char* msg = "Parameters for splitlines must be of type string";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
    
    const char* str = AS_CSTRING(args[0]);
	int itemSize = ITEM_BUFFER_SIZE;
    char* item = malloc(ITEM_BUFFER_SIZE);
    int start = 0;
    int end = strlen(str);

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
                    //printf("reallocating1\n");
                    item = (char*)realloc(item, (i - start)+1);
                    itemSize = i - start;
                }
                                
                memcpy(item, str + start, i - start);
                item[i-start] = '\0';
                Value val =OBJ_VAL(copyStringRaw(item, (int)strlen(item)));
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
            //printf("reallocating2 %d\n", newSize);
            item = (char*)realloc(item, newSize + 1);
        }
        memcpy(item, str + start,  end-start);
        item[end-start] = '\0';

        Value val =OBJ_VAL(copyStringRaw(item, (int)strlen(item)));
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
        char* msg = "Parameters for split must be of type string";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }

    const char* str = AS_CSTRING(args[0]);
    const char* delim = AS_CSTRING(args[1]);

    int itemSize = ITEM_BUFFER_SIZE;
    char* item = malloc(ITEM_BUFFER_SIZE);
    int start = 0;
    int len = strlen(delim);
    int end = strlen(str);
    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected

	for(int i=0; i < end - len; i++)
	{
		//printf("looping %d %c \n", i, str[i]);
		if(memcmp(str+i,delim,len) == 0)
		{
            if(start < i)
            {
                if(itemSize < i - start)
                {
                    //printf("reallocating1\n");
                    item = (char*)realloc(item, (i - start)+1);
                    itemSize = i - start;
                }
                memcpy(item, str + start, i - start);
                item[i-start] = '\0';
                Value val =OBJ_VAL(copyStringRaw(item, (int)strlen(item)));
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
        Value val =OBJ_VAL(copyStringRaw(item, (int)strlen(item)));
        push(val);
        writeValueArray(&list->elements, val);
        pop();
    }
    args[-1] = OBJ_VAL(list);
    pop();
    free(item);

    return true;
}