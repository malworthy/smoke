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

static ObjList* list = NULL;

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    ObjTable* table = newTable();
    push(OBJ_VAL(table));
    
    for(int i=0; i<argc; i++)
    {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        ObjString* key = copyStringRaw(azColName[i], strlen(azColName[i]));
        push(OBJ_VAL(key));
        Value value = argv[i] ?  OBJ_VAL(copyStringRaw(argv[i], strlen(argv[i]))) : NIL_VAL;
        push(value);
        printf("key is %s\n", key->chars);
        //tableSet(&table->elements, key, value);
        setTable(OBJ_VAL(table), value, OBJ_VAL(key));
        
        printf("---items in table %d\n",table->elements.count);
        printValue(OBJ_VAL(table));
        printf("\n---\n");
        pop();
        pop();
    }
    
    
    writeValueArray(&list->elements, OBJ_VAL(table));
   
    pop();
    printf("\n");
    return 0;
}

Value executeSql(Value sql)
{
    sqlite3 *db;
    char* errorMessage = NULL;
    int rc = sqlite3_open("c:\\tmp\\test.db", &db);
    if (rc)
    {
        printf("%s\n",sqlite3_errmsg(db)); //for debugging
        return NIL_VAL;
    }
  
    rc = sqlite3_exec(db, AS_CSTRING(sql), callback, 0, &errorMessage);

    if(rc != SQLITE_OK)
    {
        printf("SQL error: %s\n", errorMessage);
        sqlite3_free(errorMessage);
    }

    return OBJ_VAL(list);
}

int queryNative(int argCount, Value* args) 
{   

    sqlite3 *db;
    char *err_msg = NULL;
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_open("c:\\tmp\\test.db", &db);

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
    printf("the sql is: %s\n", start);

    rc = sqlite3_prepare_v2(db, start, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        NATIVE_ERROR("Failed to execute statement");
        //fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));     
    }

    // bind the parameters
    int paramNum = 1;
    for(int i = 1; i < argCount; i++)
    {
        if(i % 2 != 0)
        {
            printf("binding arg %d\n",i);
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
        printf("executed statement %d\n", step);
    
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
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    free(start);

    args[-1] = OBJ_VAL(list);
    pop();

    return true;
    
}

bool queryNative2(int argCount, Value* args)
{
    CHECK_STRING(0, "json expects a string");

    list = newList();
    push(OBJ_VAL(list));

    args[-1] = executeSql(args[0]);

    pop();
    list = NULL;
    return true;
}