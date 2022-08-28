#ifndef min_chunk_h
#define min_chunk_h

#include "common.h"

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
} Chunk;

void initChunk(Chunk* chunk);

#endif