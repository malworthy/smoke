#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "tinydir.h"

#include "filesys.h"

bool runNative(int argCount, Value* args)
{
#define BUFFER_SIZE 1024

    char buffer[BUFFER_SIZE];
    char const* const fileName = AS_CSTRING(args[0]); 

    #if defined(_WIN32)
        FILE* file = _popen(fileName, "r");
    #else
        FILE* file = popen(fileName, "r");
    #endif

    if (file == NULL)
    {
        char* msg = "Could not run external process";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
    char * output = malloc(BUFFER_SIZE);
    output[0] = '\0';
    int allocated = BUFFER_SIZE;

    while(fgets(buffer, BUFFER_SIZE, file))
    {
        if (strlen(output) + strlen(buffer) > allocated)
        {
            allocated += BUFFER_SIZE;
            output = (char*) realloc(output, allocated);
        }
        strcat(output, buffer);
    }

    if (output != NULL)
        printf("output: |%s|", output);
    else
        printf("no output");    

    #if defined(_WIN32)
        int returnCode = _pclose(file);
    #else
        int returnCode = pclose(file);
    #endif

    printf("return code: %d", returnCode);

    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected
    
    writeValueArray(&list->elements, NUMBER_VAL((double)returnCode));

    if (output != NULL)
    {
        Value val =OBJ_VAL(copyStringRaw(output, (int)strlen(output)));
        push(val);
        writeValueArray(&list->elements, val);
        pop();
    }
    
    args[-1] = OBJ_VAL(list);

    pop();
    free(output);

    return true;
}

bool dirNative(int argCount, Value* args)
{
    if (!IS_STRING(args[0]))
    {
        char* msg = "Parameter 1 must be a string for function dir()";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }
    char* path = AS_CSTRING(args[0]);
    if (path[0] == '\0')
        path = ".";
    ObjList* list = newList();
    push(OBJ_VAL(list)); // stop list being garbage collected
    
    tinydir_dir dir;
    tinydir_open(&dir, path);

    while (dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&dir, &file);

        Value val =OBJ_VAL(copyStringRaw(file.name, (int)strlen(file.name)));
        push(val);
        writeValueArray(&list->elements, val);
        pop();
        tinydir_next(&dir);
    }

    tinydir_close(&dir);

    args[-1] = OBJ_VAL(list);

    pop(); // get rid of list from the stack

    return true;
}