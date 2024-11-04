//
// Created by ali on 10/26/24.
//
#include "lexer.h"
#include <cctype>
#include <stdexcept>

std::vector<Token> Lexer::lex(const std::string &code) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1;

    while (i < code.length()) {
        if (isdigit(code.at(i))) {
            std::string number;
            while (i < code.length() && isdigit(code.at(i))) {
                number += code[i++];
            }
            tokens.emplace_back(TokenType::INTEGER, line, number);
        } else if (isalpha(code.at(i))) {
            std::string identifier;
            while (i < code.length() && isalnum(code.at(i))) {
                identifier += code[i++];
            }

            if (identifier == "function") {
                Token funcToken(TokenType::FUNCTION, line);

                // Expect return type after 'function' keyword
                std::string returnType;
                while (i < code.length() && isspace(code[i])) i++; // Skip spaces
                while (i < code.length() && isalpha(code[i])) returnType += code[i++];

                // Expect function name after return type
                std::string functionName;
                while (i < code.length() && isspace(code[i])) i++; // Skip spaces
                while (i < code.length() && isalnum(code[i])) functionName += code[i++];

                funcToken.setReturnType(returnType);
                funcToken.setValue(functionName);

                tokens.emplace_back(funcToken);

            } else if (identifier == "return") {
                tokens.emplace_back(TokenType::KEYWORD, line, "return");
            } else if (identifier == "int" || identifier == "float" || identifier == "void") {
                tokens.emplace_back(TokenType::TYPE, line, identifier);
            } else if (tokens.back().type == TokenType::TYPE || tokens.back().type == TokenType::OPERATOR) {
                tokens.emplace_back(TokenType::IDENTIFIER, line, identifier);
            } else {
                throw std::runtime_error("Error (line "  + std::to_string(line) + "): Character '" + identifier + "' is unknown.\n");
            }
        } else if (code.at(i) == '+') {
            tokens.emplace_back(TokenType::OPERATOR, line, "+");
            i++;
        } else if (code.at(i) == '=') {
            tokens.emplace_back(TokenType::OPERATOR, line, "=");
            i++;
        } else if (code.at(i) == '{') {
            tokens.emplace_back(TokenType::CURLY_OPEN, line, "{");
            i++;
        } else if (code.at(i) == '}') {
            tokens.emplace_back(TokenType::CURLY_CLOSE, line, "}");
            i++;
        } else if (code.at(i) == '(') {
            tokens.emplace_back(TokenType::PARENTHESIS_OPEN, line, "(");
            i++;
        } else if (code.at(i) == ')') {
            tokens.emplace_back(TokenType::PARENTHESIS_CLOSE, line, ")");
            i++;
        } else if (code.at(i) == ';') {
            tokens.emplace_back(TokenType::SEMICOLON, line, ";");
            i++;
        } else if (code.at(i) == ',') {
            tokens.emplace_back(TokenType::COMMA, line, ",");
            i++;
        } else if (code.at(i) == '\n') {
            line++;
            i++;
        } else if (isspace(code.at(i))) {
            i++;
        } else {
            throw std::runtime_error("Error (line "  + std::to_string(line) +
            "): Character '" + std::string(1, code.at(i)) + "' is unknown.\n");
        }
    }

    tokens.emplace_back(TokenType::END_OF_FILE, line, "");

    printTokens(tokens);
    return tokens;
}

void Lexer::printTokens(const vector<Token>& tokens) {
    cout << "\n";
    for (const auto &token: tokens) {
        std::cout << token << "\n";
    }
}
