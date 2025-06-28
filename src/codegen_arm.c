/**
 * @file codegen_arm.c
 * @brief ARM code generation backend for BasicCodeCompiler.
 */

#include "../include/codegen_arm.h"
#include <stdio.h>
#include <string.h>

#define MAX_LOCALS 128
#define MAX_PARAMS 4

/**
 * @struct LocalVar
 * @brief Structure to hold local variable name and stack offset.
 */
typedef struct {
    char name[64];  /**< Variable name */
    int offset;     /**< Stack offset in bytes */
} LocalVar;

/**
 * @brief Emit the global entry label for the program.
 */
static void emit_entry_point(void) {
    printf(".global main\n");
}

/**
 * @brief Assign a new stack offset for a variable, or return existing one if already assigned.
 * @param locals Array of local variables.
 * @param local_count Pointer to the count of local variables.
 * @param stack_size Pointer to current total stack size in bytes.
 * @param name Name of the variable.
 * @return Stack offset assigned to the variable.
 */
static int assign_stack_offset(LocalVar *locals, int *local_count, int *stack_size, const char *name) {
    for (int i = 0; i < *local_count; ++i) {
        if (strcmp(locals[i].name, name) == 0)
            return locals[i].offset;
    }
    int offset = (*local_count) * 4;
    strncpy(locals[*local_count].name, name, 63);
    locals[*local_count].offset = offset;
    (*local_count)++;
    *stack_size += 4;
    return offset;
}

/**
 * @brief Get stack offset for an existing local variable.
 * @param locals Array of local variables.
 * @param local_count Number of local variables.
 * @param name Variable name to look for.
 * @return Stack offset if found, -1 otherwise.
 */
static int get_stack_offset(const LocalVar *locals, int local_count, const char *name) {
    for (int i = 0; i < local_count; ++i) {
        if (strcmp(locals[i].name, name) == 0)
            return locals[i].offset;
    }
    return -1;  // fallback
}

/**
 * @brief Emit load instruction if node requires loading from stack (spilled variable).
 * @param node AST node that may require load.
 */
static void emit_load_if_needed(const ASTNode *node) {
    if (node->requires_load) {
        // Load from stack slot into the assigned register
        printf("    ldr r%d, [sp, #%d]\n", node->register_assigned, node->stack_slot * 4);
    }
}

/**
 * @brief Emit store instruction if node requires storing to stack (spilled variable).
 * @param node AST node that may require store.
 */
static void emit_store_if_needed(const ASTNode *node) {
    if (node->requires_store) {
        // Store from assigned register to stack slot
        printf("    str r%d, [sp, #%d]\n", node->register_assigned, node->stack_slot * 4);
    }
}

/**
 * @brief Emit ARM assembly code for an expression subtree.
 * @param node AST node representing the expression.
 * @param locals Array of local variables.
 * @param local_count Number of local variables.
 */
