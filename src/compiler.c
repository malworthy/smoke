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
    TYPE_SCRIPT
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

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void statement();
static void declaration();
static uint8_t argumentList(); 

static Chunk* currentChunk() 
{
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) 
{
    if (parser.panicMode) return;
    parser.panicMode = true;
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

static void consume(TokenType type, const char* message) 
{
    if (parser.current.type == type) 
    {
        advance();
        return;
    }

    errorAtCurrent(message);
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

static uint8_t makeConstant(Value value) 
{
    int constant = addConstant(currentChunk(), value);
    if (constant > 256 /* UINT8_MAX */) 
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static uint8_t emitConstant(Value value) 
{
    uint8_t constant = makeConstant(value);
    emitBytes(OP_CONSTANT, constant);

    return constant;
}

static void emitReturn() 
{
    emitConstant(NUMBER_VAL(0));
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

    if (type != TYPE_SCRIPT) 
    {
        current->function->name = copyStringRaw(parser.previous.start,
                                            parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
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
        default: return; // Unreachable.
    }
}

static void call(bool canAssign) 
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign) 
{
    switch (parser.previous.type) 
    {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
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

static uint8_t identifierConstant(Token* name) 
{
    return makeConstant(OBJ_VAL(copyStringRaw(name->start,
                                           name->length)));
}

static uint8_t stringConstant(char* name) 
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

// loop statement declares a variable called 'i' to use as a counter
static void declareLoopVariable()
{
    Token name;
    char* variableName = "i";
    
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
    uint8_t getOp, setOp;
    bool isConst;
    int arg = resolveLocal(current, &name, &isConst);
    if (arg != -1) 
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } 
    else if ((arg = resolveUpvalue(current, &name, &isConst)) != -1) 
    {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
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
    else 
    {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) 
{
    namedVariable(parser.previous, canAssign);
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

static void list(bool canAssign)
{
    //printf("In List\n");
    //TODO: Create a new list here
    ObjList* list = newList();
    uint8_t listVariable = emitConstant(OBJ_VAL(list));
    do
    {
        // Stop if we hit the end of the list.
        if (check(TOKEN_RIGHT_BRACKET)) 
        {
            //printf("consuming ]\n");
            consume(TOKEN_RIGHT_BRACKET,"");
            return;
        }

        // Get function name
        char* fnName = "add";
        int arg = stringConstant(fnName);
		emitBytes(OP_GET_GLOBAL, (uint8_t)arg);

        // parameter 1 - list variable
        //emitBytes(OP_GET_GLOBAL, listVariable);
        emitConstant(OBJ_VAL(list));

        // parameter 2 - value adding to the list
        expression();

        // call the function
        emitBytes(OP_CALL, 2);
        emitByte(OP_POP);

        //printf("add element\n");
    
    } while (match(TOKEN_COMMA));

    consume(TOKEN_RIGHT_BRACKET,"Expect ']'");
}

static void subscript(bool canAssign)
{
    //printf("in subscript\n");
     
    // Get function name
    char* fnName = "get";
    int arg = stringConstant(fnName);
    emitBytes(OP_SUBSCRIPT, (uint8_t)arg); //hacky op code to insert into previous slot

    // get the index of the array
    expression();
    
    // call the function
    emitBytes(OP_CALL, 2);
    //emitByte(OP_POP);

    consume(TOKEN_RIGHT_BRACKET,"Expect ']'");
}

ParseRule rules[] = 
{
    [TOKEN_LEFT_PAREN]    = {grouping, call,        PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {list,     subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL,     NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,        PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,        PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,        PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary,      PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary,      PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,        PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary,      PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary,      PREC_FACTOR},
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
    [TOKEN_NUMBER]        = {number,   NULL,        PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,        PREC_AND},
    [TOKEN_THEN]          = {NULL,     NULL,        PREC_NONE},
    [TOKEN_DO]            = {NULL,     NULL,        PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,        PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,        PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,        PREC_NONE},
    [TOKEN_FN]            = {NULL,     NULL,        PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,        PREC_NONE},
    //[TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,         PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,        PREC_NONE},
    //[TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ME]            = {NULL,     NULL,        PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,        PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,        PREC_NONE},
    [TOKEN_CONST]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,        PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,        PREC_NONE},
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

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void markInitialized() 
{
    if (current->scopeDepth == 0) return;

    current->locals[current->localCount - 1].depth =
      current->scopeDepth;
}

static void defineVariable(uint8_t global) 
{
    if (current->scopeDepth > 0) 
    {
        markInitialized();
        return;
    }
    
    emitBytes(OP_DEFINE_GLOBAL, global);
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

static uint8_t parseVariable(const char* errorMessage, bool isConst) 
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
            
            uint8_t constant = parseVariable("Expect parameter name.", false);
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) 
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void funDeclaration() 
{
    uint8_t global = parseVariable("Expect function name.", true);
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void loopVarDeclaration() 
{
    // this does what parse variable does
    uint8_t global = 0; 
    declareLoopVariable();

    defineVariable(global);
}

static void varDeclaration(bool isConst) 
{
    uint8_t global = parseVariable("Expect variable name.", isConst);

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

static void loopStatement()
{
    beginScope();

    //First declare a variable called i and set it to zero
    loopVarDeclaration();
    emitConstant(NUMBER_VAL(0));

    //loop starts here
    int loopStart = currentChunk()->count;

    //Then compare i to the expression after "loop"
    char* variableName = "i";
    Token name;
    name.length = 1;
    name.line = parser.previous.line;
    name.start = variableName;
    name.type = TOKEN_IDENTIFIER;

    bool isConst;
    int arg = resolveLocal(current, &name, &isConst);
    emitBytes(OP_GET_LOCAL,(uint8_t)arg);
    expression();

    emitByte(OP_LESS); 
    
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);    
    consume(TOKEN_TIMES, "Expect 'times' after loop.");
    
    statement();

    //Now increase i by 1
    emitBytes(OP_GET_LOCAL,(uint8_t)arg);
    emitConstant(NUMBER_VAL(1));
    emitByte(OP_ADD);
    emitBytes(OP_SET_LOCAL, (uint8_t)arg); 
    emitByte(OP_POP);
    emitLoop(loopStart);
    patchJump(exitJump);
    emitByte(OP_POP);
    
    endScope();
}

static void ifStatement() 
{
    //consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
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
    
    if (match(TOKEN_SEMICOLON)) 
    {
        emitReturn();
    } 
    else 
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement() 
{
    //consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    int loopStart = currentChunk()->count;
    expression();
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
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
        case TOKEN_THING:
        case TOKEN_FN:
        case TOKEN_VAR:
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
    else if (match(TOKEN_LOOP))
        loopStatement();
    else
        expressionStatement();
}

ObjFunction* compile(const char* source)
{
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
