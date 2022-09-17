#include <stdio.h>
#include <string.h>

#include "console.h"

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

    BG_BLACK        = 40,
    BG_RED          = 41,
    BG_GREEN        = 42,
    BG_YELLOW       = 43,
    BG_BLUE         = 44,     
    BG_MAGENTA      = 45, 
    BG_CYAN         = 46, 
    BG_LIGHT_GRAY   = 47, 
    BG_DEFAULT      = 49,
    
    FG_DARK_GRAY = 90, 
    FG_LIGHT_RED = 91, 
    FG_LIGHT_GREEN = 92, 
    FG_LIGHT_YELLOW = 93, 
    FG_LIGHT_BLUE = 94, 
    FG_LIGHT_MAGENTA = 95, 
    FG_LIGHT_CYAN = 96, 
    FG_WHITE = 97, 

};

bool writeNative(int argCount, Value* args) 
{
    // todo - make sure it's a string
    char* string = AS_CSTRING(args[0]);
    printf("%s", string);

    return true;
}

bool locateNative(int argCount, Value* args) 
{
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
    char* color = AS_CSTRING(args[0]);

    printf("%c[%dm",0x1B, getColorCode(color, false));

    return true;
}

bool backColorNative(int argCount, Value* args) 
{
    char* color = AS_CSTRING(args[0]);

    printf("%c[%dm",0x1B, getColorCode(color, true));

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

    args[-1] = OBJ_VAL(copyString(line, (int)strlen(line)));

    return true;
}

//fgets(line, sizeof(line), stdin)