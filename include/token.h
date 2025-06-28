/**
 * @file token.h
 * @brief Token definitions and utilities for lexical analysis.
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

/**
 * @enum TokenType
 * @brief Enumeration of token types used in the lexer.
 */
typedef enum {
    // Keywords
    TOKEN_FUN,
    TOKEN_INT,
    TOKEN_RETURN,
    TOKEN_LET,

    // Identifiers and literals
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,

    // Punctuation and operators
    TOKEN_LANGLE,   // <
    TOKEN_RANGLE,   // >
    TOKEN_LPAREN,   // (
    TOKEN_RPAREN,   // )
    TOKEN_LBRACE,   // {
    TOKEN_RBRACE,   // }
    TOKEN_COLON,    // :
    TOKEN_COMMA,    // ,
    TOKEN_SEMI,     // ;
    TOKEN_EQUAL,    // =
    TOKEN_PLUS,     // +

    // Special tokens
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

/**
 * @struct Token
 * @brief Represents a lexical token with optional literal data.
 *
 * The `lexeme` string and `error_message` are owned by the token and
 * should be freed by `token_cleanup`.
 */
typedef struct {
    TokenType type;
    char *lexeme;   ///< Dynamically allocated string representing the token text; may be NULL.
    int line;       ///< Source code line number where the token appears.
    union {
        int64_t int_value;       ///< Integer value for TOKEN_INTEGER.
        char *error_message;     ///< Dynamically allocated error message for TOKEN_ERROR.
    } literal;
} Token;

/**
 * @brief Creates a generic token.
 * @param type Token type.
 * @param lexeme Dynamically allocated lexeme string, ownership transferred to token.
 * @param line Source line number.
 * @return Initialized Token struct.
 */
Token token_create(TokenType type, char *lexeme, int line);

/**
 * @brief Creates an integer token.
 * @param value Integer value.
 * @param lexeme Dynamically allocated lexeme string, ownership transferred.
 * @param line Source line number.
 * @return Initialized Token struct.
 */
Token token_create_int(int64_t value, char *lexeme, int line);

/**
 * @brief Creates an error token.
 * @param error Dynamically allocated error message string, ownership transferred.
 * @param line Source line number.
 * @return Initialized Token struct.
 */
Token token_create_error(char *error, int line);

/**
 * @brief Frees resources owned by a token.
 * @param token Pointer to the token to clean up.
 *
 * Frees `lexeme` and error message if present.
 */
void token_cleanup(const Token *token);

/**
 * @brief Returns a string representation of a token type.
 * @param type TokenType enum.
 * @return Constant string describing the token type.
 */
const char *token_type_to_string(TokenType type);

#endif // TOKEN_H
