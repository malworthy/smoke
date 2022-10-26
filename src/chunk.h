#ifndef min_chunk_h
#define min_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MOD,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SUBSCRIPT,
    OP_SLICE,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_NEW_LIST,
    OP_LIST_ADD,
    OP_FORMAT,
    OP_RANGE,
    OP_JOIN,
    OP_RETURN,
    OP_INC_LOCAL,
    OP_INC_UPVALUE,
    OP_INC_PROPERTY,
    OP_DEC_LOCAL,
    OP_DEC_UPVALUE,
    OP_DEC_PROPERTY,
    OP_CLASS,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_METHOD,
    OP_INVOKE,
    OP_WHERE,
    OP_SELECT
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
    int* lines;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value value);

#endif
