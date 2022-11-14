#include "common.h"
#include "value.h"
#include "object.h"

#include <string.h>

// function to swap elements
void swap(Value *a, Value *b) {
Value t = *a;
*a = *b;
*b = t;
}

// true if <=
static bool compareValues(Value* a, Value* b)
{
    if (IS_NUMBER(*a) && IS_NUMBER(*b))
    {
        return AS_NUMBER(*a) <= AS_NUMBER(*b);
    }

    if (IS_STRING(*a) && IS_STRING(*b))
    {
        return strcmp(AS_CSTRING(*a), AS_CSTRING(*b)) <= 0;
    }

    return a->type <= b->type;
}

// function to find the partition position
static int partition(ValueArray* array, int low, int high) 
{
    Value pivot = array->values[high];
    int i = (low - 1);

    for (int j = low; j < high; j++) {
        if (compareValues(&array->values[j], &pivot)) {
        i++;
        swap(&array->values[i], &array->values[j]);
        }
    }

    swap(&array->values[i + 1], &array->values[high]);

    return (i + 1);
}

static void quickSort(ValueArray* array, int low, int high) 
{
    if (low < high) {
        
        int pi = partition(array, low, high);
        
        quickSort(array, low, pi - 1);
        quickSort(array, pi + 1, high);
    }
}

void sort(ValueArray* data) 
{
    int n = data->count; 

    quickSort(data, 0, n - 1);
}