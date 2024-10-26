//
// Created by ali on 10/26/24.
//

#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum class TokenType {
    INTEGER,       // Integer literals
    IDENTIFIER,    // Variable names
    OPERATOR,      // +, -, *, /, etc.
    PARENTHESIS,   // ( and )
    KEYWORD,       // Keywords like 'if', 'while', etc.
    END_OF_FILE    // Indicates the end of input
};

class Token {
public:
    TokenType type;
    std::string value;

    Token(TokenType type, const std::string& value) : type(type), value(value) {}
};

#endif
