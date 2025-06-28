/**
 * @file parser.c
 * @brief Recursive descent parser implementation with AST generation for BasicCodeCompiler
 */

#include "../include/parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define CURRENT_TOKEN (parser->tokens->tokens[parser->current])
#define ADVANCE_TOKEN (parser->current++)

/* Forward declarations for recursive parsing */
static ASTNode *parse_expression(Parser *parser);

static ASTNode *parse_statement(Parser *parser);

/* Helper to check end of token stream */
static bool is_at_end(const Parser *parser) {
    return parser->current >= parser->tokens->count;
}

/* Match and consume token if it matches given type */
static bool match(Parser *parser, const TokenType type) {
    if (!is_at_end(parser) && CURRENT_TOKEN.type == type) {
        ADVANCE_TOKEN;
        return true;
    }
    return false;
}

/* Peek to check token type without consuming */
static bool peek(const Parser *parser, const TokenType type) {
    return !is_at_end(parser) && CURRENT_TOKEN.type == type;
}

/* Create a new AST node with token info */
static ASTNode *create_node(const NodeType type, const Token token) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed in create_node\n");
        exit(EXIT_FAILURE);
    }
    node->type = type;
    node->token = token;
    node->children = NULL;
    node->child_count = 0;
    node->register_assigned = -1;
    node->source_register = -1;
    node->scope_depth = 0;
    return node;
}

/* Add a child node to a parent node */
static void add_child_node(ASTNode *parent, ASTNode *child) {
    if (!child) return;
    ASTNode **new_children = realloc(parent->children, (parent->child_count + 1) * sizeof(ASTNode *));
    if (!new_children) {
        fprintf(stderr, "Memory allocation failed in add_child_node\n");
        exit(EXIT_FAILURE);
    }
    parent->children = new_children;
    parent->children[parent->child_count] = child;
    parent->child_count++;
}

/* Recursively free an AST node and its children */
void free_ast(ASTNode *node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++) {
        free_ast(node->children[i]);
    }
    free(node->children);
    free(node);
}

/* Report a syntax error and increment error count */
static void parse_error(Parser *parser, const char *message) {
    fprintf(stderr, "Syntax Error (Line %d): %s\n", CURRENT_TOKEN.line, message);
    parser->error_count++;
    exit(EXIT_FAILURE);
}

/* Parse a type: currently only 'int' supported */
static ASTNode *parse_type(Parser *parser) {
    if (CURRENT_TOKEN.type == TOKEN_INT) {
        ASTNode *type_node = create_node(NODE_VAR_INT_TYPE, CURRENT_TOKEN);
        ADVANCE_TOKEN;
        return type_node;
    }
    parse_error(parser, "Unknown type");
    return NULL;
}

/* Expect a token of a given type, error if not found */
static bool expect_token(Parser *parser, const TokenType type, const char *err_msg) {
    if (!is_at_end(parser) && CURRENT_TOKEN.type == type) {
        ADVANCE_TOKEN;
        return true;
    }
    parse_error(parser, err_msg);
    return false;
}

/* Parse generic type parameters after function name */
static void parse_generic_params(Parser *parser, ASTNode *parent) {
    if (!expect_token(parser, TOKEN_LANGLE, "Expected '<' after identifier"))
        return;

    while (CURRENT_TOKEN.type != TOKEN_RANGLE && !is_at_end(parser)) {
        if (!peek(parser, TOKEN_IDENTIFIER)) {
            parse_error(parser, "Expected type parameter name");
            break;
        }
        ASTNode *param_node = create_node(NODE_TYPE_PARAM, CURRENT_TOKEN);
        ADVANCE_TOKEN;

        if (!expect_token(parser, TOKEN_COLON, "Expected ':' after parameter name")) {
            free_ast(param_node);
            break;
        }

        add_child_node(parent, param_node);

        ASTNode *type_node = parse_type(parser);
        if (!type_node) {
            free_ast(param_node);
            break;
        }
        add_child_node(param_node, type_node);

        if (!match(parser, TOKEN_COMMA))
            break;
    }

    expect_token(parser, TOKEN_RANGLE, "Unclosed generic parameters");
}

/* Parse a variable declaration */
static ASTNode *parse_variable_decl(Parser *parser) {
    ASTNode *var_node = create_node(NODE_VAR_DECL, CURRENT_TOKEN);
    ADVANCE_TOKEN;

    if (!peek(parser, TOKEN_IDENTIFIER)) {
        parse_error(parser, "Expected variable name after 'let'");
        free_ast(var_node);
        return NULL;
    }
    ASTNode *name_node = create_node(NODE_IDENTIFIER, CURRENT_TOKEN);
    ADVANCE_TOKEN;
    add_child_node(var_node, name_node);

    if (!expect_token(parser, TOKEN_LANGLE, "Expected '<' after variable name")) {
        free_ast(var_node);
        return NULL;
    }
    ASTNode *type_node = parse_type(parser);
    if (!type_node) {
        free_ast(var_node);
        return NULL;
    }
    add_child_node(var_node, type_node);

    if (!expect_token(parser, TOKEN_RANGLE, "Expected '>' after type")) {
        free_ast(var_node);
        return NULL;
    }
    if (!expect_token(parser, TOKEN_EQUAL, "Expected '=' in declaration")) {
        free_ast(var_node);
        return NULL;
    }
    ASTNode *expr_node = parse_expression(parser);
    if (!expr_node) {
        free_ast(var_node);
        return NULL;
    }
    add_child_node(var_node, expr_node);

    if (!expect_token(parser, TOKEN_SEMI, "Expected ';' after declaration")) {
        free_ast(var_node);
        return NULL;
    }
    return var_node;
}

