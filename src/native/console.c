#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
    #include <conio.h>
#else
    #include "conio.h"
#endif

#include "console.h"
#include "native.h"

 enum Code {
    FG_BLACK        = 30,
    FG_RED          = 31,
    FG_GREEN        = 32,
    FG_YELLOW       = 33,
    FG_BLUE         = 34,     
    FG_MAGENTA      = 35, 
    FG_CYAN         = 36, 
    FG_LIGHT_GRAY   = 37, 
    FG_DEFAULT      = 39,
    
    FG_DARK_GRAY = 90, 
    FG_LIGHT_RED = 91, 
    FG_LIGHT_GREEN = 92, 
    FG_LIGHT_YELLOW = 93, 
    FG_LIGHT_BLUE = 94, 
    FG_LIGHT_MAGENTA = 95, 
    FG_LIGHT_CYAN = 96, 
    FG_WHITE = 97, 

};

bool kbhitNative(int argCount, Value* args) 
{
    int result = _kbhit();
    args[-1] = NUMBER_VAL(result);
    return true;
}

bool getchNative(int argCount, Value* args)
{
    char keyString[11];
    //keyString[0] = '\0';
    int hits = _kbhit();
    for(int i=0; i < hits && i < 10; i++)
    {
        int key = _getch();
        if (key == 27) keyString[i] = '#'; else keyString[i] = key;
        //keyString[i] = _getch();
    }

    keyString[hits] = '\0';

    args[-1] = OBJ_VAL(copyStringRaw(keyString, hits));
    return true;
}

bool getchNative_old(int argCount, Value* args) 
{
    char key[10];
    int result = _getch();
    //args[-1] = NUMBER_VAL(result);
    if (result == 27)
    {
        int nextkey = 0;
        key[0] = '#';
        int i = 0;
        while(_kbhit() > 0 && ++i < 10)
        {
            nextkey = _getch();
            key[i] = nextkey;
        }
        key[i] = '\0';
        //printf("nextkey: %d\n", nextkey);
    }
    else
    {
        key[0] = result;
        key[1] = '\0';
    }
    args[-1] = OBJ_VAL(copyStringRaw(key, (int)strlen(key)));
    return true;
}

bool writeNative(int argCount, Value* args) 
{
    CHECK_STRING(0, "Argument to write() must be a string.");

    char* string = AS_CSTRING(args[0]);

    printf("%s", string);
    fflush(stdout);    
    return true;
}

bool locateNative(int argCount, Value* args) 
{
    CHECK_NUM(0, "Argument 1 of locate must be a number");
    CHECK_NUM(1, "Argument 2 of locate must be a number");

    int x = (int)AS_NUMBER(args[0]);
    int y = (int)AS_NUMBER(args[1]);
    printf("%c[%d;%df",0x1B,y,x);

    return true;
}

bool clearNative(int argCount, Value* args) 
{
    printf("%cc",0x1B);

    return true;
}

static int getColorCode(char* color, bool isBackground)
{
    int code = FG_DEFAULT;
    if (strcmp(color, "red") == 0)
        code = FG_RED;
    if (strcmp(color, "blue") == 0)
        code = FG_BLUE;
    if (strcmp(color, "green") == 0)
        code = FG_GREEN;

    if (strcmp(color, "black") == 0)
        code = FG_BLACK;
    if (strcmp(color, "cyan") == 0)
        code = FG_CYAN;
    if (strcmp(color, "yellow") == 0)
        code = FG_YELLOW;

    if (strcmp(color, "magenta") == 0)
        code = FG_MAGENTA;
    if (strcmp(color, "gray") == 0 || strcmp(color, "grey") == 0 || strcmp(color, "lightgray") == 0)
        code = FG_LIGHT_GRAY;

    if (isBackground) code += 10;

    return code;
}

bool textColorNative(int argCount, Value* args) 
{
    CHECK_STRING(0, "Parameter to textcolor must be a string");

    char* color = AS_CSTRING(args[0]);

    printf("%c[%dm",0x1B, getColorCode(color, false));

    return true;
}

bool backColorNative(int argCount, Value* args) 
{
    CHECK_STRING(0, "Parameter to backcolor must be a string");
    
    char* color = AS_CSTRING(args[0]);

    printf("%c[%dm",0x1B, getColorCode(color, true));

    return true;
}

bool cursoffNative(int argCount, Value* args)
{
    CHECK_BOOL(0,"Parameter to cursoff() must be a boolean.");

    if(AS_BOOL(args[0]))
        printf("\e[?25l");
    else
        printf("\e[?25h");

    return true;
}

bool inputNative(int argCount, Value* args) 
{
    char line[1024];
    fgets(line, sizeof(line), stdin);

    // strip CR/LF from end of string
    if (line[0] == '\n' || line[0] == '\r')
        line[0] = '\0';
    else
    {
        for(int i=strlen(line)-2; i < strlen(line) && i >= 0; i++)
        {
            if (line[i] == '\n' || line[i] == '\r')
            {
                line[i] = '\0';
                break;
            }
        }
    }

    args[-1] = OBJ_VAL(copyStringRaw(line, (int)strlen(line)));

    return true;
}
