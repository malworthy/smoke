#include "common.h"
#include "chunk.h"

int main (int argc, const char* argv[])
{
    Chunk chunk;

    initChunk(&chunk);

    char *x = "hello, world\n";
    printf(x);

    return 0;
}