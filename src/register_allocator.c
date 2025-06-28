/**
 * @file register_allocator.c
 * @brief Linear scan register allocation for BasicCodeCompiler.
 *
 * This file implements a register allocator using a simple linear scan strategy.
 * Each function has its own isolated register and stack context to prevent cross-function interference.
 * Live ranges of variables are tracked and registers are assigned accordingly, with spilling support.
 * Parameters are always loaded from the stack when used (for now).
 *
 */

#include "../include/register_allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define CONTEXT_STACK_MAX_DEPTH 32
#define MAX_VARIABLES_PER_FUNCTION 64

/**
 * @brief Live range metadata for a variable.
 */
typedef struct {
    const char *var_name;
    int start_idx, end_idx;
    int assigned_reg;          // Register allocated to this variable
    int current_value_reg;     // Register currently holding the variable's value (-1 if not in register)
    int stack_slot;
    bool is_spilled;
} VariableLiveRange;

/**
 * @brief Stack slot mapping for variables.
 */
typedef struct {
    const char *var_name;
    int stack_slot;
} StackSlot;

/**
 * @brief Complete context for a function (registers + stack).
 */
typedef struct {
    // Register state
    const char *reg_variable_map[MAX_REGISTERS];
    int reg_usage[MAX_REGISTERS];

    // Stack state
    StackSlot stack_map[MAX_VARIABLES_PER_FUNCTION];
    int stack_map_count;
    int stack_slot_counter;

    // Live ranges for this function
    VariableLiveRange live_ranges[MAX_VARIABLES_PER_FUNCTION];
    int live_range_count;
} FunctionContext;

static FunctionContext context_stack[CONTEXT_STACK_MAX_DEPTH];
static int context_stack_top = 0;

static void push_function_context(const FunctionContext *current) {
    if (context_stack_top >= CONTEXT_STACK_MAX_DEPTH) {
        fprintf(stderr, "Function context stack overflow\n");
        abort();
    }
    context_stack[context_stack_top++] = *current;
}

static void pop_function_context(FunctionContext *current) {
    if (context_stack_top <= 0) {
        fprintf(stderr, "Function context stack underflow\n");
        abort();
    }
    *current = context_stack[--context_stack_top];
}

static int find_live_range(const FunctionContext *ctx, const char *var_name) {
    for (int i = 0; i < ctx->live_range_count; i++) {
        if (strcmp(ctx->live_ranges[i].var_name, var_name) == 0)
            return i;
    }
    return -1;
}

static int add_live_range(FunctionContext *ctx, const char *var_name) {
    // Check for duplicate variable in current function
    if (find_live_range(ctx, var_name) != -1) {
        fprintf(stderr, "Error: Redeclaration of variable '%s'\n", var_name);
        abort();
    }

    const int idx = ctx->live_range_count++;
    ctx->live_ranges[idx] = (VariableLiveRange){
        .var_name = var_name,
        .start_idx = -1,
        .end_idx = -1,
        .assigned_reg = -1,
        .current_value_reg = -1,
        .stack_slot = -1,
        .is_spilled = false
    };
    return idx;
}

static int find_stack_slot(const FunctionContext *ctx, const char *var_name) {
    for (int i = 0; i < ctx->stack_map_count; i++) {
        if (strcmp(ctx->stack_map[i].var_name, var_name) == 0) {
            return ctx->stack_map[i].stack_slot;
        }
    }
    return -1;
}

static void add_stack_slot(FunctionContext *ctx, const char *var_name) {
    // Check for duplicate variable in stack
    if (find_stack_slot(ctx, var_name) != -1) {
        fprintf(stderr, "Error: Redeclaration of variable '%s'\n", var_name);
        abort();
    }

    const int slot = ctx->stack_slot_counter++;
    ctx->stack_map[ctx->stack_map_count++] = (StackSlot){
        .var_name = var_name,
        .stack_slot = slot
    };
}

/**
 * @brief Update the current register holding a variable's value
 *
 * @param ctx Function context
 * @param var_name Variable name
 * @param reg Register currently holding the value (-1 if not in register)
 */
static void update_variable_location(FunctionContext *ctx, const char *var_name, const int reg) {
    const int lr = find_live_range(ctx, var_name);
    if (lr != -1) {
        ctx->live_ranges[lr].current_value_reg = reg;
    }
}

