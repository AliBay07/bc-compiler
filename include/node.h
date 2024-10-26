//
// Created by ali on 10/26/24.
//

#ifndef NODE_H
#define NODE_H

#include <string>
#include <utility>
#include <vector>
#include <memory>

enum class NodeType {
    PROGRAM,       // Root node
    STATEMENT,     // Represents statements like assignments
    EXPRESSION,    // Represents expressions
    OPERATOR,      // Operators like +, -, etc.
    LITERAL,       // Integer literals
    VARIABLE       // Variable identifiers
};

class ParseNode {
public:
    NodeType type;
    std::string value;  // Holds the specific operator, literal, or variable name
    std::vector<std::shared_ptr<ParseNode>> children;

    explicit ParseNode(NodeType type, std::string  value = "") : type(type), value(std::move(value)) {}

    void addChild(const std::shared_ptr<ParseNode>& child) {
        children.push_back(child);
    }
};

#endif

