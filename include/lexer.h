/**
* @file lexer.h
 * @brief Lexer interface for BasicCodeCompiler
 */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stddef.h>

/**
 * @brief Dynamic array of tokens produced by the lexer.
 */
typedef struct TokenStream {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenStream;

/**
 * @brief Lexer state for tokenizing source code.
 */
typedef struct {
    const char *source;
    size_t source_len;
    size_t start;
    size_t current;
    int line;
} Lexer;

/**
 * @brief Initializes a Lexer with the given source code.
 * @param source Null-terminated source string.
 * @return Lexer instance.
 */
Lexer lexer_create(const char *source);

/**
 * @brief Retrieves the next token from the source.
 * @param lexer Pointer to Lexer instance.
 * @return Next token.
 */
Token lexer_next_token(Lexer *lexer);

/**
 * @brief Adds a token to a TokenStream, resizing if necessary.
 * @param stream Pointer to TokenStream.
 * @param token Token to add.
 */
void token_stream_add(TokenStream *stream, Token token);

#endif // LEXER_H
