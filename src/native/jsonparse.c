#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "native.h"
#include "jsmn.h"

bool addNodesToTable(Value table, char* json, jsmntok_t* tokens, int numberOfTokens)
{
    Value key;
    Value val;
    for (int i = 1; i < numberOfTokens;)
    {
        // Get Key
        if(tokens[i].type != JSMN_STRING)
        {
            return false; // invalid json
        }
        key = OBJ_VAL(copyStringRaw(json + tokens[i].start, tokens[i].end - tokens[i].start));
        i++;
        printf("--key--\n");
        printValue(key);
        printf("\n");
        push(key);

        if(tokens[i].type == JSMN_STRING)
        {
            printf("string\n");
            val = OBJ_VAL(copyStringRaw(json + tokens[i].start, tokens[i].end - tokens[i].start));
            printValue(val);
            printf("\n");
            push(val);
        }
        else if (tokens[i].type == JSMN_PRIMITIVE)
        {
            printf("primitive\n");
            // just a test for new
            val = NUMBER_VAL(44);
            push(val);
        }
        else if (tokens[i].type == JSMN_OBJECT)
        {
            printf("object\n");
            val = OBJ_VAL(newTable());
            push(val);
            if(!addNodesToTable(val, json, tokens+i, (tokens[i].size*2)))
                return false;
            i += (tokens[i].size*2);
        }
        else
        {
            printf("token type: %d\n", tokens[i].type);
        }
        i++;
        //i += tokens[i].size;
        setTable(table, val, key);

        pop();
        pop();
    }
    return true;
}

bool jsonNative(int argCount, Value* args)
{
    //printf("in json native\n");
    CHECK_STRING(0, "json expects a string");

    ObjString* json = AS_STRING(args[0]);
    int numberOfTokens;
    jsmn_parser parser;
    jsmntok_t tokens[128]; /* We expect no more than 128 tokens */

    jsmn_init(&parser);
    numberOfTokens = jsmn_parse(&parser, json->chars, json->length, tokens, 128);
    Value table = OBJ_VAL(newTable());
    push(table);

    for (int i = 0; i < numberOfTokens; i++)
        printf("type: %d, count: %d\n", tokens[i].type, tokens[i].size);

    /* Assume the top-level element is an object */
    if (numberOfTokens < 1 || tokens[0].type != JSMN_OBJECT) 
    {
        NATIVE_ERROR("Invalid JSON");
    }

    if (!addNodesToTable(table, json->chars, tokens, numberOfTokens))
    {
        NATIVE_ERROR("Invalid JSON");
    }
    printf("*** table below ***\n");
    printValue(table);
    printf("\n******\n");

    args[-1] = table;
    pop();

    return true;
}