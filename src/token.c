/**
 * @file token.c
 * @brief Token implementation and utilities.
 */

#include "../include/token.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

/**
 * @brief Creates a generic token.
 */
Token token_create(const TokenType type, char *lexeme, const int line) {
    Token token;
    token.type = type;
    token.lexeme = lexeme;
    token.line = line;
    // No literal data for generic tokens.
    return token;
}

/**
 * @brief Creates an integer token with associated literal value.
 */
Token token_create_int(const int64_t value, char *lexeme, const int line) {
    Token token;
    token.type = TOKEN_INTEGER;
    token.lexeme = lexeme;
    token.line = line;
    token.literal.int_value = value;
    return token;
}

/**
 * @brief Creates an error token with an error message.
 */
Token token_create_error(char *error, const int line) {
    Token token;
    token.type = TOKEN_ERROR;
    token.lexeme = NULL;
    token.line = line;
    token.literal.error_message = error;
    return token;
}

/**
 * @brief Frees memory owned by the token.
 */
void token_cleanup(const Token *token) {
    if (!token) return;

    if (token->lexeme) {
        free(token->lexeme);
    }

    if (token->type == TOKEN_ERROR && token->literal.error_message) {
        free(token->literal.error_message);
    }
}

/**
 * @brief Returns string representation of token type.
 */
const char *token_type_to_string(const TokenType type) {
    switch (type) {
        case TOKEN_FUN:        return "FUN";
        case TOKEN_INT:        return "INT";
        case TOKEN_RETURN:     return "RETURN";
        case TOKEN_LET:        return "LET";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_INTEGER:    return "INTEGER";
        case TOKEN_IMPORT:     return "IMPORT";
        case TOKEN_LANGLE:     return "<";
        case TOKEN_RANGLE:     return ">";
        case TOKEN_LPAREN:     return "(";
        case TOKEN_RPAREN:     return ")";
        case TOKEN_LBRACE:     return "{";
        case TOKEN_RBRACE:     return "}";
        case TOKEN_COLON:      return ":";
        case TOKEN_COMMA:      return ",";
        case TOKEN_SEMI:       return ";";
        case TOKEN_EQUAL:      return "=";
        case TOKEN_PLUS:       return "+";
        case TOKEN_DOT:        return ".";
        case TOKEN_SLASH:      return "/";
        case TOKEN_EOF:        return "EOF";
        case TOKEN_ERROR:      return "ERROR";
        default:               return "UNKNOWN";
    }
}
