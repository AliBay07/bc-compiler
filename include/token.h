//
// Created by ali on 10/26/24.
//

#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <utility>
#include <ostream>
#include <iostream>
#include <optional>

using namespace std;

enum class TokenType {
    INTEGER,           // Integer literals
    IDENTIFIER,        // Variable names
    OPERATOR,          // +, -, *, /, etc.
    PARENTHESIS_OPEN,  // (
    PARENTHESIS_CLOSE, // )
    KEYWORD,           // Keywords like 'if', 'while', etc.
    FUNCTION,          // Function keyword
    TYPE,              // Return type of functions (e.g., int)
    COMMA,             // Separator for parameters
    SEMICOLON,         // ;
    CURLY_OPEN,        // {
    CURLY_CLOSE,       // }
    END_OF_FILE        // Indicates the end of input
};

class Token {
public:
    TokenType type; // Token type
    optional<string> value; // Token value (e.g., function/variable names)
    optional<string> returnType; // For function return types
    optional<string> varType;    // For variable types (e.g., int, float)
    int line; // Line number of the token

    explicit Token(TokenType type, int line, optional<string> value = {}, optional<string> returnType = {},
                   optional<string> varType = {})
            : type(type), value(std::move(value)), returnType(std::move(returnType)),
              varType(std::move(varType)), line(line) {}

    void setType(TokenType t) { type = t; }
    void setValue(const optional<string>& val) { value = val; }
    void setReturnType(const optional<string>& retType) { returnType = retType; }
    void setVarType(const optional<string>& varT) { varType = varT; }

    friend ostream &operator<<(ostream &os, const Token &token) {
        os << "Token(Type: ";

        switch (token.type) {
            case TokenType::INTEGER: os << "INTEGER"; break;
            case TokenType::IDENTIFIER: os << "IDENTIFIER"; break;
            case TokenType::OPERATOR: os << "OPERATOR"; break;
            case TokenType::PARENTHESIS_OPEN: os << "PARENTHESIS_OPEN"; break;
            case TokenType::PARENTHESIS_CLOSE: os << "PARENTHESIS_CLOSE"; break;
            case TokenType::KEYWORD: os << "KEYWORD"; break;
            case TokenType::FUNCTION: os << "FUNCTION"; break;
            case TokenType::TYPE: os << "TYPE"; break;
            case TokenType::COMMA: os << "COMMA"; break;
            case TokenType::CURLY_OPEN: os << "CURLY_OPEN"; break;
            case TokenType::CURLY_CLOSE: os << "CURLY_CLOSE"; break;
            case TokenType::END_OF_FILE: os << "END_OF_FILE"; break;
            case TokenType::SEMICOLON: os << "SEMICOLON"; break;
        }

        os << ", Value: " << (token.value ? *token.value : "N/A");
        os << ", Line: " << token.line;

        if (token.returnType) os << ", ReturnType: " << *token.returnType;
        if (token.varType) os << ", VarType: " << *token.varType;

        os << ")";
        return os;
    }
};

#endif

