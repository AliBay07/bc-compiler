//
// Created by ali on 10/26/24.
//

#ifndef PARSER_H
#define PARSER_H

#include <utility>
#include <vector>
#include "../../include/node.h"
#include "../../include/token.h"

using namespace std;

class Parser {
public:
    ParseNode parse(const vector<Token>& tokens);
};

#endif

