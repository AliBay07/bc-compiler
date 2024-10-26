//
// Created by ali on 10/26/24.
//

#ifndef GENERATE_H
#define GENERATE_H

#include "../../include/node.h"
#include <map>
#include <string>

class CodeGenerator {
public:
    std::string generate(const ParseNode& parseTree, const std::map<std::string, std::string>& allocations);
};

#endif


