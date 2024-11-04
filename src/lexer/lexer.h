//
// Created by ali on 10/26/24.
//

#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <utility>
#include <vector>
#include "../../include/token.h"

using namespace std;

class Lexer {
private:
    static void printTokens(const vector<Token>& tokens);
public:
    static vector<Token> lex(const string& code);
};

#endif

