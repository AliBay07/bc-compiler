/**
* @file register_allocator.h
 * @brief Register allocation for AST nodes in BasicCodeCompiler.
 *
 * This file declares the public interface for the register allocator,
 * which assigns ARM registers to AST variables on a per-function basis
 * and handles spilling to the stack when registers are exhausted.
 */

#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "parser.h"
#include <stdbool.h>

#define FIRST_VAR_REGISTER   4    ///< First general-purpose register available for variables (r4)
#define MAX_REGISTERS       12    ///< Total number of available registers (r0–r11)
#define MAX_VARIABLES      128    ///< Maximum number of tracked variables per function

/**
 * @brief Metadata about register allocation for a single variable.
 */
typedef struct {
    int assigned_reg;   ///< Assigned register index (0–11), or –1 if none
    int stack_slot;     ///< Stack slot index if spilled, or –1 otherwise
    int live_start;     ///< Instruction index where variable becomes live
    int live_end;       ///< Instruction index where variable is last used
    bool is_spilled;    ///< True if variable was spilled to the stack
} RegisterAllocationInfo;

/**
 * @brief Perform register allocation on the given AST.
 *
 * This will partition registers per function, assign r0–r3 to parameters
 * (always loaded from stack on use), assign r4–r11 to locals, and spill
 * when more than eight locals are live.  All contexts are isolated per
 * function to prevent cross-function interference.
 *
 * @param root           Root of the AST (COMPILATION_UNIT node).
 * @param show_registers If true, prints detailed mapping (for debugging).
 */
void register_allocate_ast(ASTNode *root, bool show_registers);

/**
 * @brief Reset any global allocator state.
 *
 * Must be called before any allocation pass (internally invoked by
 * register_allocate_ast).
 */
void initialize_registers(void);

#endif // REGISTER_ALLOCATOR_H
