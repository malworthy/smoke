#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "native.h"

bool nowNative(int argCount, Value* args)
{
    args[-1] = DATETIME_VAL(time(NULL));
    return true;
}

bool dateNative(int argCount, Value* args)
{
    CHECK_STRING(0, "date() expect a string");

    struct tm * timeinfo;
    time_t rawtime;

    int year , month, day, hour = 0, min = 0, sec = 0;

    char* str = AS_CSTRING(args[0]);

    int result = sscanf(str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec);
    if (result < 3)
    {
        NATIVE_ERROR("Invalid date format");
    }

    if (year < 1970 || year > 3000)
    {
        NATIVE_ERROR("Invalid Date: Year must be between 1970 and 3000");
    }

    if (month < 1 || month > 12)
    {
        NATIVE_ERROR("Invalid Date: Month must be between 1 and 12");
    }

    if (day < 1 || day > 31)
    {
        NATIVE_ERROR("Invalid Date: Day is invalid");
    }

    if (hour < 0 || hour > 23)
    {
        NATIVE_ERROR("Invalid Date: Hour must be between 0 and 23");
    }

    if (min < 0 || min > 59)
    {
        NATIVE_ERROR("Invalid Date: Minute must be between 0 and 59");
    }

    if (sec < 0 || sec > 59)
    {
        NATIVE_ERROR("Invalid Date: Second must be between 0 and 59");
    }

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    timeinfo->tm_year = year - 1900;
    timeinfo->tm_mon = month - 1;
    timeinfo->tm_mday = day;
    timeinfo->tm_hour = hour;
    timeinfo->tm_min = min;
    timeinfo->tm_sec = sec;

    time_t parsedDate = mktime(timeinfo);

    if(timeinfo->tm_mday != day)
    {
        NATIVE_ERROR("Invalid Date: Day is invalid");
    }

    args[-1] = DATETIME_VAL(parsedDate);

    return true;
}

//0=date, 1=interval, 2=number
bool dateaddNative(int argCount, Value* args)
{
    CHECK_DATE(0,"Argument 1 of dateadd() must be a date");
    CHECK_STRING(1,"Argument 2 of dateadd() must be a string");
    CHECK_NUM(2,"Argument 3 of dateadd() must be a number");

    struct tm * timeinfo = localtime(&AS_DATETIME(args[0]));
    
    ObjString* interval = AS_STRING(args[1]);
    int number = (int)AS_NUMBER(args[2]);

    if (compareStrings("day", 3, interval))
    {
        timeinfo->tm_mday += number;
    }
    else if (compareStrings("month", 5, interval))
    {
        timeinfo->tm_mon += number;
    }
    else if (compareStrings("year", 4, interval))
    {
        timeinfo->tm_year += number;
    }
    else if (compareStrings("hour", 4, interval))
    {
        timeinfo->tm_hour += number;
    }
    else if (compareStrings("min", 3, interval))
    {
        timeinfo->tm_min += number;
    }
    else if (compareStrings("sec", 3, interval))
    {
        timeinfo->tm_sec += number;
    }
    else
    {
        NATIVE_ERROR("Invalid Interval");
    }
    
    if (timeinfo->tm_year+1900 < 1970 || timeinfo->tm_year+1900 > 3000)
    {
        NATIVE_ERROR("Invalid Date: Year must be between 1970 and 3000");
    }

    time_t newDate = mktime(timeinfo);
    args[-1] = DATETIME_VAL(newDate);

    return true;
}