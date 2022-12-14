#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "core.inc"

#define MAX_INCS 256

#ifndef _WIN32

#include "native/conio.h"
#include <readline/readline.h>
#include <readline/history.h>

#endif

char* includeFiles[MAX_INCS];
char* fileContents[MAX_INCS];
char* fileContentsName[MAX_INCS];

int includeFileCount = 0;
int fileContentsCount = 0;

const char** _args;

int _argc;

static void repl() 
{
    char line[1024];
    printf("Smoke Version %s  (Press ctrl-c to exit)\n", SM_VERSION);

    for (;;) 
    {
#ifndef _WIN32
        char *buffer = readline("> ");
        strncpy(line, buffer, 1024);
        line[1023] = '\0';
        if(line[0] != '\0') add_history(line);
        free(buffer);
#else
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) 
        {
            printf("\n");
            break;
        }
#endif
        interpret(line,"");
#ifndef _WIN32
        restoreTerminal();
#endif
    }
}

static char* readFile(const char* path) 
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) 
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) 
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) 
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

// TODO: Need to add all the error handling!
static bool preProcessFile(const char* filename, const char* rootFilename)
{    
    //printf("filename:%s rootFilename:%s\n", filename, rootFilename);
    char adjustedFilename[256] = {0};
    if (strcmp(filename, rootFilename) != 0)
    {
        bool hasPath = false;
        for(int i=strlen(rootFilename)-1; i>0; i--)
        {
            if (rootFilename[i] == '/' || rootFilename[i] == '\\')
            {
                memcpy(adjustedFilename, rootFilename, i+1);
                memcpy(adjustedFilename + i+1, filename, strlen(filename)+1);
                adjustedFilename[i+strlen(filename)+1] = '\0';
                hasPath = true;
                break;
            }
        }
        if (!hasPath)
            memcpy(adjustedFilename, filename, strlen(filename)+1);
    }
    else
    {
        memcpy(adjustedFilename, filename, strlen(filename)+1);
    }

    //printf("Adjusted Filename: %s\n", adjustedFilename);

    char* buffer = readFile(adjustedFilename);

    char* localIncludeFiles[MAX_INCS];
    char* current = buffer;
    char prev = '\n';
    int localCount = 0;

    while(*current != '\0')
    {
        if(*current == '#' 
			&& current[1] != '\0' && current[2] != '\0' && current[3] != '\0' && current[4] != '\0'
			&& current[1] == 'i' && current[2] == 'n' && current[3] == 'c' && current[4] == ' '
		       && (prev == '\n' || prev == '\r'))
	
        {
            char* startOfInc = current;
                current += 5;
            char *filenameStart = current;
            
            while(*current != '\n' && *current != '\r' && *current != '\0') 
                current++;

            int len = current - filenameStart;
            char* includeFile = (char*)malloc(len+1);

            memcpy(includeFile, filenameStart, len);
            includeFile[len] = '\0';

            // only add if doesn't exist
            bool exists = false;
            for (int i = 0; i < includeFileCount; i++)
            {
                if (strcmp(includeFiles[i], includeFile) == 0)
                {
                     exists = true;
                     break;
                }
                if (strcmp(includeFile, rootFilename) == 0 && strcmp(filename, rootFilename) != 0 )
                {
                    exists = true;
                    break;
                }
            }

            // TODO: notify if greater than MAX_INCS
            if (!exists && includeFileCount < MAX_INCS)
            {
                includeFiles[includeFileCount++] = includeFile;
                localIncludeFiles[localCount++] = includeFile;
            }
            else if (includeFileCount >= MAX_INCS)
            {
                printf("Too many include files\n");
                return false;
            }
            
            // wipe away #inc by turning into a comment
            *startOfInc = '/';
            startOfInc[1] = '/'; 
        }
	    prev = *current;
        current++;	
    }
    
    for (int i=0; i<localCount; i++)
    {
        if (!preProcessFile(localIncludeFiles[i], rootFilename))
            return false;
    }
    
    fileContents[fileContentsCount] = buffer;
    fileContentsName[fileContentsCount] = (char*)malloc(strlen(filename)+1);
    memcpy(fileContentsName[fileContentsCount], filename, strlen(filename)+1);
    fileContentsCount++;

    return true;
}

static int runFile(const char* path) 
{
    preProcessFile(path, path);

    for (int i = 0;  i < fileContentsCount; i++ )
    {
        //printf("Running file %d of %d: %s\n", i+1, fileContentsCount, fileContentsName[i]);
        InterpretResult result = interpret(fileContents[i], fileContentsName[i]);

        if (result == INTERPRET_COMPILE_ERROR) return 65;
        if (result == INTERPRET_RUNTIME_ERROR) return 70;
    }

    for (int i = 0;  i < fileContentsCount; i++ )
    {
         free(fileContents[i]); 
         free(fileContentsName[i]);
    }

    return 0;
}

int main (int argc, const char* argv[])
{
    srand(time(NULL)); 

    _argc = argc;
    _args = argv;

    initVM();

    InterpretResult result = interpret(coreModuleSource, "core");
    if (result != INTERPRET_OK)
    {
        //If there are errors in the core library obvously I've stuffed up, but let me know.
        printf("Core library is corrupt\n");
        exit(1);
    }

    int exitCode = 0;

    if (argc == 1) 
    {
        repl();

        //runFile("c:\\Users\\malwo\\code\\min\\tst\\test.sm");
    } 
    else if (argc >= 2) 
    {
        exitCode = runFile(argv[1]);
    } 
    else 
    {
        fprintf(stderr, "Usage: smoke [path]\n");
        exit(64);
    }

    freeVM();
#ifndef _WIN32
    restoreTerminal();
#endif
    return exitCode;
}
