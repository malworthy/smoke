#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "native.h"
#include "jsmn.h"

static int tokenCounter = 0;
static int numberOfTokens = 0;

bool addNodesToTable(Value table, char* json, jsmntok_t* tokens, bool children)
{
    int childCount = 0;
    int numberOfChildren = 0;
    Value key;
    Value val;
    jsmntype_t type = tokens[tokenCounter].type;

    if (children)
    {
        numberOfChildren = tokens[tokenCounter].size;
    }
    tokenCounter++;

    if (children && numberOfChildren == 0)
        return true;

    while (tokenCounter < numberOfTokens)
    {
        // Get Key
        if(type == JSMN_OBJECT)
        {
            if(tokens[tokenCounter].type != JSMN_STRING)
            {
                return false; // invalid json
            }
            key = OBJ_VAL(copyString(json + tokens[tokenCounter].start, tokens[tokenCounter].end - tokens[tokenCounter].start));
            tokenCounter++;
            push(key);
        }

        if(tokens[tokenCounter].type == JSMN_STRING)
        {
            val = OBJ_VAL(copyString(json + tokens[tokenCounter].start, tokens[tokenCounter].end - tokens[tokenCounter].start));
            push(val);
            tokenCounter++;
        }
        else if (tokens[tokenCounter].type == JSMN_PRIMITIVE)
        {
            char firstChar = json[tokens[tokenCounter].start];
            if ((firstChar >= '0' && firstChar <= '9') || firstChar == '-')
                val = NUMBER_VAL(atof(json + tokens[tokenCounter].start));
            else if(firstChar == 't' && (tokens[tokenCounter].end - tokens[tokenCounter].start) == 4)
                val = BOOL_VAL(true);
            else if(firstChar == 'f' && (tokens[tokenCounter].end - tokens[tokenCounter].start) == 5)
                val = BOOL_VAL(false);
            else if(firstChar == 'n' && (tokens[tokenCounter].end - tokens[tokenCounter].start) == 4)
                val = NIL_VAL;
            else
                val = NIL_VAL;
            push(val);
            tokenCounter++;
        }
        else if (tokens[tokenCounter].type == JSMN_OBJECT)
        {
            val = OBJ_VAL(newTable());
            push(val);
            if(!addNodesToTable(val, json, tokens, true))
            {  
                printf("failed on object\n");
                return false;
            }
        }
        else if (tokens[tokenCounter].type == JSMN_ARRAY)
        {
            val = OBJ_VAL(newList());
            push(val);
            if(!addNodesToTable(val, json, tokens, true))
            {
                printf("failed on array\n");
                return false;
            }
        }
        else
        {
            printf("unknown token\n");
            return false;
        }

        if(type == JSMN_ARRAY)
        {
            ObjList* list = AS_LIST(table);
            //printf("add to array\n");
            writeValueArray(&list->elements, val);
        }
        else if (type == JSMN_OBJECT)
            setTable(table, val, key);
        else
            return false;

        pop();
        if(type == JSMN_OBJECT) pop();

        if(children)
        {
            childCount++;
            if (childCount >= numberOfChildren)
                return true;
        }
    }

    return true;
}

bool jsonNative(int argCount, Value* args)
{
    CHECK_STRING(0, "json expects a string");
    args[-1] = NIL_VAL; // If we can't parse the json, return null

    tokenCounter = 0;
    numberOfTokens = 0;

    ObjString* json = AS_STRING(args[0]);
    jsmn_parser parser;

    jsmn_init(&parser);
    numberOfTokens = jsmn_parse(&parser, json->chars, json->length, NULL, 0);
    
    jsmntok_t* tokens = (jsmntok_t*)malloc((numberOfTokens) * sizeof(jsmntok_t));
    jsmn_init(&parser);
    numberOfTokens = jsmn_parse(&parser, json->chars, json->length, tokens, numberOfTokens);
   
    //for (int i = 0; i < numberOfTokens; i++)
    //    printf("type: %d, count: %d\n", tokens[i].type, tokens[i].size);

    if (numberOfTokens < 1 ) 
    {
        free(tokens);
        return true;
    }
    Value topLevelObject;
    if (tokens[0].type == JSMN_OBJECT)
    {
        topLevelObject = OBJ_VAL(newTable());
    }
    else if (tokens[0].type == JSMN_ARRAY)
    {
        topLevelObject = OBJ_VAL(newList());
    }
    else
    {
        free(tokens);
        return true;
    }
    push(topLevelObject);

    if (!addNodesToTable(topLevelObject, json->chars, tokens, false))
    {
        free(tokens);
        return true;
    }
    free(tokens);

    args[-1] = topLevelObject;
    pop();

    return true;
}