/* Parse the return type (currently only int supported) */
static ASTNode *parse_return_type(Parser *parser) {
    if (CURRENT_TOKEN.type == TOKEN_INT) {
        ASTNode *type_node = create_node(NODE_RETURN_INT_TYPE, CURRENT_TOKEN);
        ADVANCE_TOKEN;
        return type_node;
    }
    parse_error(parser, "Unknown return type");
    return NULL;
}

/* Parse a function definition */
static ASTNode *parse_function(Parser *parser) {
    const Token fun_token = CURRENT_TOKEN;
    ADVANCE_TOKEN;

    if (!peek(parser, TOKEN_IDENTIFIER)) {
        parse_error(parser, "Expected function name");
        return NULL;
    }

    ASTNode *func_node = create_node(NODE_FUNCTION, fun_token);
    ASTNode *name_node = create_node(NODE_IDENTIFIER, CURRENT_TOKEN);
    add_child_node(func_node, name_node);
    ADVANCE_TOKEN;

    if (CURRENT_TOKEN.type == TOKEN_LANGLE) {
        parse_generic_params(parser, func_node);
    }

    if (!expect_token(parser, TOKEN_LPAREN, "Expected '(' after function name")) {
        free_ast(func_node);
        return NULL;
    }

    if (!expect_token(parser, TOKEN_RPAREN, "Expected ')' after parameters")) {
        free_ast(func_node);
        return NULL;
    }

    if (match(parser, TOKEN_COLON)) {
        ASTNode *ret_type_node = parse_return_type(parser);
        if (!ret_type_node) {
            free_ast(func_node);
            return NULL;
        }
        add_child_node(func_node, ret_type_node);
    }

    if (!expect_token(parser, TOKEN_LBRACE, "Expected '{' to start function body")) {
        free_ast(func_node);
        return NULL;
    }

    while (CURRENT_TOKEN.type != TOKEN_RBRACE && !is_at_end(parser)) {
        ASTNode *stmt = parse_statement(parser);
        if (stmt) {
            add_child_node(func_node, stmt);
        }
    }

    expect_token(parser, TOKEN_RBRACE, "Unclosed function body");
    return func_node;
}

/* Parse primary expressions: integer literals, identifiers or function calls */
static ASTNode *parse_primary(Parser *parser) {
    if (peek(parser, TOKEN_INTEGER)) {
        ASTNode *int_node = create_node(NODE_INT_LITERAL, CURRENT_TOKEN);

        char *endptr;
        errno = 0;
        long int_val = strtol(CURRENT_TOKEN.lexeme, &endptr, 10);
        if (errno == ERANGE || *endptr != '\0') {
            parse_error(parser, "Invalid integer literal");
            free_ast(int_node);
            return NULL;
        }
        int_node->token.literal.int_value = (int) int_val;
        ADVANCE_TOKEN;
        return int_node;
    }

    if (peek(parser, TOKEN_IDENTIFIER)) {
        const Token id_token = CURRENT_TOKEN;
        ADVANCE_TOKEN;

        if (peek(parser, TOKEN_LPAREN)) {
            ASTNode *call_node = create_node(NODE_FUNCTION_CALL, id_token);
            ADVANCE_TOKEN; // consume '('

            if (!peek(parser, TOKEN_RPAREN)) {
                int arg_count = 0;
                do {
                    if (arg_count >= 4) {
                        parse_error(parser, "Function calls support up to 4 arguments");
                        free_ast(call_node);
                        return NULL;
                    }
                    ASTNode *arg = parse_expression(parser);
                    if (!arg) {
                        free_ast(call_node);
                        return NULL;
                    }
                    add_child_node(call_node, arg);
                    arg_count++;
                } while (match(parser, TOKEN_COMMA));
            }

            if (!expect_token(parser, TOKEN_RPAREN, "Expected ')' after function call arguments")) {
                free_ast(call_node);
                return NULL;
            }
            return call_node;
        }

        return create_node(NODE_IDENTIFIER, id_token);
    }

    parse_error(parser, "Expected an expression");
    return NULL;
}

