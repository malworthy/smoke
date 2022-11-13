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
static int partition(ValueArray* array, int low, int high) {
  
  // select the rightmost element as pivot
  Value pivot = array->values[high];
  
  // pointer for greater element
  int i = (low - 1);

  // traverse each element of the array
  // compare them with the pivot
  for (int j = low; j < high; j++) {
    if (compareValues(&array->values[j], &pivot)) {
        
      // if element smaller than pivot is found
      // swap it with the greater element pointed by i
      i++;
      
      // swap element at i with element at j
      swap(&array->values[i], &array->values[j]);
    }
  }

  // swap the pivot element with the greater element at i
  swap(&array->values[i + 1], &array->values[high]);
  
  // return the partition point
  return (i + 1);
}

static void quickSort(ValueArray* array, int low, int high) {
  if (low < high) {
    
    // find the pivot element such that
    // elements smaller than pivot are on left of pivot
    // elements greater than pivot are on right of pivot
    int pi = partition(array, low, high);
    
    // recursive call on the left of pivot
    quickSort(array, low, pi - 1);
    
    // recursive call on the right of pivot
    quickSort(array, pi + 1, high);
  }
}

void sort(ValueArray* data) {
  int n = data->count; 
  
  quickSort(data, 0, n - 1);

}