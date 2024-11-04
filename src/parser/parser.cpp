//
// Created by ali on 10/26/24.
//

#include "parser.h"
#include <stdexcept>

int tempCount = 0;

ParseNode Parser::parse(const std::vector<Token>& tokens) {
    ParseNode root(NodeType::PROGRAM);
    size_t i = 0;

    while (i < tokens.size()) {
        if (tokens.at(i).type == TokenType::FUNCTION) {
            const auto& functionToken = tokens[i++];
            ParseNode functionNode(NodeType::FUNCTION_DECL);

            if (functionToken.value) functionNode.setValue(functionToken.value);
            if (functionToken.returnType) functionNode.setReturnType(functionToken.returnType);

            if (tokens.at(i).type == TokenType::PARENTHESIS_OPEN) {
                i++; // Skip '('

                // Parse parameters, to do later
                while (tokens.at(i).type != TokenType::PARENTHESIS_CLOSE) {
                    i++;
                }
                i++; // Skip ')'
            }

            if (tokens.at(i).type == TokenType::CURLY_OPEN) {
                i++; // Skip '{'

                while (tokens.at(i).type != TokenType::CURLY_CLOSE) {
                    if (tokens.at(i).type == TokenType::TYPE) {
                        const auto& variableType = tokens[i++];
                        const auto& variableToken = tokens[i++];
                        ParseNode varNode(NodeType::VARIABLE);
                        varNode.setValue(variableToken.value);
                        varNode.setVarType(variableType.value);

                        if (tokens.at(i).type == TokenType::OPERATOR && tokens.at(i).value.value() == "=") {
                            i++; // Skip '='
                            ParseNode exprNode = parseExpression(tokens, i);

                            if (tokens.at(i).type != TokenType::SEMICOLON) {
                                throw std::runtime_error("Error (line "  + std::to_string(tokens.at(i).line - 1) + "): Missing ';'.\n");
                            }

                            // Create the assignment node
                            ParseNode assignNode(NodeType::STATEMENT, "assignment");
                            assignNode.addChild(std::make_shared<ParseNode>(varNode));
                            assignNode.addChild(std::make_shared<ParseNode>(exprNode));
                            functionNode.addChild(std::make_shared<ParseNode>(assignNode));

                        }
                    } else if (tokens.at(i).type == TokenType::KEYWORD && tokens.at(i).value.value() == "return") {
                        i++; // Skip 'return'
                        ParseNode returnNode(NodeType::STATEMENT, "return");
                        returnNode.addChild(std::make_shared<ParseNode>(parseExpression(tokens, i)));
                        functionNode.addChild(std::make_shared<ParseNode>(returnNode));

                        if (tokens.at(i).type != TokenType::SEMICOLON) {
                            throw std::runtime_error("Error (line "  + std::to_string(tokens.at(i).line - 1) + "): Missing ';'.\n");
                        }
                    } else {
                        throw std::runtime_error("Error (line " + std::to_string(tokens.at(i).line) +
                        "): Syntax Error : " + tokens.at(i).value.value() + ".\n");
                    }
                    i++;
                }
                i++; // Skip '}'
                root.addChild(std::make_shared<ParseNode>(functionNode));
            }
        }
        i++;
    }

    printTree(root);
    return root;
}

ParseNode Parser::parseExpression(const std::vector<Token>& tokens, size_t& i) {
    ParseNode exprNode(NodeType::EXPRESSION);

    if (tokens.at(i).type == TokenType::IDENTIFIER) {
        exprNode.addChild(std::make_shared<ParseNode>(NodeType::VARIABLE, tokens[i++].value));
    } else if (tokens.at(i).type == TokenType::INTEGER) {
        exprNode.addChild(std::make_shared<ParseNode>(NodeType::LITERAL, tokens[i++].value));
    }

    std::vector<std::shared_ptr<ParseNode>> operands;
    std::vector<std::shared_ptr<ParseNode>> operators;

    while (i < tokens.size() && tokens.at(i).type == TokenType::OPERATOR) {
        auto operatorNode = std::make_shared<ParseNode>(NodeType::OPERATOR, tokens[i++].value);
        exprNode.addChild(operatorNode);

        if (tokens.at(i).type == TokenType::IDENTIFIER) {
            operands.push_back(std::make_shared<ParseNode>(NodeType::VARIABLE, tokens[i++].value));
        } else if (tokens.at(i).type == TokenType::INTEGER) {
            operands.push_back(std::make_shared<ParseNode>(NodeType::LITERAL, tokens[i++].value));
        }

        while (operands.size() > 2) {
            std::string intermediateName = "temp" + std::to_string(tempCount++);
            ParseNode intermediateNode(NodeType::VARIABLE, intermediateName);
            intermediateNode.setVarType("int");

            ParseNode assignNode(NodeType::STATEMENT, "assignment");
            assignNode.addChild(std::make_shared<ParseNode>(intermediateNode));

            ParseNode newExpr(NodeType::EXPRESSION);
            newExpr.addChild(operators[0]);
            newExpr.addChild(operands[0]);
            newExpr.addChild(operands[1]);

            assignNode.addChild(std::make_shared<ParseNode>(newExpr));
            exprNode.addChild(std::make_shared<ParseNode>(assignNode));

            operands.erase(operands.begin(), operands.begin() + 2);
            operators.erase(operators.begin());

            operands.insert(operands.begin(), std::make_shared<ParseNode>(intermediateNode));
        }
    }

    if (!operands.empty()) {
        exprNode.addChild(operands[0]);
    }

    return exprNode;
}

void Parser::printTree(const ParseNode& root) {
    cout << "\n";
    cout << root;
}