/* Parse left-associative addition expressions */
static ASTNode *parse_expression(Parser *parser) {
    ASTNode *left = parse_primary(parser);

    while (peek(parser, TOKEN_PLUS)) {
        Token plus_token = CURRENT_TOKEN;
        ADVANCE_TOKEN;

        ASTNode *right = parse_primary(parser);
        ASTNode *add_node = create_node(NODE_ADD, plus_token);

        add_child_node(add_node, left);
        add_child_node(add_node, right);

        left = add_node;
    }

    return left;
}

/* Parse statements: variable declarations, return, or expression statements */
static ASTNode *parse_statement(Parser *parser) {
    if (peek(parser, TOKEN_LET)) {
        return parse_variable_decl(parser);
    }

    if (peek(parser, TOKEN_RETURN)) {
        ASTNode *return_node = create_node(NODE_RETURN, CURRENT_TOKEN);
        ADVANCE_TOKEN;

        ASTNode *expr = parse_expression(parser);
        if (expr) {
            add_child_node(return_node, expr);
        }

        if (!expect_token(parser, TOKEN_SEMI, "Expected ';' after return statement")) {
            free_ast(return_node);
            return NULL;
        }
        return return_node;
    }

    // Handle assignment: identifier = expression;
    if (peek(parser, TOKEN_IDENTIFIER)) {
        const Token id_token = CURRENT_TOKEN;
        ADVANCE_TOKEN;

        if (peek(parser, TOKEN_EQUAL)) {
            ADVANCE_TOKEN; // consume '='

            ASTNode *assign_node = create_node(NODE_ASSIGNMENT, id_token);
            ASTNode *lhs = create_node(NODE_IDENTIFIER, id_token);
            ASTNode *rhs = parse_expression(parser);
            if (!rhs) {
                free_ast(assign_node);
                return NULL;
            }

            add_child_node(assign_node, lhs);
            add_child_node(assign_node, rhs);

            if (!expect_token(parser, TOKEN_SEMI, "Expected ';' after assignment")) {
                free_ast(assign_node);
                return NULL;
            }

            return assign_node;
        }
        // Fallback: expression statement starting with identifier
        parser->current--; // Rewind
    }

    ASTNode *expr = parse_expression(parser);
    if (expr && expect_token(parser, TOKEN_SEMI, "Expected ';' after expression statement")) {
        ASTNode *expr_stmt = create_node(NODE_EXPRESSION, (Token){0});
        add_child_node(expr_stmt, expr);
        return expr_stmt;
    }

    parse_error(parser, "Unexpected statement");
    return NULL;
}


/* Pretty-print AST nodes recursively */
void print_ast(const ASTNode *node, const int depth) {
    const char *type_str = "Unknown";
    const char *val_str = node->token.lexeme ? node->token.lexeme : "";

    switch (node->type) {
        case NODE_COMPILATION_UNIT: type_str = "CompilationUnit";
            break;
        case NODE_FUNCTION: type_str = "\nFunction";
            break;
        case NODE_FUNCTION_CALL: type_str = "FunctionCall";
            break;
        case NODE_VAR_DECL: type_str = "VarDecl";
            break;
        case NODE_RETURN: type_str = "Return";
            break;
        case NODE_TYPE_PARAM: type_str = "TypeParam";
            break;
        case NODE_EXPRESSION: type_str = "Expression";
            break;
        case NODE_ADD: type_str = "Add";
            break;
        case NODE_INT_LITERAL: type_str = "IntLiteral";
            break;
        case NODE_VAR_INT_TYPE: type_str = "VarIntType";
            break;
        case NODE_RETURN_INT_TYPE: type_str = "ReturnIntType";
            break;
        case NODE_IDENTIFIER: type_str = "Identifier";
            break;
        case NODE_ASSIGNMENT: type_str = "Assignment";
            break;
        default: type_str = "Unknown";
            break;
    }

    printf("%*s%s", depth * 2, "", type_str);
    if (val_str[0]) printf(" (%s)", val_str);
    printf("\n");

    for (size_t i = 0; i < node->child_count; i++) {
        print_ast(node->children[i], depth + 1);
    }
}

/* Top-level parse function: expects one or more functions */
int parse(Parser *parser) {
    parser->ast_root = create_node(NODE_COMPILATION_UNIT, (Token){0});

    while (!is_at_end(parser)) {
        if (peek(parser, TOKEN_FUN)) {
            ASTNode *func = parse_function(parser);
            if (func) {
                add_child_node(parser->ast_root, func);
            }
        } else if (peek(parser, TOKEN_EOF)) {
            break;
        } else {
            parse_error(parser, "Top-level declaration must be a function");
            ADVANCE_TOKEN;
        }
    }

    return parser->error_count;
}

/* Initialize parser state */
Parser parser_create(TokenStream *tokens) {
    return (Parser){
        .tokens = tokens,
        .current = 0,
        .error_count = 0,
        .ast_root = NULL
    };
}

/* Cleanup parser AST */
void parser_cleanup(Parser *parser) {
    if (parser->ast_root) {
        free_ast(parser->ast_root);
        parser->ast_root = NULL;
    }
}
