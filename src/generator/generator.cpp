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
            body += "\tpush ebp\n"; //Save the stack-frame base pointer (of the calling function).
            body += "\tmov %ebp, %esp\n"; //; Set the stack-frame base pointer to be the current location on the stack.
//            body += "\tsub %esp, N"; // The N immediate value is the number of bytes reserved on the stack for local use

            for (const auto& stmt : child->children) {
                if (stmt->type == NodeType::STATEMENT) {
                    body += "\t; Process statements here\n";
                }
            }

            body += "\tmov %esp, %ebp\n"; // Put the stack pointer back where it was when this function was called.
            body += "\tpop ebp\n"; // Restore the calling function's stack frame.
            body += "\tret\n\n"; // Return to the calling function.
        }
    }

    out << header << body;

    out.close();
}
