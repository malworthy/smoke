#ifndef native_date_h
#define native_date_h

#define CHECK_STRING(argnum, msg) \
    if (!IS_STRING(args[argnum])) \
    { \
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg))); \
        return false; \
    }

#define CHECK_DATE(argnum, msg) \
    if (!IS_DATETIME(args[argnum])) \
    { \
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg))); \
        return false; \
    }

#define CHECK_NUM(argnum, msg) \
    if (!IS_NUMBER(args[argnum])) \
    { \
        args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg))); \
        return false; \
    }

#define NATIVE_ERROR(msg) \
    args[-1] = OBJ_VAL(copyStringRaw(msg, (int)strlen(msg))); \
    return false;

#endif