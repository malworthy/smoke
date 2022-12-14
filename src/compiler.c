#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "memory.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn) (bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool constant;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
    TYPE_METHOD,
    TYPE_INITIALIZER,
    TYPE_ANON
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;
Chunk* compilingChunk;
char* currentFilename;

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void statement();
static void declaration();
static uint8_t argumentList(); 
static void function(FunctionType type);
static uint16_t parseVariable(const char* errorMessage, bool isConst);
static void defineVariable(uint16_t global);
static void block();

static Chunk* currentChunk() 
{
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) 
{
    static char* lastMessage;
    if (parser.panicMode) return;

    if (parser.hadError && message == lastMessage) return;
    lastMessage = (char*) message;
    parser.panicMode = true;

    if ( currentFilename != NULL && currentFilename[0] != '\0')
        fprintf(stderr,"File '%s' ", currentFilename);

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void errorAtCurrent(const char* message) 
{
    errorAt(&parser.current, message);
}

static void error(const char* message) 
{
    errorAt(&parser.previous, message);
}

static void advance() 
{
    parser.previous = parser.current;

    for (;;) 
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static bool consume(TokenType type, const char* message) 
{
    if (parser.current.type == type) 
    {
        advance();
        return true;
    }
    if (type == TOKEN_SEMICOLON) return true;
    errorAtCurrent(message);

    return false;
}

static bool isReturnAtEndOfBlock()
{
    return (parser.previous.line < parser.current.line || parser.current.type == TOKEN_EOF 
        || parser.current.type == TOKEN_RIGHT_BRACE) && parser.previous.type == TOKEN_RETURN;
}

static bool check(TokenType type) 
{
    return parser.current.type == type;
}

static bool match(TokenType type) 
{
    if (!check(type)) return false;

    advance();

    return true;
}

static void emitByte(uint8_t byte) 
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) 
{
    emitByte(byte1);
    emitByte(byte2);
}

static void emitBytes16(uint8_t byte1, uint16_t byte2) 
{
    emitByte(byte1);
    emitByte((byte2 >> 8) & 0xff);
    emitByte(byte2 & 0xff);
}

static void emitLoop(int loopStart) 
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_T_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) 
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);

    return currentChunk()->count - 2;
}

static uint16_t makeConstant(Value value) 
{
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT16_T_MAX) 
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint16_t)constant;
}

static uint16_t emitConstant(Value value) 
{
    uint16_t constant = makeConstant(value);
    emitBytes16(OP_CONSTANT, constant);

    return constant;
}

static void emitReturn() 
{ 
    if (current->type == TYPE_INITIALIZER) 
    {
        emitBytes(OP_GET_LOCAL, 0);
    } 
    else 
    {
        //emitConstant(NUMBER_VAL(0));
        emitConstant(NIL_VAL);
    }
    emitByte(OP_RETURN);
}

