/**
 * @file register_allocator.c
 * @brief Linear scan register allocation for BasicCodeCompiler.
 *
 * This file implements a register allocator using a simple linear scan strategy.
 * Each function has its own isolated register context to prevent cross-function interference.
 * Live ranges of variables are tracked and registers are assigned accordingly, with spilling support.
 */

#include "../include/register_allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define CONTEXT_STACK_MAX_DEPTH 32

/**
 * @brief Live range metadata for a variable.
 */
typedef struct {
    const char *var_name;
    int start_idx, end_idx;
    int assigned_reg;
    int stack_slot;
    bool is_spilled;
} VariableLiveRange;

/**
 * @brief Mapping of variables to registers in a function context.
 */
typedef struct {
    const char *reg_variable_map[MAX_REGISTERS];
    int reg_usage[MAX_REGISTERS];
} RegisterContext;

static VariableLiveRange live_ranges[MAX_VARIABLES];
static int live_range_count = 0;
static int stack_slot_counter = 0;

static RegisterContext context_stack[CONTEXT_STACK_MAX_DEPTH];
static int context_stack_top = 0;

static void push_register_context(RegisterContext *current) {
    if (context_stack_top >= CONTEXT_STACK_MAX_DEPTH) {
        fprintf(stderr, "Register context stack overflow\n");
        abort();
    }
    context_stack[context_stack_top++] = *current;
}

static void pop_register_context(RegisterContext *current) {
    if (context_stack_top <= 0) {
        fprintf(stderr, "Register context stack underflow\n");
        abort();
    }
    *current = context_stack[--context_stack_top];
}

static int find_live_range(const char *var_name) {
    for (int i = 0; i < live_range_count; i++) {
        if (strcmp(live_ranges[i].var_name, var_name) == 0)
            return i;
    }
    return -1;
}

static int add_live_range(const char *var_name) {
    int idx = live_range_count++;
    live_ranges[idx] = (VariableLiveRange){
        .var_name = var_name,
        .start_idx = -1,
        .end_idx = -1,
        .assigned_reg = -1,
        .stack_slot = -1,
        .is_spilled = false
    };
    return idx;
}

static void annotate_live_ranges(ASTNode *node, int *idx) {
    if (!node) return;

    if (node->type == NODE_VAR_DECL) {
        const char *var = node->children[0]->token.lexeme;
        int lr = find_live_range(var);
        if (lr == -1) lr = add_live_range(var);
        live_ranges[lr].start_idx = *idx;
        live_ranges[lr].end_idx = *idx;
    }

    if (node->type == NODE_IDENTIFIER) {
        const char *var = node->token.lexeme;
        int lr = find_live_range(var);
        if (lr == -1) lr = add_live_range(var);
        if (live_ranges[lr].start_idx == -1)
            live_ranges[lr].start_idx = *idx;
        if (live_ranges[lr].end_idx < *idx)
            live_ranges[lr].end_idx = *idx;
    }

    (*idx)++;
    for (size_t i = 0; i < node->child_count; i++) {
        annotate_live_ranges(node->children[i], idx);
    }
}

static void reset_allocator_state(void) {
    live_range_count = 0;
    stack_slot_counter = 0;
    context_stack_top = 0;
}

