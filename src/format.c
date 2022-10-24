#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "common.h"
#include "object.h"

#define BUFFER_SIZE 200

typedef struct {
    char cfmt[10];
    char fmt[10];
} DateFormat;

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

static bool toCDateFormat(char* format, char* buffer)
{
    DateFormat formats[] =
    {
        [0] = {"\%x", "defdate"},
        [1] = {"\%X", "deftime"},
        [2] = {"\%T", "time1"},
        [3] = {"\%R", "time2"},
        [4] = {"\%F", "date"},
        [5] = {"\%c", "def"},
        [6] = {"\%V", "isowk"},


        [7] = {"\%B", "mmmm"},
        [8] = {"\%b", "mmm" },
        [9] = {"\%m", "mm"  },
        [10] = {"\%A", "dddd"},
        [11] = {"\%a", "ddd" },
        [12] = {"\%d", "dd"  },
        [13] = {"\%Y", "yyyy"},
        [14] = {"\%y", "yy"  },
        [15] = {"\%p", "ampm"},
        [16] = {"\%p", "AMPM"},
        [17] = {"\%H", "HH"},
        [18] = {"\%I", "hh"},
        [19] = {"\%j", "yd"},
        [20] = {"\%M", "MM"},
        [21] = {"\%S", "ss"},
        [22] = {"\%u", "wd"},
        [23] = {"\%W", "wk"},
        [24] = {"\%z", "zz"},

        [25] = {"\%e", "d1"},
    };
    char* tmp;
    strcpy(buffer, format);
    for (int i=0; i<=25; i++)
    {
        //printf("i:%d, fmt: %s, cfmt: %s\n",i, formats[i].fmt, formats[i].cfmt);
        tmp = str_replace(buffer, formats[i].fmt, formats[i].cfmt);
        if (tmp != NULL)
        {
            //printf("replaced: %s\n", tmp);
            strcpy(buffer, tmp);
            free(tmp);
        }
    }
}

Value format(Value fmt, Value val)
{
    // any format string > half the buffer size is invalid.  The will prevent buffer over run, since we are using 
    // a fixed buffer size.
    if (AS_STRING(fmt)->length > (BUFFER_SIZE / 2))
    {
        return val;
    }

    if (IS_NUMBER(val))
    {
        char cformatstring[BUFFER_SIZE]; 
        char* formatString = AS_CSTRING(fmt);

        bool result = toCFormat(formatString, cformatstring);
        if (!result)
            return val;

        //printf("format string: %s\n\n", cformatstring);
        char buffer[BUFFER_SIZE];
        int len = sprintf(buffer, cformatstring, AS_NUMBER(val));
        if (formatString[0] == 'c')
        {
            char buffer2[BUFFER_SIZE];
            len = addThousandsSeparator(buffer2, buffer, ',');
            return OBJ_VAL(copyStringRaw(buffer2, len));
        }
        return OBJ_VAL(copyStringRaw(buffer, len));
        
    }

    if (IS_DATETIME(val))
    {
        char cformatstring[BUFFER_SIZE];
        char* formatstring = AS_CSTRING(fmt);
        bool result = toCDateFormat(formatstring, cformatstring);
        //TODO:check result

        time_t t = AS_DATETIME(val);
        struct tm *tm = localtime(&t);
        char buffer[BUFFER_SIZE];
        int len = strftime(buffer, sizeof(buffer), cformatstring, tm);
        if (len == 0)
            return val;

        return OBJ_VAL(copyStringRaw(buffer, len));
    }

    // if neither date or number, ignore format string and leave value alone
    return val;
}
