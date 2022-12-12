#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "object.h"

void disassembleChunk(Chunk* chunk, const char* name) 
{
    printf("== %s ==\n", name);
    //printf("Length of chunk %d\n", chunk->count);
    for (int offset = 0; offset < chunk->count;)
    {
        //printf("In loop. offset is %d\n", offset);
        offset = disassembleInstruction(chunk, offset);
    }
    
}

static int simpleInstruction(const char* name, int offset) 
{
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk,
                           int offset) 
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2; 
}

static int jumpInstruction(const char* name, int sign,
                           Chunk* chunk, int offset) 
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);

    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
            offset + 3 + sign * jump);

    return offset + 3;
}

static int constantInstruction(const char* name, Chunk* chunk,
                               int offset) 
{
    uint16_t constant = (uint16_t)((chunk->code[offset+1] << 8) | chunk->code[offset+2]);
    //uint16_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int invokeInstruction(const char* name, Chunk* chunk,
                                int offset) 
{
    uint16_t constant = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
    uint8_t argCount = chunk->code[offset + 3];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

int disassembleInstruction(Chunk* chunk, int offset) 
{
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) 
    {
        printf("   | ");
    } 
    else 
    {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];

    switch (instruction) 
    {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SUBSCRIPT:
            return simpleInstruction("OP_SUBSCRIPT", offset);
        case OP_LIST_ADD:
            return simpleInstruction("OP_LIST_ADD", offset);
        case OP_FORMAT:
            return simpleInstruction("OP_FORMAT", offset);
        case OP_RANGE:
            return simpleInstruction("OP_RANGE", offset);
        case OP_JOIN:
            return simpleInstruction("OP_JOIN", offset);
        case OP_POP_LIST:
            return simpleInstruction("OP_POP_LIST", offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_INC_LOCAL:
            return byteInstruction("OP_INC_LOCAL", chunk, offset);
        case OP_INC_UPVALUE:
            return byteInstruction("OP_INC_UPVALUE", chunk, offset);
        case OP_DEC_LOCAL:
            return byteInstruction("OP_DEC_LOCAL", chunk, offset);
        case OP_ADD_LOCAL:
            return byteInstruction("OP_ADD_LOCAL", chunk, offset);
        case OP_ADD_UPVALUE:
            return byteInstruction("OP_ADD_UPVALUE", chunk, offset);
        case OP_SUBSCRIPT_INC:
            return byteInstruction("OP_SUBSCRIPT_INC", chunk, offset);
        case OP_SUBSCRIPT_ADD:
            return byteInstruction("OP_SUBSCRIPT_ADD", chunk, offset);
        case OP_DEC_UPVALUE:
            return byteInstruction("OP_DEC_UPVALUE", chunk, offset);
        case OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_NEW_LIST:
            return simpleInstruction("OP_NEW_LIST", offset);
        case OP_NEW_TABLE:
            return simpleInstruction("OP_NEW_TABLE", offset);
        case OP_TABLE_ADD:
            return simpleInstruction("OP_TABLE_ADD", offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_CLASS:
            return constantInstruction("OP_CLASS", chunk, offset);
        case OP_MODULE:
            return constantInstruction("OP_MODULE", chunk, offset);
        case OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_INC_PROPERTY:
            return constantInstruction("OP_INC_PROPERTY", chunk, offset);
        case OP_ADD_PROPERTY:
            return constantInstruction("OP_ADD_PROPERTY", chunk, offset);
        case OP_DEC_PROPERTY:
            return constantInstruction("OP_DEC_PROPERTY", chunk, offset);
        case OP_METHOD:
            return constantInstruction("OP_METHOD", chunk, offset);
        case OP_INVOKE:
            return invokeInstruction("OP_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint16_t constant = (chunk->code[offset++] << 8) | chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");

            ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n",
                    offset - 2, isLocal ? "local" : "upvalue", index);
            }
            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_WHERE:
            return simpleInstruction("OP_WHERE", offset);
        case OP_ENUM:
            return constantInstruction("OP_ENUM", chunk, offset);
        case OP_ENUM_FIELD:
            return constantInstruction("OP_ENUM_FIELD", chunk, offset);
        case OP_SUBSCRIPT_SET:
            return simpleInstruction("OP_SUBSCRIPT_SET", offset);
        case OP_ENUM_FIELD_SET:
            return constantInstruction("OP_CONSTANT", chunk, offset);            
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

