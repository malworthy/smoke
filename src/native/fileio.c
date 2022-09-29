#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"

#define BUFFER_SIZE 200


bool readlinesNative(int argCount, Value* args)
{
    if (!IS_STRING(args[0]))
    {
        char* msg = "Parameter 1 must be a string for function readline()";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }

    char const* const fileName = AS_CSTRING(args[0]); 
    FILE* file = fopen(fileName, "r");
    if (file == NULL)
    {
        char* msg = "Could not open file";
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg)));

        return false;
    }

    char buffer[BUFFER_SIZE];
    char* line = (char*)malloc(BUFFER_SIZE);
    int lineLength = BUFFER_SIZE;

    line[0] = '\0';
    //int i = 0;
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