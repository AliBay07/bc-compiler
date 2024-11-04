//
// Created by ali on 10/26/24.
//

#include "allocator.h"
#include <unordered_map>
#include <list>
#include <algorithm>
#include <iostream>

using namespace std;

vector<string> registers = {"eax", "ebx", "ecx", "edx"};

map<string, string> Allocator::allocate(ParseNode& parseTree) {
    map<string, string> allocations;
    unordered_map<string, int> usage;
    list<string> freeRegisters(registers.begin(), registers.end());

    int time = 0;

    function<void(ParseNode&)> allocateRegisters = [&](ParseNode& node) {
        if (node.type == NodeType::VARIABLE) {
            const string& varName = node.value.value();

            if (allocations.find(varName) == allocations.end()) {
                if (!freeRegisters.empty()) {
                    allocations[varName] = freeRegisters.front();
                    freeRegisters.pop_front();
                } else {
                    string toFree = min_element(usage.begin(), usage.end(),
                                                [](const auto& a, const auto& b) {
                                                    return a.second < b.second;
                                                })->first;

                    string regToReuse = allocations[toFree];
                    allocations.erase(toFree);
                    allocations[varName] = regToReuse;

                    auto freeNode = make_shared<ParseNode>(NodeType::FREE, toFree);
                    freeNode->address = regToReuse;
                    node.addChild(freeNode);

                    auto loadNode = make_shared<ParseNode>(NodeType::LOAD, varName);
                    loadNode->address = regToReuse;
                    node.addChild(loadNode);
                }
            }

            usage[varName] = time++;
        }

        for (auto& child : node.children) {
            allocateRegisters(*child);
        }
    };

    allocateRegisters(parseTree);

    return allocations;
}