static void annotate_live_ranges(ASTNode *node, int *idx, FunctionContext *ctx) {
    if (!node) return;

    if (node->type == NODE_VAR_DECL) {
        const char *var = node->children[0]->token.lexeme;
        int lr = find_live_range(ctx, var);
        if (lr == -1) lr = add_live_range(ctx, var);
        ctx->live_ranges[lr].start_idx = *idx;
        ctx->live_ranges[lr].end_idx = *idx;
    }

    if (node->type == NODE_IDENTIFIER) {
        const char *var = node->token.lexeme;
        int lr = find_live_range(ctx, var);
        if (lr == -1) lr = add_live_range(ctx, var);
        if (ctx->live_ranges[lr].start_idx == -1)
            ctx->live_ranges[lr].start_idx = *idx;
        if (ctx->live_ranges[lr].end_idx < *idx)
            ctx->live_ranges[lr].end_idx = *idx;
    }

    (*idx)++;
    for (size_t i = 0; i < node->child_count; i++) {
        annotate_live_ranges(node->children[i], idx, ctx);
    }
}

static int find_variable_in_registers(const char *var_name, const FunctionContext *ctx) {
    for (int i = FIRST_VAR_REGISTER; i <= 11; i++) {
        if (ctx->reg_variable_map[i] && strcmp(ctx->reg_variable_map[i], var_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int allocate_register(const char *for_var, FunctionContext *ctx, int *spilled_slot) {
    for (int i = FIRST_VAR_REGISTER; i <= 11; i++) {
        if (!ctx->reg_usage[i]) {
            ctx->reg_usage[i] = 1;
            ctx->reg_variable_map[i] = for_var;
            return i;
        }
    }

    for (int i = FIRST_VAR_REGISTER; i <= 11; i++) {
        const char *spilled_var = ctx->reg_variable_map[i];
        if (spilled_var) {
            const int lr = find_live_range(ctx, spilled_var);
            if (lr != -1 && !ctx->live_ranges[lr].is_spilled) {
                ctx->live_ranges[lr].is_spilled = true;
                ctx->live_ranges[lr].stack_slot = ctx->stack_slot_counter;
                add_stack_slot(ctx, spilled_var);
                if (spilled_slot) *spilled_slot = ctx->live_ranges[lr].stack_slot;
            }
            ctx->reg_usage[i] = 0;
            ctx->reg_variable_map[i] = NULL;
            ctx->reg_usage[i] = 1;
            ctx->reg_variable_map[i] = for_var;
            return i;
        }
    }

    return FIRST_VAR_REGISTER;
}

static void allocate_expr(ASTNode *node, FunctionContext *ctx) {
    if (!node) return;

    switch (node->type) {
        case NODE_INT_LITERAL:
            node->register_assigned = -1;
            break;
        case NODE_IDENTIFIER: {
            const char *var = node->token.lexeme;
            int reg = find_variable_in_registers(var, ctx);
            int lr = find_live_range(ctx, var);
            if (lr == -1) lr = add_live_range(ctx, var);

            const int current_reg = ctx->live_ranges[lr].current_value_reg;
            if (current_reg != -1) {
                node->register_assigned = current_reg;
                node->source_register = current_reg;
                node->requires_load = false;
                break;
            }

            const int stack_slot = find_stack_slot(ctx, var);
            if (stack_slot != -1) {
                if (reg == -1) {
                    reg = allocate_register(var, ctx, NULL);
                }
                node->register_assigned = reg;
                node->requires_load = true;
                node->stack_slot = stack_slot;
                node->source_register = -1;
                update_variable_location(ctx, var, reg);
            } else if (reg != -1) {
                node->register_assigned = reg;
                node->requires_load = ctx->live_ranges[lr].is_spilled;
                node->stack_slot = ctx->live_ranges[lr].is_spilled ?
                                   ctx->live_ranges[lr].stack_slot : -1;
                node->source_register = node->requires_load ? -1 : reg;
                if (!node->requires_load) {
                    update_variable_location(ctx, var, reg);
                }
            } else {
                reg = allocate_register(var, ctx, NULL);
                node->register_assigned = reg;
                node->requires_load = ctx->live_ranges[lr].is_spilled;
                node->stack_slot = ctx->live_ranges[lr].is_spilled ?
                                   ctx->live_ranges[lr].stack_slot : -1;
                node->source_register = node->requires_load ? -1 : reg;
                if (!node->requires_load) {
                    update_variable_location(ctx, var, reg);
                }
            }
            break;
        }
        case NODE_ADD: {
            allocate_expr(node->children[0], ctx);
            allocate_expr(node->children[1], ctx);

            // Allocate register for result
            int spilled_slot = -1;
            const char *result_var = "add_result";
            const int reg = allocate_register(result_var, ctx, &spilled_slot);
            node->register_assigned = reg;
            break;
        }
        case NODE_FUNCTION_CALL:
            for (size_t i = 0; i < node->child_count; i++)
                allocate_expr(node->children[i], ctx);
            node->register_assigned = 0;
            break;
        default:
            break;
    }
}

static void allocate_registers(ASTNode *node, int *idx, FunctionContext *ctx, const bool show_registers) {
    if (!node) return;

    if (node->type == NODE_FUNCTION) {
        // Save parent context
        push_function_context(ctx);
        FunctionContext child_ctx = {0};

        // Process parameters first
        int param_count = 0;
        for (size_t i = 0; i < node->child_count; ++i) {
            const ASTNode *child = node->children[i];
            if (child->type == NODE_TYPE_PARAM) {
                param_count++;
                const char *param_name = child->token.lexeme;
                // Allocate stack slot for parameter
                add_stack_slot(&child_ctx, param_name);
                if (show_registers) {
                    printf("Parameter '%s' assigned to stack slot %d\n",
                           param_name, child_ctx.stack_map[child_ctx.stack_map_count-1].stack_slot);
                }
            }
        }
        child_ctx.stack_slot_counter = param_count;

        // Annotate live ranges for this function
        int func_idx = 0;
        annotate_live_ranges(node, &func_idx, &child_ctx);

        // Allocate registers for function body
        func_idx = 0;
        for (size_t i = 0; i < node->child_count; ++i) {
            allocate_registers(node->children[i], &func_idx, &child_ctx, show_registers);
        }

        // Restore parent context
        pop_function_context(ctx);
        return;
    }

    switch (node->type) {
        case NODE_TYPE_PARAM:
            // Parameters are handled in NODE_FUNCTION case
            break;
        case NODE_VAR_DECL: {
            const char *var = node->children[0]->token.lexeme;
            const int lr = find_live_range(ctx, var);
            ASTNode *expr = node->children[2];
            allocate_expr(expr, ctx);

            int spilled_slot = -1;
            const int reg = allocate_register(var, ctx, &spilled_slot);
            node->register_assigned = reg;
            ctx->reg_variable_map[reg] = var;
            ctx->reg_usage[reg] = 1;
            if (lr != -1) {
                ctx->live_ranges[lr].assigned_reg = reg;
                ctx->live_ranges[lr].current_value_reg = reg;
            }

            expr->register_assigned = reg;

            if (spilled_slot != -1) {
                node->requires_store = true;
                node->stack_slot = spilled_slot;
                ctx->live_ranges[lr].is_spilled = true;
                ctx->live_ranges[lr].stack_slot = spilled_slot;
                update_variable_location(ctx, var, -1); // Value not in register after store
            } else {
                node->requires_store = false;
                update_variable_location(ctx, var, reg);
            }

            if (show_registers) {
                const char *loc_type = "register r";
                int loc = reg;
                if (node->requires_store) {
                    loc_type = "stack slot ";
                    loc = node->stack_slot;
                }
                printf("Variable '%s' assigned to %s%d\n", var, loc_type, loc);
            }
            break;
        }
        case NODE_RETURN:
            if (node->child_count > 0) {
                allocate_expr(node->children[0], ctx);
            }
            break;
        case NODE_FUNCTION_CALL:
            for (size_t i = 0; i < node->child_count; i++) {
                allocate_expr(node->children[i], ctx);
            }
            node->register_assigned = 0;
            break;
        case NODE_ASSIGNMENT: {
            const char *var = node->children[0]->token.lexeme;
            ASTNode *expr = node->children[1];
            allocate_expr(expr, ctx);

            int reg = find_variable_in_registers(var, ctx);
            const int lr = find_live_range(ctx, var);
            if (lr == -1) {
                fprintf(stderr, "Error: Assignment to undeclared variable '%s'\n", var);
                abort();
            }

            if (reg == -1) {
                reg = allocate_register(var, ctx, NULL);
            }

            node->register_assigned = reg;
            expr->register_assigned = reg;
            update_variable_location(ctx, var, reg);
            break;
        }
        default:
            for (size_t i = 0; i < node->child_count; i++) {
                allocate_registers(node->children[i], idx, ctx, show_registers);
            }
            break;
    }

    (*idx)++;
}

void register_allocate_ast(ASTNode *node, const bool show_registers) {
    FunctionContext root_ctx = {0};
    int idx = 0;
    allocate_registers(node, &idx, &root_ctx, show_registers);
}
