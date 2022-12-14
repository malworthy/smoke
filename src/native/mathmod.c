#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../vm.h"
#include "../memory.h"
#include "native.h"

#define MATH_FN(name, cname) \
bool name(int argCount, Value* args) { \
    CHECK_NUM(0, "Argument 1 must be a number"); \
    args[-1] = NUMBER_VAL(cname(AS_NUMBER(args[0]))); \
    return true; }

MATH_FN(atanNative, atan);
MATH_FN(cosNative, cos);
MATH_FN(sinNative, sin);
MATH_FN(tanNative, tan);
MATH_FN(expNative, exp);
MATH_FN(logNative, log);
MATH_FN(sqrtNative, sqrt); 
MATH_FN(floorNative, floor); 
MATH_FN(ceilNative, ceil); 

/*
bool atanNative(int argCount, Value* args)
{
    CHECK_NUM(0, "Argument 1 must be a number");

    args[-1] = NUMBER_VAL(atan(AS_NUMBER(args[0])));

    return true;
}*/

bool bitandNative(int argCount, Value* args)
{
    if (!(IS_BOOL(args[0]) || IS_NUMBER(args[0])))
    {
        NATIVE_ERROR("bitand expects a Number or Boolean as parameter 1");
    }    

    if (!(IS_BOOL(args[1]) || IS_NUMBER(args[1])))
    {
        NATIVE_ERROR("bitand expects a Number or Boolean as parameter 2");
    }
    int a;
    int b;

    if(IS_BOOL(args[0])) 
        a = (int)AS_BOOL(args[0]);
    else 
        a = (int)AS_NUMBER(args[0]);

    if(IS_BOOL(args[1])) 
        b = (int)AS_BOOL(args[1]);
    else 
        b = (int)AS_NUMBER(args[1]);

    int result = a & b;

    args[-1] = NUMBER_VAL((double)result);

    return true;
}

bool bitorNative(int argCount, Value* args)
{
    if (!(IS_BOOL(args[0]) || IS_NUMBER(args[0])))
    {
        NATIVE_ERROR("bitand expects a Number or Boolean as parameter 1");
    }    

    if (!(IS_BOOL(args[1]) || IS_NUMBER(args[1])))
    {
        NATIVE_ERROR("bitand expects a Number or Boolean as parameter 2");
    }
    int a;
    int b;

    if(IS_BOOL(args[0])) 
        a = (int)AS_BOOL(args[0]);
    else 
        a = (int)AS_NUMBER(args[0]);

    if(IS_BOOL(args[1])) 
        b = (int)AS_BOOL(args[1]);
    else 
        b = (int)AS_NUMBER(args[1]);

    int result = a | b;

    args[-1] = NUMBER_VAL((double)result);

    return true;
}