//
// Created by ali on 10/26/24.
//

#ifndef NODE_H
#define NODE_H

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <ostream>

using namespace std;

enum class NodeType {
    PROGRAM,        // Root node
    FUNCTION_DECL,  // Represents a function declaration
    PARAMETER,      // Function parameter
    STATEMENT,      // Represents statements like assignments
    EXPRESSION,     // Represents expressions
    OPERATOR,       // Operators like +, -, etc.
    LITERAL,        // Integer literals
    VARIABLE,       // Variable identifiers
    FREE,           // Represents freeing a register
    LOAD            // Represents loading a variable into a register
};

class ParseNode {
private:

    static ostream& printNode(ostream& os, const ParseNode& node, int depth) {
        os << string(depth, '\t');
        os << "NodeType: " << nodeTypeToString(node.type)
           << ", Value: " << (node.value ? *node.value : "N/A");

        if (node.returnType) os << ", ReturnType: " << *node.returnType;
        if (node.varType) os << ", VarType: " << *node.varType;
        if (node.address) os << ", Address: " << *node.address;

        os << "\n";

        for (const auto& child : node.children) {
            printNode(os, *child, depth + 1);
        }
        return os;
    }

    static string nodeTypeToString(NodeType type) {
        switch (type) {
            case NodeType::PROGRAM:
                return "PROGRAM";
            case NodeType::FUNCTION_DECL:
                return "FUNCTION_DECL";
            case NodeType::PARAMETER:
                return "PARAMETER";
            case NodeType::STATEMENT:
                return "STATEMENT";
            case NodeType::EXPRESSION:
                return "EXPRESSION";
            case NodeType::OPERATOR:
                return "OPERATOR";
            case NodeType::LITERAL:
                return "LITERAL";
            case NodeType::VARIABLE:
                return "VARIABLE";
            case NodeType::FREE:
                return "FREE";
            case NodeType::LOAD:
                return "LOAD";
            default:
                return "UNKNOWN";
        }
    }


public:
    NodeType type;
    optional<string> value;       // Token value (e.g., function/variable names)
    optional<string> returnType;  // Function return types
    optional<string> varType;     // Variable types (e.g., int, float)
    optional<string> address;     // Memory address for LOAD/FREE nodes
    vector<shared_ptr<ParseNode>> children;

    explicit ParseNode(NodeType type, const optional<string> &value = {},
                       const optional<string> &returnType = {}, const optional<string> &varType = {},
                       const optional<string> &address = {})
            : type(type), value(value), returnType(returnType), varType(varType), address(address) {}

    void addChild(const shared_ptr<ParseNode> &child) {
        children.push_back(child);
    }

    // Setters
    void setValue(const optional<string> &val) { value = val; }

    void setReturnType(const optional<string> &retType) { returnType = retType; }

    void setVarType(const optional<string> &varT) { varType = varT; }

    void setAddress(const optional<string> &addr) { address = addr; }

    friend ostream& operator<<(ostream& os, const ParseNode& node) {
        return printNode(os, node, 0);
    }
};

#endif