static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_T_MAX) 
  {
        error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) 
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT && type != TYPE_ANON) 
    {
        current->function->name = copyStringRaw(parser.previous.start,
                                            parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    if (type != TYPE_FUNCTION && type != TYPE_ANON) 
    {
        local->name.start = "me";
        local->name.length = 2;
    } 
    else 
    {
        local->name.start = "";
        local->name.length = 0;
    }
    local->isCaptured = false;
    local->constant = false;
}

static ObjFunction* endCompiler() 
{
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) 
    {
        disassembleChunk(currentChunk(), function->name != NULL
        ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() 
{
    current->scopeDepth++;
}

static void endScope() 
{
    current->scopeDepth--;

    while (current->localCount > 0 &&
            current->locals[current->localCount - 1].depth >
            current->scopeDepth) 
    {
        if (current->locals[current->localCount - 1].isCaptured) 
        {
            emitByte(OP_CLOSE_UPVALUE);
        } 
        else 
        {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
    
}

static void binary(bool canAssign) 
{
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) 
    {
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        case TOKEN_PERCENT:       emitByte(OP_MOD); break;
        default: return; // Unreachable.
    }
}

static void call(bool canAssign) 
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static uint16_t identifierConstant(Token* name) 
{
    return makeConstant(OBJ_VAL(copyStringRaw(name->start,
                                           name->length)));
}

static void dot(bool canAssign) 
{
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint16_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) 
    {
        expression();
        emitBytes16(OP_SET_PROPERTY, name);
    } 
    else if (canAssign && match(TOKEN_PLUS_PLUS))
    {
        emitBytes16(OP_INC_PROPERTY, name);
    } 
    else if (canAssign && match(TOKEN_MINUS_MINUS))
    {
        emitBytes16(OP_DEC_PROPERTY, name);
    } 
    else if (canAssign && match(TOKEN_PLUS_EQUAL))
    {
        expression();
        emitBytes16(OP_ADD_PROPERTY, name);
    }
    else if (canAssign && match(TOKEN_MINUS_EQUAL))
    {
        expression();
        emitByte(OP_NEGATE);
        emitBytes16(OP_ADD_PROPERTY, name);
    }
    else if (match(TOKEN_LEFT_PAREN)) 
    {
        uint8_t argCount = argumentList();
        emitBytes16(OP_INVOKE, name);
        emitByte(argCount);
    }
    else 
    {
        emitBytes16(OP_GET_PROPERTY, name);
    }
}

static void literal(bool canAssign) 
{
    switch (parser.previous.type) 
    {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        default: return; // Unreachable.
    }
}

static void grouping(bool canAssign) 
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) 
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign) 
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void and_(bool canAssign) 
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void string(bool canAssign) 
{
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
}

static void rawString(bool canAssign) 
{
    emitConstant(OBJ_VAL(copyStringRaw(parser.previous.start + 3,
                                       parser.previous.length - 4)));
}

static void formatString(bool canAssign) 
{
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                    parser.previous.length - 1)));
}

static uint16_t stringConstant(char* name) 
{
    return makeConstant(OBJ_VAL(copyStringRaw(name,strlen(name))));
}

static bool identifiersEqual(Token* a, Token* b) 
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name, bool* isConst) 
{
    for (int i = compiler->localCount - 1; i >= 0; i--) 
    {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) 
        {
            if (local->depth == -1) 
                error("Can't read local variable in its own initializer.");
            *isConst = local->constant;
            return i;
        }
    }
    *isConst = false;
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index,
                      bool isLocal) 
{
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) 
    {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) 
            return i;
    }

    if (upvalueCount == UINT8_COUNT) 
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;

    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name, bool* isConst) 
{
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name, isConst);
    if (local != -1) 
    {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name, isConst);
    if (upvalue != -1) 
        return addUpvalue(compiler, (uint8_t)upvalue, false);

    return -1;
}

static void addLocal(Token name, bool isConst) 
{
    if (current->localCount == UINT8_COUNT) 
    {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->constant = isConst;
    local->isCaptured = false;
}

static void declareLoopVariable(char* variableName)
{
    Token name;
    
    name.length = 1;
    name.line = parser.previous.line;
    name.start = variableName;
    name.type = TOKEN_IDENTIFIER;

    addLocal(name, false);
}

static void declareVariable(bool isConst) 
{
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) 
    {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) 
        {
            break; 
        }

        if (identifiersEqual(name, &local->name)) 
        {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name, isConst);
}

static void namedVariable(Token name, bool canAssign) 
{
    uint8_t getOp, setOp, incOp, decOp, addOp;
    bool isConst;
    int arg = resolveLocal(current, &name, &isConst);
    if (arg != -1) 
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        incOp = OP_INC_LOCAL;
        decOp = OP_DEC_LOCAL;
        addOp = OP_ADD_LOCAL;
    } 
    else if ((arg = resolveUpvalue(current, &name, &isConst)) != -1) 
    {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
        incOp = OP_INC_UPVALUE;
        decOp = OP_DEC_UPVALUE;
        addOp = OP_ADD_UPVALUE;
    }
    else 
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;

        canAssign = false; //globals are immutable
    }

    if(isConst)
        canAssign = false;

    if (canAssign && match(TOKEN_EQUAL)) 
    {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else if (canAssign && match(TOKEN_PLUS_PLUS))
    {
        emitBytes(incOp, (uint8_t)arg);
    } 
    else if (canAssign && match(TOKEN_MINUS_MINUS))
    {
        emitBytes(decOp, (uint8_t)arg);
    }
    else if (canAssign && match(TOKEN_PLUS_EQUAL))
    {
        expression();
        emitBytes(addOp, (uint8_t)arg);
    } 
    else if (canAssign && match(TOKEN_MINUS_EQUAL))
    {
        expression();
        emitByte(OP_NEGATE);
        emitBytes(addOp, (uint8_t)arg);
    } 
    else 
    {
        if (getOp == OP_GET_GLOBAL)
            emitBytes16(getOp, (uint16_t)arg);
        else
            emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) 
{
    namedVariable(parser.previous, canAssign);
}

static void me(bool canAssign) 
{
    if (currentClass == NULL) 
    {
        error("Can't use 'me' outside of a class.");
        return;
    }

    variable(false);
}

static void unary(bool canAssign) 
{
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
     parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) 
    {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG:  emitByte(OP_NOT); break;
        default: return; // Unreachable.
    }
}

