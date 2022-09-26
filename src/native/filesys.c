#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "tinydir.h"

#include "filesys.h"

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
    
    tinydir_dir dir;
    tinydir_open(&dir, path);

    while (dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&dir, &file);

        //printf("%s", file.name);
        writeValueArray(&list->elements, OBJ_VAL(copyStringRaw(file.path, (int)strlen(file.path))));
        //if (file.is_dir)
        

        tinydir_next(&dir);
    }

    tinydir_close(&dir);

    args[-1] = OBJ_VAL(list);

    return true;
}