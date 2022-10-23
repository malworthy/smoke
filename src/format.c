#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "common.h"
#include "object.h"

// https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c
// You must free the result if result is non-NULL.
static char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

static int addThousandsSeparator(char* buffer, char* string, char sep)
{
    int count = 0;
    bool negative = string[0] == '-';
    
    if(negative)  
    { 
        string++; 
        count = 1; 
        buffer[0]='-'; 
    }

    char* start = string;
    
    while(*string!='.' && *string!='\0') string++;

    int len = string - start;
    //printf("len:%d\n", len);
    //printf("start: %s\n", start);
    int firstComma = len % 3;
    int digitCount = 1;
    while(*start != '\0')
    {
        buffer[count] = *start;
        //printf("char: %c count: %d buf: %s (len-count): %d\n", *start, count, buffer, (len-count));
        count++;
        if (digitCount < len && digitCount >= firstComma && (digitCount - firstComma) % 3 == 0) 
        {
            buffer[count++] = sep;
            //printf("char: %c count: %d buf: %s\n", *start, count, buffer);
        }
        start++;
        digitCount++;
    }
    //printf("final count: %d\n", count);
    buffer[count] = '\0';

    return count;
}

static bool toCFormat(char* format, char* buffer)
{
    if(format[0] == 'n' || format[0] == 'c')
    {
        int decimalPlaces = 0;
        if (strlen(format) > 1 ) decimalPlaces = atoi(format+1);
        if (decimalPlaces > 30) decimalPlaces = 30;
        if (decimalPlaces < 0) decimalPlaces = 0;
        sprintf(buffer, "%%0.%df", decimalPlaces);
        return true;
    }

    return false;
}

Value format(Value fmt, Value val)
{
    // any format string > 30 chars is invalid.  The will prevent buffer over run, since we are using 
    // a fixed buffer size.
    if (AS_STRING(fmt)->length > 30)
    {
        return val;
    }

    if (IS_NUMBER(val))
    {
        char cformatstring[100]; 
        char* formatString = AS_CSTRING(fmt);

        bool result = toCFormat(formatString, cformatstring);
        if (!result)
            return val;

        //printf("format string: %s\n\n", cformatstring);
        char buffer[100];
        int len = sprintf(buffer, cformatstring, AS_NUMBER(val));
        if (formatString[0] == 'c')
        {
            char buffer2[100];
            len = addThousandsSeparator(buffer2, buffer, ',');
            return OBJ_VAL(copyStringRaw(buffer2, len));
        }
        return OBJ_VAL(copyStringRaw(buffer, len));
        
    }

    if (IS_DATETIME(val))
    {
        char* cformatstring = AS_CSTRING(fmt);
        time_t t = AS_DATETIME(val);
        struct tm *tm = localtime(&t);
        char buffer[100];
        int len = strftime(buffer, sizeof(buffer), cformatstring, tm);
        if (len == 0)
            return val;

        return OBJ_VAL(copyStringRaw(buffer, len));
    }

    // if neither date or number, ignore format string and leave value alone
    return val;
}