static void hashTable(bool canAssign)
{
    emitByte(OP_NEW_TABLE);
    do
    {
        // Stop if we hit the end of the list.
        if (check(TOKEN_RIGHT_BRACE)) 
        {
            consume(TOKEN_RIGHT_BRACE,"");
            return;
        }

        // parameter 2 - value adding to the list
        expression();
        consume(TOKEN_COLON, "Missing ':'");
        expression();
        emitByte(OP_TABLE_ADD);
    } while (match(TOKEN_COMMA));
    
    consume(TOKEN_RIGHT_BRACE,"Expect '}'");
}

static void list(bool canAssign)
{
    emitByte(OP_NEW_LIST);
 
    do
    {
        // Stop if we hit the end of the list.
        if (check(TOKEN_RIGHT_BRACKET)) 
        {
            consume(TOKEN_RIGHT_BRACKET,"");
            return;
        }

        // parameter 2 - value adding to the list
        expression();

        // .. so we're doing range
        if(match(TOKEN_DOT_DOT))
        {
            expression();
            emitByte(OP_RANGE);
        }
        else
        {
            emitByte(OP_LIST_ADD);
        }
    } while (match(TOKEN_COMMA));
    
    consume(TOKEN_RIGHT_BRACKET,"Expect ']'");
}

static void subscript(bool canAssign)
{
    if (match(TOKEN_GREATER_GREATER))
    {
        consume(TOKEN_RIGHT_BRACKET,"Expect ']'");
        emitByte(OP_POP_LIST);
    }
    else
    {
        // get the index of the array
        expression();

        if (!match(TOKEN_COLON))
        {
            consume(TOKEN_RIGHT_BRACKET,"Expect ']'");
            if (match(TOKEN_EQUAL))
            {
                expression();
                emitByte(OP_SUBSCRIPT_SET);
            }
            else if (match(TOKEN_PLUS_PLUS))
            {
                emitConstant(NUMBER_VAL(1));
                emitByte(OP_SUBSCRIPT_INC);
            }
            else if (match(TOKEN_MINUS_MINUS))
            {
                emitConstant(NUMBER_VAL(-1));
                emitByte(OP_SUBSCRIPT_INC);
            }
            else if (match(TOKEN_PLUS_EQUAL))
            {
                expression();
                emitByte(OP_SUBSCRIPT_ADD);
            }
            else if (match(TOKEN_MINUS_EQUAL))
            {
                expression();
                emitByte(OP_NEGATE);
                emitByte(OP_SUBSCRIPT_ADD);
            }
            else
            {
                emitByte(OP_SUBSCRIPT);
            }
            
        }
        else
        {
            expression();
            consume(TOKEN_RIGHT_BRACKET,"Expect ']'");
            emitByte(OP_SLICE);           
        }
    }
    
    
}

// Interpolation is syntactic sugar for calling "join()" on a list. So the
// string:
//
//     "a %{b + c} d"
//
// is compiled roughly like:
//
//     join(["a ", b + c, " d"])
// 
static void interpolation(bool canAssign)
{
    // Create a new list
    emitByte(OP_NEW_LIST);  

    do
    {
        // add string part
        string(false);
        emitByte(OP_LIST_ADD);

        // add interpolated part
        expression();
        // check for format string and format the expression
        if (match(TOKEN_FORMAT_STRING))
        {
            formatString(canAssign);
            emitByte(OP_FORMAT);
        }
        emitByte(OP_LIST_ADD);

    } while (match(TOKEN_INTERPOLATION));

    
    
    if (!match(TOKEN_STRING))
    {
        errorAtCurrent("string iterpolation error");
        return;
    }        
    
    // add final part of string
    string(canAssign);
    emitByte(OP_LIST_ADD);

    // now perform the join
    emitByte(OP_JOIN);
}

