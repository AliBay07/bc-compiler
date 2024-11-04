//
// Created by ali on 10/26/24.
//

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <map>
#include <string>
#include "../../include/node.h"

class Allocator {
public:
    static map<string, string> allocate(ParseNode &parseTree);
};

#endif