static int find_variable_in_registers(const char *var_name, RegisterContext *ctx) {
    for (int i = FIRST_VAR_REGISTER; i <= 11; i++) {
        if (ctx->reg_variable_map[i] && strcmp(ctx->reg_variable_map[i], var_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int allocate_register(const char *for_var, RegisterContext *ctx, int *spilled_slot) {
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
            int lr = find_live_range(spilled_var);
            if (lr != -1 && !live_ranges[lr].is_spilled) {
                live_ranges[lr].is_spilled = true;
                live_ranges[lr].stack_slot = stack_slot_counter++;
                if (spilled_slot) *spilled_slot = live_ranges[lr].stack_slot;
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

static void allocate_expr(ASTNode *node, RegisterContext *ctx) {
    if (!node) return;

    switch (node->type) {
        case NODE_INT_LITERAL:
            node->register_assigned = -1;
            break;
        case NODE_IDENTIFIER: {
            const char *var = node->token.lexeme;
            int reg = find_variable_in_registers(var, ctx);
            int lr = find_live_range(var);
            if (lr == -1) lr = add_live_range(var);

            if (reg != -1) {
                node->register_assigned = reg;
                node->requires_load = false;
            } else {
                reg = allocate_register(var, ctx, NULL);
                node->register_assigned = reg;
                node->requires_load = live_ranges[lr].is_spilled;
                node->stack_slot = live_ranges[lr].is_spilled ? live_ranges[lr].stack_slot : -1;
            }
            break;
        }
        case NODE_ADD:
            allocate_expr(node->children[0], ctx);
            allocate_expr(node->children[1], ctx);
            node->register_assigned = -1;
            break;
        case NODE_FUNCTION_CALL:
            for (size_t i = 0; i < node->child_count; i++)
                allocate_expr(node->children[i], ctx);
            node->register_assigned = 0;
            break;
        default:
            break;
    }
}

static void allocate_registers(ASTNode *node, int *idx, RegisterContext *ctx, bool show_registers) {
    if (!node) return;

    if (node->type == NODE_FUNCTION) {
        // Save parent context
        push_register_context(ctx);
        // Create a new context for this function
        RegisterContext child_ctx = {0};

        int param_count = 0;
        for (size_t i = 0; i < node->child_count; ++i) {
            ASTNode *child = node->children[i];
            if (child->type == NODE_TYPE_PARAM && param_count < 4) {
                ctx->reg_variable_map[param_count] = child->token.lexeme;
                ctx->reg_usage[param_count] = 1;
                param_count++;
            }
        }

        for (size_t i = 0; i < node->child_count; ++i) {
            allocate_registers(node->children[i], idx, &child_ctx, show_registers);
        }

        // Restore parent context
        pop_register_context(ctx);
        return;
    }

    switch (node->type) {
        case NODE_TYPE_PARAM: {
            static int param_idx = 0;
            if (param_idx < 4) {
                ctx->reg_variable_map[param_idx] = node->token.lexeme;
                ctx->reg_usage[param_idx] = 1;
                node->register_assigned = param_idx;
                if (show_registers) {
                    printf("// Parameter '%s' assigned to register r%d\n", node->token.lexeme, param_idx);
                }
                param_idx++;
            }
            break;
        }
        case NODE_VAR_DECL: {
            const char *var = node->children[0]->token.lexeme;
            const int lr = find_live_range(var);
            ASTNode *expr = node->children[2];
            allocate_expr(expr, ctx);

            int spilled_slot = -1;
            const int reg = allocate_register(var, ctx, &spilled_slot);
            node->register_assigned = reg;
            ctx->reg_variable_map[reg] = var;
            ctx->reg_usage[reg] = 1;
            live_ranges[lr].assigned_reg = reg;

            expr->register_assigned = reg;

            if (live_ranges[lr].is_spilled) {
                node->requires_store = true;
                node->stack_slot = live_ranges[lr].stack_slot;
            } else {
                node->requires_store = false;
            }

            if (show_registers) {
                printf("// Variable '%s' assigned to %s%d\n", var,
                       live_ranges[lr].is_spilled ? "stack slot " : "register r",
                       live_ranges[lr].is_spilled ? live_ranges[lr].stack_slot : reg);
            }
            break;
        }
        case NODE_RETURN:
            if (node->child_count > 0)
                allocate_expr(node->children[0], ctx);
            break;
        case NODE_FUNCTION_CALL:
            for (size_t i = 0; i < node->child_count; i++)
                allocate_expr(node->children[i], ctx);
            node->register_assigned = 0;
            break;
        default:
            for (size_t i = 0; i < node->child_count; i++)
                allocate_registers(node->children[i], idx, ctx, show_registers);
            break;
    }

    (*idx)++;
}

void register_allocate_ast(ASTNode *node, bool show_registers) {
    reset_allocator_state();
    int idx = 0;
    annotate_live_ranges(node, &idx);
    idx = 0;
    RegisterContext ctx = {0};
    allocate_registers(node, &idx, &ctx, show_registers);
}
