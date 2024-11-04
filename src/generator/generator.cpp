//
// Created by ali on 10/26/24.
//

#include <fstream>
#include <iostream>
#include "generator.h"

using namespace std;

void CodeGenerator::generate(const ParseNode& parseTree, const map<string, string>& allocations, const string& outputFile) {
    ofstream out(outputFile);

    string header = ".section .text\n";
    header += ".global main\n\n";

    string body;

    for (const auto& child : parseTree.children) {
        if (child->type == NodeType::FUNCTION_DECL) {
            body += child->value.value() + ":\n";
            body += "\tpush ebp\n";
            body += "\tmov %ebp, %esp\n";

            for (const auto& stmt : child->children) {
                if (stmt->type == NodeType::STATEMENT) {
                    body += "\t; Process statements here\n";
                }
            }

            body += "\tmov %esp, %ebp\n";
            body += "\tpop ebp\n";
            body += "\tret\n\n";
        }
    }

    out << header << body;

    out.close();
}
