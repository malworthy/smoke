#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
  const char* start;
  const char* current;
  int line;
  int interpolation;
  int sqlParam;
} Scanner;

Scanner scanner;

void initScanner(const char* source) 
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.interpolation = 0;
    scanner.sqlParam = 0;
}

static bool isAtEnd() 
{
    return *scanner.current == '\0';
}

static Token makeToken(TokenType type) 
{
    Token token;
    
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;

    return token;
}

static Token errorToken(const char* message) 
{
    Token token;

    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    
    return token;
}

static char advance() 
{
    scanner.current++;
    return scanner.current[-1];
}

static char peek() 
{
    return *scanner.current;
}

static char peekPrev() 
{
    if (scanner.start == scanner.current) return peek();

    return scanner.current[-1];
}

static bool match(char expected) 
{
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static bool matchRawString()
{
    if (isAtEnd()) return false;
    if (*scanner.current == '"' && scanner.current[1] == '"')
    {
        scanner.current += 2;
        return true;
    }
    return false;
}

static bool checkRawString()
{
    if (isAtEnd()) return false;
    // note need to handle 4 or more quotes ("""") at end of a string, that's why i'm doing the last check...
    return (*scanner.current == '"' && scanner.current[1] == '"' && scanner.current[2] == '"' && scanner.current[3] != '"'); 
}

static char peekNext() 
{
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static void skipWhitespace() 
{
    for (;;) 
    {
        char c = peek();
        switch (c) 
        {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') 
                {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) advance();
                } 
                else 
                {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(
    int start, 
    int length,
    const char* rest, 
    TokenType type) 
{
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) 
        return type;
  
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() 
{
    switch (scanner.start[0]) 
    {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'o': return checkKeyword(2, 3, "nst", TOKEN_CONST);
                    case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
                }
            }
            break;
         
        case 'd': return checkKeyword(1, 1, "o", TOKEN_DO);
        //case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'e':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'n': return checkKeyword(2, 2, "um", TOKEN_ENUM);
                    case 'l': return checkKeyword(2, 2, "se", TOKEN_ELSE);
                }
            }
            break;

        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'n': return checkKeyword(1, 1, "n", TOKEN_FN);
                }
            }
            break;
        case 'i':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'f': return checkKeyword(1, 1, "f", TOKEN_IF);
                    case 'n': return checkKeyword(1, 1, "n", TOKEN_IN);
                }
            }
            break;
        case 'm': //return checkKeyword(1, 2, "e", TOKEN_ME);
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return checkKeyword(1, 1, "e", TOKEN_ME);
                    case 'o': return checkKeyword(1, 2, "od", TOKEN_MOD);
                }
            }
            break;
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 5, "elect", TOKEN_SELECT);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkKeyword(2, 2, "en", TOKEN_THEN);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': //return checkKeyword(1, 4, "hile", TOKEN_WHILE);
            if (scanner.current - scanner.start > 2 && scanner.start[1] == 'h') {
                switch (scanner.start[2]) {
                    case 'i': return checkKeyword(3, 2, "le", TOKEN_WHILE);
                    case 'e': return checkKeyword(3, 2, "re", TOKEN_WHERE);
                }
            }
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token embeddedSql()
{
    bool escaped = false;
    while ((peek() != '"' || escaped) && !isAtEnd()) 
    {
        if (peek() == '\n') scanner.line++;
        escaped = (peek() == '\\' && !escaped);

        if (peek() == ':' && peekPrev() != '\\' && peekNext() == '{')
        {
            //if (peekNext() != '{') return errorToken("Expect '{' after ':' for parameters in SQL.");
            advance();
            scanner.sqlParam++;
            Token t = makeToken(TOKEN_SQL_PARAM); 
            advance();
            
            return t;   
        }
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    
    return makeToken(TOKEN_SQL);
}

static Token string() 
{
    bool escaped = false;
    while ((peek() != '"' || escaped) && !isAtEnd()) 
    {
        if (peek() == '\n') scanner.line++;
        escaped = (peek() == '\\' && !escaped);
        // Interpolation %{hello}
        if (peek() == '%' && peekPrev() != '\\')
        {
            if (peekNext() != '{') return errorToken("Expect '{' after '%' in string.");
            advance();
            scanner.interpolation++;
            Token t = makeToken(TOKEN_INTERPOLATION); 
            advance();
            
            return t;   
        }
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    return makeToken(TOKEN_STRING);
}

static Token rawString() 
{
    while (!isAtEnd() && !checkRawString()) 
    {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated raw string.");

    // First closing quote.
    advance();
    Token result = makeToken(TOKEN_RAW_STRING);
    // Seconds and third closing quotes
    advance();
    advance();

    return result;
}


static Token formatString() 
{
    while (peek() != '}' && !isAtEnd()) 
    {
        advance();
    }

    if (isAtEnd()) return errorToken("Expect '}' after format string.");

    // The closing }.
    //advance();
    return makeToken(TOKEN_FORMAT_STRING);
}

static bool isDigit(char c) 
{
    return c >= '0' && c <= '9';
}

static Token number() 
{
    while (isDigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the ".".
        advance();

        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

static bool isAlpha(char c) 
{
      return (c >= 'a' && c <= 'z') ||
             (c >= 'A' && c <= 'Z') ||
              c == '_' || c == '@';
}

static Token identifier() 
{
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

Token scanToken() 
{
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) 
    {
        if(scanner.interpolation > 0)
        {
            scanner.interpolation = 0;
            return errorToken("String interpolation missing '}' at end.");
        }

        return makeToken(TOKEN_EOF);
    }

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) 
    {
        case '$': 
        {
            if (match('"'))
            {
                scanner.start++;
                return embeddedSql();
            }
            return makeToken(TOKEN_ERROR);
        }//return match('"') ? embeddedSql() : makeToken(TOKEN_ERROR);
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': 
            if (scanner.interpolation > 0)
            {
                scanner.interpolation--;
                return string();
            }
            if (scanner.sqlParam > 0)
            {
                if (scanner.sqlParam > 1) return makeToken(TOKEN_ERROR);
                scanner.sqlParam--;
                return embeddedSql();
            }
            return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': 
            return makeToken(match('.') ? TOKEN_DOT_DOT : TOKEN_DOT); 
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case '[': return makeToken(TOKEN_LEFT_BRACKET);
        case ']': return makeToken(TOKEN_RIGHT_BRACKET);
        case ':': return makeToken(TOKEN_COLON);
        case '|':
            if (scanner.interpolation > 0)
            {
                return formatString();
            }
            
        case '%': return makeToken(TOKEN_PERCENT);
        case '-': 
            return makeToken(match('-') ? TOKEN_MINUS_MINUS : (match('=') ? TOKEN_MINUS_EQUAL: TOKEN_MINUS));
        case '+': 
            return makeToken(match('+') ? TOKEN_PLUS_PLUS : (match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS));
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : (match('>') ? TOKEN_ARROW : TOKEN_EQUAL));
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : (match('<') ? TOKEN_LESS_LESS : TOKEN_LESS));
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : (match('>') ? TOKEN_GREATER_GREATER : TOKEN_GREATER));
        case '"':
            if(matchRawString())
                return rawString();
            else 
                return string();
    }

    return errorToken("Unexpected character.");
}