static void codegen_expr(const ASTNode *node, const LocalVar *locals, int local_count) {
    if (!node) return;

    switch (node->type) {
        case NODE_INT_LITERAL:
            // Only emit mov if a register is assigned (i.e., for variable initialization)
            if (node->register_assigned >= 0) {
                printf("    mov r%d, #%ld\n", node->register_assigned, node->token.literal.int_value);
            }
            break;

        case NODE_IDENTIFIER: {
            const int offset = get_stack_offset(locals, local_count, node->token.lexeme);
            emit_load_if_needed(node);
            (void)offset;
            break;
        }

        case NODE_ADD: {
            codegen_expr(node->children[0], locals, local_count);
            emit_load_if_needed(node->children[0]);
            codegen_expr(node->children[1], locals, local_count);
            emit_load_if_needed(node->children[1]);

            const int left = node->children[0]->register_assigned;
            const int right = node->children[1]->register_assigned;
            const int result = node->register_assigned;
            // Only emit add if all registers are valid
            if (result >= 0 && left >= 0 && right >= 0) {
                printf("    add r%d, r%d, r%d\n", result, left, right);
            }
            break;
        }

        case NODE_FUNCTION_CALL: {
            for (size_t i = 0; i < node->child_count; ++i) {
                codegen_expr(node->children[i], locals, local_count);
                emit_load_if_needed(node->children[i]);
                if (node->children[i]->register_assigned != (int)i && node->children[i]->register_assigned >= 0) {
                    printf("    mov r%zu, r%d\n", i, node->children[i]->register_assigned);
                }
            }
            printf("    bl %s\n", node->token.lexeme);
            if (node->register_assigned != 0 && node->register_assigned >= 0) {
                printf("    mov r%d, r0\n", node->register_assigned);
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Emit ARM assembly code for a statement node.
 * @param node AST node representing the statement.
 * @param locals Array of local variables.
 * @param local_count Pointer to number of local variables.
 * @param stack_size Pointer to current total stack size in bytes.
 */
static void codegen_stmt(const ASTNode *node, LocalVar *locals, const int *local_count, int *stack_size) {
    if (!node) return;

    switch (node->type) {
        case NODE_VAR_DECL: {
            const ASTNode *expr = node->children[2];

            codegen_expr(expr, locals, *local_count);

            // Store variable value into stack if spilled
            emit_store_if_needed(node);

            break;
        }
        case NODE_RETURN: {
            const ASTNode *expr = node->children[0];
            codegen_expr(expr, locals, *local_count);
            emit_load_if_needed(expr);
            if (expr->type == NODE_INT_LITERAL) {
                printf("    mov r0, #%ld\n", expr->token.literal.int_value);
            } else if (expr->register_assigned >= 0) {
                printf("    mov r0, r%d\n", expr->register_assigned);
            }
            break;
        }
        case NODE_EXPRESSION:
            codegen_expr(node->children[0], locals, *local_count);
            emit_load_if_needed(node->children[0]);
            break;

        default:
            break;
    }
}

/**
 * @brief Emit ARM assembly code for a function including prologue, body, and epilogue.
 * @param node AST node representing the function.
 */
static void codegen_function(const ASTNode *node) {
    if (!node || node->type != NODE_FUNCTION) return;

    LocalVar locals[MAX_LOCALS];
    int local_count = 0;
    int stack_size = 0;
    int param_count = 0;

    // Collect parameters first (and assign stack offsets)
    for (size_t i = 0; i < node->child_count; i++) {
        const ASTNode *child = node->children[i];
        if (child->type == NODE_TYPE_PARAM && param_count < MAX_PARAMS) {
            assign_stack_offset(locals, &local_count, &stack_size, child->token.lexeme);
            param_count++;
        }
    }

    // Pre-assign space for declared variables
    for (size_t i = 0; i < node->child_count; i++) {
        const ASTNode *child = node->children[i];
        if (child->type == NODE_VAR_DECL) {
            const ASTNode *name = child->children[0];
            assign_stack_offset(locals, &local_count, &stack_size, name->token.lexeme);
        }
    }

    // Function label and prologue
    const char *func_name = node->children[0]->token.lexeme;
    printf("\n%s:\n", func_name);
    printf("    push {lr}\n");
    if (stack_size > 0)
        printf("    sub sp, sp, #%d\n", stack_size);

    // Store parameters into stack slots
    for (int i = 0; i < param_count; ++i) {
        const int offset = locals[i].offset;
        printf("    str r%d, [sp, #%d]\n", i, offset);
    }

    // Generate statements
    for (size_t i = 0; i < node->child_count; i++) {
        const ASTNode *child = node->children[i];
        if (child->type == NODE_VAR_DECL ||
            child->type == NODE_RETURN ||
            child->type == NODE_EXPRESSION) {
            codegen_stmt(child, locals, &local_count, &stack_size);
        }
    }

    // Function epilogue
    if (stack_size > 0)
        printf("    add sp, sp, #%d\n", stack_size);
    printf("    pop {pc}\n");
}

/**
 * @brief Generate ARM assembly code for the full program from the root AST node.
 * @param root Root node of the AST representing the program.
 */
void codegen_arm(const ASTNode *root) {
    if (!root) return;

    emit_entry_point();

    if (root->type == NODE_COMPILATION_UNIT) {
        for (size_t i = 0; i < root->child_count; i++) {
            codegen_function(root->children[i]);
        }
    }
}
