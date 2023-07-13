#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sqlite3.h"
#include "../native/native.h"
#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "../memory.h"

#define MAX_CACHE_QUERY 8192

static char dbname[1025] = {0};
static sqlite3* db = NULL;
static char lastQuery[MAX_CACHE_QUERY] = {0};
static sqlite3_stmt *stmt = NULL;

int queryNative(int argCount, Value* args) 
{   
    ObjList* list;
    //char *err_msg = NULL;
    int rc;
    
    if (db == NULL)
    {
        if (dbname[0] == '\0')
            rc = sqlite3_open(":memory:", &db);
        else
            rc = sqlite3_open(dbname, &db);

        if (rc != SQLITE_OK) 
        {
            char buffer[100];

            sprintf(buffer, "Failed to open database: %s", dbname);
            NATIVE_ERROR(buffer);
        }
    }

    // get sql string
    // step 1 - work out how much memory to allocate
    int length = 1;
    for(int i = 0; i < argCount; i++)
    {
        if(i % 2 == 0)
        {
            length += AS_STRING(args[i])->length;
        }
        else
        {
            length += 3;
        }
    }
    char* sql = (char*)malloc(length);
    char *start = sql;
    
    // step 2 - create the string
    for(int i = 0; i < argCount; i++)
    {
        if(i % 2 == 0)
        {
            memcpy(sql, AS_STRING(args[i])->chars, AS_STRING(args[i])->length);
            sql += AS_STRING(args[i])->length;
        }
        else
        {
            memcpy(sql, " ? ", 3);
            sql += 3;
        }

    }
    *sql = '\0';

    //step 3 cashe the query 
    if (!(lastQuery[0] != '\0' && strcmp(lastQuery, start) == 0))
    {
        //printf("execute new query\n");
        if (length < MAX_CACHE_QUERY)
            memcpy(lastQuery, start, length);

        sqlite3_finalize(stmt);
        rc = sqlite3_prepare_v2(db, start, -1, &stmt, NULL);
        if (rc != SQLITE_OK) 
        {
            NATIVE_ERROR("Failed to execute statement");
            //fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));     
        }
    }

    // bind the parameters
    sqlite3_reset(stmt);
    int paramNum = 1;
    for(int i = 1; i < argCount; i++)
    {
        if(i % 2 != 0)
        {
            if (IS_NUMBER(args[i]))
                sqlite3_bind_double(stmt, paramNum++, AS_NUMBER(args[i]));
            else if (IS_STRING(args[i]))
                sqlite3_bind_text(stmt, paramNum++, AS_CSTRING(args[i]), -1, SQLITE_STATIC);
            else if (IS_NIL(args[i]))
                sqlite3_bind_null(stmt, paramNum++);
            else
            {
                NATIVE_ERROR("Only numbers are strings can be passed as parameter values to a sql");
            }
        }
    }

    list = newList();
    push(OBJ_VAL(list));

    int step;  

    do
    {
        step = sqlite3_step(stmt);
        //printf("executed statement %d\n", step);
    
        if (step == SQLITE_ROW) 
        {
            ObjTable* table = newTable();
            push(OBJ_VAL(table));
            int count = sqlite3_column_count(stmt);

            //printf("%s: ", sqlite3_column_text(res, 0));
            //printf("%s\n", sqlite3_column_text(res, 1));
            //-----
            for(int i=0; i < count; i++)
            {
                const char* columnName = sqlite3_column_name(stmt, i);
                ObjString* key = copyStringRaw(columnName, strlen(columnName));
                push(OBJ_VAL(key));
                const char* columnValue = (const char*)sqlite3_column_text(stmt, i);
                Value value = columnValue ?  OBJ_VAL(copyStringRaw(columnValue, strlen(columnValue))) : NIL_VAL;
                push(value);
                setTable(OBJ_VAL(table), value, OBJ_VAL(key));
                
                pop();
                pop();
            }
            //-----
            writeValueArray(&list->elements, OBJ_VAL(table));
            pop();
        } 
    } while (step == SQLITE_ROW);
    
    //sqlite3_finalize(stmt);
    //sqlite3_close(db);

    free(start);

    args[-1] = OBJ_VAL(list);
    pop();

    return true;
}

bool setdbNative(int argCount, Value* args)
{
    CHECK_STRING(0, "setdb expects a string");

    int length = AS_STRING(args[0])->length;

    if (length > 1024)
    {
        NATIVE_ERROR("File path too large.  Max is 1024 characters.");
    }

    if (length > 0)
    {
        //dbname = AS_CSTRING(args[0]);
        memcpy(dbname, AS_CSTRING(args[0]), length);
        dbname[length] = '\0';
        sqlite3_close_v2(db);
        db = NULL;
    }

    args[-1] = OBJ_VAL(copyStringRaw(dbname, strlen(dbname)));

    return true;
}

bool closedbNative(int argCount, Value* args)
{
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    stmt = NULL;
    db = NULL;
    lastQuery[0] = '\0';

    return true;
}