static void fn(bool canAssign)
{
    function(TYPE_ANON);
}

static void where(bool canAssign)
{
    Compiler compiler;
    initCompiler(&compiler, TYPE_ANON);
    beginScope(); 

    uint16_t constant = parseVariable("Expect lambda expression.", false);
    defineVariable(constant);
    current->function->arity = 1;

    if (match(TOKEN_ARROW))
    {
        expression();
        emitByte(OP_RETURN);
    }
    else
    {
        consume(TOKEN_LEFT_BRACE, "Expect '{' or '=>' before function body.");
        block();
    }

    ObjFunction* function = endCompiler();
    emitBytes16(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) 
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
    emitByte(OP_WHERE);
    emitBytes(OP_CALL, 2);
}

static void _select(bool canAssign)
{
    Compiler compiler;
    initCompiler(&compiler, TYPE_ANON);
    beginScope(); 

    uint16_t constant = parseVariable("Expect lambda expression.", false);
    defineVariable(constant);
    current->function->arity = 1;

    if (match(TOKEN_ARROW))
    {
        expression();
        emitByte(OP_RETURN);
    }
    else
    {
        consume(TOKEN_LEFT_BRACE, "Expect '{' or '=>' before function body.");
        block();
    }

    ObjFunction* function = endCompiler();
    emitBytes16(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) 
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
    emitByte(OP_SELECT);
    emitBytes(OP_CALL, 2);
}

static void addList(bool canAssign)
{
    //printf("parsed <<\n");
    expression();
    emitByte(OP_LIST_ADD);
}

ParseRule rules[] = 
{
    [TOKEN_INTERPOLATION] = {interpolation, NULL,   PREC_NONE},
    [TOKEN_LEFT_PAREN]    = {grouping, call,        PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {list,     subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL,     NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {hashTable,NULL,        PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,        PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_DOT]           = {NULL,     dot,         PREC_CALL},
    [TOKEN_MINUS]         = {unary,    binary,      PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary,      PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,        PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary,      PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary,      PREC_FACTOR},
    [TOKEN_PERCENT]       = {NULL,     binary,      PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,        PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary,      PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,      PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary,      PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary,      PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary,      PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary,      PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,        PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,        PREC_NONE},
    [TOKEN_RAW_STRING]    = {rawString,NULL,        PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,        PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,        PREC_AND},
    [TOKEN_THEN]          = {NULL,     NULL,        PREC_NONE},
    [TOKEN_DO]            = {NULL,     NULL,        PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,        PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,        PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,        PREC_NONE},
    [TOKEN_FN]            = {fn,       NULL,        PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,        PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,        PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,         PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,        PREC_NONE},
    //[TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ME]            = {me,       NULL,        PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,        PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,        PREC_NONE},
    [TOKEN_CONST]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,        PREC_NONE},
    [TOKEN_WHERE]         = {NULL,     where,       PREC_TERM},
    [TOKEN_SELECT]        = {NULL,     _select,     PREC_TERM},
    [TOKEN_LESS_LESS]   = {NULL,     addList,     PREC_TERM},
};

static void parsePrecedence(Precedence precedence) 
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    
    if (prefixRule == NULL) 
    {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) 
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) 
        error("Invalid assignment target.");
    
}

