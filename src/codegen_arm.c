/**
 * @file codegen_arm.c
 * @brief ARM code generator for BasicCodeCompiler
 *
 * This file emits ARM assembly for the compiled program.
 * It assumes stack-based variable storage and register allocation
 * has already been performed.
 */

#include "../include/codegen_arm.h"
#include <stdio.h>

/**
 * @brief Emit the .text section directive.
 */
static void emit_text_section(void) {
    printf(".text\n");
}

/**
 * @brief Emit .global for each function name.
 *
 * @param root The AST root (NODE_COMPILATION_UNIT).
 */
static void emit_global_directives(const ASTNode *root) {
    if (!root || root->type != NODE_COMPILATION_UNIT) return;

    for (size_t i = 0; i < root->child_count; ++i) {
        const ASTNode *fn = root->children[i];
        if (fn && fn->type == NODE_FUNCTION) {
            const char *name = fn->children[0]->token.lexeme;
            printf(".global %s\n", name);
        }
    }
}

/**
 * @brief Emit a load instruction if the node is marked as requiring one
 *
 * @param node The AST node to load
 */
static void emit_load_if_needed(const ASTNode *node) {
    if (node->requires_load) {
        // Stack grows downward; stack slots are at negative offsets from FP
        printf("    ldr r%d, [fp, #%d]\n", node->register_assigned, -(node->stack_slot + 1) * 4);
    }
}

/**
 * @brief Emit a store instruction if the node is marked as requiring one
 *
 * @param node The AST node to store
 */
static void emit_store_if_needed(const ASTNode *node) {
    if (node->requires_store) {
        printf("    str r%d, [fp, #%d]\n", node->register_assigned, -(node->stack_slot + 1) * 4);
    }
}

/**
 * @brief Recursively emit ARM instructions for an expression subtree
 *
 * @param node The AST node representing an expression
 */
static void codegen_expr(const ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_INT_LITERAL:
            if (node->register_assigned >= 0) {
                printf("    mov r%d, #%ld\n", node->register_assigned, node->token.literal.int_value);
            }
            break;

        case NODE_IDENTIFIER:
            if (node->requires_load) {
                emit_load_if_needed(node);
            } else if (node->source_register != node->register_assigned) {
                printf("    mov r%d, r%d\n", node->register_assigned, node->source_register);
            }
            break;

        case NODE_ADD: {
            codegen_expr(node->children[0]);
            emit_load_if_needed(node->children[0]);

            codegen_expr(node->children[1]);
            emit_load_if_needed(node->children[1]);

            const int dst = node->register_assigned;
            const int lhs = node->children[0]->register_assigned;
            const int rhs = node->children[1]->register_assigned;

            printf("    add r%d, r%d, r%d\n", dst, lhs, rhs);
            break;
        }

        case NODE_ASSIGNMENT: {
            const ASTNode *rhs = node->children[1];

            codegen_expr(rhs);
            emit_load_if_needed(rhs);

            if (rhs->register_assigned != node->register_assigned) {
                printf("    mov r%d, r%d\n", node->register_assigned, rhs->register_assigned);
            }

            emit_store_if_needed(node);
            break;
        }

        case NODE_FUNCTION_CALL: {
            for (size_t i = 0; i < node->child_count; i++) {
                codegen_expr(node->children[i]);

                // Assign function parameters to registers r0, r1, r2 and r3
                if (node->children[i]->register_assigned != (int) i) {
                    printf("    mov r%zu, r%d\n", i, node->children[i]->register_assigned);
                }
            }

            // Call the function
            printf("    bl %s\n", node->token.lexeme);

            // Move return value from r0 if needed
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
 * @brief Emit ARM instructions for a statement node
 *
 * @param node The AST node representing a statement
 */
static void codegen_stmt(const ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_VAR_DECL:
            codegen_expr(node->children[2]);
            emit_store_if_needed(node);
            break;

        case NODE_RETURN: {
            const ASTNode *retval = node->children[0];
            codegen_expr(retval);

            if (retval->type == NODE_INT_LITERAL) {
                printf("    mov r0, #%ld\n", retval->token.literal.int_value);
            } else {
                emit_load_if_needed(retval);
                printf("    mov r0, r%d\n", retval->register_assigned);
            }
            break;
        }

        case NODE_EXPRESSION:
            codegen_expr(node->children[0]);
            emit_load_if_needed(node->children[0]);
            break;

        default:
            break;
    }
}

/**
 * @brief Emit ARM instructions for a function definition
 *
 * @param node The AST node representing a function
 */
static void codegen_function(const ASTNode *node) {
    if (!node || node->type != NODE_FUNCTION) return;

    const char *func_name = node->children[0]->token.lexeme;

    printf("\n%s:\n", func_name);

    // Function prologue: preserve FP & LR, set up new frame
    printf("    push {fp, lr}\n");
    printf("    mov fp, sp\n");
    printf("    sub sp, sp, #512\n"); // Fixed frame size for now

    // Store function parameters in their assigned stack slots
    int stack_slot = 0;
    for (size_t i = 0; i < node->child_count; ++i) {
        const ASTNode *child = node->children[i];
        if (child->type == NODE_TYPE_PARAM) {
            printf("    str r%d, [fp, #%d]\n", stack_slot, -(stack_slot + 1) * 4);
            stack_slot++;
        }
    }

    // Emit function body (statements)
    for (size_t i = 0; i < node->child_count; ++i) {
        const ASTNode *child = node->children[i];
        switch (child->type) {
            case NODE_VAR_DECL:
            case NODE_RETURN:
            case NODE_EXPRESSION:
            case NODE_ASSIGNMENT:
                codegen_stmt(child);
                break;
            default:
                break;
        }
    }

    // Function epilogue: restore frame and return
    printf("    add sp, fp, #0\n");
    printf("    pop {fp, pc}\n");
}

/**
 * @brief Entry point for ARM code generation
 *
 * @param root The root of the AST (should be NODE_COMPILATION_UNIT)
 */
void codegen_arm(const ASTNode *root) {
    if (!root || root->type != NODE_COMPILATION_UNIT) return;

    emit_text_section();
    emit_global_directives(root);

    for (size_t i = 0; i < root->child_count; ++i) {
        codegen_function(root->children[i]);
    }
}

