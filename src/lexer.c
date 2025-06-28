/**
 * @file lexer.c
 * @brief Lexer implementation for tokenizing source code.
 */

#define _POSIX_C_SOURCE 200809L
#include "../include/lexer.h"
#include "../include/token.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool is_at_end(const Lexer *lexer) {
    return lexer->current >= lexer->source_len;
}

static char advance(Lexer *lexer) {
    return lexer->source[lexer->current++];
}

static char peek(const Lexer *lexer) {
    return is_at_end(lexer) ? '\0' : lexer->source[lexer->current];
}

static void skip_whitespace(Lexer *lexer) {
    while (true) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            default:
                return;
        }
    }
}

/**
 * @brief Helper: Allocates and returns a lexeme string for a single character.
 * Returns NULL on allocation failure.
 */
static char *make_single_char_lexeme(char c) {
    char *lexeme = malloc(2);
    if (lexeme) {
        lexeme[0] = c;
        lexeme[1] = '\0';
    }
    return lexeme;
}

static Token identifier(Lexer *lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    size_t length = lexer->current - lexer->start;
    char *lexeme = malloc(length + 1);
    if (!lexeme) {
        return token_create_error(strdup("Out of memory"), lexer->line);
    }
    memcpy(lexeme, lexer->source + lexer->start, length);
    lexeme[length] = '\0';

    static struct {
        const char *keyword;
        TokenType type;
    } keywords[] = {
        {"fun", TOKEN_FUN},
        {"int", TOKEN_INT},
        {"return", TOKEN_RETURN},
        {"let", TOKEN_LET},
        {NULL, TOKEN_IDENTIFIER}
    };

    for (size_t i = 0; keywords[i].keyword; i++) {
        if (strcmp(lexeme, keywords[i].keyword) == 0) {
            return token_create(keywords[i].type, lexeme, lexer->line);
        }
    }
    return token_create(TOKEN_IDENTIFIER, lexeme, lexer->line);
}

static Token number(Lexer *lexer) {
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
    size_t length = lexer->current - lexer->start;
    char *lexeme = malloc(length + 1);
    if (!lexeme) {
        return token_create_error(strdup("Out of memory"), lexer->line);
    }
    memcpy(lexeme, lexer->source + lexer->start, length);
    lexeme[length] = '\0';

    char *end;
    int64_t value = strtoll(lexeme, &end, 10);
    if (*end != '\0') {
        size_t msg_len = 32 + length;
        char *error = malloc(msg_len);
        if (!error) {
            free(lexeme);
            return token_create_error(strdup("Out of memory"), lexer->line);
        }
        snprintf(error, msg_len, "Invalid integer literal '%s'", lexeme);
        free(lexeme);
        Token tok = token_create_error(error, lexer->line);
        free(error);
        return tok;
    }
    return token_create_int(value, lexeme, lexer->line);
}

Lexer lexer_create(const char *source) {
    Lexer lexer;
    lexer.source = source;
    lexer.source_len = strlen(source);
    lexer.start = 0;
    lexer.current = 0;
    lexer.line = 1;
    return lexer;
}

void token_stream_add(TokenStream *stream, Token token) {
    if (stream->count >= stream->capacity) {
        size_t new_capacity = stream->capacity ? stream->capacity * 2 : 16;
        Token *new_tokens = realloc(stream->tokens, new_capacity * sizeof(Token));
        if (!new_tokens) {
            fprintf(stderr, "Fatal error: Token stream allocation failed\n");
            exit(EXIT_FAILURE);
        }
        stream->tokens = new_tokens;
        stream->capacity = new_capacity;
    }
    stream->tokens[stream->count++] = token;
}

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) {
        return token_create(TOKEN_EOF, NULL, lexer->line);
    }

    const char c = advance(lexer);

    if (isdigit(c)) {
        return number(lexer);
    }

    if (isalpha(c) || c == '_') {
        return identifier(lexer);
    }

    switch (c) {
        case '(': return token_create(TOKEN_LPAREN, make_single_char_lexeme(c), lexer->line);
        case ')': return token_create(TOKEN_RPAREN, make_single_char_lexeme(c), lexer->line);
        case '{': return token_create(TOKEN_LBRACE, make_single_char_lexeme(c), lexer->line);
        case '}': return token_create(TOKEN_RBRACE, make_single_char_lexeme(c), lexer->line);
        case '<': return token_create(TOKEN_LANGLE, make_single_char_lexeme(c), lexer->line);
        case '>': return token_create(TOKEN_RANGLE, make_single_char_lexeme(c), lexer->line);
        case ':': return token_create(TOKEN_COLON, make_single_char_lexeme(c), lexer->line);
        case ',': return token_create(TOKEN_COMMA, make_single_char_lexeme(c), lexer->line);
        case ';': return token_create(TOKEN_SEMI, make_single_char_lexeme(c), lexer->line);
        case '=': return token_create(TOKEN_EQUAL, make_single_char_lexeme(c), lexer->line);
        case '+': return token_create(TOKEN_PLUS, make_single_char_lexeme(c), lexer->line);
        default: {
            char error_msg[32];
            snprintf(error_msg, sizeof(error_msg), "Unexpected character '%c'", c);
            char *err_str = strdup(error_msg);
            if (!err_str) {
                return token_create_error(strdup("Out of memory"), lexer->line);
            }
            return token_create_error(err_str, lexer->line);
        }
    }
}
