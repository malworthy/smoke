#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "native.h"

#define BUFFER_SIZE 200
#define MAX_FILES 255

static FILE* files[MAX_FILES];
static int fileCount = 0;

bool openNative(int argCount, Value* args)
{
    CHECK_STRING(0, "Parameter 1 must be a string for function open()");
    CHECK_STRING(1, "Parameter 2 must be a string for function open()");

    if (fileCount >= MAX_FILES)
    {
        //TODO: File a blank space in the array, if there are none then error
        NATIVE_ERROR("Too many open files");
    }

    FILE* file = fopen(AS_CSTRING(args[0]), AS_CSTRING(args[1]));
    files[fileCount] = file;

    args[-1] = NUMBER_VAL((double)fileCount);
    fileCount++;

    return true;
}

bool closeNative(int argCount, Value* args)
{
    CHECK_NUM(0, "Parameter 1 must be a number for function close()");

    int index = (int)AS_NUMBER(args[0]);

    if (index >= fileCount) return true;

    FILE* fp = files[index];

    if (fp == NULL) return true;

    fclose(fp);

    files[index] = NULL;

    if (index == fileCount - 1) fileCount--;

    return true;
}

bool writeFileNative(int argCount, Value* args)
{
    CHECK_NUM(0, "Parameter 1 must be a number for function write()");

    int result = 0;

    args[-1] = NUMBER_VAL(0);

    int index = (int)AS_NUMBER(args[0]);
    if (index >= fileCount) return true;
    FILE* fp = files[index];
    if (fp == NULL) return true;

    if (IS_STRING(args[1]))
    {
        int result = fprintf(fp, "%s", AS_CSTRING(args[1]));
    }

    args[-1] = NUMBER_VAL(result);

    return true;
}

bool readlinesNative(int argCount, Value* args)
{
    CHECK_STRING(0, "Parameter 1 must be a string for function readline()");

    char const* const fileName = AS_CSTRING(args[0]); 
    FILE* file = fopen(fileName, "r");
    if (file == NULL)
    {
        NATIVE_ERROR("Could not open file");
    }

    char buffer[BUFFER_SIZE];
    char* line = (char*)malloc(BUFFER_SIZE);
    int lineLength = BUFFER_SIZE;

    line[0] = '\0';
    int buffCount = 0;
    
    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected

    while (fgets(buffer, sizeof(buffer), file)) 
    {
        //printf("buffer: %s\n", buffer);
        if (strlen(buffer) == BUFFER_SIZE - 1 && buffer[BUFFER_SIZE - 2] != '\n')
        {
            buffCount++;
            if (BUFFER_SIZE * buffCount > lineLength)
            {
                line = (char*)realloc(line, BUFFER_SIZE * buffCount);
                lineLength = BUFFER_SIZE * buffCount;
            }
            strcat(line, buffer);    
        }
        else
        {
            buffCount++;
            if (BUFFER_SIZE * buffCount > lineLength)
            {
                line = (char*)realloc(line, BUFFER_SIZE * buffCount);
                lineLength = BUFFER_SIZE * buffCount;
            }
            strcat(line, buffer);
            // remove any CR/LF
            for(int i=strlen(line)-1; i>=0; i--) 
            {
                if (line[i] == '\n' || line[i] == '\r') 
                    line[i] = '\0';
                else
                    break;
            }
            //printf("line:%s\n", line);
            Value val = OBJ_VAL(copyStringRaw(line, (int)strlen(line)));
            push(val);
            writeValueArray(&list->elements, val);
            pop();
            line[0] = '\0';
            buffCount = 0;
            //i++;
        }
    }
    /* may check feof here to make a difference between eof and io failure -- network
       timeout for instance */

    fclose(file);
    free(line);

    args[-1] = OBJ_VAL(list);

    pop(); // get rid of list from the stack

    return true;
}