static ParseRule* getRule(TokenType type) 
{
    return &rules[type];
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() 
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
    {
        declaration();
    }
    if (!parser.hadError)
        consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void markInitialized() 
{
    if (current->scopeDepth == 0) return;

    current->locals[current->localCount - 1].depth =
      current->scopeDepth;
}

static void defineVariable(uint16_t global) 
{
    if (current->scopeDepth > 0) 
    {
        markInitialized();
        return;
    }
    
    emitBytes16(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() 
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) 
    {
        do {
            expression();
            if (argCount == 255) 
                error("Can't have more than 255 arguments.");
            
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static uint16_t parseVariable(const char* errorMessage, bool isConst) 
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable(isConst);
    if (current->scopeDepth > 0) return 0;

    // Mal's rule: global variables are immutable
    if (!isConst)
        errorAtCurrent("Global variables must be marked 'const'");

    return identifierConstant(&parser.previous);
}

static void function(FunctionType type) 
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope(); 

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if (!check(TOKEN_RIGHT_PAREN)) 
    {
        do 
        {
            current->function->arity++;
            if (current->function->arity > 255) 
                errorAtCurrent("Can't have more than 255 parameters.");
            
            uint16_t constant = parseVariable("Expect parameter name.", false);
            
            if (match(TOKEN_EQUAL)) 
            {
                current->function->optionals++;
                expression(); 
            }
            else if (current->function->optionals > 0)
            {
                error("Optional parameter expected");
            }

            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    if (match(TOKEN_ARROW))
    {
        expression();
        emitByte(OP_RETURN);
    }
    else
    {
        consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
        block();
    }

    ObjFunction* function = endCompiler();
    emitBytes16(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) 
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method() 
{
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint16_t constant = identifierConstant(&parser.previous);
    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) 
    {
        type = TYPE_INITIALIZER;
    } 
    function(type);
    emitBytes16(OP_METHOD, constant);
}

static void enumDeclaration()
{
    consume(TOKEN_IDENTIFIER, "Expect enum name.");
    
    Table dupeCheck;
    Token enumName = parser.previous;
    uint16_t nameConstant = identifierConstant(&parser.previous);
    Value dummyVal;

    declareVariable(false);
    emitBytes16(OP_ENUM, nameConstant);
    defineVariable(nameConstant);
    namedVariable(enumName, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before enum body.");
    initTable(&dupeCheck);
    
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
    {
        consume(TOKEN_IDENTIFIER, "Expect enum name.");
        uint16_t fieldConstant = identifierConstant(&parser.previous);
        ObjString* fieldName = copyStringRaw(parser.previous.start, parser.previous.length);
        if (tableGet(&dupeCheck, fieldName, &dummyVal))
        {
            error("Duplicate enum field");
            break;
        }
        tableSet(&dupeCheck, fieldName, NIL_VAL);

        if (match(TOKEN_EQUAL))
        {
            //printf("in equals\n");
            if (match(TOKEN_NUMBER)) 
            {
                //printf("matched number\n");
                number(false);
            }
            else if (match(TOKEN_STRING))
            {
                //printf("string\n");
                string(false);
            }
            else
            {
                errorAtCurrent("enum value must be a number or a string");
                return;
            }
            emitBytes16(OP_ENUM_FIELD_SET, fieldConstant);
        }
        else
        {
            emitBytes16(OP_ENUM_FIELD, fieldConstant);
        }
        
        if (check(TOKEN_RIGHT_BRACE))
            break;
        if (!consume(TOKEN_COMMA, "Expect ',' after enum field"))
            break;
    }
    freeTable(&dupeCheck);
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after enum body.");
}

static void classDeclaration() 
{
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint16_t nameConstant = identifierConstant(&parser.previous);
    declareVariable(false);

    emitBytes16(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
    {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    currentClass = currentClass->enclosing;
}

// modules are just classes you can't instantiate, and all methods are static
static void modDeclaration() 
{
    consume(TOKEN_IDENTIFIER, "Expect module name.");
    Token modName = parser.previous;
    uint16_t nameConstant = identifierConstant(&parser.previous);
    declareVariable(false);

    emitBytes16(OP_MODULE, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    namedVariable(modName, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before mod body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) 
    {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after mod body.");
    emitByte(OP_POP);

    currentClass = currentClass->enclosing;
}

static void funDeclaration() 
{
    uint16_t global = parseVariable("Expect function name.", true);
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void loopVarDeclaration(char* name) 
{
    // this does what parse variable does
    uint16_t global = 0; 
    declareLoopVariable(name);

    defineVariable(global);
}

static void varDeclaration(bool isConst) 
{
    uint16_t global = parseVariable("Expect variable name.", isConst);

    if (match(TOKEN_EQUAL)) 
        expression();
    else 
        errorAtCurrent("All variables must be initialised.");
    
    consume(TOKEN_SEMICOLON,
            "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void expressionStatement() 
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}
/*
for x in list

var i = 0;
var x = 0;
while i > len(list) do
{
    x = list[i]
} 
*/
static void forStatement()
{
    //for x in list
    beginScope();

    loopVarDeclaration("~counter");
    
    uint8_t counter = current->localCount - 1;

    loopVarDeclaration("~enumerable");
    uint8_t list = current->localCount - 1;
  
    uint16_t global = parseVariable("Expect variable name.", false);
    emitConstant(NUMBER_VAL(0));
    defineVariable(global);
    uint8_t var = current->localCount - 1;

    consume(TOKEN_IN,"missing in");

    expression();
    emitBytes(OP_SET_LOCAL, list);
    
    //loop starts here
    int loopStart = currentChunk()->count;

    // jump over
    emitBytes(OP_GET_LOCAL, counter);
    
    // ** Call to get length of list/string **
    // Get function name
    char* fnName = "len";
    int arg = stringConstant(fnName);
    emitBytes16(OP_GET_GLOBAL, (uint16_t)arg);

    // parameter 1 - the list or string we're getting the length of
    emitBytes(OP_GET_LOCAL, list);

    // call the function
    emitBytes(OP_CALL, 1);
    // end len

    emitByte(OP_LESS);     
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    emitBytes(OP_GET_LOCAL, list);
    emitBytes(OP_GET_LOCAL, counter);
    emitByte(OP_SUBSCRIPT);
    emitBytes(OP_SET_LOCAL, var);
    
    statement();

    //Now increase i by 1
    emitBytes(OP_GET_LOCAL, counter);
    emitConstant(NUMBER_VAL(1));
    emitByte(OP_ADD);
    emitBytes(OP_SET_LOCAL, counter); 
    emitByte(OP_POP);
    emitByte(OP_POP); 

    emitLoop(loopStart);

    patchJump(exitJump);
    endScope();
}

static void ifStatement() 
{
    expression();
    if(!check(TOKEN_LEFT_BRACE))
        consume(TOKEN_THEN, "Expect 'then' after condition."); 

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement();

    patchJump(elseJump);
}

static void printStatement() 
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement() 
{
    if (current->type == TYPE_SCRIPT) 
    {
        error("Can't return from top-level code.");
    }
    
    //if (match(TOKEN_SEMICOLON)) 
    if (isReturnAtEndOfBlock())
    {
        emitReturn();
    } 
    else 
    {
        if (current->type == TYPE_INITIALIZER) 
            error("Can't return a value from an initializer.");
    
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement() 
{
    int loopStart = currentChunk()->count;
    expression();
    if(!check(TOKEN_LEFT_BRACE))
        consume(TOKEN_DO, "Expect 'do' after while.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void synchronize() 
{
    //TODO: This function does not work.  until it does, we only report the one error

    //parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) 
    {
        //if (parser.previous.line != parser.current.line) return;
        //printf("In Syncronize\n");
        switch (parser.current.type) 
        {
            case TOKEN_CLASS:
            case TOKEN_MOD:
            case TOKEN_FN:
            case TOKEN_VAR:
            case TOKEN_CONST:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
}

static void declaration() 
{
    bool isConst = match(TOKEN_CONST);

    if (match(TOKEN_FN)) 
        funDeclaration();
    else if (isConst || match(TOKEN_VAR)) 
        varDeclaration(isConst);
    else if (match(TOKEN_CLASS))
        classDeclaration();
    else if (match(TOKEN_MOD))
        modDeclaration();
    else if (match(TOKEN_ENUM))
        enumDeclaration();
    else 
        statement();

    if (parser.panicMode) synchronize();
}

static void statement() 
{
    if (match(TOKEN_PRINT)) 
        printStatement();
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else if (match(TOKEN_IF)) 
        ifStatement();
    else if (match(TOKEN_RETURN)) 
        returnStatement();
    else if (match(TOKEN_WHILE)) 
        whileStatement();
    else if (match(TOKEN_FOR))
        forStatement();
    else
        expressionStatement();
}

ObjFunction* compile(const char* source, char* filename)
{
    currentFilename = filename;
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while (!match(TOKEN_EOF)) 
    {
        declaration();
    }
    
    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() 
{
    Compiler* compiler = current;
    while (compiler != NULL) 
    {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
