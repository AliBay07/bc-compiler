//
// Created by ali on 10/26/24.
//
//
// Created by ali on 10/26/24.
//

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "allocator/allocator.h"
#include "generator/generator.h"

using namespace std;
namespace fs = std::filesystem;

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

    auto tokens = Lexer::lex(code);
    auto parseTree = Parser::parse(tokens);
    auto allocations = Allocator::allocate(parseTree);

    fs::path inputPath(filename);
    string filenameWithoutExt = inputPath.stem().string();
    string outputFile = (inputPath.parent_path() / (filenameWithoutExt + ".s")).string();

    CodeGenerator::generate(parseTree, allocations, outputFile);

    cout << "\nGenerated ARM Assembly Code in " << filenameWithoutExt + ".s" << endl;
    return 0;
}



