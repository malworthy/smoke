#include "common.h"
#include "table.h"
#include "value.h"
#include "object.h"

int main(int argc, const char* argv[])
{
    Table table;
    Value val = NUMBER_VAL(123);
    ObjString string;
    string.chars = "hello";
    string.hash = 1;
    string.length =6;
    Value result;
    initTable(&table);
    tableSet(&table, &string, val);
    tableGet(&table, &string, &result);

    double number = AS_NUMBER(result);
    freeTable(&table);
    if (number == 123) return 0;

    return 1;
}