/**
* @file parser.h
 * @brief Parser interface for BasicCodeCompiler
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "token.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief AST node types enumeration
 */
typedef enum {
  NODE_COMPILATION_UNIT,
  NODE_FUNCTION,
  NODE_FUNCTION_CALL,
  NODE_VAR_DECL,
  NODE_RETURN,
  NODE_EXPRESSION,
  NODE_ADD,
  NODE_TYPE_PARAM,
  NODE_INT_LITERAL,
  NODE_VAR_INT_TYPE,
  NODE_RETURN_INT_TYPE,
  NODE_IDENTIFIER,
  NODE_ASSIGNMENT
} NodeType;

/**
 * @brief AST node structure representing syntax tree nodes
 */
typedef struct ASTNode {
  NodeType type;
  Token token;
  struct ASTNode **children;
  size_t child_count;
  int register_assigned;  // Assigned register index or -1 if none
  int source_register; // Source register for the value (if applicable)
  int scope_depth;        // Optional: scope depth for future use
  bool requires_load;  // Load from stack into register before use
  bool requires_store; // Store to stack from register after assignment
  int stack_slot;      // If spilled, where in the stack it lives
} ASTNode;

/**
 * @brief Parser state structure
 */
typedef struct {
  TokenStream *tokens;
  size_t current;
  size_t error_count;
  ASTNode *ast_root;
} Parser;

/**
 * @brief Initialize a parser from a token stream.
 * @param tokens Token stream produced by the lexer.
 * @return Initialized Parser instance.
 */
Parser parser_create(TokenStream *tokens);

/**
 * @brief Free resources associated with the parser.
 * @param parser Pointer to parser instance.
 */
void parser_cleanup(Parser *parser);

/**
 * @brief Parse tokens into an AST.
 * @param parser Parser instance.
 * @return Number of syntax errors found (0 if successful).
 */
int parse(Parser *parser);

/**
 * @brief Print the AST for debugging.
 * @param node AST node to print.
 * @param depth Current indentation level.
 */
void print_ast(const ASTNode *node, int depth);

/**
 * @brief Recursively free an AST node and its children.
 * @param node AST node to free.
 */
void free_ast(ASTNode *node);

#endif // PARSER_H
