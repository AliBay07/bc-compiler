//
// Created by ali on 10/26/24.
//
#include <iostream>
#include <fstream>
#include <string>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "allocator/allocator.h"
#include "generator/generator.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: compiler <source_file.b>\n";
        return 1;
    }

    string filename = argv[1];
    if (filename.size() < 3 || filename.substr(filename.size() - 2) != ".b") {
        cerr << "Error: Source file must have a .b extension\n";
        return 1;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << "\n";
        return 1;
    }

    string code((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    Lexer lexer;
    Parser parser;
    Allocator allocator;
    CodeGenerator codeGen;

    auto tokens = lexer.lex(code);
    auto parseTree = parser.parse(tokens);
    auto allocations = allocator.allocate(parseTree);
    auto assemblyCode = codeGen.generate(parseTree, allocations);

    cout << "Compilation successful!\n";
    return 0;
}

